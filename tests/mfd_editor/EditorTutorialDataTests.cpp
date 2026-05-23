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

TEST(EditorTutorialDataTests, StepsExposeNonEmptyCoreFields)
{
    int uiStepCount = 0;
    int referenceStepCount = 0;
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

        if (editor::tutorial::IsReferenceDocumentStep(step))
        {
            ++referenceStepCount;
            ASSERT_NE(step.filePath, nullptr);
            ASSERT_NE(step.explanation, nullptr);
            ASSERT_NE(step.advanceLabel, nullptr);
            EXPECT_FALSE(std::string_view(step.filePath).empty());
            EXPECT_FALSE(std::string_view(step.explanation).empty());
            EXPECT_FALSE(std::string_view(step.advanceLabel).empty());
        }
    }

    EXPECT_GT(uiStepCount, 0);
    EXPECT_GT(referenceStepCount, 0);
}

TEST(EditorTutorialDataTests, TutorialMetadataGuidesRadarTrackLayerFlow)
{
    const auto& selectTitleStep = Step(editor::tutorial::TutorialStepId::SelectPage1TitleChrome);
    EXPECT_STREQ(selectTitleStep.targetId, "page_select_title_chrome");
    EXPECT_NE(std::string_view(selectTitleStep.instruction).find("title chrome"), std::string_view::npos);
    EXPECT_NE(std::string_view(selectTitleStep.instruction).find("scale"), std::string_view::npos);

    const auto& frameTitleStep = Step(editor::tutorial::TutorialStepId::FramePage1Title);
    EXPECT_STREQ(frameTitleStep.targetId, "page_title_decoration");
    EXPECT_NE(std::string_view(frameTitleStep.instruction).find("Frame"), std::string_view::npos);
    EXPECT_NE(std::string_view(frameTitleStep.instruction).find("line style"), std::string_view::npos);

    const auto& createLayerStep = Step(editor::tutorial::TutorialStepId::CreateRadarTrackLayerOnPage1);
    EXPECT_STREQ(createLayerStep.targetId, "inspector_add_radar_track_layer");
    EXPECT_NE(std::string_view(createLayerStep.instruction).find("RadarTrackLayer"), std::string_view::npos);
    EXPECT_NE(std::string_view(createLayerStep.instruction).find("steering cue"), std::string_view::npos);

    const auto& dynamicTemplateStep = Step(editor::tutorial::TutorialStepId::AllowPage1DynamicReticleTemplate);
    EXPECT_STREQ(dynamicTemplateStep.targetId, "page_dynamic_template_mfd_tutorial_radar_track");
    EXPECT_NE(std::string_view(dynamicTemplateStep.instruction).find("mfd_tutorial_radar_track"),
              std::string_view::npos);
    EXPECT_NE(std::string_view(dynamicTemplateStep.instruction).find("RadarTrackLayer"), std::string_view::npos);
    EXPECT_NE(std::string_view(dynamicTemplateStep.instruction).find("inspired_steering_cue"),
              std::string_view::npos);
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

TEST(EditorTutorialDataTests, TutorialMetadataCoversEditorWorkflowDiscoverySteps)
{
    const auto& pageContextStep = Step(editor::tutorial::TutorialStepId::ShowPageContext);
    EXPECT_STREQ(pageContextStep.targetId, "page_preview_view_menu");

    const auto& importStep = Step(editor::tutorial::TutorialStepId::InspectPageImportWorkflow);
    EXPECT_STREQ(importStep.targetId, "menu_page");
    EXPECT_NE(std::string_view(importStep.instruction).find("cancel"), std::string_view::npos);

    const auto& pageRenameStep = Step(editor::tutorial::TutorialStepId::InspectPageRenameWorkflow);
    EXPECT_STREQ(pageRenameStep.targetId, "menu_page");
    EXPECT_NE(std::string_view(pageRenameStep.instruction).find("rename"), std::string_view::npos);

    const auto& reticleRenameStep = Step(editor::tutorial::TutorialStepId::InspectReticleRenameWorkflow);
    EXPECT_STREQ(reticleRenameStep.targetId, "menu_reticle");

    const auto& exportStep = Step(editor::tutorial::TutorialStepId::InspectDesignExportWorkflow);
    EXPECT_STREQ(exportStep.targetId, "menu_file");
    EXPECT_NE(std::string_view(exportStep.instruction).find("export"), std::string_view::npos);
}

TEST(EditorTutorialDataTests, TutorialMetadataPointsToFollowUpDocuments)
{
    const auto& followUpStep = Step(editor::tutorial::TutorialStepId::OpenTutorialFollowUpGuide);
    EXPECT_STREQ(followUpStep.filePath, "docs/tutorials/15_review_integrated_editor_tutorial_outputs.md");
    EXPECT_NE(std::string_view(followUpStep.explanation).find("authoring"), std::string_view::npos);
    EXPECT_NE(std::string_view(followUpStep.explanation).find("runtime"), std::string_view::npos);

    const auto& documentationStep = Step(editor::tutorial::TutorialStepId::ReviewDocumentationPath);
    EXPECT_STREQ(documentationStep.filePath, "docs/tutorials/README.md");
    EXPECT_NE(std::string_view(documentationStep.explanation).find("mockup"), std::string_view::npos);
    EXPECT_NE(std::string_view(documentationStep.explanation).find("generated client API"), std::string_view::npos);
}
