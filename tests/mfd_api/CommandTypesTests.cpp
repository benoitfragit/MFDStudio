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
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd_commands.pb.h"

namespace
{
std::optional<mfd::CommandBatch> DeserializeEnvelope(const mfd::transport::CommandEnvelope& envelope,
                                                     std::string* error = nullptr)
{
    std::string payload;
    if (!envelope.SerializeToString(&payload))
    {
        return std::nullopt;
    }

    return mfd::DeserializeCommandBatch(payload, error);
}
} // namespace

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
    primitivePatch.timeValue = mfd::TimeValue {2026, 6, 5, 14, 3, 9};
    primitivePatch.timeUtc = true;
    primitivePatch.timeFields = mfd::TimeFieldVisibility {true, true, true, true, true, false};

    mfd::ReticlePatch patch;
    patch.blinkTypeId = 44U;
    patch.scale = mfd::Vec2 {1.25f, 0.80f};
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
    ASSERT_TRUE(update->patch.scale.has_value());
    EXPECT_FLOAT_EQ(update->patch.scale->x, 1.25f);
    EXPECT_FLOAT_EQ(update->patch.scale->y, 0.80f);
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
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).timeValue.has_value());
    EXPECT_EQ(update->patch.primitivePatchesById.at(55U).timeValue->year, 2026);
    EXPECT_EQ(update->patch.primitivePatchesById.at(55U).timeValue->month, 6);
    EXPECT_EQ(update->patch.primitivePatchesById.at(55U).timeValue->day, 5);
    EXPECT_EQ(update->patch.primitivePatchesById.at(55U).timeValue->hour, 14);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).timeUtc.has_value());
    EXPECT_TRUE(*update->patch.primitivePatchesById.at(55U).timeUtc);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).timeFields.has_value());
    EXPECT_TRUE(update->patch.primitivePatchesById.at(55U).timeFields->year);
    EXPECT_FALSE(update->patch.primitivePatchesById.at(55U).timeFields->second);
}

TEST(CommandTypesTests, CommandBatchRoundTripsFragmentIdentity)
{
    mfd::CommandBatch batch;
    batch.sequence = 17U;
    batch.mappingHash = "map_hash";
    batch.fragment = mfd::CommandBatchFragment {101U, 202U, 303U, 4U, 9U};
    batch.commands.emplace_back(mfd::ResetWindowCommand {});

    const auto decoded = mfd::DeserializeCommandBatch(mfd::SerializeCommandBatch(batch));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->fragment.has_value());
    EXPECT_EQ(decoded->fragment->clientId, 101U);
    EXPECT_EQ(decoded->fragment->sessionEpoch, 202U);
    EXPECT_EQ(decoded->fragment->batchId, 303U);
    EXPECT_EQ(decoded->fragment->chunkIndex, 4U);
    EXPECT_EQ(decoded->fragment->chunkCount, 9U);
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

