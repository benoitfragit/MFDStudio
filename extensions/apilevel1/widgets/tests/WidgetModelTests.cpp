#include <gtest/gtest.h>
#include "mfd/apilevel1/widgets/WidgetJson.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/apilevel1/internal/ContainerCatalogJson.h"
#include <nlohmann/json.hpp>
#include <limits>
#include "mfd/apilevel1/widgets/WidgetVisuals.h"
#include "mfd/apilevel1/widgets/WidgetOcclusion.h"

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
    EXPECT_EQ(mfd::api1::ResolveWidgetAsset(root / "pages", root, "../widgets/button.json"),
              std::filesystem::weakly_canonical(root / "widgets/button.json"));
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

TEST(WidgetLayoutModes, TypedLeavesUseEveryLayoutAndInheritVisibility)
{
    for (const auto type : {mfd::api1::ContainerType::Canvas, mfd::api1::ContainerType::Linear,
                           mfd::api1::ContainerType::Grid, mfd::api1::ContainerType::Overlay})
    {
        mfd::api1::ContainerDefinition parent; parent.id = "parent"; parent.type = type;
        parent.children.push_back({"widget"}); parent.children.back().kind = mfd::api1::ContainerChildKind::Widget;
        parent.visible = false; parent.enabled = false;
        std::vector<mfd::api1::ContainerLayoutNode> layouts; std::string error;
        ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout({parent}, {Button("widget")}, layouts, error)) << error;
        ASSERT_EQ(layouts.size(), 2U);
        EXPECT_FALSE(layouts[1].effectiveVisible); EXPECT_FALSE(layouts[1].effectiveEnabled);
        EXPECT_GT(layouts[1].bounds.size.x, 0); EXPECT_GT(layouts[1].bounds.size.y, 0);
    }
}
TEST(WidgetLayoutModes, Accepts64ContainerLevelsWithLeafButRejects65)
{
    std::vector<mfd::api1::ContainerDefinition> containers(64);
    for (std::size_t index = 0; index < containers.size(); ++index)
    {
        containers[index].id = "c" + std::to_string(index);
        if (index + 1 < containers.size()) containers[index].children.push_back({"c" + std::to_string(index + 1)});
    }
    containers.back().children.push_back({"widget"});
    containers.back().children.back().kind = mfd::api1::ContainerChildKind::Widget;
    std::string error;
    EXPECT_TRUE(mfd::api1::ValidatePageWidgets(containers, {Button("widget")}, error)) << error;
    mfd::api1::ContainerDefinition root; root.id = "root"; root.children.push_back({"c0"});
    containers.push_back(root);
    EXPECT_FALSE(mfd::api1::ValidatePageWidgets(containers, {Button("widget")}, error));
}
TEST(WidgetVisualBinding, FormatsValueAndAppliesParentTransformOnce)
{
    auto widget = Button("value");
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>(*widget.definition);
    definition->type = mfd::api1::WidgetType::NumericDisplay;
    mfd::Primitive label; label.id = "text"; label.type = mfd::PrimitiveType::Text;
    label.geometry = mfd::TextGeometry {};
    definition->visuals[0].composition.id = "Visual";
    definition->visuals[0].composition.primitives.push_back(label);
    definition->visuals[0].bindings.push_back({"text", mfd::api1::WidgetBindingTarget::Text});
    widget.definition = definition;
    auto state = mfd::api1::InitialWidgetState(widget); state.confirmed.value = 0.375;
    const auto visual = mfd::api1::BuildWidgetVisual(widget, state, {{0.5f,0.4f},{0.8f,0.4f}});
    ASSERT_EQ(visual.primitives.size(), 1U);
    EXPECT_EQ(std::get<mfd::TextGeometry>(visual.primitives[0].geometry).text, "0.38");
    EXPECT_FLOAT_EQ(visual.transform.position.x, 0.5f);
    EXPECT_FLOAT_EQ(visual.transform.scale.x, 2.0f);
}
TEST(WidgetOcclusion, InvisibleAndFullyTransparentReticlesDoNotBlockInput)
{
    mfd::ReticleGroup reticle; mfd::Primitive shape;
    shape.type = mfd::PrimitiveType::Rectangle; shape.geometry = mfd::RectangleGeometry {1,1};
    shape.style.filled = true; shape.style.color.a = 0; shape.style.fillColor.a = 0;
    reticle.primitives.push_back(shape);
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0,0}));
    reticle.primitives[0].style.fillColor.a = 255;
    EXPECT_TRUE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0,0}));
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {2,2}));
    reticle.visible = false;
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0,0}));
}

namespace
{
bool SampleOpaqueRightHalf(void*, const mfd::ImageGeometry&, const mfd::Vec2 uv) noexcept { return uv.x >= 0.5f; }
}
TEST(WidgetOcclusion, TransparentImagePixelsAndUnfilledShapeInteriorsPassThrough)
{
    mfd::ReticleGroup reticle; mfd::Primitive shape;
    shape.type = mfd::PrimitiveType::Rectangle; shape.geometry = mfd::RectangleGeometry {1,1};
    shape.style.filled = false; shape.style.thickness = 0.01f;
    reticle.primitives.push_back(shape);
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0,0}));
    EXPECT_TRUE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0.5f,0}));
    reticle.primitives[0].geometry = mfd::CircleGeometry {0.5f};
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0,0}));
    EXPECT_TRUE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0.5f,0}));
    reticle.primitives[0].type = mfd::PrimitiveType::Image;
    reticle.primitives[0].geometry = mfd::ImageGeometry {"alpha.png",1,1};
    EXPECT_FALSE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {-0.25f,0}, SampleOpaqueRightHalf));
    EXPECT_TRUE(mfd::api1::ReticleOccludesWidgetPointer(reticle, {0.25f,0}, SampleOpaqueRightHalf));
}

TEST(WidgetOcclusion, ClipRestorationUsesMaskGeometryEvenWhenMaskPrimitiveIsHidden)
{
    mfd::ReticleGroup reticle; mfd::Primitive mask;
    mask.id = "Mask"; mask.type = mfd::PrimitiveType::Circle;
    mask.geometry = mfd::CircleGeometry {0.5f}; mask.style.visible = false;
    reticle.primitives.push_back(mask);
    reticle.clipping.mode = mfd::ReticleClipMode::Inner; reticle.clipping.primitiveId = "Mask";
    EXPECT_TRUE(mfd::api1::ReticleErasesWidgetOcclusion(reticle, {0,0}));
    EXPECT_FALSE(mfd::api1::ReticleErasesWidgetOcclusion(reticle, {1,0}));
    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    EXPECT_FALSE(mfd::api1::ReticleErasesWidgetOcclusion(reticle, {0,0}));
    EXPECT_TRUE(mfd::api1::ReticleErasesWidgetOcclusion(reticle, {1,0}));
}
