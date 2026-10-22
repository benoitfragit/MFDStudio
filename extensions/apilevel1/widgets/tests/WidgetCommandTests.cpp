#include <gtest/gtest.h>
#include "mfd/control/CommandTypes.h"
#include "mfd/control/WindowFeedback.h"
#include "mfd/client/Animation.h"
#include "mfd/client/Widget.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/control/CommandProcessor.h"
#include "mfd/apilevel1/widgets/WidgetRuntimeAbi.h"
#include <limits>

TEST(WidgetCommand, RoundTripsOptionalFalseZeroAndEmptyText)
{
    mfd::api1::UpdateWidgetCommand update;
    update.target = {"", "", 101U, 202U};
    update.patch.visible = false;
    update.patch.enabled = false;
    update.patch.checked = false;
    update.patch.warning = true;
    update.patch.value = 0;
    update.patch.text = "";
    update.patch.requestId = 42;
    mfd::CommandBatch batch;
    batch.commands.emplace_back(update);
    const auto decoded = mfd::DeserializeCommandBatch(mfd::SerializeCommandBatch(batch));
    ASSERT_TRUE(decoded);
    const auto& patch = std::get<mfd::api1::UpdateWidgetCommand>(decoded->commands.at(0)).patch;
    EXPECT_EQ(patch.visible, false);
    EXPECT_EQ(patch.enabled, false);
    EXPECT_EQ(patch.checked, false);
    EXPECT_EQ(patch.warning, true);
    EXPECT_EQ(patch.value, 0);
    EXPECT_EQ(patch.text, "");
    EXPECT_EQ(patch.requestId, 42);
}
TEST(WidgetCommand, RejectsNonFiniteAndOversizedPayloads)
{
    mfd::api1::UpdateWidgetCommand update;
    update.target = {"", "", 101U, 202U};
    update.patch.value = std::numeric_limits<double>::infinity();
    EXPECT_THROW(mfd::SerializeUserCommand(update), std::runtime_error);
    update.patch.value.reset();
    update.patch.text = std::string(4097,'a');
    EXPECT_THROW(mfd::SerializeUserCommand(update), std::runtime_error);
}
TEST(WidgetFeedback, RejectsDuplicatesStaleSequenceAndOldSession)
{
    mfd::client::RuntimeFeedbackState state;
    mfd::api1::WidgetFeedback feedback {100,1,10,20,1,mfd::api1::WidgetEvent::Pressed,true,false,0.5};
    const auto decoded = mfd::DeserializeFeedbackPayload(mfd::SerializeFeedbackPayload(feedback));
    ASSERT_TRUE(decoded);
    EXPECT_TRUE(state.Apply(std::get<mfd::api1::WidgetFeedback>(*decoded)));
    EXPECT_FALSE(state.Apply(feedback));
    feedback.sequence=2;
    feedback.event=mfd::api1::WidgetEvent::Released;
    feedback.pressed=false;
    EXPECT_TRUE(state.Apply(feedback));
    auto events=state.TakeWidgetEvents();
    ASSERT_EQ(events.size(),2U);
    EXPECT_TRUE(events[0].pressed);
    EXPECT_FALSE(events[1].pressed);
    feedback.session=99;
    feedback.sequence=50;
    EXPECT_FALSE(state.Apply(feedback));
    feedback.session=101;
    feedback.sequence=1;
    EXPECT_TRUE(state.Apply(feedback));
}
TEST(WidgetAbi, GuardsSizesEnumsFlagsAndFiniteInputs)
{
    MfdWidgetRuntimeApiV1 api {};
    EXPECT_NE(mfd_get_widgets_runtime_api_v1(&api),0U);
    api.struct_size=sizeof(api);
    ASSERT_EQ(mfd_get_widgets_runtime_api_v1(&api),0U);
    MfdWidgetInputV1 input {};
    input.struct_size=sizeof(input);
    input.maximum=1;
    MfdWidgetOutputV1 output {};
    output.struct_size=sizeof(output);
    ASSERT_EQ(api.evaluate_input(&input,&output),0U);
    input.began=2;
    EXPECT_NE(api.evaluate_input(&input,&output),0U);
    input.began=0;
    input.value=std::numeric_limits<double>::quiet_NaN();
    EXPECT_NE(api.evaluate_input(&input,&output),0U);
}
TEST(WidgetAbi, CombinedSliderSamplePreservesAllTransitionsWithoutConfirmingValue)
{
    MfdWidgetRuntimeApiV1 api {};
    api.struct_size=sizeof(api);
    ASSERT_EQ(mfd_get_widgets_runtime_api_v1(&api),0U);
    MfdWidgetInputV1 input {};
    input.struct_size=sizeof(input);
    input.type=4;
    input.maximum=1;
    input.normalized_position=0.75;
    input.hovered=1; input.began=1; input.released=1;
    MfdWidgetOutputV1 output {};
    output.struct_size=sizeof(output);
    ASSERT_EQ(api.evaluate_input(&input,&output),0U);
    ASSERT_EQ(output.event_count,5U);
    EXPECT_EQ(output.events[0],0U);
    EXPECT_EQ(output.events[4],7U);
    EXPECT_EQ(output.pressed,0U);
    EXPECT_EQ(output.proposed_value,0.75);
    EXPECT_EQ(input.value,0.0);
}
TEST(WidgetAbi, CancelNeverActivatesOrChecks)
{
    MfdWidgetRuntimeApiV1 api {};
    api.struct_size=sizeof(api);
    ASSERT_EQ(mfd_get_widgets_runtime_api_v1(&api),0U);
    MfdWidgetInputV1 input {};
    input.struct_size=sizeof(input); input.type=1; input.maximum=1;
    input.pressed=1; input.pending=1; input.cancel=1;
    MfdWidgetOutputV1 output {};
    output.struct_size=sizeof(output);
    ASSERT_EQ(api.evaluate_input(&input,&output),0U);
    ASSERT_EQ(output.event_count,1U);
    EXPECT_EQ(output.events[0],8U);
    EXPECT_EQ(output.pressed,0U);
    EXPECT_EQ(output.pending,0U);
    EXPECT_EQ(output.proposed_checked,0U);
}
TEST(WidgetClient, TypedSettersAndAcknowledgementShareOneCommand)
{
    mfd::client::NumericWidget widget("Main","slider",1,2,true,true,false,false,0,"",0,10);
    widget.SetValue(7);
    widget.Acknowledge(15);
    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(widget.AppendCommands(commands));
    const auto& update=std::get<mfd::api1::UpdateWidgetCommand>(commands.front());
    EXPECT_EQ(update.patch.value,7);
    EXPECT_EQ(update.patch.requestId,15);
    EXPECT_FALSE(widget.AppendCommands(commands));
    EXPECT_THROW(widget.SetValue(11),std::invalid_argument);
}