TEST(CommandTypesTests, DeserializeCommandBatchRejectsInvalidPrimitiveTimePatch)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* primitivePatch = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[33U];
    primitivePatch->mutable_time_value()->set_year(2026);
    primitivePatch->mutable_time_value()->set_month(2);
    primitivePatch->mutable_time_value()->set_day(30);

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("day"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsAmbiguousPrimitiveTimeBypassPatch)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* primitivePatch = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[33U];
    primitivePatch->mutable_time_value()->set_year(2026);
    primitivePatch->mutable_time_value()->set_month(6);
    primitivePatch->mutable_time_value()->set_day(5);
    primitivePatch->set_clear_time_value(true);

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("set and clear"), std::string::npos);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsHiddenPrimitiveTimeFields)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    (*update->mutable_patch()->mutable_primitive_patches_by_id())[33U].mutable_time_fields();

    std::string payload;
    ASSERT_TRUE(envelope.SerializeToString(&payload));

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(payload, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_NE(error.find("at least one"), std::string::npos);
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

TEST(CommandTypesTests, DeserializeCommandBatchDecodesActivatePageCommand)
{
    mfd::transport::CommandEnvelope envelope;
    envelope.add_commands()->mutable_activate_page()->set_page_id(11U);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::ActivatePageCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_TRUE(command->page.empty());
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesSetPageViewCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* pageView = envelope.add_commands()->mutable_set_page_view();
    pageView->set_page_id(11U);
    pageView->mutable_center()->set_x(0.25f);
    pageView->mutable_center()->set_y(-0.5f);
    pageView->set_zoom(2.0f);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::SetPageViewCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_FLOAT_EQ(command->view.center.x, 0.25f);
    EXPECT_FLOAT_EQ(command->view.center.y, -0.5f);
    EXPECT_FLOAT_EQ(command->view.zoom, 2.0f);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesUpdateWindowDisplayCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* patch = envelope.add_commands()->mutable_update_window_display()->mutable_patch();
    patch->set_invert_colors(true);
    patch->set_brightness(0.5f);
    patch->set_disabled(false);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::UpdateWindowDisplayCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->patch.invertColors.has_value());
    EXPECT_TRUE(*command->patch.invertColors);
    ASSERT_TRUE(command->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*command->patch.brightness, 0.5f);
    ASSERT_TRUE(command->patch.disabled.has_value());
    EXPECT_FALSE(*command->patch.disabled);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesUpdateReticleCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* patch = update->mutable_patch();
    patch->set_visible(true);
    patch->set_blink_enabled(true);
    patch->set_blink_type_id(44U);
    patch->mutable_position()->set_x(0.1f);
    patch->mutable_position()->set_y(0.2f);
    patch->set_rotation_degrees(45.0f);
    patch->set_thickness(1.5f);
    patch->set_text("HDG");

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 11U);
    EXPECT_EQ(command->target.reticleId, 22U);
    ASSERT_TRUE(command->patch.visible.has_value());
    EXPECT_TRUE(*command->patch.visible);
    ASSERT_TRUE(command->patch.blinkEnabled.has_value());
    EXPECT_TRUE(*command->patch.blinkEnabled);
    ASSERT_TRUE(command->patch.blinkTypeId.has_value());
    EXPECT_EQ(*command->patch.blinkTypeId, 44U);
    ASSERT_TRUE(command->patch.position.has_value());
    EXPECT_FLOAT_EQ(command->patch.position->x, 0.1f);
    EXPECT_FLOAT_EQ(command->patch.position->y, 0.2f);
    ASSERT_TRUE(command->patch.rotationDegrees.has_value());
    EXPECT_FLOAT_EQ(*command->patch.rotationDegrees, 45.0f);
    ASSERT_TRUE(command->patch.thickness.has_value());
    EXPECT_FLOAT_EQ(*command->patch.thickness, 1.5f);
    ASSERT_TRUE(command->patch.text.has_value());
    EXPECT_EQ(*command->patch.text, "HDG");
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesUpdateStrobeCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* strobe = envelope.add_commands()->mutable_update_strobe();
    strobe->set_page_id(11U);
    strobe->set_strobe_id(7U);
    strobe->set_active(true);
    strobe->mutable_position()->set_x(0.3f);
    strobe->mutable_position()->set_y(-0.4f);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::UpdateStrobeCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->strobeId, 7U);
    ASSERT_TRUE(command->active.has_value());
    EXPECT_TRUE(*command->active);
    ASSERT_TRUE(command->position.has_value());
    EXPECT_FLOAT_EQ(command->position->x, 0.3f);
    EXPECT_FLOAT_EQ(command->position->y, -0.4f);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesUpsertDynamicReticleCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* upsert = envelope.add_commands()->mutable_upsert_dynamic_reticle();
    upsert->mutable_target()->set_page_id(11U);
    upsert->mutable_target()->set_runtime_reticle_id(99U);
    upsert->set_template_transport_id(55U);
    upsert->mutable_patch()->set_visible(true);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::UpsertDynamicReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 11U);
    EXPECT_EQ(command->target.runtimeReticleId, 99U);
    EXPECT_EQ(command->templateTransportId, 55U);
    ASSERT_TRUE(command->patch.visible.has_value());
    EXPECT_TRUE(*command->patch.visible);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesUpsertDynamicReticlesCommandWithSeveralReticles)
{
    mfd::transport::CommandEnvelope envelope;
    auto* upsert = envelope.add_commands()->mutable_upsert_dynamic_reticles();
    upsert->set_page_id(11U);
    upsert->set_template_transport_id(55U);

    auto* first = upsert->add_reticles();
    first->set_runtime_reticle_id(1U);
    first->mutable_patch()->mutable_position()->set_x(0.1f);
    first->mutable_patch()->mutable_position()->set_y(0.2f);

    auto* second = upsert->add_reticles();
    second->set_runtime_reticle_id(2U);
    second->mutable_patch()->set_rotation_degrees(90.0f);

    auto* third = upsert->add_reticles();
    third->set_runtime_reticle_id(3U);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::UpsertDynamicReticlesCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->templateTransportId, 55U);
    ASSERT_EQ(command->reticles.size(), 3U);
    EXPECT_EQ(command->reticles[0].runtimeReticleId, 1U);
    ASSERT_TRUE(command->reticles[0].patch.position.has_value());
    EXPECT_FLOAT_EQ(command->reticles[0].patch.position->x, 0.1f);
    EXPECT_FLOAT_EQ(command->reticles[0].patch.position->y, 0.2f);
    EXPECT_EQ(command->reticles[1].runtimeReticleId, 2U);
    ASSERT_TRUE(command->reticles[1].patch.rotationDegrees.has_value());
    EXPECT_FLOAT_EQ(*command->reticles[1].patch.rotationDegrees, 90.0f);
    EXPECT_EQ(command->reticles[2].runtimeReticleId, 3U);
    EXPECT_FALSE(command->reticles[2].patch.position.has_value());
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesSetDynamicReticleSetVisibilityCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* visibility = envelope.add_commands()->mutable_set_dynamic_reticle_set_visibility();
    visibility->set_page_id(11U);
    visibility->set_template_transport_id(55U);
    visibility->set_visible(false);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->templateTransportId, 55U);
    EXPECT_FALSE(command->visible);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesSetDynamicReticleSetStrobeMagnetEnabledCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* magnet = envelope.add_commands()->mutable_set_dynamic_reticle_set_strobe_magnet_enabled();
    magnet->set_page_id(11U);
    magnet->set_template_transport_id(55U);
    magnet->set_enabled(true);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command =
        std::get_if<mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->templateTransportId, 55U);
    EXPECT_TRUE(command->enabled);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesRemoveDynamicReticleCommand)
{
    mfd::transport::CommandEnvelope envelope;
    auto* target = envelope.add_commands()->mutable_remove_dynamic_reticle()->mutable_target();
    target->set_page_id(11U);
    target->set_runtime_reticle_id(99U);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::RemoveDynamicReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 11U);
    EXPECT_EQ(command->target.runtimeReticleId, 99U);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesResetWindowCommand)
{
    mfd::transport::CommandEnvelope envelope;
    envelope.add_commands()->mutable_reset_window();

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&decoded->commands.front()), nullptr);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesReticlePatchTextsById)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    (*update->mutable_patch()->mutable_texts_by_id())[55U] = "HDG";
    (*update->mutable_patch()->mutable_texts_by_id())[56U] = "ALT";

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_EQ(command->patch.textsById.size(), 2U);
    EXPECT_EQ(command->patch.textsById.at(55U), "HDG");
    EXPECT_EQ(command->patch.textsById.at(56U), "ALT");
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesReticlePatchLetterSpacingsById)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    (*update->mutable_patch()->mutable_letter_spacings_by_id())[55U] = 0.02f;
    (*update->mutable_patch()->mutable_letter_spacings_by_id())[56U] = -0.01f;

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_EQ(command->patch.letterSpacingsById.size(), 2U);
    EXPECT_FLOAT_EQ(command->patch.letterSpacingsById.at(55U), 0.02f);
    EXPECT_FLOAT_EQ(command->patch.letterSpacingsById.at(56U), -0.01f);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesReticlePatchPrimitivePatchesById)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* firstPrimitive = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[33U];
    firstPrimitive->set_visible(true);
    firstPrimitive->set_radius(0.4f);
    auto* secondPrimitive = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[34U];
    secondPrimitive->set_text("123");
    secondPrimitive->set_letter_spacing(0.05f);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_EQ(command->patch.primitivePatchesById.size(), 2U);
    const mfd::PrimitivePatch& first = command->patch.primitivePatchesById.at(33U);
    ASSERT_TRUE(first.visible.has_value());
    EXPECT_TRUE(*first.visible);
    ASSERT_TRUE(first.radius.has_value());
    EXPECT_FLOAT_EQ(*first.radius, 0.4f);
    const mfd::PrimitivePatch& second = command->patch.primitivePatchesById.at(34U);
    ASSERT_TRUE(second.text.has_value());
    EXPECT_EQ(*second.text, "123");
    ASSERT_TRUE(second.letterSpacing.has_value());
    EXPECT_FLOAT_EQ(*second.letterSpacing, 0.05f);
}

