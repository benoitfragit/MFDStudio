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

#include <string>
#include <vector>

#include "mfd/control/CommandTypes.h"

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

    mfd::ReticlePatch patch;
    patch.blinkTypeId = 44U;
    patch.textsById.emplace(55U, "HDG");
    patch.letterSpacingsById.emplace(55U, 0.02f);
    patch.primitivePatchesById.emplace(55U, primitivePatch);

    mfd::CommandBatch batch;
    batch.sequence = 9U;
    batch.mappingHash = "deadbeef";
    batch.commands.push_back(mfd::UpdateReticleCommand {
        mfd::ReticleHandle {"Radar", "heading_box", 11U, 22U},
        patch});

    const std::string payload = mfd::SerializeCommandBatch(batch);
    const auto decoded = mfd::DeserializeCommandBatch(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 9U);
    EXPECT_EQ(decoded->mappingHash, "deadbeef");
    ASSERT_EQ(decoded->commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&decoded->commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.page, "Radar");
    EXPECT_EQ(update->target.reticle, "heading_box");
    EXPECT_EQ(update->target.pageId, 11U);
    EXPECT_EQ(update->target.reticleId, 22U);
    ASSERT_TRUE(update->patch.blinkTypeId.has_value());
    EXPECT_EQ(*update->patch.blinkTypeId, 44U);
    EXPECT_EQ(update->patch.textsById.at(55U), "HDG");
    EXPECT_FLOAT_EQ(update->patch.letterSpacingsById.at(55U), 0.02f);
    ASSERT_EQ(update->patch.primitivePatchesById.size(), 1U);
    ASSERT_TRUE(update->patch.primitivePatchesById.at(55U).text.has_value());
    EXPECT_EQ(*update->patch.primitivePatchesById.at(55U).text, "123");
}
