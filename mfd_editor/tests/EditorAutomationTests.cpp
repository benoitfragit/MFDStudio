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

#include <string>

#include "EditorAutomationDocumentUtils.h"
#include "EditorAutomationServices.h"

#include "EditorAutomationTestUtils.h"

TEST(EditorAutomationTests, BuildDocumentSnapshotExposesStableIds)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();

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
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
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
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
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
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
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
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
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
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
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

TEST(EditorAutomationTests, CreateWindowDocumentAndSaveAssetSupportGranularPersistence)
{
    FakeEditorAutomationBridge bridge;
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const std::filesystem::path tempFolder = MakeEditorAutomationTemporaryFolder();
    const std::filesystem::path windowFile = tempFolder / "automation_window.json";
    const std::filesystem::path pageFile = tempFolder / "automation_page.json";
    const std::filesystem::path reticleLibraryFolder = tempFolder / "reticles";

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"create-window"});
    ASSERT_TRUE(session.ok());

    mfd::WindowAssetDefinition window;
    window.title = "Automation window";

    mfd::PageDefinition page;
    page.name = "Main";
    page.title = "Main";

    const auto createWindowStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::CreateWindowDocumentRequest {
                window,
                page,
                windowFile,
                reticleLibraryFolder,
                pageFile}});
    ASSERT_TRUE(createWindowStatus.ok()) << createWindowStatus.error.message;

    const auto commitStatus = facade->SessionService().CommitSession(
        editor::automation::CommitSessionRequest {session.value});
    ASSERT_TRUE(commitStatus.ok()) << commitStatus.error.message;

    mfd::ReticleGroup reticle = editor::automation::MakePrimitiveReticle("save_me", mfd::PrimitiveType::Rectangle);
    const auto createReticleStatus = facade->AuthoringService().ApplyAction(
        editor::automation::CreateReticleAssetRequest {reticle, std::optional<std::filesystem::path> {"save_me.json"}});
    ASSERT_TRUE(createReticleStatus.ok()) << createReticleStatus.error.message;

    const auto pageSave = facade->PersistenceService().SaveAsset(
        editor::automation::SaveAssetRequest {
            editor::automation::SaveAssetRequest::Kind::Page,
            editor::automation::MakePageId(bridge.loaded_.document.pages.front()).value});
    ASSERT_TRUE(pageSave.ok()) << pageSave.error.message;
    ASSERT_EQ(pageSave.value.savedFiles.size(), 2U);
    EXPECT_TRUE(std::filesystem::exists(windowFile));
    EXPECT_TRUE(std::filesystem::exists(pageFile));

    const auto reticleSave = facade->PersistenceService().SaveAsset(
        editor::automation::SaveAssetRequest {
            editor::automation::SaveAssetRequest::Kind::ReticleAsset,
            editor::automation::MakeReticleAssetId("save_me").value});
    ASSERT_TRUE(reticleSave.ok()) << reticleSave.error.message;
    ASSERT_EQ(reticleSave.value.savedFiles.size(), 1U);
    EXPECT_TRUE(std::filesystem::exists(reticleLibraryFolder / "save_me.json"));
}

