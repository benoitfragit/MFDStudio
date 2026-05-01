/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for private numeric JSON parsing helpers.
 */

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "JsonNumberParsing.h"

TEST(JsonNumberParsingTests, ParsePixelNumberRejectsNonFiniteValues)
{
    EXPECT_THROW(mfd::json_loader_detail::ParsePixelNumber(
                     nlohmann::json(std::numeric_limits<double>::infinity()),
                     "window width"),
                 std::runtime_error);
    EXPECT_THROW(mfd::json_loader_detail::ParsePixelNumber(
                     nlohmann::json(-std::numeric_limits<double>::infinity()),
                     "window width"),
                 std::runtime_error);
    EXPECT_THROW(mfd::json_loader_detail::ParsePixelNumber(
                     nlohmann::json(std::numeric_limits<double>::quiet_NaN()),
                     "window width"),
                 std::runtime_error);
}

TEST(JsonNumberParsingTests, ParsePixelNumberRejectsValuesOutsideSignedIntRange)
{
    EXPECT_THROW(mfd::json_loader_detail::ParsePixelNumber(nlohmann::json(3000000000.0), "window width"),
                 std::runtime_error);
    EXPECT_THROW(mfd::json_loader_detail::ParsePixelNumber(nlohmann::json(-3000000000.0), "window x"),
                 std::runtime_error);
}

TEST(JsonNumberParsingTests, ParseDurationRejectsNonPositiveValuesAfterRounding)
{
    EXPECT_THROW(mfd::json_loader_detail::ParseDurationMilliseconds(nlohmann::json(0), "blink duration"),
                 std::runtime_error);
    EXPECT_THROW(mfd::json_loader_detail::ParseDurationMilliseconds(nlohmann::json(-0.49), "blink duration"),
                 std::runtime_error);
    EXPECT_EQ(mfd::json_loader_detail::ParseDurationMilliseconds(nlohmann::json(12.6), "blink duration"), 13U);
}
