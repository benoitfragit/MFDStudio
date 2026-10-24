#include "internal/application/EditorWidgetPreviewController.h"

#include "mfd/apilevel1/widgets/WidgetModel.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
std::shared_ptr<const mfd::api1::WidgetDefinition> MakeDefinition(
    const mfd::api1::WidgetType type,
    const std::string& id)
{
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>();
    definition->id = id + "Definition";
    definition->type = type;
    definition->preferredSize = {0.4f, 0.2f};
    definition->hitRegion = {{0.0f, 0.0f}, {0.4f, 0.2f}};
    definition->range = {0.0, 100.0, 5.0, 0U};
    definition->defaults.value = 50.0;
    definition->visuals.push_back({});
    return definition;
}

mfd::api1::WidgetInstance MakeWidget(const mfd::api1::WidgetType type,
                                     const std::string& id)
{
    mfd::api1::WidgetInstance widget;
    widget.id = id;
    widget.definition = MakeDefinition(type, id);
    return widget;
}

std::vector<mfd::api1::ContainerLayoutNode> Evaluate(
    const std::vector<mfd::api1::WidgetInstance>& widgets)
{
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    EXPECT_TRUE(mfd::api1::EvaluateWidgetLayout({}, widgets, layouts, error)) << error;
    return layouts;
}

editor::app::WidgetPreviewPointerSample Sample(const float x,
                                               const bool down,
                                               const bool began,
                                               const bool released)
{
    editor::app::WidgetPreviewPointerSample sample;
    sample.pointer = {x, 0.0f};
    sample.down = down;
    sample.began = began;
    sample.released = released;
    return sample;
}
} // namespace

TEST(EditorWidgetPreviewController, ToggleUsesRuntimePressReleaseSemanticsWithoutAuthoringMutation)
{
    const std::vector<mfd::api1::WidgetInstance> widgets {MakeWidget(mfd::api1::WidgetType::Toggle, "Toggle")};
    const std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
    ASSERT_EQ(layouts.size(), 1U);

    editor::app::EditorWidgetPreviewController controller;
    controller.SetEnabled(true);
    EXPECT_TRUE(controller.Update({}, layouts, widgets, Sample(0.0f, true, true, false)));
    EXPECT_TRUE(controller.PreviewState(0U, widgets[0], true, true).pressed);
    EXPECT_TRUE(controller.Update({}, layouts, widgets, Sample(0.0f, false, false, true)));

    const mfd::api1::WidgetState preview = controller.PreviewState(0U, widgets[0], true, true);
    EXPECT_FALSE(preview.pressed);
    EXPECT_FALSE(preview.pending);
    EXPECT_TRUE(preview.confirmed.checked);
    EXPECT_FALSE(widgets[0].overrides.checked.has_value());
}

TEST(EditorWidgetPreviewController, CheckboxRadioAndButtonCoverEveryDiscreteSemanticType)
{
    const std::array<mfd::api1::WidgetType, 3U> types {
        mfd::api1::WidgetType::Button,
        mfd::api1::WidgetType::Checkbox,
        mfd::api1::WidgetType::Radio};

    for (const mfd::api1::WidgetType type : types)
    {
        const std::vector<mfd::api1::WidgetInstance> widgets {MakeWidget(type, "Discrete")};
        const std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
        editor::app::EditorWidgetPreviewController controller;
        controller.SetEnabled(true);
        controller.Update({}, layouts, widgets, Sample(0.0f, true, true, false));
        controller.Update({}, layouts, widgets, Sample(0.0f, false, false, true));
        const bool expectedChecked = type != mfd::api1::WidgetType::Button;
        EXPECT_EQ(controller.PreviewState(0U, widgets[0], true, true).confirmed.checked, expectedChecked);
    }
}

TEST(EditorWidgetPreviewController, SliderAndKnobClampAndStepProposedValues)
{
    const std::array<mfd::api1::WidgetType, 2U> types {
        mfd::api1::WidgetType::Slider,
        mfd::api1::WidgetType::Knob};

    for (const mfd::api1::WidgetType type : types)
    {
        const std::vector<mfd::api1::WidgetInstance> widgets {MakeWidget(type, "Numeric")};
        const std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
        editor::app::EditorWidgetPreviewController controller;
        controller.SetEnabled(true);
        controller.Update({}, layouts, widgets, Sample(0.15f, true, true, false));
        controller.Update({}, layouts, widgets, Sample(0.15f, false, false, true));
        const double value = controller.PreviewState(0U, widgets[0], true, true).confirmed.value;
        EXPECT_GE(value, 0.0);
        EXPECT_LE(value, 100.0);
        EXPECT_DOUBLE_EQ(std::fmod(value, 5.0), 0.0);
    }
}