TEST(EditorAutomationTests, PageAssetReplacementDeletionPreviewAndSelectionSupportAutomation)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"page-asset-matrix"});
    ASSERT_TRUE(session.ok());

    const editor::automation::PageId originalPageId =
        editor::automation::MakePageId(bridge.loaded_.document.pages.front());

    mfd::PageDefinition supportPage;
    supportPage.name = "Support";
    supportPage.title = "Support";
    const auto createPageStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::CreatePageAssetRequest {supportPage, std::optional<std::filesystem::path> {"support.json"}}});
    ASSERT_TRUE(createPageStatus.ok()) << createPageStatus.error.message;
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 2U);

    mfd::PageDefinition replacementPage = bridge.loaded_.document.pages.front();
    replacementPage.name = "Navigation";
    replacementPage.title = "Navigation";
    replacementPage.backgroundColor = mfd::ColorRgba {12, 24, 40, 255};
    replacementPage.view.center = {0.15f, -0.20f};
    replacementPage.view.zoom = 1.75f;
    replacementPage.defaultPage = true;
    replacementPage.blinkTypes = {mfd::PageBlinkDefinition {"pulse", {}, 800U}};
    replacementPage.defaultBlinkTypeName = "pulse";

    const auto replacePageStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageAssetRequest {
                originalPageId,
                replacementPage,
                std::optional<std::filesystem::path> {"navigation.json"}}});
    ASSERT_TRUE(replacePageStatus.ok()) << replacePageStatus.error.message;
    ASSERT_EQ(bridge.loaded_.document.pages.front().name, "Navigation");
    EXPECT_EQ(bridge.loaded_.document.pages.front().title, "Navigation");
    EXPECT_EQ(bridge.files_.pageFiles.front().filename().string(), "navigation.json");
    EXPECT_EQ(bridge.loaded_.document.pages.front().normalizedDefaultBlinkTypeName, "pulse");

    const editor::automation::PageId navigationPageId =
        editor::automation::MakePageId(bridge.loaded_.document.pages.front());
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");
    const auto instantiateStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                navigationPageId,
                reticleAssetId,
                std::optional<std::string> {"nav_track"},
                mfd::Transform2D {{0.10f, 0.05f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateStatus.ok()) << instantiateStatus.error.message;
    ASSERT_EQ(bridge.loaded_.document.pages.front().staticReticles.size(), 1U);

    const editor::automation::PageReticleInstanceId pageReticleId =
        editor::automation::MakePageReticleInstanceId(
            bridge.loaded_.document.pages.front(),
            bridge.loaded_.document.pages.front().staticReticles.front());

    const auto selectPageReticleStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SelectEntityRequest {
                editor::automation::AutomationSelectionKind::PageReticleInstance,
                pageReticleId.value}});
    ASSERT_TRUE(selectPageReticleStatus.ok()) << selectPageReticleStatus.error.message;

    auto snapshot = facade->QueryService().GetSnapshot();
    ASSERT_TRUE(snapshot.ok());
    EXPECT_EQ(snapshot.value.uiState.selectionKind, editor::automation::AutomationSelectionKind::PageReticleInstance);
    ASSERT_EQ(snapshot.value.uiState.selectedPageReticleIds.size(), 1U);
    EXPECT_EQ(snapshot.value.uiState.selectedPageReticleIds.front().value, pageReticleId.value);

    const auto selectReticleAssetStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SelectEntityRequest {
                editor::automation::AutomationSelectionKind::ReticleAsset,
                reticleAssetId.value}});
    ASSERT_TRUE(selectReticleAssetStatus.ok()) << selectReticleAssetStatus.error.message;

    snapshot = facade->QueryService().GetSnapshot();
    ASSERT_TRUE(snapshot.ok());
    EXPECT_EQ(snapshot.value.uiState.selectionKind, editor::automation::AutomationSelectionKind::ReticleAsset);
    EXPECT_EQ(snapshot.value.uiState.selectedReticleAssetId.value, reticleAssetId.value);

    const auto windowPreview = facade->PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {
            editor::automation::ExportJsonPreviewRequest::Kind::Window,
            editor::automation::MakeWindowId(bridge.loaded_).value});
    ASSERT_TRUE(windowPreview.ok());
    EXPECT_NE(windowPreview.value.json.find("navigation.json"), std::string::npos);

    const auto pageReticlePreview = facade->PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {
            editor::automation::ExportJsonPreviewRequest::Kind::PageReticleInstance,
            pageReticleId.value});
    ASSERT_TRUE(pageReticlePreview.ok());
    EXPECT_NE(pageReticlePreview.value.json.find("\"id\": \"nav_track\""), std::string::npos);
    EXPECT_NE(pageReticlePreview.value.json.find("\"template\": \"track_box\""), std::string::npos);

    const editor::automation::PageId supportPageId =
        editor::automation::MakePageId(bridge.loaded_.document.pages.back());
    const auto deletePageStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::DeletePageAssetRequest {supportPageId}});
    ASSERT_TRUE(deletePageStatus.ok()) << deletePageStatus.error.message;
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 1U);
    ASSERT_EQ(bridge.files_.removedPageFiles.size(), 1U);
    EXPECT_EQ(bridge.files_.removedPageFiles.front().filename().string(), "support.json");
}

