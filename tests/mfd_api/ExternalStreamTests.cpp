#include <gtest/gtest.h>
#include <filesystem>
#include <limits>
#include "mfd/control/CommandProcessor.h"
#include "mfd/control/WindowFeedback.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/RuntimeBudgets.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
std::filesystem::path StreamExample()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
           "examples/external_stream/assets/windows/external_stream_window.json";
}
mfd::ExternalStreamGeometry StreamGeometry(const mfd::SceneRegistry& scene)
{
    for (const auto& view : scene.CollectActiveReticleViews())
        if (view.group && view.group->id == "left")
            return std::get<mfd::ExternalStreamGeometry>(view.group->primitives.front().geometry);
    throw std::runtime_error("Example stream not found");
}
TEST(ExternalStreamModel, LoadsExampleAndRejectsUnsafeGeometry)
{
    mfd::JsonLoader loader;
    const auto config = loader.LoadWindowConfiguration(StreamExample());
    ASSERT_EQ(config.document.pages.size(), 1U);
    auto primitive = config.document.pages.front().staticReticles.front().primitives.front();
    EXPECT_EQ(primitive.type, mfd::PrimitiveType::ExternalStream);
    EXPECT_TRUE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
    auto& geometry = std::get<mfd::ExternalStreamGeometry>(primitive.geometry);
    geometry.width = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
    geometry.width = 1; geometry.height = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
    geometry.height = 1; geometry.streamId = "../invalid";
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
}
TEST(ExternalStreamModel, StopThenPlayRetainsInvalidationAndTransformsRemainAvailable)
{
    mfd::JsonLoader loader;
    auto config = loader.LoadWindowConfiguration(StreamExample());
    mfd::SceneRegistry scene; scene.LoadDocument(std::move(config.document));
    mfd::CommandProcessor processor(scene);
    mfd::UpdateReticleCommand update;
    update.target.page = "Streams"; update.target.reticle = "left";
    auto& patch = update.patch.primitivePatches["surface"];
    patch.streamPlayback = mfd::StreamPlayback::Stop;
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {update}));
    EXPECT_EQ(StreamGeometry(scene).playback, mfd::StreamPlayback::Stop);
    const auto revision = StreamGeometry(scene).stopRevision;
    EXPECT_GT(revision, 0U);
    patch.streamPlayback = mfd::StreamPlayback::Play;
    patch.position = mfd::Vec2 {0.1f, 0.2f}; patch.size = mfd::Vec2 {0.8f, 0.6f};
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {update}));
    EXPECT_EQ(StreamGeometry(scene).stopRevision, revision);
    EXPECT_EQ(StreamGeometry(scene).playback, mfd::StreamPlayback::Play);
    EXPECT_FLOAT_EQ(StreamGeometry(scene).width, 0.8f);
    patch.streamPlayback = static_cast<mfd::StreamPlayback>(99);
    EXPECT_FALSE(processor.Submit(mfd::UserCommand {update}));
    EXPECT_EQ(StreamGeometry(scene).playback, mfd::StreamPlayback::Play);
}
TEST(ExternalStreamModel, WirePatchRoundTripPreservesModeAndStopBarrier)
{
    mfd::UpdateReticleCommand update;
    update.target.pageId = 11; update.target.reticleId = 22;
    auto& patch = update.patch.primitivePatchesById[33];
    patch.streamPlayback = mfd::StreamPlayback::Play; patch.streamStopToken = 42;
    mfd::CommandBatch batch; batch.commands.emplace_back(update);
    const auto decoded = mfd::DeserializeCommandBatch(mfd::SerializeCommandBatch(batch));
    ASSERT_TRUE(decoded);
    const auto& actual = std::get<mfd::UpdateReticleCommand>(decoded->commands.front()).patch.primitivePatchesById.at(33);
    EXPECT_EQ(actual.streamPlayback, mfd::StreamPlayback::Play);
    EXPECT_EQ(actual.streamStopToken, 42U);
}
TEST(ExternalStreamModel, FeedbackRoundTripDistinguishesStillImageAndConnection)
{
    mfd::ExternalStreamFeedback feedback;
    feedback.streamId = "camera_a"; feedback.instance = "1234-1";
    feedback.status.producerConnected = false; feedback.status.hasImage = true;
    feedback.status.consumedFrame = 123; feedback.status.generation = 2;
    feedback.status.playback = mfd::StreamPlayback::Pause;
    const auto decoded = mfd::DeserializeFeedbackPayload(mfd::SerializeFeedbackPayload(feedback));
    ASSERT_TRUE(decoded);
    const auto& actual = std::get<mfd::ExternalStreamFeedback>(*decoded);
    EXPECT_TRUE(actual.status.hasImage); EXPECT_FALSE(actual.status.producerConnected);
    EXPECT_EQ(actual.status.consumedFrame, 123U); EXPECT_EQ(actual.status.playback, mfd::StreamPlayback::Pause);
    feedback.status.diagnostic.assign(513, 'x');
    EXPECT_THROW(mfd::SerializeFeedbackPayload(feedback), std::runtime_error);
}
}
