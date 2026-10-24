#include <gtest/gtest.h>
#include "CockpitDemoController.h"
#include "WidgetsExampleUi.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/control/CommandProcessor.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"

#include <algorithm>
#include <array>
#include <limits>
#include <set>

namespace
{
const mfd::api1::WidgetState* FindState(const std::vector<mfd::api1::WidgetInstance>& instances,
                                        const std::vector<mfd::api1::WidgetState>& states,
                                        const std::string_view id)
{
    for (std::size_t index = 0U; index < instances.size() && index < states.size(); ++index)
    {
        if (instances[index].id == id)
        {
            return &states[index];
        }
    }
    return nullptr;
}

mfd::api1::WidgetState StateForVisual(const mfd::api1::WidgetInstance& widget,
                                      const mfd::api1::WidgetVisualState visual)
{
    mfd::api1::WidgetState state = mfd::api1::InitialWidgetState(widget);
    state.enabled = true;
    state.hovered = false;
    state.pressed = false;
    state.pending = false;
    state.confirmed.checked = false;
    state.confirmed.warning = false;
    switch (visual)
    {
    case mfd::api1::WidgetVisualState::Hover:
        state.hovered = true;
        break;
    case mfd::api1::WidgetVisualState::Pressed:
        state.pressed = true;
        break;
    case mfd::api1::WidgetVisualState::Checked:
        state.confirmed.checked = true;
        break;
    case mfd::api1::WidgetVisualState::Disabled:
        state.enabled = false;
        break;
    case mfd::api1::WidgetVisualState::Warning:
        state.confirmed.warning = true;
        break;
    case mfd::api1::WidgetVisualState::Pending:
        state.pending = true;
        break;
    case mfd::api1::WidgetVisualState::Normal:
        break;
    }
    return state;
}
} // namespace

TEST(WidgetExample, LoadsSharedDefinitionsAndCompilesGeneratedAuthorityPath)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    ASSERT_TRUE(loaded.generatedTransportMap);
    ASSERT_EQ(loaded.document.pages.size(), 1U);
    const auto& page = loaded.document.pages.front();
    ASSERT_EQ(page.widgets.size(), 21U);
    std::vector<mfd::api1::ContainerLayoutNode> layout;
    std::string error;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page.containers, page.widgets, layout, error)) << error;
    bool scrollable = false;
    for (const auto& node : layout)
        if (node.id.find("ScrollLinear") != std::string::npos) scrollable = node.verticalScrollbar.visible;
    EXPECT_TRUE(scrollable);
    mfd::SceneRegistry scene(std::move(loaded.document));
    scene.LoadDocument(scene.Document(), loaded.generatedTransportMap);
    mfd::CommandProcessor processor(scene);
    widgets_example::WidgetsExampleUi ui;
    ui.Widgets().ToggleAWidget().SetChecked(true);
    ui.Widgets().ToggleBWidget().SetChecked(false);
    ui.Widgets().SliderWidget().SetValue(2400);
    const auto batch = ui.BuildCommandBatch();
    ASSERT_TRUE(processor.Submit(batch)) << processor.LastError();
    std::vector<mfd::api1::WidgetInstance> instances;
    std::vector<mfd::api1::WidgetState> states;
    scene.CollectActiveWidgets(instances, states);
    ASSERT_EQ(instances.size(), states.size());
    for (std::size_t index = 0; index < instances.size(); ++index)
    {
        if (instances[index].id == "ToggleA") EXPECT_TRUE(states[index].confirmed.checked);
        if (instances[index].id == "ToggleB") EXPECT_FALSE(states[index].confirmed.checked);
        if (instances[index].id == "Slider") EXPECT_DOUBLE_EQ(states[index].confirmed.value, 2400);
        if (instances[index].id == "PrivateButton") EXPECT_NE(scene.ActiveWidgetTransportId(instances[index].id), 0U);
    }
}

