/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the hidden editor automation core and in-process facade.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

#include "EditorAutomationDocumentUtils.h"
#include "EditorAutomationServices.h"

namespace
{
class FakeEditorAutomationBridge final : public editor::automation::IEditorAutomationEditorBridge
{
public:
    bool HasOpenWindow() const noexcept override
    {
        return !windowFile_.empty();
    }

    const mfd::LoadedWindowConfiguration& Loaded() const noexcept override
    {
        return loaded_;
    }

    mfd::LoadedWindowConfiguration& MutableLoaded() noexcept override
    {
        return loaded_;
    }

    const editor::EditorFileLayout& Files() const noexcept override
    {
        return files_;
    }

    editor::EditorFileLayout& MutableFiles() noexcept override
    {
        return files_;
    }

    std::filesystem::path WindowFile() const override
    {
        return windowFile_;
    }

    void SetWindowFile(std::filesystem::path path) override
    {
        windowFile_ = std::move(path);
    }

    editor::automation::EditorStateSnapshot CaptureSnapshot() const override
    {
        return editor::automation::EditorStateSnapshot {windowFile_, loaded_, files_, uiState_};
    }

    void RestoreSnapshot(const editor::automation::EditorStateSnapshot& snapshot) override
    {
        windowFile_ = snapshot.windowFile;
        loaded_ = snapshot.loaded;
        files_ = snapshot.files;
        uiState_ = snapshot.uiState;
    }

    void PushUndoSnapshot(const editor::automation::EditorStateSnapshot& snapshot) override
    {
        pushedUndoSnapshots_.push_back(snapshot);
    }

    editor::automation::AutomationUiState CaptureUiState() const override
    {
        return uiState_;
    }

    void SelectPage(const int pageIndex) override
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

    void SelectPageReticle(const int pageIndex, const int reticleIndex) override
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

    void TogglePageReticleSelection(const int pageIndex, const int reticleIndex) override
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

    void SelectReticleAsset(std::string templateId) override
    {
        uiState_.selectionKind = editor::automation::AutomationSelectionKind::ReticleAsset;
        uiState_.selectedReticleAssetId = editor::automation::MakeReticleAssetId(templateId);
        uiState_.selectedPrimitiveId = {};
        uiState_.selectedPrimitiveOwnerId.clear();
        uiState_.selectedPrimitiveIndex = -1;
    }

    void SelectReticlePrimitive(std::string templateId, const int primitiveIndex) override
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

    std::filesystem::path windowFile_ {};
    mfd::LoadedWindowConfiguration loaded_ {};
    editor::EditorFileLayout files_ {};
    editor::automation::AutomationUiState uiState_ {};
    std::vector<editor::automation::EditorStateSnapshot> pushedUndoSnapshots_ {};
};

std::filesystem::path MakeTemporaryFolder()
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "mfd_editor_automation_tests";
    const std::filesystem::path folder =
        base / std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    return folder;
}

FakeEditorAutomationBridge MakeSeedBridge()
{
    FakeEditorAutomationBridge bridge;
    bridge.windowFile_ = MakeTemporaryFolder() / "window.json";
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
} // namespace

TEST(EditorAutomationTests, BuildDocumentSnapshotExposesStableIds)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();

    const editor::automation::DocumentSnapshot snapshot = editor::automation::BuildDocumentSnapshot(
        bridge.loaded_, bridge.files_, bridge.uiState_, std::nullopt);

    ASSERT_TRUE(snapshot.hasOpenWindow);
    ASSERT_EQ(snapshot.pages.size(), 1U);
    ASSERT_EQ(snapshot.reticleAssets.size(), 1U);
    EXPECT_EQ(snapshot.pages.front().id.value, "page:radar");
    EXPECT_EQ(snapshot.reticleAssets.front().id.value, "reticle-asset:track_box");
}

