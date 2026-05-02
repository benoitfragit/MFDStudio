/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Concrete bridge mapping automation operations onto one live editor application.
 */

#include "EditorAutomationBridge.h"

#include <algorithm>
#include <optional>

#include "EditorAutomationDocumentUtils.h"
#include "EditorApplication.h"
#include "mfd/model/Types.h"

namespace editor::automation
{
EditorApplicationAutomationBridge::EditorApplicationAutomationBridge(::EditorApplication& application)
    : application_(application)
{
}

bool EditorApplicationAutomationBridge::HasOpenWindow() const noexcept
{
    return application_.HasOpenWindow();
}

const mfd::LoadedWindowConfiguration& EditorApplicationAutomationBridge::Loaded() const noexcept
{
    return application_.loaded_;
}

mfd::LoadedWindowConfiguration& EditorApplicationAutomationBridge::MutableLoaded() noexcept
{
    return application_.loaded_;
}

const editor::EditorFileLayout& EditorApplicationAutomationBridge::Files() const noexcept
{
    return application_.files_;
}

editor::EditorFileLayout& EditorApplicationAutomationBridge::MutableFiles() noexcept
{
    return application_.files_;
}

std::filesystem::path EditorApplicationAutomationBridge::WindowFile() const
{
    return application_.windowFile_;
}

void EditorApplicationAutomationBridge::SetWindowFile(std::filesystem::path path)
{
    application_.windowFile_ = std::move(path);
}

EditorStateSnapshot EditorApplicationAutomationBridge::CaptureSnapshot() const
{
    return EditorStateSnapshot {
        application_.windowFile_,
        application_.loaded_,
        application_.files_,
        CaptureUiState()};
}

void EditorApplicationAutomationBridge::RestoreSnapshot(const EditorStateSnapshot& snapshot)
{
    application_.windowFile_ = snapshot.windowFile;
    application_.loaded_ = snapshot.loaded;
    application_.files_ = snapshot.files;
    application_.ApplyPreviewFontFile(application_.loaded_.window.fontFile);
    application_.pagePreviewView_ = snapshot.uiState.pagePreviewView;
    application_.pagePreviewView_.zoom = mfd::SanitizeZoom(application_.pagePreviewView_.zoom);
    application_.libraryPreviewView_ = snapshot.uiState.libraryPreviewView;
    application_.libraryPreviewView_.zoom = mfd::SanitizeZoom(application_.libraryPreviewView_.zoom);
    application_.pagePreviewViewOptions_ = snapshot.uiState.pagePreviewViewOptions;

    if (!snapshot.uiState.activePageId.empty())
    {
        int pageIndex = -1;
        if (FindPageById(application_.loaded_, snapshot.uiState.activePageId, &pageIndex) != nullptr)
        {
            application_.SelectPage(pageIndex);
        }
    }
    else if (!application_.loaded_.document.pages.empty())
    {
        application_.SelectPage(0);
    }

    switch (snapshot.uiState.selectionKind)
    {
    case AutomationSelectionKind::PageReticleInstance:
    {
        bool first = true;
        for (const PageReticleInstanceId& pageReticleId : snapshot.uiState.selectedPageReticleIds)
        {
            int pageIndex = -1;
            int reticleIndex = -1;
            if (FindPageReticleById(application_.loaded_, pageReticleId, nullptr, &pageIndex, &reticleIndex) == nullptr)
            {
                continue;
            }

            if (first)
            {
                application_.SelectPageReticle(pageIndex, reticleIndex);
                first = false;
            }
            else
            {
                application_.TogglePageReticleSelection(pageIndex, reticleIndex);
            }
        }
        break;
    }
    case AutomationSelectionKind::ReticleAsset:
    {
        if (const mfd::ReticleGroup* reticle =
                FindReticleAssetById(application_.loaded_, snapshot.uiState.selectedReticleAssetId);
            reticle != nullptr)
        {
            application_.SelectLibraryReticle(reticle->id);
        }
        break;
    }
    case AutomationSelectionKind::Primitive:
    {
        if (snapshot.uiState.selectedPrimitiveOwnerKind == AutomationPrimitiveOwnerKind::ReticleAsset &&
            snapshot.uiState.selectedPrimitiveIndex >= 0)
        {
            if (const mfd::ReticleGroup* reticle = FindReticleAssetById(
                    application_.loaded_, ReticleAssetId {snapshot.uiState.selectedPrimitiveOwnerId});
                reticle != nullptr &&
                snapshot.uiState.selectedPrimitiveIndex < static_cast<int>(reticle->primitives.size()))
            {
                application_.SelectLibraryPrimitive(reticle->id, snapshot.uiState.selectedPrimitiveIndex);
            }
        }
        break;
    }
    case AutomationSelectionKind::Page:
    case AutomationSelectionKind::None:
        break;
    }
}

void EditorApplicationAutomationBridge::PushUndoSnapshot(const EditorStateSnapshot& snapshot)
{
    ::EditorApplication::Selection selection {};

    int pageIndex = 0;
    if (!snapshot.uiState.activePageId.empty())
    {
        const mfd::PageDefinition* page = FindPageById(snapshot.loaded, snapshot.uiState.activePageId, &pageIndex);
        if (page == nullptr)
        {
            pageIndex = 0;
        }
    }
    selection.pageIndex = pageIndex;

    switch (snapshot.uiState.selectionKind)
    {
    case AutomationSelectionKind::Page:
        selection.kind = ::EditorApplication::SelectionKind::Page;
        break;
    case AutomationSelectionKind::PageReticleInstance:
        selection.kind = ::EditorApplication::SelectionKind::PageReticle;
        for (const PageReticleInstanceId& reticleId : snapshot.uiState.selectedPageReticleIds)
        {
            int selectedPageIndex = -1;
            int reticleIndex = -1;
            if (FindPageReticleById(snapshot.loaded, reticleId, nullptr, &selectedPageIndex, &reticleIndex) == nullptr)
            {
                continue;
            }

            selection.pageIndex = selectedPageIndex;
            selection.pageReticleIndices.push_back(reticleIndex);
            selection.pageReticleIndex = reticleIndex;
        }
        break;
    case AutomationSelectionKind::ReticleAsset:
        selection.kind = ::EditorApplication::SelectionKind::LibraryReticle;
        if (const mfd::ReticleGroup* reticle = FindReticleAssetById(snapshot.loaded, snapshot.uiState.selectedReticleAssetId);
            reticle != nullptr)
        {
            selection.libraryReticleId = reticle->id;
            selection.libraryBrowserReticleId = reticle->id;
        }
        break;
    case AutomationSelectionKind::Primitive:
        selection.kind = ::EditorApplication::SelectionKind::LibraryPrimitive;
        if (const mfd::ReticleGroup* reticle = FindReticleAssetById(
                snapshot.loaded, ReticleAssetId {snapshot.uiState.selectedPrimitiveOwnerId});
            reticle != nullptr)
        {
            selection.libraryReticleId = reticle->id;
            selection.libraryBrowserReticleId = reticle->id;
            selection.primitiveIndex = snapshot.uiState.selectedPrimitiveIndex;
        }
        break;
    case AutomationSelectionKind::None:
        break;
    }

    if (application_.undoStack_.size() >= 64U)
    {
        application_.undoStack_.erase(application_.undoStack_.begin());
    }

    application_.undoStack_.push_back(
        EditorApplication::UndoSnapshot {
            snapshot.loaded,
            snapshot.files,
            selection,
            snapshot.uiState.pagePreviewView,
            snapshot.uiState.libraryPreviewView});
}

AutomationUiState EditorApplicationAutomationBridge::CaptureUiState() const
{
    AutomationUiState state;
    if (const mfd::PageDefinition* page = application_.ActivePage(); page != nullptr)
    {
        state.activePageId = MakePageId(*page);
    }

    state.pagePreviewView = application_.pagePreviewView_;
    state.libraryPreviewView = application_.libraryPreviewView_;
    state.pagePreviewViewOptions = application_.pagePreviewViewOptions_;

    switch (application_.selection_.kind)
    {
    case EditorApplication::SelectionKind::Page:
        state.selectionKind = AutomationSelectionKind::Page;
        break;
    case EditorApplication::SelectionKind::PageReticle:
    {
        state.selectionKind = AutomationSelectionKind::PageReticleInstance;
        if (const mfd::PageDefinition* page = application_.ActivePage(); page != nullptr)
        {
            for (const int reticleIndex : application_.SelectedPageReticleIndices())
            {
                if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
                {
                    continue;
                }

                state.selectedPageReticleIds.push_back(
                    MakePageReticleInstanceId(*page, page->staticReticles[static_cast<std::size_t>(reticleIndex)]));
            }
        }
        break;
    }
    case EditorApplication::SelectionKind::LibraryReticle:
        state.selectionKind = AutomationSelectionKind::ReticleAsset;
        state.selectedReticleAssetId = MakeReticleAssetId(application_.selection_.libraryReticleId);
        break;
    case EditorApplication::SelectionKind::LibraryPrimitive:
    {
        state.selectionKind = AutomationSelectionKind::Primitive;
        state.selectedReticleAssetId = MakeReticleAssetId(application_.selection_.libraryReticleId);
        state.selectedPrimitiveOwnerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
        state.selectedPrimitiveOwnerId = state.selectedReticleAssetId.value;
        state.selectedPrimitiveIndex = application_.selection_.primitiveIndex;
        if (const mfd::ReticleGroup* reticle = application_.SelectedLibraryReticle();
            reticle != nullptr &&
            application_.selection_.primitiveIndex >= 0 &&
            application_.selection_.primitiveIndex < static_cast<int>(reticle->primitives.size()))
        {
            state.selectedPrimitiveId = MakeReticleAssetPrimitiveId(
                reticle->id,
                reticle->primitives[static_cast<std::size_t>(application_.selection_.primitiveIndex)],
                application_.selection_.primitiveIndex);
        }
        break;
    }
    }

    return state;
}

void EditorApplicationAutomationBridge::SelectPage(const int pageIndex)
{
    application_.SelectPage(pageIndex);
}

void EditorApplicationAutomationBridge::SelectPageReticle(const int pageIndex, const int reticleIndex)
{
    application_.SelectPageReticle(pageIndex, reticleIndex);
}

void EditorApplicationAutomationBridge::TogglePageReticleSelection(const int pageIndex, const int reticleIndex)
{
    application_.TogglePageReticleSelection(pageIndex, reticleIndex);
}

void EditorApplicationAutomationBridge::SelectReticleAsset(std::string templateId)
{
    application_.SelectLibraryReticle(std::move(templateId));
}

void EditorApplicationAutomationBridge::SelectReticlePrimitive(std::string templateId, const int primitiveIndex)
{
    application_.SelectLibraryPrimitive(std::move(templateId), primitiveIndex);
}
} // namespace editor::automation