TEST(WidgetExample, EveryDefinitionAuthorsAndBuildsAllSevenVisualStates)
{
    const auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    const auto& page = loaded.document.pages.front();
    const std::array<mfd::api1::WidgetVisualState, 7U> visualStates {
        mfd::api1::WidgetVisualState::Normal,
        mfd::api1::WidgetVisualState::Hover,
        mfd::api1::WidgetVisualState::Pressed,
        mfd::api1::WidgetVisualState::Checked,
        mfd::api1::WidgetVisualState::Disabled,
        mfd::api1::WidgetVisualState::Warning,
        mfd::api1::WidgetVisualState::Pending};

    for (const std::shared_ptr<const mfd::api1::WidgetDefinition>& definition : loaded.document.widgetLibrary)
    {
        ASSERT_NE(definition, nullptr);
        std::set<mfd::api1::WidgetVisualState> authoredStates;
        for (const mfd::api1::WidgetVisual& visual : definition->visuals)
        {
            authoredStates.insert(visual.state);
        }
        EXPECT_EQ(authoredStates.size(), visualStates.size()) << definition->id;
        for (const mfd::api1::WidgetVisualState state : visualStates)
        {
            EXPECT_EQ(authoredStates.count(state), 1U) << definition->id;
        }
    }

    const mfd::api1::ContainerRect bounds {{0.0f, 0.0f}, {0.5f, 0.3f}};
    for (const mfd::api1::WidgetInstance& widget : page.widgets)
    {
        for (const mfd::api1::WidgetVisualState visual : visualStates)
        {
            const mfd::api1::WidgetState state = StateForVisual(widget, visual);
            EXPECT_EQ(mfd::api1::ResolveWidgetVisualState(state), visual);
            const mfd::ReticleGroup rendered = mfd::api1::BuildWidgetVisual(widget, state, bounds);
            EXPECT_FALSE(rendered.primitives.empty()) << widget.id;
        }
    }
}

TEST(WidgetExample, FuelSchematicConnectsPortsWithoutCoveringValveCenters)
{
    const auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    const mfd::PageDefinition& page = loaded.document.pages.front();
    EXPECT_FALSE(page.titleDisplay.visible);
    std::vector<mfd::api1::ContainerLayoutNode> layout;
    std::string error;
    ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page.containers, page.widgets, layout, error)) << error;

    const std::array<std::array<std::string_view, 3U>, 3U> connections {{
        {"LeftTankFeed", "LeftTank", "LeftValve"},
        {"RightTankFeed", "RightTank", "RightValve"},
        {"CenterTankFeed", "ProgressBar", "RootToggle"},
    }};
    for (const auto& connection : connections)
    {
        const auto pipe = std::find_if(page.staticReticles.begin(), page.staticReticles.end(),
            [&connection](const mfd::ReticleGroup& reticle) { return reticle.id == connection[0]; });
        ASSERT_NE(pipe, page.staticReticles.end());
        ASSERT_EQ(pipe->primitives.size(), 1U);
        const auto* geometry = std::get_if<mfd::LineGeometry>(&pipe->primitives.front().geometry);
        ASSERT_NE(geometry, nullptr);
        const std::string tankId = "widget/" + std::string(connection[1]);
        const std::string valveId = "widget/" + std::string(connection[2]);
        const auto tank = std::find_if(layout.begin(), layout.end(),
            [&tankId](const mfd::api1::ContainerLayoutNode& node) { return node.id == tankId; });
        const auto valve = std::find_if(layout.begin(), layout.end(),
            [&valveId](const mfd::api1::ContainerLayoutNode& node) { return node.id == valveId; });
        ASSERT_NE(tank, layout.end());
        ASSERT_NE(valve, layout.end());
        EXPECT_NEAR(geometry->start.x, tank->bounds.center.x, 0.001f);
        EXPECT_NEAR(geometry->end.x, valve->bounds.center.x, 0.001f);
        EXPECT_NEAR(geometry->start.y, tank->bounds.center.y + 0.170f, 0.003f);
        EXPECT_NEAR(geometry->end.y, valve->bounds.center.y - 0.038f, 0.003f);
    }

    // The editor selects classic reticles first. Keep their painted bounds clear of
    // the central 0.04 x 0.04 target used to click each valve.
    for (const std::string_view widgetId : {std::string_view {"LeftValve"},
                                          std::string_view {"RightValve"},
                                          std::string_view {"RootToggle"}})
    {
        const std::string nodeId = "widget/" + std::string(widgetId);
        const auto node = std::find_if(layout.begin(), layout.end(),
            [&nodeId](const mfd::api1::ContainerLayoutNode& candidate) { return candidate.id == nodeId; });
        ASSERT_NE(node, layout.end());
        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            for (const mfd::Primitive& primitive : reticle.primitives)
            {
                if (const auto* pipe = std::get_if<mfd::LineGeometry>(&primitive.geometry))
                {
                    const float margin = 0.020f + primitive.style.thickness * 0.5f;
                    const bool overlapsX = node->bounds.center.x >= std::min(pipe->start.x, pipe->end.x) - margin &&
                                           node->bounds.center.x <= std::max(pipe->start.x, pipe->end.x) + margin;
                    const bool overlapsY = node->bounds.center.y >= std::min(pipe->start.y, pipe->end.y) - margin &&
                                           node->bounds.center.y <= std::max(pipe->start.y, pipe->end.y) + margin;
                    EXPECT_FALSE(overlapsX && overlapsY) << reticle.id << " covers " << widgetId;
                }
            }
        }
    }
}