TEST(EditorAutomationTests, ReticleAssetAndPageReticleLifecycleSupportsFlagsLayerClippingAndDeletion)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    bridge.loaded_.document.pages.front().editor.layers.push_back(mfd::EditorLayerDefinition {"overlay", true});
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"reticle-lifecycle"});
    ASSERT_TRUE(session.ok());

    const editor::automation::PageId pageId = editor::automation::MakePageId(bridge.loaded_.document.pages.front());
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");

    const auto instantiateStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"track_box_instance"},
                mfd::Transform2D {{0.2f, 0.15f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateStatus.ok()) << instantiateStatus.error.message;

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    ASSERT_EQ(page.staticReticles.size(), 1U);
    const editor::automation::PageReticleInstanceId pageReticleId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());
    const editor::automation::LayerId overlayLayerId =
        editor::automation::MakeLayerId(page, page.editor.layers.back());

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleAssetVisibilityRequest {reticleAssetId, false}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleAssetDrawOnTopRequest {reticleAssetId, true}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleVisibilityRequest {pageReticleId, false}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleDrawOnTopRequest {pageReticleId, true}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetPageReticleLayerRequest {pageReticleId, overlayLayerId}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleClippingRequest {
                            editor::automation::AutomationReticleTargetKind::ReticleAsset,
                            reticleAssetId.value,
                            mfd::ReticleClipState {mfd::ReticleClipMode::Inner, "primitive_01"}}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleClippingRequest {
                            editor::automation::AutomationReticleTargetKind::PageReticleInstance,
                            pageReticleId.value,
                            mfd::ReticleClipState {mfd::ReticleClipMode::Outer, "primitive_01"}}})
                    .ok());

    EXPECT_FALSE(bridge.loaded_.document.reticleLibrary.at("track_box").visible);
    EXPECT_TRUE(bridge.loaded_.document.reticleLibrary.at("track_box").drawOnTop);
    EXPECT_FALSE(page.staticReticles.front().visible);
    EXPECT_TRUE(page.staticReticles.front().drawOnTop);
    EXPECT_EQ(page.staticReticles.front().editor.layerId, "overlay");
    EXPECT_EQ(bridge.loaded_.document.reticleLibrary.at("track_box").clipping.mode, mfd::ReticleClipMode::Inner);
    EXPECT_EQ(page.staticReticles.front().clipping.mode, mfd::ReticleClipMode::Outer);

    const auto invalidClipStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetReticleClippingRequest {
                editor::automation::AutomationReticleTargetKind::PageReticleInstance,
                pageReticleId.value,
                mfd::ReticleClipState {mfd::ReticleClipMode::Inner, "missing_primitive"}}});
    EXPECT_FALSE(invalidClipStatus.ok());
    EXPECT_EQ(invalidClipStatus.error.code, editor::automation::AutomationErrorCode::ValidationFailed);

    mfd::ReticleGroup renamedReticle = bridge.loaded_.document.reticleLibrary.at("track_box");
    renamedReticle.id = "threat_box";
    const auto replaceReticleStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplaceReticleAssetRequest {
                reticleAssetId,
                renamedReticle,
                std::optional<std::filesystem::path> {"threat_box.json"}}});
    ASSERT_TRUE(replaceReticleStatus.ok()) << replaceReticleStatus.error.message;

    EXPECT_TRUE(bridge.loaded_.document.reticleLibrary.find("track_box") == bridge.loaded_.document.reticleLibrary.end());
    ASSERT_TRUE(bridge.loaded_.document.reticleLibrary.find("threat_box") != bridge.loaded_.document.reticleLibrary.end());
    EXPECT_EQ(page.staticReticles.front().sourceTemplateId, "threat_box");
    EXPECT_TRUE(bridge.files_.templateFiles.find("threat_box") != bridge.files_.templateFiles.end());

    const auto deleteReferencedAssetStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::DeleteReticleAssetRequest {editor::automation::MakeReticleAssetId("threat_box")}});
    EXPECT_FALSE(deleteReferencedAssetStatus.ok());
    EXPECT_EQ(deleteReferencedAssetStatus.error.code, editor::automation::AutomationErrorCode::Conflict);

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::DeletePageReticleInstanceRequest {pageReticleId}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::DeleteReticleAssetRequest {editor::automation::MakeReticleAssetId("threat_box")}})
                    .ok());

    EXPECT_TRUE(page.staticReticles.empty());
    EXPECT_TRUE(bridge.loaded_.document.reticleLibrary.empty());
    ASSERT_EQ(bridge.files_.removedTemplateFiles.size(), 1U);

    const auto commitStatus = facade->SessionService().CommitSession(
        editor::automation::CommitSessionRequest {session.value});
    ASSERT_TRUE(commitStatus.ok());
}

