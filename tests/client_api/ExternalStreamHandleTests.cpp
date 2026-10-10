#include <gtest/gtest.h>
#include "mfd/client/Animation.h"

TEST(ExternalStreamHandle, StopBarrierSurvivesPlayBeforePublish)
{
    mfd::ReticlePatch patch;
    bool dirty = false;
    mfd::client::PrimitiveBaseline baseline;
    baseline.streamId = "camera_a";
    mfd::client::ExternalStreamHandle stream(patch, &dirty, "surface", 0, nullptr, baseline);
    stream.Stop(); stream.Play();
    ASSERT_TRUE(patch.primitivePatches.at("surface").streamStopToken);
    EXPECT_EQ(patch.primitivePatches.at("surface").streamPlayback, mfd::StreamPlayback::Play);
    EXPECT_TRUE(dirty);
    stream.SetPosition({0.2f, 0.3f}); stream.SetScale({2, 1}); stream.SetVisible(false);
    EXPECT_FALSE(stream.GetVisible());
    mfd::client::RuntimeFeedbackState feedback;
    stream.BindFeedback(&feedback);
    mfd::ExternalStreamFeedback report;
    report.streamId = "camera_a"; report.instance = "host-1"; report.sequence = 2;
    report.status.consumedFrame = 77; report.status.hasImage = true; report.status.fresh = true;
    EXPECT_TRUE(feedback.Apply(report));
    report.sequence = 1; report.status.consumedFrame = 3;
    EXPECT_FALSE(feedback.Apply(report));
    EXPECT_EQ(stream.GetLastConsumedFrame(), 77U);
    EXPECT_TRUE(stream.GetStatus().hasImage);
    feedback.Reset(); EXPECT_EQ(stream.GetLastConsumedFrame(), 0U);
}