TEST(WidgetExample, TankLevelsStayAnchoredAtEmptyPartialAndFullCapacity)
{
    const auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    const mfd::PageDefinition& page = loaded.document.pages.front();
    for (const mfd::api1::WidgetInstance& widget : page.widgets)
    {
        if (widget.id != "LeftTank" && widget.id != "ProgressBar" && widget.id != "RightTank") continue;
        ASSERT_NE(widget.definition, nullptr);
        const mfd::api1::ContainerRect bounds {{0.0f, 0.0f}, widget.definition->preferredSize};
        for (const double quantityKg : {0.0, 500.0, 1500.0, 2000.0})
        {
            mfd::api1::WidgetState state = mfd::api1::InitialWidgetState(widget);
            state.confirmed.value = quantityKg;
            const mfd::ReticleGroup visual = mfd::api1::BuildWidgetVisual(widget, state, bounds);
            ASSERT_FALSE(visual.primitives.empty()) << widget.id << " at " << quantityKg;
            const auto fill = std::find_if(visual.primitives.begin(), visual.primitives.end(),
                [](const mfd::Primitive& primitive) { return primitive.id == "Fill"; });
            ASSERT_NE(fill, visual.primitives.end());
            const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&fill->geometry);
            ASSERT_NE(rectangle, nullptr);
            const float height = rectangle->height * fill->transform.scale.y;
            EXPECT_NEAR(fill->transform.position.y - height * 0.5f, -0.15f, 0.0001f);
            EXPECT_NEAR(height, 0.30f * quantityKg / 2000.0, 0.0001f);
            EXPECT_EQ(fill->style.color.a, 0U) << "The level must not inherit the default green rectangle border.";
            EXPECT_EQ(std::count_if(visual.primitives.begin(), visual.primitives.end(),
                [](const mfd::Primitive& primitive)
                {
                    return primitive.id == "FillMask0" || primitive.id == "FillMask1";
                }), 2);
        }
    }
}

