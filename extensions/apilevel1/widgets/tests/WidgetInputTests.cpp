#include <gtest/gtest.h>
#include "mfd/apilevel1/widgets/WidgetInputController.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/control/CommandProcessor.h"
#include "mfd/runtime/SceneRegistry.h"
#include <filesystem>

namespace
{
class WidgetInputTest : public testing::Test
{
protected:
    mfd::api1::WidgetRuntimeHost runtime;
    mfd::api1::WidgetInputController input;
    mfd::SceneRegistry scene;
    std::vector<mfd::api1::WidgetInstance> instances;
    std::vector<mfd::api1::WidgetState> states;
    void SetUp() override
    {
#if !defined(_WIN32)
        GTEST_SKIP() << "Runtime DLL integration requires Windows.";
#else
        std::string error;
        ASSERT_TRUE(runtime.LoadForProcess({}, error)) << error;
#endif
        auto definition = std::make_shared<mfd::api1::WidgetDefinition>();
        definition->id = "Shared"; definition->type = mfd::api1::WidgetType::Toggle;
        definition->preferredSize = {0.4f,0.2f}; definition->hitRegion.size = definition->preferredSize;
        definition->visuals.push_back({});
        mfd::api1::WidgetInstance first; first.id = "A"; first.definition = definition; first.position.x = -0.5f;
        auto second = first; second.id = "B"; second.position.x = 0.5f;
        mfd::PageDefinition page; page.name = "Main"; page.normalizedName = "main"; page.layers.push_back({"default"});
        page.widgets = {first,second};
        mfd::PageDefinition empty; empty.name = "Empty"; empty.normalizedName = "empty"; empty.layers.push_back({"default"});
        mfd::MfdDocument document; document.pages = {page,empty};
        mfd::GeneratedTransportMap map; map.schemaVersion = 1; map.mappingHash = "widgets";
        map.pages = {{1,"Main","main"},{2,"Empty","empty"}};
        map.widgets = {{10,1,"A","a"},{11,1,"B","b"}};
        scene.LoadDocument(std::move(document), map);
    }
    void Sample(const mfd::Vec2 point, const bool down, const bool began, const bool released,
                const float dt = 0.01f, const bool cancel = false, const bool blocked = false)
    {
        scene.CollectActiveWidgets(instances, states);
        const auto* page = &scene.Document().pages[scene.ActivePageName() == "Main" ? 0U : 1U];
        const auto nodes = mfd::api1::ProjectWidgetLayoutNodes(page->containers, instances);
        std::vector<mfd::api1::ContainerLayoutNode> layouts; std::string error;
        ASSERT_TRUE(mfd::api1::EvaluateWidgetLayout(page->containers, instances, layouts, error)) << error;
        input.Update(scene, runtime, nodes, layouts, instances, states, point, down, began, released, cancel, dt, blocked);
        scene.CollectActiveWidgets(instances, states);
    }
    void Click(const float x)
    { Sample({x,0},true,true,false); Sample({x,0},false,false,true); }
};
}
TEST(WidgetRuntimeHost, MissingModuleLeavesNoCallableApi)
{
    mfd::api1::WidgetRuntimeHost runtime; std::string error;
    EXPECT_FALSE(runtime.Load(std::filesystem::temp_directory_path() / "missing_mfd_widget_module.dll", error));
    EXPECT_FALSE(runtime.IsLoaded());
    EXPECT_NE(error.find("Missing or incompatible"),std::string::npos);
    MfdWidgetInputV1 input {}; MfdWidgetOutputV1 output {};
    EXPECT_FALSE(runtime.Evaluate(input, output));
    runtime.Unload(); runtime.Unload();
}
TEST_F(WidgetInputTest, PrivateInstanceCapturesOutsideThenReleasesWithoutChecking)
{
    Sample({-0.5f,0},true,true,false);
    ASSERT_EQ(states.size(),2U); EXPECT_TRUE(states[0].pressed);
    Sample({4,4},true,false,false);
    EXPECT_TRUE(states[0].pressed);
    Sample({4,4},false,false,true);
    EXPECT_FALSE(states[0].pressed); EXPECT_FALSE(states[0].confirmed.checked);
    const auto events = scene.TakeWidgetFeedback();
    ASSERT_GE(events.size(),3U);
    EXPECT_EQ(events.front().widgetId,10U);
    EXPECT_EQ(events.front().event,mfd::api1::WidgetEvent::Pressed);
    EXPECT_TRUE(events.front().pressed);
    EXPECT_EQ(events.back().event,mfd::api1::WidgetEvent::Cancelled);
}
TEST_F(WidgetInputTest, PendingTimeoutsAreIndependentAndNeverConfirmProposals)
{
    Click(-0.5f);
    EXPECT_TRUE(states[0].pending); EXPECT_FALSE(states[0].confirmed.checked);
    Sample({0,0},false,false,false,1.0f);
    Click(0.5f);
    Sample({0,0},false,false,false,1.0f);
    EXPECT_FALSE(states[0].pending); EXPECT_TRUE(states[1].pending);
    EXPECT_FALSE(states[0].confirmed.checked); EXPECT_FALSE(states[1].confirmed.checked);
}
TEST_F(WidgetInputTest, PageSwitchAndHiddenInstanceCancelCapture)
{
    Sample({-0.5f,0},true,true,false);
    scene.SetActivePage("Empty"); Sample({0,0},true,false,false);
    auto events = scene.TakeWidgetFeedback();
    ASSERT_FALSE(events.empty()); EXPECT_EQ(events.back().event,mfd::api1::WidgetEvent::Cancelled);
    scene.SetActivePage("Main"); Sample({-0.5f,0},true,true,false);
    mfd::api1::UpdateWidgetCommand hide; hide.target = {"Main","A"}; hide.patch.visible = false;
    mfd::CommandProcessor processor(scene); ASSERT_TRUE(processor.Submit(hide));
    Sample({-0.5f,0},true,false,false);
    EXPECT_FALSE(states[0].pressed); EXPECT_FALSE(states[0].pending);
}
TEST_F(WidgetInputTest, OcclusionAndFocusLossNeverLeavePressedState)
{
    Sample({-0.5f,0},true,true,false,0.01f,false,true);
    EXPECT_FALSE(states[0].pressed);
    Sample({-0.5f,0},true,true,false);
    Sample({-0.5f,0},true,false,false,0.01f,true);
    EXPECT_FALSE(states[0].pressed); EXPECT_FALSE(states[0].pending);
}
