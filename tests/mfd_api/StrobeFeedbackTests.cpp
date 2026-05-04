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

#include <random>
#include <string>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"

namespace
{
mfd::StrobeStatusFeedback MakeFullFeedback()
{
    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 41U;
    feedback.pageId = 11U;
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
    feedback.magnet.runtimeReticleId = 7001U;
    feedback.magnet.reticleId = "track_17";
    feedback.magnet.targetPosition = {0.19f, -0.22f};
    feedback.magnet.distance = 0.04f;

    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = 7001U;
    capture.sourceTemplateTransportId = 55U;
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

void ExpectFeedbackEquals(const mfd::StrobeStatusFeedback& expected, const mfd::StrobeStatusFeedback& actual)
{
    EXPECT_EQ(actual.sequence, expected.sequence);
    EXPECT_EQ(actual.pageId, expected.pageId);
    EXPECT_EQ(actual.pageName, expected.pageName);
    EXPECT_EQ(actual.strobeId, expected.strobeId);
    EXPECT_EQ(actual.active, expected.active);
    EXPECT_FLOAT_EQ(actual.position.x, expected.position.x);
    EXPECT_FLOAT_EQ(actual.position.y, expected.position.y);
    EXPECT_EQ(actual.capture.shape, expected.capture.shape);
    EXPECT_FLOAT_EQ(actual.capture.radius, expected.capture.radius);
    EXPECT_FLOAT_EQ(actual.capture.size.x, expected.capture.size.x);
    EXPECT_FLOAT_EQ(actual.capture.size.y, expected.capture.size.y);
    EXPECT_EQ(actual.magnet.enabled, expected.magnet.enabled);
    EXPECT_FLOAT_EQ(actual.magnet.radius, expected.magnet.radius);
    EXPECT_FLOAT_EQ(actual.magnet.strength, expected.magnet.strength);
    EXPECT_EQ(actual.magnet.magnetized, expected.magnet.magnetized);
    EXPECT_EQ(actual.magnet.runtimeReticleId, expected.magnet.runtimeReticleId);
    EXPECT_EQ(actual.magnet.reticleId, expected.magnet.reticleId);
    EXPECT_FLOAT_EQ(actual.magnet.targetPosition.x, expected.magnet.targetPosition.x);
    EXPECT_FLOAT_EQ(actual.magnet.targetPosition.y, expected.magnet.targetPosition.y);
    EXPECT_FLOAT_EQ(actual.magnet.distance, expected.magnet.distance);
    EXPECT_EQ(actual.captureResult.has_value(), expected.captureResult.has_value());
    if (!expected.captureResult.has_value())
    {
        return;
    }

    ASSERT_TRUE(actual.captureResult.has_value());
    EXPECT_EQ(actual.captureResult->runtimeReticleId, expected.captureResult->runtimeReticleId);
    EXPECT_EQ(actual.captureResult->sourceTemplateTransportId, expected.captureResult->sourceTemplateTransportId);
    EXPECT_EQ(actual.captureResult->reticleId, expected.captureResult->reticleId);
    EXPECT_EQ(actual.captureResult->sourceTemplateId, expected.captureResult->sourceTemplateId);
    EXPECT_EQ(actual.captureResult->label, expected.captureResult->label);
    EXPECT_EQ(actual.captureResult->category, expected.captureResult->category);
    EXPECT_FLOAT_EQ(actual.captureResult->position.x, expected.captureResult->position.x);
    EXPECT_FLOAT_EQ(actual.captureResult->position.y, expected.captureResult->position.y);
    EXPECT_FLOAT_EQ(actual.captureResult->distance, expected.captureResult->distance);
    EXPECT_EQ(actual.captureResult->metadata, expected.captureResult->metadata);
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
    EXPECT_EQ(decoded->pageId, 11U);
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
    EXPECT_EQ(decoded->magnet.runtimeReticleId, 7001U);
    EXPECT_EQ(decoded->magnet.reticleId, "track_17");
    EXPECT_FLOAT_EQ(decoded->magnet.targetPosition.x, 0.19f);
    EXPECT_FLOAT_EQ(decoded->magnet.targetPosition.y, -0.22f);
    EXPECT_FLOAT_EQ(decoded->magnet.distance, 0.04f);

    ASSERT_TRUE(decoded->captureResult.has_value());
    EXPECT_EQ(decoded->captureResult->runtimeReticleId, 7001U);
    EXPECT_EQ(decoded->captureResult->sourceTemplateTransportId, 55U);
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
    original.pageId = 19U;
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
    EXPECT_EQ(decoded->pageId, 19U);
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

TEST(StrobeFeedbackTests, RoundTripsActivePageFeedbackEnvelope)
{
    mfd::ActivePageFeedback original;
    original.sequence = 19U;
    original.pageName = "Page1";

    const std::string payload = mfd::SerializeActivePageFeedback(original);
    std::string error;
    const auto decoded = mfd::DeserializeActivePageFeedback(payload, &error);

    ASSERT_TRUE(decoded.has_value()) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(decoded->sequence, 19U);
    EXPECT_EQ(decoded->pageName, "Page1");
}

TEST(StrobeFeedbackTests, RejectsInvalidPayload)
{
    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback("not a protobuf payload", &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Unable to parse Protocol Buffers runtime feedback payload");
}

TEST(StrobeFeedbackTests, RejectsCommandEnvelopeAsUnsupportedFeedbackPayload)
{
    mfd::CommandBatch batch;
    batch.sequence = 99U;
    batch.commands.push_back(mfd::ResetWindowCommand {});

    const std::string payload = mfd::SerializeCommandBatch(batch);
    std::string error;
    const auto decoded = mfd::DeserializeFeedbackPayload(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Unsupported feedback payload");
}

TEST(StrobeFeedbackTests, StrobeDecoderIgnoresActivePageFeedbackWithoutError)
{
    mfd::ActivePageFeedback activePage;
    activePage.sequence = 23U;
    activePage.pageName = "Radar";

    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback(mfd::SerializeActivePageFeedback(activePage), &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_TRUE(error.empty());
}

TEST(StrobeFeedbackTests, GenericDecoderReturnsActivePageVariant)
{
    mfd::ActivePageFeedback activePage;
    activePage.sequence = 31U;
    activePage.pageName = "Navigation";

    std::string error;
    const auto decoded = mfd::DeserializeFeedbackPayload(mfd::SerializeActivePageFeedback(activePage), &error);

    ASSERT_TRUE(decoded.has_value()) << error;
    const auto* value = std::get_if<mfd::ActivePageFeedback>(&(*decoded));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->sequence, 31U);
    EXPECT_EQ(value->pageName, "Navigation");
}

TEST(StrobeFeedbackTests, DeterministicPseudoFuzzRoundTripsFeedbackVariants)
{
    std::mt19937 rng(1337U);
    std::uniform_int_distribution<std::uint32_t> sequenceDist(1U, 100000U);
    std::uniform_real_distribution<float> signedDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> positiveDist(0.0f, 1.0f);

    for (int iteration = 0; iteration < 128; ++iteration)
    {
        mfd::StrobeStatusFeedback original;
        original.sequence = sequenceDist(rng);
        original.pageId = 100U + static_cast<mfd::TransportId>(iteration);
        original.pageName = "Page_" + std::to_string(iteration % 5);
        original.strobeId = "strobe_" + std::to_string(iteration);
        original.active = (iteration % 2) == 0;
        original.position = {signedDist(rng), signedDist(rng)};
        original.capture.shape =
            (iteration % 3) == 0 ? mfd::StrobeCaptureShape::Rectangle : mfd::StrobeCaptureShape::Circle;
        original.capture.radius = positiveDist(rng);
        original.capture.size = {positiveDist(rng), positiveDist(rng)};
        original.magnet.enabled = (iteration % 4) != 0;
        original.magnet.radius = positiveDist(rng);
        original.magnet.strength = positiveDist(rng);
        original.magnet.magnetized = (iteration % 3) == 0;
        original.magnet.runtimeReticleId = 4000U + static_cast<mfd::RuntimeDynamicId>(iteration);
        original.magnet.reticleId = "track_" + std::to_string(iteration % 11);
        original.magnet.targetPosition = {signedDist(rng), signedDist(rng)};
        original.magnet.distance = positiveDist(rng);

        if ((iteration % 2) == 1)
        {
            mfd::StrobeFeedbackCapture capture;
            capture.runtimeReticleId = 8000U + static_cast<mfd::RuntimeDynamicId>(iteration);
            capture.sourceTemplateTransportId = 9000U + static_cast<mfd::TransportId>(iteration % 7);
            capture.reticleId = "capture_" + std::to_string(iteration);
            capture.sourceTemplateId = (iteration % 3) == 0 ? "geometry_template" : "radar_track";
            capture.label = "Label_" + std::to_string(iteration);
            capture.category = (iteration % 4) == 0 ? "friendly" : "hostile";
            capture.position = {signedDist(rng), signedDist(rng)};
            capture.distance = positiveDist(rng);
            for (int metadataIndex = 0; metadataIndex < (iteration % 4); ++metadataIndex)
            {
                capture.metadata.emplace(
                    "key_" + std::to_string(metadataIndex),
                    "value_" + std::to_string(iteration + metadataIndex));
            }
            original.captureResult = std::move(capture);
        }

        const std::string payload = mfd::SerializeStrobeStatusFeedback(original);
        std::string error;
        const auto decoded = mfd::DeserializeStrobeStatusFeedback(payload, &error);

        ASSERT_TRUE(decoded.has_value()) << "iteration=" << iteration << " error=" << error;
        EXPECT_TRUE(error.empty());
        ExpectFeedbackEquals(original, *decoded);
    }
}
