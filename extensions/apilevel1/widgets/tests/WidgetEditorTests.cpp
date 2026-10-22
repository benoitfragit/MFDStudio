#include "mfd/apilevel1/widgets/WidgetEditorModel.h"
#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
#include "mfd/apilevel1/widgets/WidgetAssetService.h"
#include "mfd/apilevel1/widgets/WidgetJson.h"
#include "mfd/apilevel1/ContainerEditorModel.h"
#include "EditorPageImportService.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <limits>
namespace
{
using json = nlohmann::json;
json Visual(const mfd::ReticleGroup& visual, const std::filesystem::path& base)
{ return json::parse(editor::SerializeReticleTemplateToJsonString(visual, base)); }
void Write(const std::filesystem::path& file, const std::string& bytes)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary); output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Fixture write failed.");
}
class WidgetFiles : public testing::Test
{
protected:
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("mfd_widgets_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path source = root / "source" / "assets";
    std::filesystem::path target = root / "target" / "assets";
    void SetUp() override
    {
        for (const auto& folder : {source, target})
        {
            std::filesystem::create_directories(folder / "reticles");
            Write(folder / "windows/window.json", R"({"title":"Widgets","apiLevel":1,"reticleLibraryFolder":"../reticles","pages":["../pages/Home.json"],"defaultPage":"Home"})");
            Write(folder / "pages/Home.json", R"({"name":"Home","layers":[{"id":"main"}],"title":"Home"})");
        }
        auto definition = mfd::api1::WidgetEditorModel::CreateDefinition(source / "widgets/Button.json", "Button", mfd::api1::WidgetType::Button);
        Write(definition->sourceFile, mfd::api1::SerializeWidgetDefinition(*definition, Visual).dump());
        Write(source / "pages/Home.json", R"({"name":"Home","layers":[{"id":"main"}],"widgets":[{"id":"A","definition":"../widgets/Button.json"},{"id":"B","definition":"../widgets/Button.json","overrides":{"text":"Independent"}}],"containers":[{"id":"Panel","children":[{"id":"A","kind":"widget"}]}]})");
    }
    void TearDown() override { std::error_code error; std::filesystem::remove_all(root, error); }
    mfd::LoadedWindowConfiguration Target() const
    { return mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(target / "windows/window.json"); }
    editor::EditorFileLayout Layout(const mfd::LoadedWindowConfiguration& loaded) const
    { editor::EditorFileLayout result; result.pageFiles = loaded.window.pageFiles; return result; }
    editor::PageImportRequest Request() const
    { return {source / "pages/Home.json", target / "pages", target / "reticles"}; }
    static bool Available(const std::filesystem::path&, std::string&) { return true; }
};
}
TEST(WidgetEditorAbi, RejectsShortTablesAndNeverCallsMissingCallbacks)
{
    MfdWidgetEditorApiV1 api {}; api.struct_size = sizeof(api) - 1;
    EXPECT_NE(mfd_get_widgets_editor_api_v1(&api), 0U);
    api.struct_size = sizeof(api);
    ASSERT_EQ(mfd_get_widgets_editor_api_v1(&api), 0U);
    EXPECT_EQ(api.abi_version, 1U);
    MfdWidgetEditorUiV1 ui {}; ui.struct_size = sizeof(ui); ui.version = 1;
    MfdWidgetEditorDraftV1 draft {}; draft.struct_size = sizeof(draft);
    EXPECT_EQ(api.draw(&ui, &draft), 0U);
}
TEST_F(WidgetFiles, MixedContainerDuplicateAndDeletePreserveIndependentLeaves)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    auto& page = loaded.document.pages.front();
    std::string error;
    const auto id = mfd::api1::ContainerEditorModel::Duplicate(page, "Panel", error);
    ASSERT_FALSE(id.empty()) << error;
    ASSERT_EQ(page.widgets.size(), 3U);
    EXPECT_EQ(page.widgets[0].definition, page.widgets[2].definition);
    page.widgets[2].overrides.text = "Copy";
    EXPECT_NE(mfd::api1::InitialWidgetState(page.widgets[0]).confirmed.text,
              mfd::api1::InitialWidgetState(page.widgets[2]).confirmed.text);
    ASSERT_TRUE(mfd::api1::ContainerEditorModel::Delete(page, id, error));
    EXPECT_EQ(page.widgets.size(), 2U);
    EXPECT_FALSE(loaded.document.widgetLibrary.empty());
}
TEST_F(WidgetFiles, InvalidRenameAndDefinitionEditLeaveDocumentUntouched)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    auto& page = loaded.document.pages.front();
    auto replacement = page.widgets[0]; replacement.id = "panel";
    std::string error;
    EXPECT_FALSE(mfd::api1::WidgetEditorModel::Replace(page, "A", replacement, "", nullptr, error));
    EXPECT_EQ(page.widgets[0].id, "A");
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>(*page.widgets[0].definition);
    definition->range.minimum = 4;
    EXPECT_FALSE(mfd::api1::WidgetEditorModel::ReplaceDefinition(loaded.document, definition, error));
    EXPECT_EQ(page.widgets[0].definition->range.minimum, 0);
}
TEST_F(WidgetFiles, ReparentPreservesOverridesAndTypedOwnership)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    auto& page = loaded.document.pages.front();
    auto widget = page.widgets[1]; std::string error;
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::Replace(page, "B", widget, "Panel", nullptr, error)) << error;
    EXPECT_EQ(page.containers.front().children.back().kind, mfd::api1::ContainerChildKind::Widget);
    EXPECT_EQ(*page.widgets[1].overrides.text, "Independent");
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::Replace(page, "B", widget, "", nullptr, error));
    EXPECT_EQ(page.containers.front().children.size(), 1U);
}
TEST_F(WidgetFiles, WidgetOnlyLevelZeroAndMissingEditorAreRejected)
{
    auto loaded = Target(); auto files = Layout(loaded);
    loaded.window.apiLevel = mfd::ApiLevel::Legacy;
    const editor::PageImportService supported(Available);
    EXPECT_FALSE(supported.BuildPlan(loaded, files, Request()).canExecute);
    loaded.window.apiLevel = mfd::ApiLevel::Level1;
    const editor::PageImportService unavailable([](const std::filesystem::path&, std::string& error)
        { error = "missing Widgets editor"; return false; });
    const auto plan = unavailable.BuildPlan(loaded, files, Request());
    EXPECT_FALSE(plan.canExecute);
    EXPECT_NE(plan.error.find("Widgets"), std::string::npos);
}
TEST_F(WidgetFiles, ImportRemapsDefinitionsAndPreservesTypedChildrenWithoutWriting)
{
    auto loaded = Target(); auto files = Layout(loaded); const editor::PageImportService service(Available);
    const auto plan = service.BuildPlan(loaded, files, Request());
    ASSERT_TRUE(plan.canExecute) << plan.error;
    std::string error;
    ASSERT_TRUE(service.Execute(plan, loaded, files, nullptr, &error)) << error;
    const auto& page = loaded.document.pages.back();
    ASSERT_EQ(page.widgets.size(), 2U);
    EXPECT_EQ(page.widgets[0].definition, page.widgets[1].definition);
    EXPECT_EQ(page.containers.front().children.front().childId, "A");
    EXPECT_NE(page.widgets[0].definition->sourceFile.generic_string().find("target"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(page.widgets[0].definition->sourceFile));
    ASSERT_TRUE(editor::SaveEditorDocument(loaded, files, &error)) << error;
    const auto reloaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(loaded.window.sourceFile);
    EXPECT_EQ(reloaded.document.pages.back().widgets.size(), 2U);
}
TEST_F(WidgetFiles, StaleDefinitionOrCapabilityRejectsImportWithoutMutation)
{
    auto loaded = Target(); auto files = Layout(loaded); bool capability = true;
    const editor::PageImportService service([&capability](const std::filesystem::path&, std::string& error)
        { error = capability ? "" : "Widgets unavailable"; return capability; });
    const auto plan = service.BuildPlan(loaded, files, Request()); ASSERT_TRUE(plan.canExecute) << plan.error;
    capability = false; std::string error;
    EXPECT_FALSE(service.Execute(plan, loaded, files, nullptr, &error));
    capability = true;
    auto bytes = editor::WidgetAssetService::ReadBounded(source / "widgets/Button.json"); bytes += "\n";
    Write(source / "widgets/Button.json", bytes);
    EXPECT_FALSE(service.Execute(plan, loaded, files, nullptr, &error));
    EXPECT_EQ(loaded.document.pages.size(), 1U);
    EXPECT_TRUE(loaded.document.widgetLibrary.empty());
    EXPECT_TRUE(files.stagedResources.empty());
}
TEST_F(WidgetFiles, DefinitionCollisionDeduplicatesIdenticalContentAndRenamesDifferentContent)
{
    auto loaded = Target(); auto files = Layout(loaded);
    auto definition = mfd::JsonLoader {}.LoadWidgetAsset(source / "widgets/Button.json", source, {});
    const auto first = editor::WidgetAssetService::Stage(*definition, source, target, loaded.document, files);
    const auto second = editor::WidgetAssetService::Stage(*definition, source, target, loaded.document, files);
    EXPECT_EQ(first, second);
    auto changed = *definition; changed.defaults.text = "Different";
    const auto third = editor::WidgetAssetService::Stage(changed, source, target, loaded.document, files);
    EXPECT_NE(first->id, third->id);
    EXPECT_NE(first->sourceFile, third->sourceFile);
}
TEST_F(WidgetFiles, ResourceAndDefinitionRecoveryIsLosslessAndConflictFailsBeforePromotion)
{
    auto loaded = Target(); auto files = Layout(loaded);
    auto definition = mfd::api1::WidgetEditorModel::CreateDefinition(source / "widgets/Image.json", "Image", mfd::api1::WidgetType::Label);
    auto imageDefinition = *definition;
    mfd::Primitive image; image.id = "Raster"; image.type = mfd::PrimitiveType::Image;
    image.geometry = mfd::ImageGeometry {source / "images/image.png", 0.1f, 0.1f};
    imageDefinition.visuals[0].composition.primitives.push_back(image);
    const std::string bytes("PNG\0\xff", 5);
    Write(source / "images/image.png", bytes);
    const auto staged = editor::WidgetAssetService::Stage(imageDefinition, source, target, loaded.document, files);
    ASSERT_EQ(files.stagedResources.size(), 1U);
    const auto resource = files.stagedResources.begin()->first;
    EXPECT_FALSE(std::filesystem::exists(resource));
    const auto bundle = editor::SerializeRecoveryBundleToJsonString(loaded, files);
    std::filesystem::path restored; std::string error;
    ASSERT_TRUE(editor::RestoreRecoveryBundle(bundle, target, restored, &error)) << error;
    EXPECT_EQ(editor::WidgetAssetService::ReadBounded(resource), bytes);
    EXPECT_TRUE(std::filesystem::exists(staged->sourceFile));
    Write(resource, "conflict");
    const auto windowBefore = editor::WidgetAssetService::ReadBounded(loaded.window.sourceFile);
    loaded.window.title = "Must not persist";
    EXPECT_FALSE(editor::SaveEditorDocument(loaded, files, &error));
    EXPECT_EQ(editor::WidgetAssetService::ReadBounded(loaded.window.sourceFile), windowBefore);
}

TEST_F(WidgetFiles, DefinitionRenameCannotCollideWithAnUnusedLibraryDefinition)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    loaded.document.widgetLibrary.push_back(mfd::api1::WidgetEditorModel::CreateDefinition(
        source / "widgets/Other.json", "Other", mfd::api1::WidgetType::Button));
    const auto original = loaded.document.pages.front().widgets.front().definition;
    auto renamed = std::make_shared<mfd::api1::WidgetDefinition>(*original);
    renamed->id = "other";
    std::string error;
    EXPECT_FALSE(mfd::api1::WidgetEditorModel::ReplaceDefinition(loaded.document, renamed, error));
    EXPECT_EQ(loaded.document.pages.front().widgets.front().definition, original);
    EXPECT_EQ(loaded.document.widgetLibrary.size(),2U);
}

