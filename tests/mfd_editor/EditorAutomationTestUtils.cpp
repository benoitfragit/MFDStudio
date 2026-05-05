/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Shared GoogleTest helpers for editor automation facade and plugin loader scenarios.
 */

#include "EditorAutomationTestUtils.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "EditorAutomationDocumentUtils.h"

bool FakeEditorAutomationBridge::HasOpenWindow() const noexcept
{
    return !windowFile_.empty();
}

const mfd::LoadedWindowConfiguration& FakeEditorAutomationBridge::Loaded() const noexcept
{
    return loaded_;
}

mfd::LoadedWindowConfiguration& FakeEditorAutomationBridge::MutableLoaded() noexcept
{
    return loaded_;
}

const editor::EditorFileLayout& FakeEditorAutomationBridge::Files() const noexcept
{
    return files_;
}

editor::EditorFileLayout& FakeEditorAutomationBridge::MutableFiles() noexcept
{
    return files_;
}

std::filesystem::path FakeEditorAutomationBridge::WindowFile() const
{
    return windowFile_;
}

void FakeEditorAutomationBridge::SetWindowFile(std::filesystem::path path)
{
    windowFile_ = std::move(path);
}

editor::automation::EditorStateSnapshot FakeEditorAutomationBridge::CaptureSnapshot() const
{
    return editor::automation::EditorStateSnapshot {windowFile_, loaded_, files_, uiState_};
}

void FakeEditorAutomationBridge::RestoreSnapshot(const editor::automation::EditorStateSnapshot& snapshot)
{
    windowFile_ = snapshot.windowFile;
    loaded_ = snapshot.loaded;
    files_ = snapshot.files;
    uiState_ = snapshot.uiState;
}

void FakeEditorAutomationBridge::PushUndoSnapshot(const editor::automation::EditorStateSnapshot& snapshot)
{
    pushedUndoSnapshots_.push_back(snapshot);
}

editor::automation::AutomationUiState FakeEditorAutomationBridge::CaptureUiState() const
{
    return uiState_;
}

void FakeEditorAutomationBridge::SelectPage(const int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return;
    }

    uiState_.selectionKind = editor::automation::AutomationSelectionKind::Page;
    uiState_.activePageId = editor::automation::MakePageId(loaded_.document.pages[static_cast<std::size_t>(pageIndex)]);
    uiState_.selectedPageReticleIds.clear();
    uiState_.selectedReticleAssetId = {};
    uiState_.selectedPrimitiveId = {};
    uiState_.selectedPrimitiveOwnerId.clear();
    uiState_.selectedPrimitiveIndex = -1;
}

void FakeEditorAutomationBridge::SelectPageReticle(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return;
    }

    const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()))
    {
        return;
    }

    uiState_.selectionKind = editor::automation::AutomationSelectionKind::PageReticleInstance;
    uiState_.activePageId = editor::automation::MakePageId(page);
    uiState_.selectedPageReticleIds = {
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)])};
}

void FakeEditorAutomationBridge::TogglePageReticleSelection(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return;
    }

    const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()))
    {
        return;
    }

    const editor::automation::PageReticleInstanceId id =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)]);
    const auto iterator = std::find_if(
        uiState_.selectedPageReticleIds.begin(),
        uiState_.selectedPageReticleIds.end(),
        [&id](const editor::automation::PageReticleInstanceId& candidate)
        {
            return candidate.value == id.value;
        });
    if (iterator == uiState_.selectedPageReticleIds.end())
    {
        uiState_.selectedPageReticleIds.push_back(id);
    }
    else
    {
        uiState_.selectedPageReticleIds.erase(iterator);
    }
}

void FakeEditorAutomationBridge::SelectReticleAsset(std::string templateId)
{
    uiState_.selectionKind = editor::automation::AutomationSelectionKind::ReticleAsset;
    uiState_.selectedReticleAssetId = editor::automation::MakeReticleAssetId(templateId);
    uiState_.selectedPrimitiveId = {};
    uiState_.selectedPrimitiveOwnerId.clear();
    uiState_.selectedPrimitiveIndex = -1;
}

void FakeEditorAutomationBridge::SelectReticlePrimitive(std::string templateId, const int primitiveIndex)
{
    const auto iterator = loaded_.document.reticleLibrary.find(templateId);
    if (iterator == loaded_.document.reticleLibrary.end() ||
        primitiveIndex < 0 ||
        primitiveIndex >= static_cast<int>(iterator->second.primitives.size()))
    {
        return;
    }

    uiState_.selectionKind = editor::automation::AutomationSelectionKind::Primitive;
    uiState_.selectedReticleAssetId = editor::automation::MakeReticleAssetId(templateId);
    uiState_.selectedPrimitiveOwnerKind = editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset;
    uiState_.selectedPrimitiveOwnerId = uiState_.selectedReticleAssetId.value;
    uiState_.selectedPrimitiveIndex = primitiveIndex;
    uiState_.selectedPrimitiveId = editor::automation::MakeReticleAssetPrimitiveId(
        templateId,
        iterator->second.primitives[static_cast<std::size_t>(primitiveIndex)],
        primitiveIndex);
}

std::filesystem::path MakeEditorAutomationTemporaryFolder()
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "mfd_editor_automation_tests";
    const std::filesystem::path folder =
        base / std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    return folder;
}

FakeEditorAutomationBridge MakeSeedEditorAutomationBridge()
{
    FakeEditorAutomationBridge bridge;
    bridge.windowFile_ = MakeEditorAutomationTemporaryFolder() / "window.json";
    bridge.loaded_.window.sourceFile = bridge.windowFile_;
    bridge.loaded_.window.title = "Automation";
    bridge.loaded_.window.reticleLibraryFolder = bridge.windowFile_.parent_path() / "reticles";
    bridge.loaded_.document.sourceFile = bridge.windowFile_;
    bridge.loaded_.document.reticleLibraryFolder = bridge.loaded_.window.reticleLibraryFolder;

    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = "Radar";
    page.defaultPage = true;
    page.layers.push_back(mfd::PageLayerDefinition {"layer"});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    bridge.loaded_.document.pages.push_back(page);
    bridge.files_.pageFiles.push_back(bridge.windowFile_.parent_path() / "radar.json");
    bridge.loaded_.window.pageFiles = bridge.files_.pageFiles;
    bridge.SelectPage(0);

    mfd::ReticleGroup reticle = editor::automation::MakePrimitiveReticle("track_box", mfd::PrimitiveType::Rectangle);
    bridge.loaded_.document.reticleLibrary[reticle.id] = reticle;
    bridge.files_.templateFiles[reticle.id] = bridge.loaded_.window.reticleLibraryFolder / "track_box.json";
    return bridge;
}
