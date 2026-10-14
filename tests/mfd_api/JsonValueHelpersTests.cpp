/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "JsonValueHelpers.h"

#include <cstdint>
#include <limits>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

TEST(JsonValueHelpersTests, ZeroKeepsItsDecimalRepresentation)
{
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(std::int64_t {0})), "0");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(std::uint64_t {0})), "0");
}

TEST(JsonValueHelpersTests, NegativeIntegerKeepsItsSign)
{
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(std::int64_t {-1})), "-1");
}

TEST(JsonValueHelpersTests, SignedMaximumKeepsItsDecimalRepresentation)
{
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(
                  nlohmann::json(std::numeric_limits<std::int64_t>::max())),
              "9223372036854775807");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(
                  nlohmann::json::parse("9223372036854775807")),
              "9223372036854775807");
}

TEST(JsonValueHelpersTests, UnsignedIntegerAboveSignedMaximumDoesNotWrap)
{
    const std::uint64_t value = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(value)), "9223372036854775808");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(
                  nlohmann::json::parse("9223372036854775808")),
              "9223372036854775808");
}

TEST(JsonValueHelpersTests, UnsignedMaximumKeepsAllDigits)
{
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(
                  nlohmann::json(std::numeric_limits<std::uint64_t>::max())),
              "18446744073709551615");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(
                  nlohmann::json::parse("18446744073709551615")),
              "18446744073709551615");
}

TEST(JsonValueHelpersTests, OtherScalarTypesKeepExistingRepresentations)
{
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json("text")), "text");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(true)), "true");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(false)), "false");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(1.5)), "1.5");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json(nullptr)), "null");
    EXPECT_EQ(mfd::json_loader_detail::JsonScalarToString(nlohmann::json::array({1, 2})), "[1,2]");
}
