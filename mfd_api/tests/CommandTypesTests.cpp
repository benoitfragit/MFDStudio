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