TEST(EditorAutomationTests, ReplacePageReticleInstanceSupportsFullReplacementAndConflictChecks)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"replace-page-reticle"});
    ASSERT_TRUE(session.ok());

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    const editor::automation::PageId pageId = editor::automation::MakePageId(page);
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");

    mfd::PageBlinkDefinition blinkType;
    blinkType.name = "pulse";
    blinkType.durationMs = 450U;
    const auto blinkCatalogStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageBlinkTypesRequest {pageId, {blinkType}}});
    ASSERT_TRUE(blinkCatalogStatus.ok()) << blinkCatalogStatus.error.message;

    const auto instantiateAlphaStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"alpha"},
                mfd::Transform2D {{0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateAlphaStatus.ok()) << instantiateAlphaStatus.error.message;
    const auto instantiateBetaStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"beta"},
                mfd::Transform2D {{0.3f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateBetaStatus.ok()) << instantiateBetaStatus.error.message;
    ASSERT_EQ(page.staticReticles.size(), 2U);

    const editor::automation::PageReticleInstanceId alphaId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());
    const editor::automation::PageReticleInstanceId betaId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.back());

    mfd::ReticleGroup replacement = page.staticReticles.front();
    replacement.id = "alpha_custom";
    replacement.visible = false;
    replacement.drawOnTop = true;
    replacement.transform = mfd::Transform2D {{0.42f, -0.18f}, 35.0f, {1.2f, 0.7f}};
    replacement.blink.enabled = true;
    replacement.blink.typeName = "pulse";
    replacement.clipping = mfd::ReticleClipState {mfd::ReticleClipMode::Inner, "primitive_01"};
    replacement.overrides.color = mfd::ColorRgba {255, 120, 10, 255};
    replacement.overrides.thickness = 0.009f;

    const auto replaceStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageReticleInstanceRequest {alphaId, replacement}});
    ASSERT_TRUE(replaceStatus.ok()) << replaceStatus.error.message;
    EXPECT_EQ(page.staticReticles.front().id, "alpha_custom");
    EXPECT_FALSE(page.staticReticles.front().visible);
    EXPECT_TRUE(page.staticReticles.front().drawOnTop);
    EXPECT_EQ(page.staticReticles.front().blink.normalizedTypeName, "pulse");
    EXPECT_EQ(page.staticReticles.front().blink.durationMs, 450U);
    EXPECT_EQ(page.staticReticles.front().clipping.mode, mfd::ReticleClipMode::Inner);
    ASSERT_TRUE(page.staticReticles.front().overrides.color.has_value());

    mfd::ReticleGroup conflictingReplacement = page.staticReticles.back();
    conflictingReplacement.id = "alpha_custom";
    const auto conflictStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageReticleInstanceRequest {betaId, conflictingReplacement}});
    EXPECT_FALSE(conflictStatus.ok());
    EXPECT_EQ(conflictStatus.error.code, editor::automation::AutomationErrorCode::Conflict);

    ASSERT_TRUE(facade->SessionService().CommitSession(editor::automation::CommitSessionRequest {session.value}).ok());
}

TEST(EditorAutomationTests, PrimitiveActionsSupportTransformVisibilityExposureAcrossOwners)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"primitive-actions"});
    ASSERT_TRUE(session.ok());

    const editor::automation::PageId pageId = editor::automation::MakePageId(bridge.loaded_.document.pages.front());
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");

    mfd::Primitive assetPrimitive;
    assetPrimitive.id = "asset_line";
    assetPrimitive.type = mfd::PrimitiveType::Line;
    assetPrimitive.geometry = mfd::LineGeometry {{-0.10f, 0.0f}, {0.10f, 0.0f}};
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::AddPrimitiveRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            assetPrimitive,
                            0}})
                    .ok());

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetPrimitiveTransformRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            editor::automation::PrimitiveSelector {"asset_line", -1},
                            mfd::Transform2D {{0.30f, -0.20f}, 45.0f, {1.5f, 0.5f}}}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetPrimitiveVisibilityRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            editor::automation::PrimitiveSelector {"asset_line", -1},
                            false}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetPrimitiveExposedRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            editor::automation::PrimitiveSelector {"asset_line", -1},
                            true}})
                    .ok());

    mfd::Primitive replacementPrimitive = bridge.loaded_.document.reticleLibrary.at("track_box").primitives.front();
    replacementPrimitive.id = "asset_circle";
    replacementPrimitive.type = mfd::PrimitiveType::Circle;
    replacementPrimitive.geometry = mfd::CircleGeometry {0.22f};
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::ReplacePrimitiveRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            editor::automation::PrimitiveSelector {"asset_line", -1},
                            replacementPrimitive}})
                    .ok());

    const auto reticlePreview = facade->PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {
            editor::automation::ExportJsonPreviewRequest::Kind::ReticleAsset,
            reticleAssetId.value});
    ASSERT_TRUE(reticlePreview.ok());
    EXPECT_NE(reticlePreview.value.json.find("\"asset_circle\""), std::string::npos);
    EXPECT_NE(reticlePreview.value.json.find("\"exposed\": true"), std::string::npos);

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::InstantiateReticleOnPageRequest {
                            pageId,
                            reticleAssetId,
                            std::optional<std::string> {"page_owner"},
                            mfd::Transform2D {{0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                            true}})
                    .ok());
    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    ASSERT_EQ(page.staticReticles.size(), 1U);
    const editor::automation::PageReticleInstanceId pageReticleId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());

    mfd::Primitive pagePrimitive;
    pagePrimitive.id = "page_line";
    pagePrimitive.type = mfd::PrimitiveType::Line;
    pagePrimitive.geometry = mfd::LineGeometry {{0.0f, -0.05f}, {0.0f, 0.05f}};
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::AddPrimitiveRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::PageReticleInstance,
                            pageReticleId.value,
                            pagePrimitive,
                            std::nullopt}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::DeletePrimitiveRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::PageReticleInstance,
                            pageReticleId.value,
                            editor::automation::PrimitiveSelector {"page_line", -1}}})
                    .ok());
    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::DeletePrimitiveRequest {
                            editor::automation::AutomationPrimitiveOwnerKind::ReticleAsset,
                            reticleAssetId.value,
                            editor::automation::PrimitiveSelector {"asset_circle", -1}}})
                    .ok());

    EXPECT_EQ(bridge.loaded_.document.reticleLibrary.at("track_box").primitives.front().id, "primitive_01");
    ASSERT_EQ(page.staticReticles.front().primitives.size(), 2U);
    EXPECT_EQ(page.staticReticles.front().primitives.front().id, "asset_circle");
    EXPECT_EQ(page.staticReticles.front().primitives.back().id, "primitive_01");
}

