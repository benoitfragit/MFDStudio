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

namespace
{
const editor::tutorial::TutorialStepDefinition& Step(const editor::tutorial::TutorialStepId id)
{
    return editor::tutorial::Steps()[static_cast<std::size_t>(id)];
}
} // namespace

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

TEST(EditorTutorialDataTests, TutorialSnippetsPreferGeneratedHandlesWithoutUserManagedIds)
{
    const std::string_view creationAfter = Step(editor::tutorial::TutorialStepId::ReviewDynamicReticleCreation).afterText;
    EXPECT_NE(creationAfter.find("generatedDynamicTracks.Create()"), std::string_view::npos);
    EXPECT_EQ(creationAfter.find("Upsert("), std::string_view::npos);
    EXPECT_EQ(creationAfter.find("trackId"), std::string_view::npos);

    const std::string_view removalAfter = Step(editor::tutorial::TutorialStepId::ReviewDynamicReticleRemoval).afterText;
    EXPECT_NE(removalAfter.find("generatedDynamicTracks.Remove"), std::string_view::npos);
    EXPECT_EQ(removalAfter.find("RemoveDynamicReticle"), std::string_view::npos);

    const std::string_view staticAfter = Step(editor::tutorial::TutorialStepId::ReviewStaticReticleCommands).afterText;
    EXPECT_NE(staticAfter.find("page1Circle.Primitive01().SetRadius"), std::string_view::npos);
    EXPECT_NE(staticAfter.find("progressFill.SetSize"), std::string_view::npos);
    EXPECT_NE(staticAfter.find("page2ProgressBar.SetVisible"), std::string_view::npos);

    const std::string_view integrationAfter = Step(editor::tutorial::TutorialStepId::ReviewGeneratedUiIntegration).afterText;
    EXPECT_NE(integrationAfter.find("DynamicMfdTutorialRadarTrack()"), std::string_view::npos);
    EXPECT_NE(integrationAfter.find("page1.strobe"), std::string_view::npos);
    EXPECT_NE(integrationAfter.find("page2.mfdTutorialProgressBar"), std::string_view::npos);
    EXPECT_NE(integrationAfter.find("FillBar()"), std::string_view::npos);
}

TEST(EditorTutorialDataTests, ProgressBarTutorialStepsTargetReticleExposureFlow)
{
    const auto& createStep = Step(editor::tutorial::TutorialStepId::CreateProgressBarReticle);
    EXPECT_STREQ(createStep.targetId, "menu_reticle");

    const auto& exposeStep = Step(editor::tutorial::TutorialStepId::ExposeProgressBarFillPrimitive);
    EXPECT_STREQ(exposeStep.targetId, "primitive_exposed_checkbox");

    const auto& addStep = Step(editor::tutorial::TutorialStepId::AddProgressBarToPage2);
    EXPECT_STREQ(addStep.targetId, "library_add_to_page");
}

TEST(EditorTutorialDataTests, TutorialMetadataReflectsSharedWindowLauncherFlow)
{
    const auto& framebufferStep = Step(editor::tutorial::TutorialStepId::ReviewRgba32FramebufferCapture);
    EXPECT_STREQ(framebufferStep.filePath, "examples/mfd_framebuffer_stdout_plugin/src/FramebufferStdoutPlugin.cpp");
    EXPECT_NE(std::string_view(framebufferStep.afterText).find("MfdWindowFramebufferCallback"), std::string_view::npos);
    EXPECT_EQ(std::string_view(framebufferStep.afterText).find("RunLauncher"), std::string_view::npos);

    const auto& registrationStep = Step(editor::tutorial::TutorialStepId::ReviewTutorialTargetRegistration);
    EXPECT_NE(std::string_view(registrationStep.afterText).find("add_subdirectory(client_tutorial)"),
              std::string_view::npos);
    EXPECT_EQ(std::string_view(registrationStep.afterText).find("add_subdirectory(examples/mfd_tutorial)"),
              std::string_view::npos);
    EXPECT_NE(std::string_view(registrationStep.explanation).find("Start-MfdTutorial.bat"), std::string_view::npos);
}
