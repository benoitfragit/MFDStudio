#include <gtest/gtest.h>
#include "WidgetsExampleUi.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/control/CommandProcessor.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"

TEST(WidgetExample, LoadsSharedDefinitionsAndCompilesGeneratedAuthorityPath)
{
    auto loaded = mfd::JsonLoader {}.LoadWindowConfiguration(MFD_WIDGET_EXAMPLE_WINDOW);
    ASSERT_TRUE(loaded.generatedTransportMap);
    ASSERT_EQ(loaded.document.pages.size(), 1U);
    const auto& page = loaded.document.pages.front();
    ASSERT_EQ(page.widgets.size(), 15U);
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
    ui.Widgets().SliderWidget().SetValue(42);
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
        if (instances[index].id == "Slider") EXPECT_DOUBLE_EQ(states[index].confirmed.value, 42);
        if (instances[index].id == "PrivateButton") EXPECT_NE(scene.ActiveWidgetTransportId(instances[index].id), 0U);
    }
}