TEST(EditorWidgetPreviewController, CancelDisabledAndInvalidPointerCannotLeaveCapturePressed)
{
    std::vector<mfd::api1::WidgetInstance> widgets {MakeWidget(mfd::api1::WidgetType::Toggle, "Toggle")};
    std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
    editor::app::EditorWidgetPreviewController controller;
    controller.SetEnabled(true);
    controller.Update({}, layouts, widgets, Sample(0.0f, true, true, false));

    editor::app::WidgetPreviewPointerSample cancel = Sample(0.0f, false, false, false);
    cancel.cancel = true;
    controller.Update({}, layouts, widgets, cancel);
    EXPECT_FALSE(controller.PreviewState(0U, widgets[0], true, true).pressed);

    widgets[0].enabled = false;
    layouts = Evaluate(widgets);
    controller.SetEnabled(false);
    controller.SetEnabled(true);
    EXPECT_TRUE(controller.Update({}, layouts, widgets, Sample(0.0f, true, true, false)));
    EXPECT_FALSE(controller.PreviewState(0U, widgets[0], true, false).pressed);

    editor::app::WidgetPreviewPointerSample invalid;
    invalid.pointer.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(controller.Update({}, layouts, widgets, invalid));
}

TEST(EditorWidgetPreviewController, DefinitionAndOverrideChangesResetStalePreviewValues)
{
    std::vector<mfd::api1::WidgetInstance> widgets {MakeWidget(mfd::api1::WidgetType::Slider, "Slider")};
    std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
    editor::app::EditorWidgetPreviewController controller;
    controller.SetEnabled(true);
    controller.Update({}, layouts, widgets, Sample(0.18f, true, true, false));
    controller.Update({}, layouts, widgets, Sample(0.18f, false, false, true));
    EXPECT_GT(controller.PreviewState(0U, widgets[0], true, true).confirmed.value, 50.0);

    auto replacement = std::make_shared<mfd::api1::WidgetDefinition>(*widgets[0].definition);
    replacement->range = {0.0, 10.0, 1.0, 0U};
    replacement->defaults.value = 4.0;
    widgets[0].definition = std::move(replacement);
    layouts = Evaluate(widgets);
    EXPECT_FALSE(controller.Update({}, layouts, widgets, Sample(2.0f, false, false, false)));
    EXPECT_DOUBLE_EQ(controller.PreviewState(0U, widgets[0], true, true).confirmed.value, 4.0);

    widgets[0].overrides.value = 3.0;
    controller.Update({}, layouts, widgets, Sample(2.0f, false, false, false));
    EXPECT_DOUBLE_EQ(controller.PreviewState(0U, widgets[0], true, true).confirmed.value, 3.0);
}

TEST(EditorWidgetPreviewController, MaximumWidgetBudgetRemainsBoundedAcrossPointerFrames)
{
    const auto sharedDefinition = MakeDefinition(mfd::api1::WidgetType::Button, "Shared");
    std::vector<mfd::api1::WidgetInstance> widgets;
    widgets.reserve(mfd::api1::kMaximumWidgetCount);
    for (std::size_t index = 0U; index < mfd::api1::kMaximumWidgetCount; ++index)
    {
        mfd::api1::WidgetInstance widget;
        widget.id = "Widget" + std::to_string(index);
        widget.definition = sharedDefinition;
        widget.position = {static_cast<float>(index % 64U), static_cast<float>(index / 64U)};
        widgets.push_back(std::move(widget));
    }
    const std::vector<mfd::api1::ContainerLayoutNode> layouts = Evaluate(widgets);
    ASSERT_EQ(layouts.size(), widgets.size());

    editor::app::EditorWidgetPreviewController controller;
    controller.SetEnabled(true);
    for (std::size_t frame = 0U; frame < 8U; ++frame)
    {
        EXPECT_TRUE(controller.Update({}, layouts, widgets, Sample(0.0f, frame == 0U, frame == 0U, false)));
    }
    controller.SetEnabled(false);
    EXPECT_FALSE(controller.Enabled());
}
