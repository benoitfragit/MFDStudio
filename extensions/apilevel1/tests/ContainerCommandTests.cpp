/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Protobuf command contract tests for API LEVEL 1 containers. */

#include "mfd/control/CommandTypes.h"

#include <cmath>
#include <limits>
#include <string>
#include <variant>

#include <gtest/gtest.h>

TEST(ContainerCommandTests, SerializesEveryContainerPatchField)
{
    mfd::api1::UpdateContainerCommand update;
    update.target.pageId = 101U;
    update.target.containerTransportId = 202U;
    update.patch.visible = false;
    update.patch.enabled = true;
    update.patch.scrollOffset = mfd::Vec2 {0.25f, 0.5f};
    mfd::CommandBatch batch;
    batch.mappingHash = "mapping";
    batch.commands.emplace_back(update);

    const auto decoded = mfd::DeserializeCommandBatch(mfd::SerializeCommandBatch(batch));
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->commands.size(), 1U);
    const auto* command = std::get_if<mfd::api1::UpdateContainerCommand>(&decoded->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 101U);
    EXPECT_EQ(command->target.containerTransportId, 202U);
    ASSERT_TRUE(command->patch.visible.has_value());
    EXPECT_FALSE(*command->patch.visible);
    ASSERT_TRUE(command->patch.enabled.has_value());
    EXPECT_TRUE(*command->patch.enabled);
    ASSERT_TRUE(command->patch.scrollOffset.has_value());
    EXPECT_FLOAT_EQ(command->patch.scrollOffset->y, 0.5f);
}

TEST(ContainerCommandTests, RejectsEmptyNegativeAndNonFinitePatches)
{
    mfd::api1::UpdateContainerCommand update;
    update.target.pageId = 1U;
    update.target.containerTransportId = 2U;
    EXPECT_THROW(mfd::SerializeUserCommand(mfd::UserCommand {update}), std::runtime_error);

    update.patch.scrollOffset = mfd::Vec2 {-1.0f, 0.0f};
    EXPECT_THROW(mfd::SerializeUserCommand(mfd::UserCommand {update}), std::runtime_error);

    update.patch.scrollOffset = mfd::Vec2 {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    EXPECT_THROW(mfd::SerializeUserCommand(mfd::UserCommand {update}), std::runtime_error);
}