TEST(CommandTypesTests, DeserializeCommandBatchDecodesPrimitivePatchPoints)
{
    mfd::transport::CommandEnvelope envelope;
    auto* update = envelope.add_commands()->mutable_update_reticle();
    update->mutable_target()->set_page_id(11U);
    update->mutable_target()->set_reticle_id(22U);
    auto* primitivePatch = &(*update->mutable_patch()->mutable_primitive_patches_by_id())[33U];
    auto* firstPoint = primitivePatch->add_points();
    firstPoint->set_x(-0.2f);
    firstPoint->set_y(0.1f);
    auto* secondPoint = primitivePatch->add_points();
    secondPoint->set_x(0.0f);
    secondPoint->set_y(0.25f);
    auto* thirdPoint = primitivePatch->add_points();
    thirdPoint->set_x(0.3f);
    thirdPoint->set_y(-0.1f);

    const auto decoded = DeserializeEnvelope(envelope);

    ASSERT_TRUE(decoded.has_value());
    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    const mfd::PrimitivePatch& primitive = command->patch.primitivePatchesById.at(33U);
    ASSERT_TRUE(primitive.points.has_value());
    ASSERT_EQ(primitive.points->size(), 3U);
    EXPECT_FLOAT_EQ(primitive.points->at(0).x, -0.2f);
    EXPECT_FLOAT_EQ(primitive.points->at(0).y, 0.1f);
    EXPECT_FLOAT_EQ(primitive.points->at(1).x, 0.0f);
    EXPECT_FLOAT_EQ(primitive.points->at(1).y, 0.25f);
    EXPECT_FLOAT_EQ(primitive.points->at(2).x, 0.3f);
    EXPECT_FLOAT_EQ(primitive.points->at(2).y, -0.1f);
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsMalformedProtobufPayload)
{
    std::string error;
    const std::string malformed = "\xFF\xFF\xFF\xFF not a protobuf payload";

    const auto decoded = mfd::DeserializeCommandBatch(malformed, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Unable to parse Protocol Buffers command payload");
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsEnvelopeWithoutCommands)
{
    mfd::transport::CommandEnvelope envelope;
    envelope.set_sequence(9U);

    std::string error;
    const auto decoded = DeserializeEnvelope(envelope, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Protocol Buffers command payload is empty");
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsCommandWithoutPayload)
{
    mfd::transport::CommandEnvelope envelope;
    envelope.add_commands();

    std::string error;
    const auto decoded = DeserializeEnvelope(envelope, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "Protocol Buffers command payload is empty");
}

TEST(CommandTypesTests, DeserializeCommandBatchRejectsInvalidCommandInsideBatch)
{
    mfd::transport::CommandEnvelope envelope;
    envelope.add_commands()->mutable_activate_page()->set_page_id(11U);
    envelope.add_commands()->mutable_activate_page()->set_page_id(0U);

    std::string error;
    const auto decoded = DeserializeEnvelope(envelope, &error);

    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(error, "ActivatePageCommand requires a pageId or page name");
}