TEST(EditorAutomationTests, PageStrobeActionsSupportAssignReplaceClippingAndDelete)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"strobe-actions"});
    ASSERT_TRUE(session.ok());

    const editor::automation::PageId pageId = editor::automation::MakePageId(bridge.loaded_.document.pages.front());
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::AssignPageStrobeTemplateRequest {
                            pageId,
                            reticleAssetId,
                            std::optional<std::string> {"radar_strobe"}}})
                    .ok());

    ASSERT_TRUE(bridge.loaded_.document.pages.front().strobe.has_value());
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->reticle.id, "radar_strobe");
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->reticle.sourceTemplateId, "track_box");

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::SetReticleClippingRequest {
                            editor::automation::AutomationReticleTargetKind::PageStrobe,
                            pageId.value,
                            mfd::ReticleClipState {mfd::ReticleClipMode::Inner, "primitive_01"}}})
                    .ok());
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->reticle.clipping.mode, mfd::ReticleClipMode::Inner);

    mfd::PageStrobeDefinition replacement = *bridge.loaded_.document.pages.front().strobe;
    replacement.reticle.id = "custom_strobe";
    replacement.capture.shape = mfd::StrobeCaptureShape::Rectangle;
    replacement.capture.size = {0.42f, 0.24f};
    replacement.magnet.enabled = true;
    replacement.magnet.radius = 0.33f;
    replacement.magnet.strength = 0.55f;
    replacement.magnet.visualShapeEnabled = true;
    replacement.magnet.visualShape = mfd::StrobeMagnetVisualShape::Square;
    replacement.magnet.visualShapeSize = 0.27f;

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::ReplacePageStrobeRequest {pageId, replacement}})
                    .ok());

    ASSERT_TRUE(bridge.loaded_.document.pages.front().strobe.has_value());
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->reticle.id, "custom_strobe");
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->capture.shape, mfd::StrobeCaptureShape::Rectangle);
    EXPECT_FLOAT_EQ(bridge.loaded_.document.pages.front().strobe->capture.size.x, 0.42f);
    EXPECT_TRUE(bridge.loaded_.document.pages.front().strobe->magnet.enabled);
    EXPECT_EQ(bridge.loaded_.document.pages.front().strobe->magnet.visualShape, mfd::StrobeMagnetVisualShape::Square);

    const auto pagePreview = facade->PersistenceService().ExportJsonPreview(
        editor::automation::ExportJsonPreviewRequest {
            editor::automation::ExportJsonPreviewRequest::Kind::Page,
            pageId.value});
    ASSERT_TRUE(pagePreview.ok());
    EXPECT_NE(pagePreview.value.json.find("\"strobe\""), std::string::npos);
    EXPECT_NE(pagePreview.value.json.find("\"magnet\""), std::string::npos);
    EXPECT_NE(pagePreview.value.json.find("\"clipping\""), std::string::npos);

    ASSERT_TRUE(facade->SessionService().ApplyAction(
                    editor::automation::ApplyActionRequest {
                        session.value,
                        editor::automation::DeletePageStrobeRequest {pageId}})
                    .ok());
    EXPECT_FALSE(bridge.loaded_.document.pages.front().strobe.has_value());
}

