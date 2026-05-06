/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for Protocol Buffers command payload validation.
 */

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd_commands.pb.h"

TEST(CommandTypesTests, DeserializeUserCommandsRejectsEmptyPayload)
{
    std::string error;
    const auto commands = mfd::DeserializeUserCommands(std::string_view {}, &error);

    EXPECT_FALSE(commands.has_value());
    EXPECT_EQ(error, "Protocol Buffers command payload is empty");
}

TEST(CommandTypesTests, DeserializeUserCommandsRejectsOversizedPayload)
{
    std::string error;
    const std::string oversized(1024U * 1024U + 1U, 'x');

    const auto commands = mfd::DeserializeUserCommands(oversized, &error);

    EXPECT_FALSE(commands.has_value());
    EXPECT_EQ(error, "Protocol Buffers command payload exceeds safety size limit");
}

TEST(CommandTypesTests, CommandBatchRoundTripsMappingHashAndGeneratedIds)
{
    mfd::PrimitivePatch primitivePatch;
    primitivePatch.text = "123";
    primitivePatch.fillColor = mfd::ColorRgba {1, 2, 3, 200};
    primitivePatch.filled = true;
    primitivePatch.lineStyle = mfd::LineStyle::Dashed;
    primitivePatch.points = std::vector<mfd::Vec2> {{-0.2f, 0.1f}, {0.0f, 0.25f}, {0.3f, -0.1f}};
    primitivePatch.closed = true;
    primitivePatch.segments = 48;
    primitivePatch.startAngleDegrees = -30.0f;
    primitivePatch.endAngleDegrees = 210.0f;

    mfd::ReticlePatch patch;
    patch.blinkTypeId = 44U;
    patch.textsById.emplace(55U, "HDG");
    patch.letterSpacingsById.emplace(55U, 0.02f);
    patch.primitivePatchesById.emplace(55U, primitivePatch);

    mfd::CommandBatch batch;
    batch.sequence = 9U;
    batch.mappingHash = "deadbeef";
    batch.commands.push_back(mfd::UpdateReticleCommand {
        mfd::StaticReticleHandle {"Radar", "heading_box", 11U, 22U},
        patch});

    const std::string payload = mfd::SerializeCommandBatch(batch);
    const auto decoded = mfd::DeserializeCommandBatch(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 9U);
    EXPECT_EQ(decoded->mappingHash, "deadbeef");
    ASSERT_EQ(decoded->commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_TRUE(update->target.page.empty());
    EXPECT_TRUE(update->target.reticle.empty());
    EXPECT_EQ(update->target.pageId, 11U);
    EXPECT_EQ(update->target.reticleId, 22U);
    ASSERT_TRUE(update->patch.blinkTypeId.has_value());
    EXPECT_EQ(*update->patch.blinkTypeId, 44U);
    EXPECT_EQ(update->patch.textsById.at(55U), "HDG");
    EXPECT_FLOAT_EQ(update->patch.letterSpacingsById.at(55U), 0.02f);
    ASSERT_EQ(update->patch.primitivePatchesById.size(), 1U);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).text.has_value());
    EXPECT_EQ(*update->patch.primitivePatchesById.at(55U).text, "123");
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).fillColor.has_value());
    EXPECT_EQ(update->patch.primitivePatchesById.at(55U).fillColor->r, 1U);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).filled.has_value());
    EXPECT_TRUE(*update->patch.primitivePatchesById.at(55U).filled);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).lineStyle.has_value());
    EXPECT_EQ(*update->patch.primitivePatchesById.at(55U).lineStyle, mfd::LineStyle::Dashed);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).points.has_value());
    ASSERT_EQ(update->patch.primitivePatchesById.at(55U).points->size(), 3U);
    EXPECT_FLOAT_EQ(update->patch.primitivePatchesById.at(55U).points->at(1).y, 0.25f);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).closed.has_value());
    EXPECT_TRUE(*update->patch.primitivePatchesById.at(55U).closed);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).segments.has_value());
    EXPECT_EQ(*update->patch.primitivePatchesById.at(55U).segments, 48);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).startAngleDegrees.has_value());
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).endAngleDegrees.has_value());
    EXPECT_FLOAT_EQ(*update->patch.primitivePatchesById.at(55U).startAngleDegrees, -30.0f);
    EXPECT_FLOAT_EQ(*update->patch.primitivePatchesById.at(55U).endAngleDegrees, 210.0f);
}

TEST(CommandTypesTests, SerializeCommandBatchRejectsLegacyNamedTargetsWithoutGeneratedIds)
{
    mfd::CommandBatch batch;
    batch.commands.push_back(mfd::UpdateReticleCommand {
        mfd::StaticReticleHandle {"Radar", "heading_box"},
        {}});

    EXPECT_THROW(mfd::SerializeCommandBatch(batch), std::runtime_error);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsNonFinitePageViewValues)
{
    mfd::transport::CommandEnvelope envelope;
    auto* pageView = envelope.add_commands()->mutable_set_page_view();
    pageView->set_page_id(11U);
    pageView->mutable_center()->set_x(std::numeric_limits<float>::quiet_NaN());
    pageView->mutable_center()->set_y(0.0f);
    pageView->set_zoom(1.0f);

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("finite"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsExplicitUnspecifiedPrimitiveLineStyle)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    (*update->mutable_patch()->mutable_primitive_patches_by_id())[33U]
        .set_line_style(mfd::transport::PRIMITIVE_LINE_STYLE_UNSPECIFIED);

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("UNSPECIFIED"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsTooManyCommands)
{
    mfd::transport::CommandEnvelope envelope;
    for (std::size_t index = 0; index < 1025U; ++index)
    {
        envelope.add_commands()->mutable_reset_window();
    }

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("CommandEnvelope.commands"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsTooManyDynamicReticles)
{
    mfd::transport::CommandEnvelope envelope;
    auto* command = envelope.add_commands()->mutable_upsert_dynamic_reticles();
    command->set_page_id(11U);
    command->set_template_transport_id(55U);
    for (std::size_t index = 0; index < 4097U; ++index)
    {
        command->add_reticles()->set_runtime_reticle_id(static_cast<std::uint64_t>(index + 1U));
    }

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("reticles"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsOversizedPrimitivePointLists)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* primitivePatch = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[33U];
    for (std::size_t index = 0; index < 2049U; ++index)
    {
        auto* point = primitivePatch->add_points();
        point->set_x(static_cast<float>(index) * 0.01f);
        point->set_y(0.0f);
    }

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("PrimitivePatch.points"), std::string::npos);
}
