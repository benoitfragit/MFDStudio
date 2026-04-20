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
    int uiStepCount = 0;
    int fileStepCount = 0;
    for (const auto& step : editor::tutorial::Steps())
    {
        ASSERT_NE(step.title, nullptr);
        ASSERT_NE(step.instruction, nullptr);
        ASSERT_NE(step.targetId, nullptr);
        EXPECT_FALSE(std::string_view(step.title).empty());
        EXPECT_FALSE(std::string_view(step.instruction).empty());

        if (editor::tutorial::IsUiStep(step))
        {
            ++uiStepCount;
            EXPECT_FALSE(std::string_view(step.targetId).empty());
        }

        if (editor::tutorial::IsFileReviewStep(step))
        {
            ++fileStepCount;
            ASSERT_NE(step.filePath, nullptr);
            ASSERT_NE(step.beforeText, nullptr);
            ASSERT_NE(step.afterText, nullptr);
            ASSERT_NE(step.explanation, nullptr);
            ASSERT_NE(step.advanceLabel, nullptr);
            EXPECT_FALSE(std::string_view(step.filePath).empty());
            EXPECT_FALSE(std::string_view(step.beforeText).empty());
            EXPECT_FALSE(std::string_view(step.afterText).empty());
            EXPECT_FALSE(std::string_view(step.explanation).empty());
            EXPECT_FALSE(std::string_view(step.advanceLabel).empty());
            EXPECT_GE(step.beforeFirstLine, 1);
            EXPECT_GE(step.afterFirstLine, 1);
        }
    }

    EXPECT_GT(uiStepCount, 0);
    EXPECT_GT(fileStepCount, 0);
}