TEST(WidgetExample, CockpitControllerPublishesLifeAndConfirmsUserControls)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    mfd::SceneRegistry scene(std::move(loaded.document));
    scene.LoadDocument(scene.Document(), loaded.generatedTransportMap);
    mfd::CommandProcessor processor(scene);
    widgets_example::WidgetsExampleUi ui;
    widgets_example::CockpitDemoController cockpit;

    cockpit.Initialize(ui);
    EXPECT_TRUE(cockpit.Advance(ui, 0.1));
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();

    mfd::api1::WidgetFeedback sliderFeedback;
    sliderFeedback.pageId = ui.Widgets().SliderWidget().Target().pageId;
    sliderFeedback.widgetId = ui.Widgets().SliderWidget().Target().widgetTransportId;
    sliderFeedback.requestId = 41U;
    sliderFeedback.event = mfd::api1::WidgetEvent::ValueChangeRequested;
    sliderFeedback.value = 5220.0;
    scene.SetActiveWidgetInteraction("Slider", false, false, true, sliderFeedback.requestId);
    EXPECT_TRUE(cockpit.HandleFeedback(ui, sliderFeedback));

    mfd::api1::WidgetFeedback invalidSliderFeedback = sliderFeedback;
    invalidSliderFeedback.requestId = 44U;
    invalidSliderFeedback.value = std::numeric_limits<double>::quiet_NaN();
    scene.SetActiveWidgetInteraction("Slider", false, false, true, invalidSliderFeedback.requestId);
    EXPECT_TRUE(cockpit.HandleFeedback(ui, invalidSliderFeedback));

    mfd::api1::WidgetFeedback valveFeedback;
    valveFeedback.pageId = ui.Widgets().RightValveWidget().Target().pageId;
    valveFeedback.widgetId = ui.Widgets().RightValveWidget().Target().widgetTransportId;
    valveFeedback.requestId = 43U;
    valveFeedback.event = mfd::api1::WidgetEvent::CheckedChangeRequested;
    valveFeedback.checked = true;
    scene.SetActiveWidgetInteraction("RightValve", false, false, true, valveFeedback.requestId);
    EXPECT_TRUE(cockpit.HandleFeedback(ui, valveFeedback));

    mfd::api1::WidgetFeedback bitFeedback;
    bitFeedback.pageId = ui.Widgets().RootActionWidget().Target().pageId;
    bitFeedback.widgetId = ui.Widgets().RootActionWidget().Target().widgetTransportId;
    bitFeedback.requestId = 42U;
    bitFeedback.event = mfd::api1::WidgetEvent::Activated;
    scene.SetActiveWidgetInteraction("RootAction", false, false, true, bitFeedback.requestId);
    EXPECT_TRUE(cockpit.HandleFeedback(ui, bitFeedback));
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();

    std::vector<mfd::api1::WidgetInstance> instances;
    std::vector<mfd::api1::WidgetState> states;
    scene.CollectActiveWidgets(instances, states);
    const mfd::api1::WidgetState* sliderState = FindState(instances, states, "Slider");
    const mfd::api1::WidgetState* gaugeState = FindState(instances, states, "Gauge");
    const mfd::api1::WidgetState* leftTankState = FindState(instances, states, "LeftTank");
    const mfd::api1::WidgetState* centerTankState = FindState(instances, states, "ProgressBar");
    const mfd::api1::WidgetState* rightValveState = FindState(instances, states, "RightValve");
    const mfd::api1::WidgetState* indicatorState = FindState(instances, states, "Indicator");
    const mfd::api1::WidgetState* labelState = FindState(instances, states, "Label");
    ASSERT_NE(sliderState, nullptr);
    ASSERT_NE(gaugeState, nullptr);
    ASSERT_NE(leftTankState, nullptr);
    ASSERT_NE(centerTankState, nullptr);
    ASSERT_NE(rightValveState, nullptr);
    ASSERT_NE(indicatorState, nullptr);
    ASSERT_NE(labelState, nullptr);
    EXPECT_DOUBLE_EQ(sliderState->confirmed.value, 5220.0);
    EXPECT_NE(gaugeState->confirmed.value, 68.0);
    EXPECT_GT(leftTankState->confirmed.value, 1280.0);
    EXPECT_LT(centerTankState->confirmed.value, 1080.0);
    EXPECT_TRUE(rightValveState->confirmed.checked);
    EXPECT_TRUE(indicatorState->confirmed.warning);
    EXPECT_EQ(labelState->confirmed.text, "FUEL TEST IN PROGRESS");
}

