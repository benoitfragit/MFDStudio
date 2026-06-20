/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for shared runtime validation budgets.
 */

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

#include "mfd/model/RuntimeBudgets.h"

TEST(RuntimeValidationTests, Vec2ValidationRejectsNonFiniteAndOutOfBudgetCoordinates)
{
    using namespace mfd::runtime_validation;

    EXPECT_TRUE(IsFiniteVec2({0.0f, 0.0f}));
    EXPECT_TRUE(IsValidVec2({kMaxAbsCoordinate, -kMaxAbsCoordinate}));
    EXPECT_FALSE(IsFiniteVec2({std::numeric_limits<float>::infinity(), 0.0f}));
    EXPECT_FALSE(IsValidVec2({kMaxAbsCoordinate + 1.0f, 0.0f}));
    EXPECT_FALSE(IsValidScale({kMaxAbsScale + 1.0f, 1.0f}));
}

TEST(RuntimeValidationTests, PayloadValidationRejectsOversizedTextAndPointLists)
{
    using namespace mfd::runtime_validation;

    EXPECT_TRUE(IsValidTextPayload(std::string(kMaxTextBytes, 'x')));
    EXPECT_FALSE(IsValidTextPayload(std::string(kMaxTextBytes + 1U, 'x')));

    std::vector<mfd::Vec2> validPoints = {{0.0f, 0.0f}, {0.25f, -0.25f}};
    EXPECT_TRUE(AreValidPoints(validPoints, 2U));

    validPoints.push_back({0.5f, 0.5f});
    EXPECT_TRUE(AreValidPoints(validPoints, 2U));

    std::vector<mfd::Vec2> oversizedPoints(kMaxPrimitivePoints + 1U, mfd::Vec2 {});
    EXPECT_FALSE(AreValidPoints(oversizedPoints, 2U));

    std::vector<mfd::Vec2> invalidPoints = {{0.0f, 0.0f}, {std::numeric_limits<float>::quiet_NaN(), 0.0f}};
    EXPECT_FALSE(AreValidPoints(invalidPoints, 2U));
}
