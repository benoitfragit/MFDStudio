#include <gtest/gtest.h>
#include "mfd/apilevel1/widgets/WidgetJson.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/apilevel1/internal/ContainerCatalogJson.h"
#include <nlohmann/json.hpp>
#include <limits>

namespace
{
mfd::api1::WidgetInstance Button(const std::string& id)
{
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>();
    definition->id = "button_style";
    definition->preferredSize = {0.4f, 0.2f};
    definition->visuals.push_back({});
    mfd::api1::WidgetInstance widget;
    widget.id = id;
    widget.definition = std::move(definition);
    return widget;
}
TEST(WidgetModel, SharedDefinitionKeepsIndependentOverrides)
{
    auto first = Button("first");
    auto second = first;
    second.id = "second";
    second.overrides.checked = true;
    EXPECT_EQ(first.definition, second.definition);
    EXPECT_FALSE(mfd::api1::InitialWidgetState(first).confirmed.checked);
    EXPECT_TRUE(mfd::api1::InitialWidgetState(second).confirmed.checked);
}
TEST(WidgetModel, RejectsCaseCollisionWithContainers)
{
    mfd::api1::ContainerDefinition container;
    container.id = "Control";
    std::string error;
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets({container}, {Button("control")}, error));
}
TEST(WidgetModel, RejectsNonFiniteOverride)
{
    auto widget = Button("button");
    widget.overrides.value = std::numeric_limits<double>::quiet_NaN();
    std::string error;
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets({}, {widget}, error));
}
TEST(WidgetModel, RejectsMissingAndUnsupportedDefinitions)
{
    auto widget = Button("button");
    widget.definition.reset();
    std::string error;
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets({}, {widget}, error));
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>(*Button("button").definition);
    definition->schemaVersion = 2;
    EXPECT_FALSE(mfd::api1::ValidateWidgetDefinition(*definition, error));
}
TEST(WidgetLayout, RootAndTypedLeafContributeToFittedSize)
{
    mfd::api1::ContainerDefinition parent;
    parent.id = "parent";
    parent.type = mfd::api1::ContainerType::Linear;
    parent.sizeMode = mfd::api1::ContainerSizeMode::FitContent;
    mfd::api1::ContainerChildPlacement child;
    child.childId = "nested";
    child.kind = mfd::api1::ContainerChildKind::Widget;
    parent.children.push_back(child);
    std::vector<mfd::api1::ContainerLayoutNode> layout;
    std::string error;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout({parent}, {Button("nested"), Button("root")}, layout, error)) << error;
    ASSERT_EQ(layout.size(), 3U);
    EXPECT_FLOAT_EQ(layout[0].bounds.size.x, 0.4f);
    EXPECT_FLOAT_EQ(layout[0].bounds.size.y, 0.2f);
    EXPECT_EQ(layout[1].parentSourceIndex, 0U);
    EXPECT_EQ(layout[2].parentSourceIndex, std::numeric_limits<std::size_t>::max());
}
TEST(WidgetLayout, RejectsMultipleOwnershipAndWrongKind)
{
    mfd::api1::ContainerDefinition first;
    first.id = "one";
    mfd::api1::ContainerChildPlacement child;
    child.childId = "button";
    first.children.push_back(child);
    std::string error;
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets({first}, {Button("button")}, error));
    first.children[0].kind = mfd::api1::ContainerChildKind::Widget;
    auto second = first;
    second.id = "two";
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets({first, second}, {Button("button")}, error));
}
TEST(WidgetLayout, PreservesContainerOnlyValidationAndJson)
{
    mfd::api1::ContainerDefinition container;
    container.id = "container";
    const auto json = mfd::api1::SerializePageContainers({container});
    EXPECT_EQ(json, mfd::api1::detail::SerializeContainerCatalog({container}));
    std::vector<mfd::api1::ContainerLayoutNode> legacy;
    std::vector<mfd::api1::ContainerLayoutNode> mixed;
    std::string error;
    ASSERT_TRUE(mfd::api1::ContainerLayoutEngine {}.Evaluate({container}, legacy, error));
    ASSERT_TRUE(mfd::api1::EvaluateContainersWithWidgets({container}, {}, mixed, error));
    ASSERT_EQ(mixed.size(), legacy.size());
    EXPECT_EQ(mixed[0].id, legacy[0].id);
    EXPECT_FLOAT_EQ(mixed[0].bounds.size.x, legacy[0].bounds.size.x);
}
TEST(WidgetJson, TypedChildRoundTripsWithoutAddingLegacyKind)
{
    mfd::api1::ContainerDefinition parent;
    parent.id = "parent";
    parent.children.push_back({"child"});
    parent.children.push_back({"button"});
    parent.children.back().kind = mfd::api1::ContainerChildKind::Widget;
    const auto json = mfd::api1::detail::SerializeContainerCatalog({parent});
    EXPECT_FALSE(json[0]["children"][0].contains("kind"));
    EXPECT_EQ(json[0]["children"][1]["kind"], "widget");
    const auto parsed = mfd::api1::detail::ParseContainerCatalog({{"containers", json}});
    EXPECT_EQ(parsed[0].children[1].kind, mfd::api1::ContainerChildKind::Widget);
}
TEST(WidgetJson, RejectsAbsoluteAndEscapingDependencies)
{
    const auto root = std::filesystem::temp_directory_path() / "mfd_widget_assets";
    EXPECT_THROW(mfd::api1::ResolveWidgetAsset(root, root, "../secret.json"), std::runtime_error);
    EXPECT_THROW(mfd::api1::ResolveWidgetAsset(root, root, root.string()), std::runtime_error);
    EXPECT_EQ(mfd::api1::ResolveWidgetAsset(root / "pages", root, "../widgets/button.json"), root / "widgets/button.json");
}
TEST(WidgetVisual, HasDeterministicPriority)
{
    auto state = mfd::api1::InitialWidgetState(Button("button"));
    state.hovered = true;
    state.confirmed.checked = true;
    state.confirmed.warning = true;
    state.pending = true;
    state.pressed = true;
    EXPECT_EQ(mfd::api1::ResolveWidgetVisualState(state), mfd::api1::WidgetVisualState::Pressed);
    state.enabled = false;
    EXPECT_EQ(mfd::api1::ResolveWidgetVisualState(state), mfd::api1::WidgetVisualState::Disabled);
}
}
