/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for strobe feedback serialization and validation.
 */

#include <gtest/gtest.h>

#include <string>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"

namespace
{
mfd::StrobeStatusFeedback MakeFullFeedback()
{
    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 41U;
    feedback.pageName = "Radar";
    feedback.strobeId = "cursor";
    feedback.active = true;
    feedback.position = {0.15f, -0.25f};
    feedback.capture.shape = mfd::StrobeCaptureShape::Rectangle;
    feedback.capture.radius = 0.33f;
    feedback.capture.size = {0.28f, 0.12f};
    feedback.magnet.enabled = true;
    feedback.magnet.radius = 0.20f;
    feedback.magnet.strength = 0.65f;
    feedback.magnet.magnetized = true;
    feedback.magnet.reticleId = "track_17";
    feedback.magnet.targetPosition = {0.19f, -0.22f};
    feedback.magnet.distance = 0.04f;

    mfd::StrobeFeedbackCapture capture;
    capture.reticleId = "track_17";
    capture.sourceTemplateId = "radar_track";
    capture.label = "Bogey 17";
    capture.category = "hostile";
    capture.position = {0.19f, -0.22f};
    capture.distance = 0.04f;
    capture.metadata.emplace("callsign", "Viper");
    capture.metadata.emplace("priority", "high");
    feedback.captureResult = std::move(capture);

    return feedback;
}
} // namespace

TEST(StrobeFeedbackTests, RoundTripsCompleteFeedbackEnvelope)
{
    const mfd::StrobeStatusFeedback original = MakeFullFeedback();

    const std::string payload = mfd::SerializeStrobeStatusFeedback(original);
    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback(payload, &error);

    ASSERT_TRUE(decoded.has_value()) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(decoded->sequence, 41U);
    EXPECT_EQ(decoded->pageName, "Radar");
    EXPECT_EQ(decoded->strobeId, "cursor");
    EXPECT_TRUE(decoded->active);
    EXPECT_FLOAT_EQ(decoded->position.x, 0.15f);
    EXPECT_FLOAT_EQ(decoded->position.y, -0.25f);
    EXPECT_EQ(decoded->capture.shape, mfd::StrobeCaptureShape::Rectangle);
    EXPECT_FLOAT_EQ(decoded->capture.radius, 0.33f);
    EXPECT_FLOAT_EQ(decoded->capture.size.x, 0.28f);
    EXPECT_FLOAT_EQ(decoded->capture.size.y, 0.12f);
    EXPECT_TRUE(decoded->magnet.enabled);
    EXPECT_FLOAT_EQ(decoded->magnet.radius, 0.20f);
    EXPECT_FLOAT_EQ(decoded->magnet.strength, 0.65f);
    EXPECT_TRUE(decoded->magnet.magnetized);
    EXPECT_EQ(decoded->magnet.reticleId, "track_17");
    EXPECT_FLOAT_EQ(decoded->magnet.targetPosition.x, 0.19f);
    EXPECT_FLOAT_EQ(decoded->magnet.targetPosition.y, -0.22f);
    EXPECT_FLOAT_EQ(decoded->magnet.distance, 0.04f);

    ASSERT_TRUE(decoded->captureResult.has_value());
    EXPECT_EQ(decoded->captureResult->reticleId, "track_17");
    EXPECT_EQ(decoded->captureResult->sourceTemplateId, "radar_track");
    EXPECT_EQ(decoded->captureResult->label, "Bogey 17");
    EXPECT_EQ(decoded->captureResult->category, "hostile");
    EXPECT_FLOAT_EQ(decoded->captureResult->position.x, 0.19f);
    EXPECT_FLOAT_EQ(decoded->captureResult->position.y, -0.22f);
    EXPECT_FLOAT_EQ(decoded->captureResult->distance, 0.04f);
    ASSERT_EQ(decoded->captureResult->metadata.size(), 2U);
    EXPECT_EQ(decoded->captureResult->metadata.at("callsign"), "Viper");
    EXPECT_EQ(decoded->captureResult->metadata.at("priority"), "high");
}

TEST(StrobeFeedbackTests, RoundTripsFeedbackWithoutOptionalCaptureResult)
{
    mfd::StrobeStatusFeedback original;
    original.sequence = 7U;
    original.pageName = "HUD";
    original.strobeId = "designator";
    original.active = false;
    original.position = {-0.05f, 0.06f};
    original.capture.shape = mfd::StrobeCaptureShape::Circle;
    original.capture.radius = 0.11f;
    original.magnet.enabled = false;
    original.magnet.radius = 0.09f;
    original.magnet.strength = 1.0f;

    const std::string payload = mfd::SerializeStrobeStatusFeedback(original);
    const auto decoded = mfd::DeserializeStrobeStatusFeedback(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 7U);
    EXPECT_EQ(decoded->pageName, "HUD");
    EXPECT_EQ(decoded->strobeId, "designator");
    EXPECT_FALSE(decoded->active);
    EXPECT_EQ(decoded->capture.shape, mfd::StrobeCaptureShape::Circle);
    EXPECT_FLOAT_EQ(decoded->capture.radius, 0.11f);
    EXPECT_FLOAT_EQ(decoded->capture.size.x, 0.175f);
    EXPECT_FLOAT_EQ(decoded->capture.size.y, 0.175f);
    EXPECT_FALSE(decoded->magnet.enabled);
    EXPECT_FALSE(decoded->magnet.magnetized);
    EXPECT_TRUE(decoded->magnet.reticleId.empty());
    EXPECT_FALSE(decoded->captureResult.has_value());
}

TEST(StrobeFeedbackTests, RejectsInvalidPayload)
{
    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback("not a protobuf payload", &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Unable to parse Protocol Buffers strobe feedback payload");
}

TEST(StrobeFeedbackTests, RejectsCommandEnvelopeAsUnsupportedFeedbackPayload)
{
    mfd::CommandBatch batch;
    batch.sequence = 99U;
    batch.commands.push_back(mfd::ResetWindowCommand {});

    const std::string payload = mfd::SerializeCommandBatch(batch);
    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Unsupported feedback payload");
}