TEST_F(WidgetFiles, ProportionalGestureFollowsPageSpaceAndKeepsSharedDefinition)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    auto& page = loaded.document.pages.front();
    page.containers[0].geometry.size = {2.0f,1.0f};
    page.containers[0].layoutReferenceSize = {1.0f,1.0f};
    page.containers[0].children[0].resizeMode = mfd::api1::ChildResizeMode::Proportional;
    const auto initial = page.widgets[0];
    std::string error;
    std::vector<mfd::api1::ContainerLayoutNode> before, after;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page.containers,page.widgets,before,error));
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,{0.2f,0.1f},false,error)) << error;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page.containers,page.widgets,after,error));
    EXPECT_NEAR(after[1].bounds.center.x-before[1].bounds.center.x,0.2f,0.00001f);
    EXPECT_NEAR(after[1].bounds.center.y-before[1].bounds.center.y,0.1f,0.00001f);
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,{0.2f,-0.1f},true,error)) << error;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page.containers,page.widgets,after,error));
    EXPECT_NEAR(after[1].bounds.size.x-before[1].bounds.size.x,0.4f,0.00001f);
    EXPECT_NEAR(after[1].bounds.size.y-before[1].bounds.size.y,0.2f,0.00001f);
    EXPECT_EQ(page.widgets[0].definition,initial.definition);
    EXPECT_FALSE(page.widgets[1].size.has_value());
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,{},true,error));
    EXPECT_FALSE(page.widgets[0].size.has_value());
    EXPECT_FLOAT_EQ(page.widgets[0].position.x,initial.position.x);
}
TEST_F(WidgetFiles, ParentControlledGestureDoesNotRecordInvisibleGeometryChanges)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfigurationForAuthoring(source / "windows/window.json");
    auto& page = loaded.document.pages.front();
    const auto initial = page.widgets[0];
    std::string error;
    page.containers[0].type = mfd::api1::ContainerType::Grid;
    EXPECT_FALSE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,{0.2f,0.1f},false,error));
    EXPECT_EQ(page.widgets[0].position.x,initial.position.x);
    page.containers[0].children[0].horizontalAlignment = mfd::api1::ContainerAlignment::Stretch;
    ASSERT_TRUE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,{0.2f,-0.1f},true,error)) << error;
    EXPECT_FLOAT_EQ(page.widgets[0].size->x,initial.definition->preferredSize.x);
    EXPECT_GT(page.widgets[0].size->y,initial.definition->preferredSize.y);
    const auto size = page.widgets[0].size;
    EXPECT_FALSE(mfd::api1::WidgetEditorModel::ApplyGesture(page,initial,
        {std::numeric_limits<float>::quiet_NaN(),0.0f},true,error));
    EXPECT_EQ(page.widgets[0].size->x,size->x);
    EXPECT_EQ(page.widgets[0].size->y,size->y);
}