TEST(EditorAutomationTests, WindowLayerAndPrimitiveSelectionActionsSupportAutomation)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"window-layer-selection"});
    ASSERT_TRUE(session.ok());

    mfd::WindowAssetDefinition window = bridge.loaded_.window;
    window.title = "Automation Updated";
    window.width = 1024;
    window.height = 768;
    window.fontFile = "fonts/editor_font.ttf";

    const auto replaceWindowStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplaceWindowAssetRequest {
                window,
                std::nullopt,
                std::optional<std::filesystem::path> {"shared_reticles"}}});
    ASSERT_TRUE(replaceWindowStatus.ok()) << replaceWindowStatus.error.message;
    EXPECT_EQ(bridge.loaded_.window.title, "Automation Updated");
    EXPECT_EQ(bridge.loaded_.window.width, 1024);
    EXPECT_EQ(bridge.loaded_.document.reticleLibraryFolder, bridge.windowFile_.parent_path() / "shared_reticles");

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    const editor::automation::PageId pageId = editor::automation::MakePageId(page);
    const auto createLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::CreateLayerRequest {
                pageId,
                mfd::EditorLayerDefinition {"overlay", false},
                0}});
    ASSERT_TRUE(createLayerStatus.ok()) << createLayerStatus.error.message;
    ASSERT_EQ(page.editor.layers.size(), 2U);
    EXPECT_EQ(page.editor.layers.front().id, "overlay");

    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");
    const auto instantiateStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"track_overlay"},
                mfd::Transform2D {{0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateStatus.ok()) << instantiateStatus.error.message;

    const editor::automation::PageReticleInstanceId pageReticleId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());
    editor::automation::LayerId overlayLayerId = editor::automation::MakeLayerId(page, page.editor.layers.front());

    const auto assignLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetPageReticleLayerRequest {pageReticleId, overlayLayerId}});
    ASSERT_TRUE(assignLayerStatus.ok()) << assignLayerStatus.error.message;
    EXPECT_EQ(page.staticReticles.front().editor.layerId, "overlay");

    const auto replaceLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplaceLayerRequest {
                pageId,
                overlayLayerId,
                mfd::EditorLayerDefinition {"overlay_top", true}}});
    ASSERT_TRUE(replaceLayerStatus.ok()) << replaceLayerStatus.error.message;
    EXPECT_EQ(page.editor.layers.front().id, "overlay_top");
    EXPECT_EQ(page.staticReticles.front().editor.layerId, "overlay_top");

    const auto clearLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetPageReticleLayerRequest {pageReticleId, {}}});
    ASSERT_TRUE(clearLayerStatus.ok()) << clearLayerStatus.error.message;
    EXPECT_TRUE(page.staticReticles.front().editor.layerId.empty());

    overlayLayerId = editor::automation::MakeLayerId(page, page.editor.layers.front());
    const auto deleteLayerStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::DeleteLayerRequest {pageId, overlayLayerId}});
    ASSERT_TRUE(deleteLayerStatus.ok()) << deleteLayerStatus.error.message;
    ASSERT_EQ(page.editor.layers.size(), 1U);
    EXPECT_EQ(page.editor.layers.front().id, "layer");

    const editor::automation::PrimitiveId primitiveId = editor::automation::MakeReticleAssetPrimitiveId(
        "track_box",
        bridge.loaded_.document.reticleLibrary.at("track_box").primitives.front(),
        0);
    const auto selectPrimitiveStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SelectEntityRequest {
                editor::automation::AutomationSelectionKind::Primitive,
                primitiveId.value}});
    ASSERT_TRUE(selectPrimitiveStatus.ok()) << selectPrimitiveStatus.error.message;

    const auto snapshot = facade->QueryService().GetSnapshot();
    ASSERT_TRUE(snapshot.ok());
    EXPECT_EQ(snapshot.value.uiState.selectionKind, editor::automation::AutomationSelectionKind::Primitive);
    EXPECT_EQ(snapshot.value.uiState.selectedPrimitiveId.value, primitiveId.value);

    ASSERT_TRUE(facade->SessionService().CommitSession(editor::automation::CommitSessionRequest {session.value}).ok());
}

