/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests validating static tutorial metadata exposed by the editor module.
 */

#include "EditorTutorialData.h"

#include <string_view>

#include <gtest/gtest.h>

TEST(EditorTutorialDataTests, StepCountMatchesExposedSpanSize)
{
    const auto steps = editor::tutorial::Steps();
    ASSERT_EQ(static_cast<int>(steps.size()), editor::tutorial::StepCount());
    ASSERT_GT(editor::tutorial::StepCount(), 0);
}

TEST(EditorTutorialDataTests, StepsExposeNonEmptyTextFields)
{
    for (const auto& step : editor::tutorial::Steps())
    {
        ASSERT_NE(step.title, nullptr);
        ASSERT_NE(step.instruction, nullptr);
        ASSERT_NE(step.targetId, nullptr);
        EXPECT_FALSE(std::string_view(step.title).empty());
        EXPECT_FALSE(std::string_view(step.instruction).empty());
        EXPECT_FALSE(std::string_view(step.targetId).empty());
    }
}
