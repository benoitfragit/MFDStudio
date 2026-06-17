/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Document load/save/recovery, asset watching, and undo/redo extracted from `EditorApplication`.
 */

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "EditorSnapping.h"
#include "internal/application/EditorApplicationInternal.h"
#include "internal/application/EditorViewportGrid.h"

namespace
{
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::DefaultPageIndex;
using editor::detail::kRecoveryFileName;

mfd::Vec2 ApplyNudgeToPosition(mfd::Vec2 position, const mfd::Vec2 delta, const bool snap, const float gridStep)
{
    position.x = std::clamp(position.x + delta.x, -1.0f, 1.0f);
    position.y = std::clamp(position.y + delta.y, -1.0f, 1.0f);
    return snap ? editor::app::SnapToGrid(position, gridStep) : position;
}
}

bool EditorApplication::LoadWindowConfiguration(const std::filesystem::path& path)
{
    try
    {
        documentState_.loaded = documentState_.loader.LoadWindowConfiguration(path);
        for (auto& page : documentState_.loaded.document.pages)
        {
            BootstrapEditorLayersForPage(page);
        }
        documentState_.files.pageFiles = documentState_.loaded.window.pageFiles;
        documentState_.files.removedPageFiles.clear();
        documentState_.files.removedTemplateFiles.clear();
        std::string error;
        if (!editor::DiscoverReticleTemplateFiles(documentState_.loaded.window.reticleLibraryFolder, documentState_.files, &error))
        {
            throw std::runtime_error(error);
        }

        ApplyPreviewFontFile(documentState_.loaded.window.fontFile);
        documentState_.selection = {};
        layoutState_.layerFocusState = {};
        SelectPage(DefaultPageIndex(documentState_.loaded.document.pages));
        ResetLibraryPreviewView();
        documentState_.history.Clear();
        InvalidateReticleUsageHighlightCache();
        documentState_.windowFile = path;
        workflowState_.documentDirty = false;
        autosave_.Reset();
        RearmAssetWatcher();
        workflowState_.lastRuntimeError.clear();
        RebuildStatus("Editor loaded '" + documentState_.loaded.window.title + "'.", false);
        return true;
    }
    catch (const std::exception& exception)
    {
        RebuildStatus(exception.what(), true);
        return false;
    }
}

bool EditorApplication::SaveAll()
{
    if (!HasOpenWindow())
    {
        RebuildStatus("No window asset is open yet. Create or open one before saving.", true);
        return false;
    }

    std::string error;
    if (!editor::SaveEditorDocument(documentState_.loaded, documentState_.files, &error))
    {
        RebuildStatus("Save failed: " + error, true);
        return false;
    }

    workflowState_.documentDirty = false;
    autosave_.Reset();
    RearmAssetWatcher();
    ClearRecoverySnapshot();
    RebuildStatus("Window, pages and library saved.", false);
    return true;
}

void EditorApplication::WriteRecoverySnapshot()
{
    if (!HasOpenWindow())
    {
        return;
    }

    try
    {
        const std::string bundle =
            editor::SerializeRecoveryBundleToJsonString(documentState_.loaded, documentState_.files);
        const std::filesystem::path path = documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName);
        if (path.has_parent_path())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(path.parent_path(), directoryError);
        }

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (stream.is_open())
        {
            stream << bundle;
        }
    }
    catch (const std::exception&)
    {
        // Best-effort recovery snapshot: never interrupt authoring if it cannot be written.
    }
}

void EditorApplication::ClearRecoverySnapshot()
{
    std::error_code removeError;
    std::filesystem::remove(documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName), removeError);
}

void EditorApplication::RecoverPreviousSession()
{
    const std::filesystem::path path = documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName);
    std::string bundle;
    try
    {
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        bundle = buffer.str();
    }
    catch (const std::exception&)
    {
        bundle.clear();
    }

    // The recovery file lives at the asset root, so its parent bounds every legitimate authored file.
    const std::filesystem::path assetRoot = path.parent_path();

    std::filesystem::path windowFile;
    std::string error;
    if (!bundle.empty() && editor::RestoreRecoveryBundle(bundle, assetRoot, windowFile, &error) &&
        LoadWindowConfiguration(windowFile))
    {
        RebuildStatus("Recovered unsaved work from the previous session.", false);
        // Only discard the snapshot once the restore and reload both succeeded.
        ClearRecoverySnapshot();
    }
    else if (!error.empty())
    {
        RebuildStatus("Recovery failed (snapshot kept): " + error, true);
    }
    else
    {
        RebuildStatus("Recovery snapshot could not be restored (snapshot kept).", true);
    }
}