TEST(EditorAutomationTests, ReticleOrderingTransformAndBlinkActionsSupportAutomation)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"ordering-transform-blink"});
    ASSERT_TRUE(session.ok());

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    const editor::automation::PageId pageId = editor::automation::MakePageId(page);
    const editor::automation::ReticleAssetId reticleAssetId = editor::automation::MakeReticleAssetId("track_box");

    std::vector<mfd::PageBlinkDefinition> blinkTypes;
    blinkTypes.push_back(mfd::PageBlinkDefinition {"slow", {}, 1200U});
    blinkTypes.push_back(mfd::PageBlinkDefinition {"fast", {}, 250U});
    const auto replaceBlinkCatalogStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageBlinkTypesRequest {pageId, blinkTypes}});
    ASSERT_TRUE(replaceBlinkCatalogStatus.ok()) << replaceBlinkCatalogStatus.error.message;
    const auto setDefaultBlinkStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetPageDefaultBlinkTypeRequest {pageId, "fast"}});
    ASSERT_TRUE(setDefaultBlinkStatus.ok()) << setDefaultBlinkStatus.error.message;
    EXPECT_EQ(page.normalizedDefaultBlinkTypeName, "fast");

    const auto instantiateAlphaStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"alpha"},
                mfd::Transform2D {{-0.20f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateAlphaStatus.ok()) << instantiateAlphaStatus.error.message;
    const auto instantiateBetaStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::InstantiateReticleOnPageRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"beta"},
                mfd::Transform2D {{0.20f, 0.0f}, 0.0f, {1.0f, 1.0f}},
                true}});
    ASSERT_TRUE(instantiateBetaStatus.ok()) << instantiateBetaStatus.error.message;
    ASSERT_EQ(page.staticReticles.size(), 2U);

    const editor::automation::PageReticleInstanceId alphaId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());
    const editor::automation::PageReticleInstanceId betaId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.back());

    const auto moveReticleStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::MovePageReticleRequest {betaId, 0}});
    ASSERT_TRUE(moveReticleStatus.ok()) << moveReticleStatus.error.message;
    EXPECT_EQ(page.staticReticles.front().id, "beta");
    EXPECT_EQ(page.staticReticles.back().id, "alpha");

    const auto pageReticleTransformStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetReticleTransformRequest {
                editor::automation::AutomationReticleTargetKind::PageReticleInstance,
                betaId.value,
                mfd::Transform2D {{0.35f, -0.10f}, 22.0f, {1.30f, 0.80f}}}});
    ASSERT_TRUE(pageReticleTransformStatus.ok()) << pageReticleTransformStatus.error.message;
    EXPECT_FLOAT_EQ(page.staticReticles.front().transform.position.x, 0.35f);
    EXPECT_FLOAT_EQ(page.staticReticles.front().transform.scale.y, 0.80f);

    const auto reticleAssetTransformStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetReticleTransformRequest {
                editor::automation::AutomationReticleTargetKind::ReticleAsset,
                reticleAssetId.value,
                mfd::Transform2D {{0.10f, 0.15f}, 45.0f, {2.0f, 2.0f}}}});
    ASSERT_TRUE(reticleAssetTransformStatus.ok()) << reticleAssetTransformStatus.error.message;
    EXPECT_FLOAT_EQ(bridge.loaded_.document.reticleLibrary.at("track_box").transform.scale.x, 2.0f);

    const auto betaBlinkStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetPageReticleBlinkRequest {
                betaId,
                mfd::ReticleBlinkState {true, "fast", {}, 0U}}});
    ASSERT_TRUE(betaBlinkStatus.ok()) << betaBlinkStatus.error.message;
    const auto alphaBlinkStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetPageReticleBlinkRequest {
                alphaId,
                mfd::ReticleBlinkState {true, {}, {}, 0U}}});
    ASSERT_TRUE(alphaBlinkStatus.ok()) << alphaBlinkStatus.error.message;
    EXPECT_TRUE(page.staticReticles.front().blink.enabled);
    EXPECT_EQ(page.staticReticles.front().blink.normalizedTypeName, "fast");
    EXPECT_EQ(page.staticReticles.front().blink.durationMs, 250U);
    EXPECT_EQ(page.staticReticles.back().blink.durationMs, 250U);

    const auto assignStrobeStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::AssignPageStrobeTemplateRequest {
                pageId,
                reticleAssetId,
                std::optional<std::string> {"strobe_beta"}}});
    ASSERT_TRUE(assignStrobeStatus.ok()) << assignStrobeStatus.error.message;
    ASSERT_TRUE(page.strobe.has_value());
    const auto strobeTransformStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::SetReticleTransformRequest {
                editor::automation::AutomationReticleTargetKind::PageStrobe,
                pageId.value,
                mfd::Transform2D {{0.0f, -0.33f}, 5.0f, {0.9f, 0.9f}}}});
    ASSERT_TRUE(strobeTransformStatus.ok()) << strobeTransformStatus.error.message;
    EXPECT_FLOAT_EQ(page.strobe->reticle.transform.position.y, -0.33f);

    const auto invalidBlinkCatalog = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::ReplacePageBlinkTypesRequest {
                pageId,
                std::vector<mfd::PageBlinkDefinition> {
                    mfd::PageBlinkDefinition {"dup", {}, 1000U},
                    mfd::PageBlinkDefinition {"dup", {}, 500U}}}});
    EXPECT_FALSE(invalidBlinkCatalog.ok());
    EXPECT_EQ(invalidBlinkCatalog.error.code, editor::automation::AutomationErrorCode::Conflict);

    ASSERT_TRUE(facade->SessionService().CommitSession(editor::automation::CommitSessionRequest {session.value}).ok());
}