TEST(EditorAutomationTests, SessionRollbackRestoresPreSessionState)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"rollback"});
    ASSERT_TRUE(session.ok());

    mfd::PageDefinition page;
    page.name = "Weapons";
    page.title = "Weapons";
    const auto status = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::CreatePageAssetRequest {page, std::nullopt}});
    ASSERT_TRUE(status.ok());
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 2U);

    const auto rollbackStatus = facade->SessionService().RollbackSession(
        editor::automation::RollbackSessionRequest {session.value});
    ASSERT_TRUE(rollbackStatus.ok());
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 1U);
}

TEST(EditorAutomationTests, SessionCommitPushesOneUndoSnapshot)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"commit"});
    ASSERT_TRUE(session.ok());

    const auto applyStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                editor::automation::MakePageId(bridge.loaded_.document.pages.front()),
                editor::automation::MakeReticleAssetId("track_box"),
                std::nullopt,
                mfd::Transform2D {{0.2f, -0.1f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(applyStatus.ok());

    const auto commitStatus = facade->SessionService().CommitSession(
        editor::automation::CommitSessionRequest {session.value});
    ASSERT_TRUE(commitStatus.ok());
    ASSERT_EQ(bridge.pushedUndoSnapshots_.size(), 1U);
    ASSERT_TRUE(bridge.loaded_.document.pages.front().staticReticles.size() == 1U);
}

TEST(EditorAutomationTests, SaveAllWritesEditorOwnedAssets)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto result = facade->PersistenceService().SaveAll();

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(std::filesystem::exists(bridge.windowFile_));
    EXPECT_TRUE(std::filesystem::exists(bridge.files_.pageFiles.front()));
    EXPECT_TRUE(std::filesystem::exists(bridge.files_.templateFiles.begin()->second));
}

TEST(EditorAutomationTests, SessionCanCreateReticleInstanceAndExportJsonPreview)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"page-preview"});
    ASSERT_TRUE(session.ok());

    const auto applyStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                editor::automation::MakePageId(bridge.loaded_.document.pages.front()),
                editor::automation::MakeReticleAssetId("track_box"),
                std::optional<std::string> {"track_box_instance"},
                mfd::Transform2D {{0.25f, -0.15f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(applyStatus.ok());

    const auto preview = facade->PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {
            editor::automation::ExportJsonPreviewRequest::Kind::Page,
            editor::automation::MakePageId(bridge.loaded_.document.pages.front()).value});
    ASSERT_TRUE(preview.ok());
    EXPECT_NE(preview.value.json.find("\"staticReticles\""), std::string::npos);
    EXPECT_NE(preview.value.json.find("\"template\": \"track_box\""), std::string::npos);
    EXPECT_NE(preview.value.json.find("\"id\": \"track_box_instance\""), std::string::npos);
}

TEST(EditorAutomationTests, SessionCanToggleLayerVisibilityAndSelectionState)
{
    FakeEditorAutomationBridge bridge = MakeSeedBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"layer-and-selection"});
    ASSERT_TRUE(session.ok());

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    ASSERT_FALSE(page.editor.layers.empty());
    const editor::automation::LayerId layerId = editor::automation::MakeLayerId(page, page.editor.layers.front());
    const editor::automation::PageId pageId = editor::automation::MakePageId(page);

    const auto hideLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetLayerVisibilityRequest {pageId, layerId, false}});
    ASSERT_TRUE(hideLayerStatus.ok());
    EXPECT_FALSE(page.editor.layers.front().visible);

    const auto selectStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SelectEntityRequest {editor::automation::AutomationSelectionKind::Page, pageId.value}});
    ASSERT_TRUE(selectStatus.ok());

    const auto snapshot = facade->QueryService().GetSnapshot();
    ASSERT_TRUE(snapshot.ok());
    EXPECT_EQ(snapshot.value.uiState.selectionKind, editor::automation::AutomationSelectionKind::Page);
    EXPECT_EQ(snapshot.value.uiState.activePageId.value, pageId.value);
}