std::vector<std::filesystem::path> EditorApplication::CollectWatchedAssetFiles() const
{
    std::vector<std::filesystem::path> files;
    if (!HasOpenWindow())
    {
        return files;
    }

    files.reserve(1U + documentState_.files.pageFiles.size() + documentState_.files.templateFiles.size());
    files.push_back(documentState_.loaded.window.sourceFile);
    for (const std::filesystem::path& pageFile : documentState_.files.pageFiles)
    {
        files.push_back(pageFile);
    }
    for (const auto& entry : documentState_.files.templateFiles)
    {
        files.push_back(entry.second);
    }

    return files;
}

void EditorApplication::RearmAssetWatcher()
{
    assetWatcher_.Watch(CollectWatchedAssetFiles());
    assetWatchAccumulatorSeconds_ = 0.0f;
}

void EditorApplication::Undo()
{
    std::optional<UndoSnapshot> previous = documentState_.history.Undo(CaptureCurrentSnapshot());
    if (!previous.has_value())
    {
        RebuildStatus("Nothing to undo.", true);
        return;
    }

    RestoreSnapshot(std::move(*previous));
    RebuildStatus("Undo applied.", false);
}

void EditorApplication::Redo()
{
    std::optional<UndoSnapshot> next = documentState_.history.Redo(CaptureCurrentSnapshot());
    if (!next.has_value())
    {
        RebuildStatus("Nothing to redo.", true);
        return;
    }

    RestoreSnapshot(std::move(*next));
    RebuildStatus("Redo applied.", false);
}

EditorApplication::UndoSnapshot EditorApplication::CaptureCurrentSnapshot() const
{
    return UndoSnapshot {
        documentState_.loaded,
        documentState_.files,
        documentState_.selection,
        layoutState_.pagePreviewView,
        layoutState_.libraryPreviewView};
}

void EditorApplication::RestoreSnapshot(UndoSnapshot snapshot)
{
    documentState_.loaded = std::move(snapshot.loaded);
    documentState_.files = std::move(snapshot.files);
    documentState_.selection = std::move(snapshot.selection);
    layoutState_.pagePreviewView = snapshot.pagePreviewView;
    layoutState_.pagePreviewView.zoom = mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom);
    layoutState_.libraryPreviewView = snapshot.libraryPreviewView;
    layoutState_.libraryPreviewView.zoom = mfd::SanitizeZoom(layoutState_.libraryPreviewView.zoom);

    if (documentState_.selection.pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        SelectPage(documentState_.loaded.document.pages.empty() ? 0 : static_cast<int>(documentState_.loaded.document.pages.size()) - 1);
    }

    SanitizeLayerFocusForActivePage();
    SanitizePageReticleSelectionForCurrentFocus();
    InvalidateReticleUsageHighlightCache();
    workflowState_.documentDirty = true;
}

void EditorApplication::PushUndoSnapshot()
{
    PushUndoSnapshot(CaptureCurrentSnapshot());
}

void EditorApplication::PushUndoSnapshot(UndoSnapshot snapshot)
{
    documentState_.history.Record(std::move(snapshot));
    workflowState_.documentDirty = true;
    InvalidateReticleUsageHighlightCache();
}

void EditorApplication::NudgeSelection(const mfd::Vec2 delta)
{
    if (delta.x == 0.0f && delta.y == 0.0f)
    {
        return;
    }

    const bool snap = layoutState_.pagePreviewViewOptions.snapToGrid;
    const float gridStep = editor::app::SanitizeGridStepLogical(layoutState_.pagePreviewViewOptions.gridStepLogical);

    if (IsPageTitleSelected())
    {
        if (mfd::PageTitleDisplayDefinition* title = SelectedPageTitleDisplay(); title != nullptr)
        {
            PushUndoSnapshot();
            title->transform.position = ApplyNudgeToPosition(title->transform.position, delta, snap, gridStep);
        }
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedReticles = SelectedPageReticleIndices();
    if (page != nullptr && !selectedReticles.empty())
    {
        PushUndoSnapshot();
        for (const int index : selectedReticles)
        {
            if (index >= 0 && index < static_cast<int>(page->staticReticles.size()))
            {
                mfd::Vec2& position = page->staticReticles[static_cast<std::size_t>(index)].transform.position;
                position = ApplyNudgeToPosition(position, delta, snap, gridStep);
            }
        }
        return;
    }

    if (mfd::ReticleGroup* editable = SelectedEditablePageReticle(); editable != nullptr)
    {
        PushUndoSnapshot();
        editable->transform.position = ApplyNudgeToPosition(editable->transform.position, delta, snap, gridStep);
    }
}