TEST(EditorAutomationTests, ValidationReportsInvalidBlinkCatalogAndBindings)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    mfd::PageDefinition& page = bridge.loaded_.document.pages.front();
    page.blinkTypes = {
        mfd::PageBlinkDefinition {"Pulse", "pulse", 1000U},
        mfd::PageBlinkDefinition {"pulse", "pulse", 500U}};
    page.defaultBlinkTypeName = "ghost";
    page.normalizedDefaultBlinkTypeName = "ghost";
    page.staticReticles.push_back(
        editor::automation::MakePrimitiveReticle("ghost_track", mfd::PrimitiveType::Rectangle));
    page.staticReticles.front().blink.enabled = true;
    page.staticReticles.front().blink.typeName = "ghost";
    page.staticReticles.front().blink.normalizedTypeName = "ghost";
    page.strobe = mfd::PageStrobeDefinition {
        editor::automation::MakePrimitiveReticle("strobe_cursor", mfd::PrimitiveType::Rectangle),
        {},
        {}};
    page.strobe->reticle.blink.enabled = true;
    page.strobe->reticle.blink.typeName = "ghost";
    page.strobe->reticle.blink.normalizedTypeName = "ghost";

    const auto validation = facade->PersistenceService().ValidateCurrentState();
    ASSERT_TRUE(validation.ok());
    EXPECT_FALSE(validation.value.valid);

    auto hasDiagnostic = [&](const std::string_view targetId, const std::string_view fragment)
    {
        return std::any_of(validation.value.diagnostics.begin(),
                           validation.value.diagnostics.end(),
                           [&](const editor::automation::ValidationDiagnostic& diagnostic)
                           {
                               return diagnostic.entityId == targetId &&
                                      diagnostic.message.find(fragment) != std::string::npos;
                           });
    };

    const editor::automation::PageId pageId = editor::automation::MakePageId(page);
    const editor::automation::PageReticleInstanceId pageReticleId =
        editor::automation::MakePageReticleInstanceId(page, page.staticReticles.front());

    EXPECT_TRUE(hasDiagnostic(pageId.value, "Blink type names must stay unique"));
    EXPECT_TRUE(hasDiagnostic(pageId.value, "default blink type must resolve"));
    EXPECT_TRUE(hasDiagnostic(pageReticleId.value, "blink bindings must reference"));
    EXPECT_TRUE(hasDiagnostic(pageId.value, "strobe blink bindings must reference"));
}

TEST(EditorAutomationTests, ValidationAndEventQueueCoverDiagnosticsAndSessionLifecycle)
{
    FakeEditorAutomationBridge bridge;
    std::unique_ptr<editor::automation::IEditorAutomationFacade> facade =
        editor::automation::CreateEditorAutomationFacade(bridge);

    const std::filesystem::path tempFolder = MakeEditorAutomationTemporaryFolder();
    const auto session = facade->SessionService().BeginSession(editor::automation::BeginSessionRequest {"validate-events"});
    ASSERT_TRUE(session.ok());
    ASSERT_TRUE(facade->SessionService().ActiveSessionId().has_value());

    mfd::WindowAssetDefinition window;
    window.title = "Validate";

    mfd::PageDefinition page;
    page.name = "Validate";
    page.title = "Validate";
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"dup", true});
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"dup", false});

    const auto createStatus = facade->SessionService().ApplyAction(
        editor::automation::ApplyActionRequest {
            session.value,
            editor::automation::CreateWindowDocumentRequest {
                window,
                page,
                tempFolder / "validate_window.json",
                tempFolder / "reticles",
                tempFolder / "validate_page.json"}});
    ASSERT_TRUE(createStatus.ok()) << createStatus.error.message;

    const auto saveWhileActive = facade->PersistenceService().SaveAsset(
        editor::automation::SaveAssetRequest {
            editor::automation::SaveAssetRequest::Kind::Page,
            editor::automation::MakePageId(bridge.loaded_.document.pages.front()).value});
    EXPECT_FALSE(saveWhileActive.ok());
    EXPECT_EQ(saveWhileActive.error.code, editor::automation::AutomationErrorCode::InvalidState);

    const auto validation = facade->SessionService().ValidateSession(
        editor::automation::ValidateSessionRequest {session.value});
    ASSERT_TRUE(validation.ok());
    EXPECT_FALSE(validation.value.valid);
    ASSERT_FALSE(validation.value.diagnostics.empty());

    const std::vector<editor::automation::AutomationEvent> preRollbackEvents =
        facade->EventService().ConsumePendingEvents();
    ASSERT_EQ(preRollbackEvents.size(), 1U);
    EXPECT_EQ(preRollbackEvents.front().kind, editor::automation::AutomationEventKind::DocumentChanged);
    EXPECT_TRUE(facade->EventService().ConsumePendingEvents().empty());

    const auto rollback = facade->SessionService().RollbackSession(
        editor::automation::RollbackSessionRequest {session.value});
    ASSERT_TRUE(rollback.ok());
    EXPECT_FALSE(facade->SessionService().ActiveSessionId().has_value());

    const std::vector<editor::automation::AutomationEvent> postRollbackEvents =
        facade->EventService().ConsumePendingEvents();
    ASSERT_EQ(postRollbackEvents.size(), 1U);
    EXPECT_EQ(postRollbackEvents.front().kind, editor::automation::AutomationEventKind::SessionRolledBack);
    EXPECT_TRUE(facade->EventService().ConsumePendingEvents().empty());
}