TEST(WidgetExample, RefuellingRaisesAllTankLevelsAndStopsAtCapacity)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    mfd::SceneRegistry scene(std::move(loaded.document));
    scene.LoadDocument(scene.Document(), loaded.generatedTransportMap);
    mfd::CommandProcessor processor(scene);
    widgets_example::WidgetsExampleUi ui;
    widgets_example::CockpitDemoController cockpit;
    cockpit.Initialize(ui);
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();

    mfd::api1::WidgetFeedback refuel;
    refuel.pageId = ui.Widgets().RefuelWidget().Target().pageId;
    refuel.widgetId = ui.Widgets().RefuelWidget().Target().widgetTransportId;
    refuel.requestId = 71U;
    refuel.event = mfd::api1::WidgetEvent::Activated;
    scene.SetActiveWidgetInteraction("Refuel", false, false, true, refuel.requestId);
    ASSERT_TRUE(cockpit.HandleFeedback(ui, refuel));
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();

    std::vector<mfd::api1::WidgetInstance> instances;
    std::vector<mfd::api1::WidgetState> states;
    for (int step = 0; step < 60; ++step)
    {
        ASSERT_TRUE(cockpit.Advance(ui, 0.1));
        ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();
        scene.CollectActiveWidgets(instances, states);
        for (const std::string_view tankId : {std::string_view {"LeftTank"},
                                             std::string_view {"ProgressBar"},
                                             std::string_view {"RightTank"}})
        {
            const auto* tank = FindState(instances, states, tankId);
            ASSERT_NE(tank, nullptr);
            EXPECT_GE(tank->confirmed.value, 1080.0);
            EXPECT_LE(tank->confirmed.value, 2000.0);
            if (step == 0)
            {
                EXPECT_GE(tank->confirmed.value, tankId == "LeftTank" ? 1296.0 :
                                                    tankId == "RightTank" ? 1176.0 : 1096.0);
            }
        }
    }
    const auto* button = FindState(instances, states, "Refuel");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->confirmed.text, "REFUEL");
    const auto* total = FindState(instances, states, "Gauge");
    ASSERT_NE(total, nullptr);
    EXPECT_NEAR(total->confirmed.value, 6000.0, 2.0);
}

TEST(WidgetExample, ClosedManifoldStopsTransferAndPreservesFuelAccounting)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    mfd::SceneRegistry scene(std::move(loaded.document));
    scene.LoadDocument(scene.Document(), loaded.generatedTransportMap);
    mfd::CommandProcessor processor(scene);
    widgets_example::WidgetsExampleUi ui;
    widgets_example::CockpitDemoController cockpit;
    cockpit.Initialize(ui);
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();

    mfd::api1::WidgetFeedback closeManifold;
    closeManifold.pageId = ui.Widgets().RootToggleWidget().Target().pageId;
    closeManifold.widgetId = ui.Widgets().RootToggleWidget().Target().widgetTransportId;
    closeManifold.requestId = 72U;
    closeManifold.event = mfd::api1::WidgetEvent::CheckedChangeRequested;
    closeManifold.checked = false;
    scene.SetActiveWidgetInteraction("RootToggle", false, false, true, closeManifold.requestId);
    ASSERT_TRUE(cockpit.HandleFeedback(ui, closeManifold));
    ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();
    for (int step = 0; step < 10; ++step)
    {
        ASSERT_TRUE(cockpit.Advance(ui, 0.1));
        ASSERT_TRUE(processor.Submit(ui.BuildCommandBatch())) << processor.LastError();
    }
    std::vector<mfd::api1::WidgetInstance> instances;
    std::vector<mfd::api1::WidgetState> states;
    scene.CollectActiveWidgets(instances, states);
    const auto* center = FindState(instances, states, "ProgressBar");
    const auto* total = FindState(instances, states, "Gauge");
    const auto* flow = FindState(instances, states, "NumericDisplay");
    ASSERT_NE(center, nullptr);
    ASSERT_NE(total, nullptr);
    ASSERT_NE(flow, nullptr);
    EXPECT_DOUBLE_EQ(center->confirmed.value, 1080.0);
    EXPECT_NEAR(total->confirmed.value, 3520.0 - 3.2, 0.0001);
    EXPECT_DOUBLE_EQ(flow->confirmed.value, 0.0);
}
