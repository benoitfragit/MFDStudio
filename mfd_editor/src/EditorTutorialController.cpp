/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorTutorialController.h"

/**
 * @file
 * @brief Guided tutorial controller implementation extracted from the main editor shell.
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <imgui.h>

#include "EditorApplication.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"

namespace
{
using editor::ui::AccentButton;

constexpr int kTutorialStepMin = 0;
constexpr std::string_view kEditorDefaultWindowFile = "assets/windows/demo_pages.json";
constexpr std::string_view kTutorialWindowFile = "assets/windows/mfd_tutorial.json";
constexpr std::array<std::string_view, 4> kBuildConfigurations {
    "Debug",
    "Release",
    "RelWithDebInfo",
    "MinSizeRel"};
constexpr std::string_view kTutorialAircraftTemplateId = "mfd_tutorial_aircraft";
constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";
constexpr std::string_view kTutorialProgressBarTemplateId = "mfd_tutorial_progress_bar";
constexpr std::string_view kTutorialProgressBarFillPrimitiveId = "fill_bar";
constexpr std::string_view kTutorialProgressBarFramePrimitiveId = "frame";

bool ConfigureTutorialStrobeLinePrimitive(mfd::Primitive& primitive, const bool vertical) noexcept
{
    if (primitive.type != mfd::PrimitiveType::Line)
    {
        return false;
    }

    auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry);
    if (line == nullptr)
    {
        return false;
    }

    primitive.id = vertical ? "vertical_line" : "horizontal_line";
    primitive.style.thickness = 0.0038f;
    line->start = vertical ? mfd::Vec2 {0.0f, -0.055f} : mfd::Vec2 {-0.055f, 0.0f};
    line->end = vertical ? mfd::Vec2 {0.0f, 0.055f} : mfd::Vec2 {0.055f, 0.0f};
    return true;
}

bool ConfigureTutorialProgressBarFillPrimitive(mfd::Primitive& primitive) noexcept
{
    if (primitive.type != mfd::PrimitiveType::Rectangle)
    {
        return false;
    }

    auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry);
    if (rectangle == nullptr)
    {
        return false;
    }

    primitive.id = std::string(kTutorialProgressBarFillPrimitiveId);
    primitive.style.color = mfd::ColorRgba {0, 255, 128, 255};
    primitive.style.fillColor = mfd::ColorRgba {0, 255, 128, 120};
    primitive.style.filled = true;
    primitive.style.thickness = 0.0038f;
    primitive.transform.position = {-0.16f, 0.0f};
    *rectangle = mfd::RectangleGeometry {0.12f, 0.06f};
    return true;
}

bool ConfigureTutorialProgressBarFramePrimitive(mfd::Primitive& primitive) noexcept
{
    if (primitive.type != mfd::PrimitiveType::Rectangle)
    {
        return false;
    }

    auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry);
    if (rectangle == nullptr)
    {
        return false;
    }

    primitive.id = std::string(kTutorialProgressBarFramePrimitiveId);
    primitive.style.color = mfd::ColorRgba {0, 255, 128, 255};
    primitive.style.fillColor = mfd::ColorRgba {0, 0, 0, 0};
    primitive.style.filled = false;
    primitive.style.thickness = 0.0038f;
    primitive.transform = {};
    *rectangle = mfd::RectangleGeometry {0.48f, 0.10f};
    return true;
}

bool ConfigureTutorialAircraftTrianglePrimitive(mfd::Primitive& primitive) noexcept
{
    if (primitive.type != mfd::PrimitiveType::Triangle)
    {
        return false;
    }

    auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry);
    if (triangle == nullptr)
    {
        return false;
    }

    primitive.id = "aircraft_triangle";
    primitive.style.color = mfd::ColorRgba {255, 244, 210, 255};
    primitive.style.fillColor = mfd::ColorRgba {255, 244, 210, 96};
    primitive.style.filled = true;
    primitive.style.thickness = 0.0038f;
    triangle->points = std::array<mfd::Vec2, 3> {
        mfd::Vec2 {-0.045f, -0.040f},
        mfd::Vec2 {0.0f, 0.060f},
        mfd::Vec2 {0.045f, -0.040f}};
    return true;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](const unsigned char ch)
                   {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::optional<std::filesystem::path> FindProjectRoot(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        if (std::filesystem::exists(current / "CMakeLists.txt") &&
            std::filesystem::exists(current / "mfd_editor") &&
            std::filesystem::exists(current / "examples"))
        {
            return current;
        }

        if (current == current.root_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> EnumerateConfiguredBuildDirectories(const std::filesystem::path& projectRoot)
{
    std::vector<std::filesystem::path> buildDirectories;
    const std::filesystem::path buildRoot = projectRoot / "build";
    if (!std::filesystem::is_directory(buildRoot))
    {
        return buildDirectories;
    }

    for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        const std::filesystem::path buildDirectory = entry.path();
        if (std::filesystem::exists(buildDirectory / "CMakeCache.txt"))
        {
            buildDirectories.push_back(buildDirectory);
        }
    }

    std::sort(buildDirectories.begin(), buildDirectories.end());
    return buildDirectories;
}

void RemovePathQuietly(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

void RemoveEmptyDirectoryChain(const std::filesystem::path& start, const std::filesystem::path& stopExclusive)
{
    std::error_code error;
    std::filesystem::path current = start;
    while (!current.empty() &&
           current != stopExclusive &&
           current.has_relative_path() &&
           std::filesystem::exists(current, error) &&
           std::filesystem::is_directory(current, error) &&
           std::filesystem::is_empty(current, error))
    {
        std::filesystem::remove(current, error);
        if (error)
        {
            return;
        }
        current = current.parent_path();
    }
}

void RemoveTutorialGeneratedSourceFiles(const std::filesystem::path& projectRoot)
{
    const std::array<std::filesystem::path, 11> generatedFiles {{
        projectRoot / "assets/windows/mfd_tutorial.json",
        projectRoot / "assets/windows/mfd_tutorial.generated.map",
        projectRoot / "assets/pages/mfd_tutorial_page1.json",
        projectRoot / "assets/pages/mfd_tutorial_page2.json",
        projectRoot / "assets/pages/mfd_tutor.json",
        projectRoot / "assets/reticles/mfd_tutorial_radar_track.json",
        projectRoot / "assets/reticles/mfd_tutorial_aircraft.json",
        projectRoot / "assets/reticles/mfd_tutorial_circle.json",
        projectRoot / "assets/reticles/mfd_tutorial_progress_bar.json",
        projectRoot / "assets/reticles/mfd_tutorial_text.json",
        projectRoot / "assets/reticles/mfd_tutorial_strobe_cursor.json",
    }};

    for (const auto& path : generatedFiles)
    {
        RemovePathQuietly(path);
    }
}

void RemoveTutorialBuildOutputs(const std::filesystem::path& projectRoot)
{
    const std::vector<std::filesystem::path> buildDirectories = EnumerateConfiguredBuildDirectories(projectRoot);
    for (const auto& buildDirectory : buildDirectories)
    {
        for (const std::string_view targetName : {"client_tutorial"})
        {
            const std::filesystem::path targetRoot = buildDirectory / "examples" / std::string(targetName);
            for (const std::string_view configuration : kBuildConfigurations)
            {
                RemovePathQuietly(targetRoot / std::string(configuration));
                RemovePathQuietly(targetRoot / (std::string(targetName) + ".dir") / std::string(configuration));
            }
        }
    }
}

void RemoveTutorialStageArtifacts(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path stageRoot = projectRoot / "_Exec";
    if (!std::filesystem::is_directory(stageRoot))
    {
        return;
    }

    const std::unordered_set<std::string> artifactNames {
        "client_tutorial.exe",
        "client_tutorial.ilk",
        "client_tutorial.pdb",
        "tutorialui.h",
        "tutorialui.cpp",
        "mfdtutorialmockupui.h",
        "mfdtutorialmockupui.cpp",
        "mfd_tutorial.json",
        "mfd_tutorial.generated.map",
        "mfd_tutorial_page1.json",
        "mfd_tutorial_page2.json",
        "mfd_tutor.json",
        "mfd_tutorial_radar_track.json",
        "mfd_tutorial_aircraft.json",
        "mfd_tutorial_circle.json",
        "mfd_tutorial_progress_bar.json",
        "mfd_tutorial_text.json",
        "mfd_tutorial_strobe_cursor.json"};

    std::vector<std::filesystem::path> filesToRemove;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(stageRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string filename = ToLowerAscii(entry.path().filename().string());
        if (artifactNames.find(filename) != artifactNames.end())
        {
            filesToRemove.push_back(entry.path());
        }
    }

    for (const auto& path : filesToRemove)
    {
        RemovePathQuietly(path);
        RemoveEmptyDirectoryChain(path.parent_path(), stageRoot);
    }
}

std::filesystem::path NormalizeAgainstWorkingDirectory(const std::filesystem::path& path)
{
    return path.is_absolute()
               ? path.lexically_normal()
               : std::filesystem::absolute(path).lexically_normal();
}

struct TutorialStageInfo
{
    const char* title;
    const char* summary;
    int firstStep = 0;
    int lastStep = 0;
};

constexpr std::array<TutorialStageInfo, 4> kTutorialStages {{
    {"Stage 1 - Author In Editor",
     "Create the tutorial window, page assets, the Page1 title chrome frame, reusable reticle templates including the triangle-based `mfd_tutorial_aircraft`, `RadarTrackLayer`, two named Page1 strobes, editor-only layers, and the exposed primitive that will feed the generated client.",
     static_cast<int>(editor::tutorial::TutorialStepId::CreateWindow),
     static_cast<int>(editor::tutorial::TutorialStepId::AddProgressBarToPage2)},
    {"Stage 2 - Explore Editor Tools",
     "Use the page-preview helper panels, save the tutorial assets, and discover the import, rename, and export workflows directly from the editor.",
     static_cast<int>(editor::tutorial::TutorialStepId::ShowPageContext),
     static_cast<int>(editor::tutorial::TutorialStepId::InspectDesignExportWorkflow)},
    {"Stage 3 - Review Saved Outputs",
     "Open the dedicated follow-up guide to inspect the authored assets, the generated map, and the runtime entry points without replaying long code snippets inside the editor.",
     static_cast<int>(editor::tutorial::TutorialStepId::OpenTutorialFollowUpGuide),
     static_cast<int>(editor::tutorial::TutorialStepId::OpenTutorialFollowUpGuide)},
    {"Stage 4 - Continue In Docs",
     "Leave the editor with the right next reading path for the runtime mockup, the generated client API, and the deeper architecture notes.",
     static_cast<int>(editor::tutorial::TutorialStepId::ReviewDocumentationPath),
     static_cast<int>(editor::tutorial::TutorialStepId::ReviewDocumentationPath)},
}};

const TutorialStageInfo& StageForStep(const int stepIndex) noexcept
{
    for (const TutorialStageInfo& stage : kTutorialStages)
    {
        if (stepIndex >= stage.firstStep && stepIndex <= stage.lastStep)
        {
            return stage;
        }
    }

    return kTutorialStages.back();
}

int StageIndexForStep(const int stepIndex) noexcept
{
    for (int index = 0; index < static_cast<int>(kTutorialStages.size()); ++index)
    {
        const TutorialStageInfo& stage = kTutorialStages[static_cast<std::size_t>(index)];
        if (stepIndex >= stage.firstStep && stepIndex <= stage.lastStep)
        {
            return index;
        }
    }

    return static_cast<int>(kTutorialStages.size()) - 1;
}

float OverallTutorialProgress(const int stepIndex) noexcept
{
    const int stepCount = editor::tutorial::StepCount();
    if (stepCount <= 0)
    {
        return 0.0f;
    }

    return std::clamp(static_cast<float>(stepIndex + 1) / static_cast<float>(stepCount), 0.0f, 1.0f);
}

float StageTutorialProgress(const TutorialStageInfo& stage, const int stepIndex) noexcept
{
    const int stageSpan = std::max(1, stage.lastStep - stage.firstStep + 1);
    const int stageOffset = std::clamp(stepIndex - stage.firstStep + 1, 0, stageSpan);
    return std::clamp(static_cast<float>(stageOffset) / static_cast<float>(stageSpan), 0.0f, 1.0f);
}
} // namespace

EditorTutorialController::EditorTutorialController(EditorApplication& application) :
    app_(application)
{
}

void EditorTutorialController::LoadProgress()
{
    stepIndex_ = 0;
    std::ifstream stream(progressFile_);
    if (!stream.good())
    {
        return;
    }

    int savedStep = 0;
    stream >> savedStep;
    stepIndex_ = std::clamp(savedStep, kTutorialStepMin, editor::tutorial::StepCount() - 1);
}

void EditorTutorialController::OpenFlow()
{
    LoadProgress();
    showCoach_ = true;
    if (stepIndex_ > kTutorialStepMin && stepIndex_ < editor::tutorial::StepCount())
    {
        showResumePopup_ = true;
        return;
    }

    active_ = true;
    ClampStepIndex();
    app_.PrepareTutorialStep();
    SaveProgress();
}

void EditorTutorialController::ResumeFromSavedProgress()
{
    showResumePopup_ = false;
    active_ = true;
    showCoach_ = true;
    app_.PrepareTutorialStep();
}

void EditorTutorialController::RestartFromScratch()
{
    CleanupGeneratedFiles();
    bool reloadedDefaultWindow = true;
    const std::filesystem::path normalizedCurrentWindow = NormalizeAgainstWorkingDirectory(app_.windowFile_);
    const std::filesystem::path normalizedTutorialWindow =
        NormalizeAgainstWorkingDirectory(std::filesystem::path {kTutorialWindowFile});
    if (normalizedCurrentWindow == normalizedTutorialWindow ||
        !std::filesystem::exists(normalizedCurrentWindow))
    {
        reloadedDefaultWindow = app_.LoadWindowConfiguration(std::filesystem::path {kEditorDefaultWindowFile});
    }

    trackedReticleId_.clear();
    focusLayerId_.clear();
    stepIndex_ = 0;
    active_ = true;
    showCoach_ = true;
    app_.PrepareTutorialStep();
    SaveProgress();
    if (!reloadedDefaultWindow)
    {
        app_.RebuildStatus(
            "Tutorial reset cleaned tutorial outputs, but the editor could not reload the default sample window.",
            true);
        return;
    }

    app_.RebuildStatus("Tutorial reset. The walkthrough now starts again from the first editor action.", false);
}

void EditorTutorialController::CompleteStep()
{
    if (stepIndex_ >= editor::tutorial::StepCount() - 1)
    {
        Finish();
        return;
    }

    app_.RebuildStatus("Tutorial step completed. Moving to the next guided action.", false);
    AdvanceStep();
}

void EditorTutorialController::Finish()
{
    active_ = false;
    stepPhase_ = 0;
    focusLayerId_.clear();
    showCoach_ = false;
    ClearProgress();
    app_.RebuildStatus(
        "Tutorial completed. Only the tutorial assets were updated. Continue with the follow-up guide, then tutorials 03 and 11 plus the generated-client architecture notes.",
        false);
}

void EditorTutorialController::DrawCoach()
{
    if (!showCoach_)
    {
        return;
    }

    ClampStepIndex();
    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(stepIndex_)];
    const TutorialStageInfo& stage = StageForStep(stepIndex_);
    const int stageIndex = StageIndexForStep(stepIndex_);
    const float panelHeight = editor::tutorial::IsReferenceDocumentStep(step) ? 250.0f : 220.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.09f, 0.12f, 1.0f));
    if (!ImGui::BeginChild("TutorialCoachPanel", ImVec2(0.0f, panelHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextColored(ImVec4(0.42f, 0.84f, 0.96f, 1.0f), "Tutorial coach");
    ImGui::SameLine();
    ImGui::TextDisabled("Step %d / %d", stepIndex_ + 1, editor::tutorial::StepCount());
    ImGui::ProgressBar(OverallTutorialProgress(stepIndex_), ImVec2(-1.0f, 0.0f));
    ImGui::Spacing();
    ImGui::TextDisabled("Stage %d / %zu", stageIndex + 1, kTutorialStages.size());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.42f, 0.84f, 0.96f, 1.0f), "%s", stage.title);
    ImGui::TextWrapped("%s", stage.summary);
    ImGui::ProgressBar(StageTutorialProgress(stage, stepIndex_), ImVec2(-1.0f, 0.0f));
    ImGui::Separator();
    ImGui::TextUnformatted(step.title);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", step.instruction);
    ImGui::Spacing();

    if (editor::tutorial::IsReferenceDocumentStep(step))
    {
        DrawReferenceDocument();
    }
    else
    {
        ImGui::TextDisabled("This step stays blocked until the highlighted click succeeds.");
        const std::string_view actionLabel = CurrentActionLabel();
        if (!actionLabel.empty())
        {
            ImGui::TextWrapped("Current action: %.*s", static_cast<int>(actionLabel.size()), actionLabel.data());
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Restart from scratch"))
    {
        RestartFromScratch();
    }
    ImGui::SameLine();
    if (ImGui::Button("End tutorial"))
    {
        Finish();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorTutorialController::DrawHalo(const char* targetId, const char* title, const char* reason) const
{
    if (!active_ || stepIndex_ < kTutorialStepMin || stepIndex_ >= editor::tutorial::StepCount())
    {
        return;
    }

    if (targetId == nullptr || title == nullptr || reason == nullptr || !MatchesTarget(targetId))
    {
        return;
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 innerMin(min.x - 4.0f, min.y - 4.0f);
    const ImVec2 innerMax(max.x + 4.0f, max.y + 4.0f);
    const ImVec2 outerMin(min.x - 8.0f, min.y - 8.0f);
    const ImVec2 outerMax(max.x + 8.0f, max.y + 8.0f);
    drawList->AddRect(innerMin, innerMax, IM_COL32(84, 224, 255, 255), 10.0f, 0, 2.5f);
    drawList->AddRect(outerMin, outerMax, IM_COL32(84, 224, 255, 110), 12.0f, 0, 3.5f);

    constexpr float kWrapWidth = 260.0f;
    const ImVec2 titleSize = ImGui::CalcTextSize(title, nullptr, false, kWrapWidth);
    const ImVec2 reasonSize = ImGui::CalcTextSize(reason, nullptr, false, kWrapWidth);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImVec2 bubbleMin(max.x + 18.0f, min.y - 4.0f);
    const float bubbleWidth = std::max(titleSize.x, reasonSize.x) + 24.0f;
    const float bubbleHeight = titleSize.y + reasonSize.y + 28.0f;
    if (bubbleMin.x + bubbleWidth > viewport->WorkPos.x + viewport->WorkSize.x - 8.0f)
    {
        bubbleMin.x = min.x - bubbleWidth - 18.0f;
    }
    if (bubbleMin.x < viewport->WorkPos.x + 8.0f)
    {
        bubbleMin.x = viewport->WorkPos.x + 8.0f;
    }
    if (bubbleMin.y + bubbleHeight > viewport->WorkPos.y + viewport->WorkSize.y - 8.0f)
    {
        bubbleMin.y = viewport->WorkPos.y + viewport->WorkSize.y - bubbleHeight - 8.0f;
    }
    if (bubbleMin.y < viewport->WorkPos.y + 8.0f)
    {
        bubbleMin.y = viewport->WorkPos.y + 8.0f;
    }

    const ImVec2 bubbleMax(bubbleMin.x + bubbleWidth, bubbleMin.y + bubbleHeight);
    drawList->AddRectFilled(bubbleMin, bubbleMax, IM_COL32(11, 26, 34, 235), 12.0f);
    drawList->AddRect(bubbleMin, bubbleMax, IM_COL32(84, 224, 255, 255), 12.0f, 0, 2.0f);

    const ImVec2 itemCenter((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const ImVec2 bubbleAnchor =
        bubbleMin.x > itemCenter.x ? ImVec2(bubbleMin.x, bubbleMin.y + 20.0f) : ImVec2(bubbleMax.x, bubbleMin.y + 20.0f);
    drawList->AddLine(itemCenter, bubbleAnchor, IM_COL32(84, 224, 255, 255), 2.0f);
    drawList->AddCircleFilled(itemCenter, 4.0f, IM_COL32(84, 224, 255, 255));

    const ImVec2 textPos(bubbleMin.x + 12.0f, bubbleMin.y + 10.0f);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, IM_COL32(255, 255, 255, 255), title, nullptr, kWrapWidth);
    drawList->AddText(
        ImGui::GetFont(),
        ImGui::GetFontSize(),
        ImVec2(textPos.x, textPos.y + titleSize.y + 6.0f),
        IM_COL32(181, 216, 228, 255),
        reason,
        nullptr,
        kWrapWidth);
}

bool EditorTutorialController::MatchesTarget(const std::string_view targetId) const noexcept
{
    return !targetId.empty() && CurrentTargetId() == targetId;
}

bool EditorTutorialController::IsCoachVisible() const noexcept
{
    return showCoach_;
}

bool EditorTutorialController::IsStep(const int stepIndex) const noexcept
{
    return active_ && stepIndex_ == stepIndex;
}

bool EditorTutorialController::IsStepPhase(const int stepIndex, const int stepPhase) const noexcept
{
    return active_ && stepIndex_ == stepIndex && stepPhase_ == stepPhase;
}

int EditorTutorialController::StepIndex() const noexcept
{
    return stepIndex_;
}

void EditorTutorialController::AdvancePhase() noexcept
{
    ++stepPhase_;
}

void EditorTutorialController::ResetPhase() noexcept
{
    stepPhase_ = 0;
}

void EditorTutorialController::ClampStepIndex() noexcept
{
    stepIndex_ = std::clamp(stepIndex_, kTutorialStepMin, editor::tutorial::StepCount() - 1);
}

void EditorTutorialController::ClearTrackedReticle() noexcept
{
    trackedReticleId_.clear();
}

void EditorTutorialController::SetTrackedReticleId(std::string trackedReticleId)
{
    trackedReticleId_ = std::move(trackedReticleId);
}

std::string_view EditorTutorialController::TrackedReticleId() const noexcept
{
    return trackedReticleId_;
}

void EditorTutorialController::ClearFocusLayer() noexcept
{
    focusLayerId_.clear();
}

void EditorTutorialController::SetFocusLayerId(std::string focusLayerId)
{
    focusLayerId_ = std::move(focusLayerId);
}

std::string_view EditorTutorialController::FocusLayerId() const noexcept
{
    return focusLayerId_;
}

bool EditorTutorialController::ConsumeResumePopupRequest() noexcept
{
    const bool requested = showResumePopup_;
    showResumePopup_ = false;
    return requested;
}

bool EditorTutorialController::ShouldResetFileMenuPhaseOnClose() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStepPhase(static_cast<int>(TutorialStepId::CreateWindow), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::SaveTutorialAssets), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::InspectDesignExportWorkflow), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::InspectDesignExportWorkflow), 2);
}

bool EditorTutorialController::ShouldResetPageMenuPhaseOnClose() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStepPhase(static_cast<int>(TutorialStepId::CreatePage1), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreatePage2), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::InspectPageImportWorkflow), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::InspectPageRenameWorkflow), 1);
}

bool EditorTutorialController::ShouldResetReticleMenuPhaseOnClose() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStepPhase(static_cast<int>(TutorialStepId::CreateRadarTrackReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateCircleReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateAircraftReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateProgressBarReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow), 1);
}

bool EditorTutorialController::ShouldResetPagePreviewViewPhaseOnClose() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStepPhase(static_cast<int>(TutorialStepId::ShowPageContext), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::ShowLayerInspector), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::ShowMinimap), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::ShowReticleUsageHighlights), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::ShowProblemsPanel), 1);
}

bool EditorTutorialController::ShouldResetReticleCreatePopupOnCancel() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::CreateRadarTrackReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateCircleReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateAircraftReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle));
}

bool EditorTutorialController::ShouldAdvanceReticleCreatePhase() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle));
}

bool EditorTutorialController::ValidateNewLibraryReticleDraft(const std::string_view reticleId,
                                                              const mfd::PrimitiveType primitiveType,
                                                              std::string& error) const
{
    using editor::tutorial::TutorialStepId;

    error.clear();
    if (IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
    {
        if (reticleId != kTutorialStrobeCursorTemplateId)
        {
            error = "Tutorial: keep the reticle id set to 'mfd_tutorial_strobe_cursor'.";
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Line)
        {
            error = "Tutorial: create the strobe cursor from a Line primitive.";
            return false;
        }
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle)))
    {
        if (reticleId != kTutorialProgressBarTemplateId)
        {
            error = "Tutorial: keep the reticle id set to 'mfd_tutorial_progress_bar'.";
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Rectangle)
        {
            error = "Tutorial: create the progress bar from a Rectangle primitive.";
            return false;
        }
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateAircraftReticle)))
    {
        if (reticleId != kTutorialAircraftTemplateId)
        {
            error = "Tutorial: keep the reticle id set to 'mfd_tutorial_aircraft'.";
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Triangle)
        {
            error = "Tutorial: create the aircraft reticle from a Triangle primitive.";
            return false;
        }
    }

    return true;
}

void EditorTutorialController::ConfigureCreatedLibraryReticle(mfd::ReticleGroup& reticle) const
{
    using editor::tutorial::TutorialStepId;

    if (IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)) &&
        reticle.id == kTutorialStrobeCursorTemplateId &&
        !reticle.primitives.empty())
    {
        ConfigureTutorialStrobeLinePrimitive(reticle.primitives.front(), false);
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle)) &&
             reticle.id == kTutorialProgressBarTemplateId &&
             !reticle.primitives.empty())
    {
        ConfigureTutorialProgressBarFillPrimitive(reticle.primitives.front());
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateAircraftReticle)) &&
             reticle.id == kTutorialAircraftTemplateId &&
             !reticle.primitives.empty())
    {
        ConfigureTutorialAircraftTrianglePrimitive(reticle.primitives.front());
    }
}

bool EditorTutorialController::ShouldUseHighlightedAddToPageButton() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::AddAircraftReticleToPage1)) ||
           IsStep(static_cast<int>(TutorialStepId::AddCircleReticleToPage1)) ||
           IsStep(static_cast<int>(TutorialStepId::AddProgressBarToPage2));
}

bool EditorTutorialController::ValidateAddToPage(const mfd::PageDefinition* page,
                                                 const mfd::ReticleGroup& reticle,
                                                 std::string& error) const
{
    using editor::tutorial::TutorialStepId;

    error.clear();
    if (page == nullptr)
    {
        return true;
    }

    if (IsStep(static_cast<int>(TutorialStepId::AddAircraftReticleToPage1)) &&
        (page->name != "Page1" || reticle.id != kTutorialAircraftTemplateId))
    {
        error = "Tutorial: add 'mfd_tutorial_aircraft' to Page1 for this step.";
        return false;
    }

    if (IsStep(static_cast<int>(TutorialStepId::AddCircleReticleToPage1)) &&
        (page->name != "Page1" || reticle.id != "mfd_tutorial_circle"))
    {
        error = "Tutorial: add 'mfd_tutorial_circle' to Page1 for this step.";
        return false;
    }

    if (IsStep(static_cast<int>(TutorialStepId::AddProgressBarToPage2)) &&
        (page->name != "Page2" || reticle.id != kTutorialProgressBarTemplateId))
    {
        error = "Tutorial: add 'mfd_tutorial_progress_bar' to Page2 for this step.";
        return false;
    }

    return true;
}

std::string_view EditorTutorialController::LibraryAddToPageHaloReason() const noexcept
{
    using editor::tutorial::TutorialStepId;

    if (IsStep(static_cast<int>(TutorialStepId::AddProgressBarToPage2)))
    {
        return "Instantiate the authored progress-bar template on Page2 so the generated client can drive it next.";
    }

    if (IsStep(static_cast<int>(TutorialStepId::AddAircraftReticleToPage1)))
    {
        return "Instantiate the triangle-based aircraft reticle on Page1 so the static ownship reference stays anchored at x = 0.0 and y = -0.7.";
    }

    return "Instantiate the prepared tutorial circle on Page1 so clipping can be demonstrated next.";
}

bool EditorTutorialController::ValidateAppendPrimitive(const mfd::ReticleGroup& reticle,
                                                       const mfd::PrimitiveType primitiveType,
                                                       std::string& error) const
{
    using editor::tutorial::TutorialStepId;

    error.clear();
    if (IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
    {
        if (reticle.id != kTutorialStrobeCursorTemplateId)
        {
            error = "Tutorial: append the second line inside 'mfd_tutorial_strobe_cursor'.";
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Line)
        {
            error = "Tutorial: keep the Add primitive selector on 'Line' for the strobe cursor.";
            return false;
        }
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle)))
    {
        if (reticle.id != kTutorialProgressBarTemplateId)
        {
            error = "Tutorial: append the second rectangle inside 'mfd_tutorial_progress_bar'.";
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Rectangle)
        {
            error = "Tutorial: keep the Add primitive selector on 'Rectangle' for the progress bar.";
            return false;
        }
    }

    return true;
}

void EditorTutorialController::ConfigureAppendedPrimitive(mfd::Primitive& primitive) const
{
    using editor::tutorial::TutorialStepId;

    if (IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
    {
        ConfigureTutorialStrobeLinePrimitive(primitive, true);
    }
    else if (IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle)))
    {
        ConfigureTutorialProgressBarFramePrimitive(primitive);
    }
}

std::string_view EditorTutorialController::LibraryAppendPrimitiveHaloReason() const noexcept
{
    using editor::tutorial::TutorialStepId;

    if (IsStep(static_cast<int>(TutorialStepId::CreateProgressBarReticle)))
    {
        return "Append the outline rectangle so the tutorial progress bar gets an empty frame around the fill.";
    }

    return "Append the second line so the tutorial strobe cursor becomes a simple cross.";
}

bool EditorTutorialController::IsExposedPrimitiveTutorialSelection(const std::string_view reticleId,
                                                                  const std::string_view primitiveId) const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive)) &&
           reticleId == kTutorialProgressBarTemplateId &&
           primitiveId == kTutorialProgressBarFillPrimitiveId;
}

void EditorTutorialController::AdvanceStep()
{
    stepIndex_ = std::min(stepIndex_ + 1, editor::tutorial::StepCount() - 1);
    app_.PrepareTutorialStep();
    SaveProgress();
}

std::string_view EditorTutorialController::CurrentTargetId() const noexcept
{
    using editor::tutorial::TutorialStepId;

    if (!active_ ||
        stepIndex_ < kTutorialStepMin ||
        stepIndex_ >= editor::tutorial::StepCount())
    {
        return {};
    }

    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(stepIndex_)];
    if (!editor::tutorial::IsUiStep(step))
    {
        return {};
    }

    switch (stepIndex_)
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        return stepPhase_ == 0 ? "menu_file" : (stepPhase_ == 1 ? "menu_file_new_window" : "popup_window_create");
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
    case static_cast<int>(TutorialStepId::CreateAircraftReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" : "popup_reticle_create");
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" :
                                                    (stepPhase_ == 2 ? "popup_reticle_create" : "library_append_primitive"));
    case static_cast<int>(TutorialStepId::CreatePage1):
    case static_cast<int>(TutorialStepId::CreatePage2):
        return stepPhase_ == 0 ? "menu_page" : (stepPhase_ == 1 ? "menu_page_new" : "popup_page_create");
    case static_cast<int>(TutorialStepId::SelectPage1TitleChrome):
        return "page_select_title_chrome";
    case static_cast<int>(TutorialStepId::FramePage1Title):
        return stepPhase_ == 0 ? "page_title_decoration" : "page_title_decoration_frame";
    case static_cast<int>(TutorialStepId::CreateRadarTrackLayerOnPage1):
        return "inspector_add_radar_track_layer";
    case static_cast<int>(TutorialStepId::AllowPage1DynamicReticleTemplate):
        return "page_dynamic_template_mfd_tutorial_radar_track";
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" :
                                                    (stepPhase_ == 2 ? "popup_reticle_create" : "library_append_primitive"));
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        return "primitive_exposed_checkbox";
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        return "library_add_to_page";
    case static_cast<int>(TutorialStepId::AddPage1DefaultStrobe):
        return stepPhase_ == 0 ? "page_strobe_default_name" :
                                 (stepPhase_ == 1 ? "page_strobe_default" : "page_strobe_default_add");
    case static_cast<int>(TutorialStepId::AddPage1AlternativeStrobe):
        return stepPhase_ == 0 ? "page_strobe_alternative_name" :
                                 (stepPhase_ == 1 ? "page_strobe_alternative" : "page_strobe_alternative_add");
    case static_cast<int>(TutorialStepId::AddAircraftReticleToPage1):
        return "library_add_to_page";
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
        return "library_add_to_page";
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
        return stepPhase_ == 0 ? "page_preview_clip_source" : "context_clip_outer";
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        return stepPhase_ == 0 ? "inspector_add_layer" : "inspector_layer_visibility";
    case static_cast<int>(TutorialStepId::ShowPageContext):
        return stepPhase_ == 0 ? "page_preview_view_menu" : "page_preview_view_page_context";
    case static_cast<int>(TutorialStepId::ShowLayerInspector):
        return stepPhase_ == 0 ? "page_preview_view_menu" : "page_preview_view_layer_inspector";
    case static_cast<int>(TutorialStepId::ShowMinimap):
        return stepPhase_ == 0 ? "page_preview_view_menu" : "page_preview_view_minimap";
    case static_cast<int>(TutorialStepId::ShowReticleUsageHighlights):
        return stepPhase_ == 0 ? "page_preview_view_menu" : "page_preview_view_highlight_usages";
    case static_cast<int>(TutorialStepId::ShowProblemsPanel):
        return stepPhase_ == 0 ? "page_preview_view_menu" : "page_preview_view_problems";
    case static_cast<int>(TutorialStepId::ToggleFullscreenPreview):
        return "page_preview_fullscreen";
    case static_cast<int>(TutorialStepId::SaveTutorialAssets):
        return stepPhase_ == 0 ? "menu_file" : "menu_file_save";
    case static_cast<int>(TutorialStepId::InspectPageImportWorkflow):
        return stepPhase_ == 0 ? "menu_page" : "menu_page_import";
    case static_cast<int>(TutorialStepId::InspectPageRenameWorkflow):
        return stepPhase_ == 0 ? "menu_page" :
                                 (stepPhase_ == 1 ? "menu_page_rename" : "popup_page_rename_cancel");
    case static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_rename" : "popup_reticle_rename_cancel");
    case static_cast<int>(TutorialStepId::InspectDesignExportWorkflow):
        return stepPhase_ == 0 ? "menu_file" :
                                 (stepPhase_ == 1 ? "menu_file_export" :
                                                    (stepPhase_ == 2 ? "menu_file_export_design"
                                                                     : "popup_design_export_cancel"));
    default:
        return {};
    }
}

std::string_view EditorTutorialController::CurrentActionLabel() const noexcept
{
    using editor::tutorial::TutorialStepId;

    switch (stepIndex_)
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        return stepPhase_ == 0 ? "Click File." :
                                 (stepPhase_ == 1 ? "Click New window from scratch." : "Click Create window.");
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
    case static_cast<int>(TutorialStepId::CreateAircraftReticle):
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click New library reticle from primitive." : "Click Create reticle.");
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click New library reticle from primitive." :
                                                    (stepPhase_ == 2 ? "Click Create reticle." : "Click Append primitive."));
    case static_cast<int>(TutorialStepId::CreatePage1):
    case static_cast<int>(TutorialStepId::CreatePage2):
        return stepPhase_ == 0 ? "Click Page." :
                                 (stepPhase_ == 1 ? "Click New page." : "Click Create page.");
    case static_cast<int>(TutorialStepId::SelectPage1TitleChrome):
        return "Click Select title chrome.";
    case static_cast<int>(TutorialStepId::FramePage1Title):
        return stepPhase_ == 0 ? "Open Decoration." : "Choose Frame.";
    case static_cast<int>(TutorialStepId::CreateRadarTrackLayerOnPage1):
        return "Click Add layer to create RadarTrackLayer.";
    case static_cast<int>(TutorialStepId::AllowPage1DynamicReticleTemplate):
        return "Add mfd_tutorial_radar_track on RadarTrackLayer.";
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click New library reticle from primitive." :
                                                    (stepPhase_ == 2 ? "Click Create reticle." : "Click Append primitive."));
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        return "Enable Exposed on the fill_bar primitive.";
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        return "Click Add to active page.";
    case static_cast<int>(TutorialStepId::AddPage1DefaultStrobe):
        return stepPhase_ == 0 ? "Keep the suggested strobe name set to Default." :
                                 (stepPhase_ == 1 ? "Choose mfd_tutorial_strobe_cursor as the shared template."
                                                  : "Click Add strobe to create the default authored Page1 strobe.");
    case static_cast<int>(TutorialStepId::AddPage1AlternativeStrobe):
        return stepPhase_ == 0 ? "Keep the suggested strobe name set to Strobe1." :
                                 (stepPhase_ == 1 ? "Choose mfd_tutorial_aircraft for the alternative triangle strobe."
                                                  : "Click Add strobe to create the Page1 runtime alternative.");
    case static_cast<int>(TutorialStepId::AddAircraftReticleToPage1):
        return "Click Add to active page.";
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
        return "Click Add to active page.";
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
        return stepPhase_ == 0 ? "Right-click the circle reticle in the page preview." : "Click Clip outside.";
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        return stepPhase_ == 0 ? "Click Add layer." : "Click Visible to hide the new layer.";
    case static_cast<int>(TutorialStepId::ShowPageContext):
        return stepPhase_ == 0 ? "Click View." : "Enable Page context.";
    case static_cast<int>(TutorialStepId::ShowLayerInspector):
        return stepPhase_ == 0 ? "Click View." : "Enable Layer Inspector.";
    case static_cast<int>(TutorialStepId::ShowMinimap):
        return stepPhase_ == 0 ? "Click View." : "Enable Minimap.";
    case static_cast<int>(TutorialStepId::ShowReticleUsageHighlights):
        return stepPhase_ == 0 ? "Click View." : "Enable Highlight reticle usages.";
    case static_cast<int>(TutorialStepId::ShowProblemsPanel):
        return stepPhase_ == 0 ? "Click View." : "Enable Problems.";
    case static_cast<int>(TutorialStepId::ToggleFullscreenPreview):
        return stepPhase_ == 0 ? "Click the fullscreen button." : "Click the fullscreen button again to restore the layout.";
    case static_cast<int>(TutorialStepId::SaveTutorialAssets):
        return stepPhase_ == 0 ? "Click File." : "Click Save.";
    case static_cast<int>(TutorialStepId::InspectPageImportWorkflow):
        return stepPhase_ == 0 ? "Click Page." : "Click Import page...";
    case static_cast<int>(TutorialStepId::InspectPageRenameWorkflow):
        return stepPhase_ == 0 ? "Click Page." :
                                 (stepPhase_ == 1 ? "Click Rename current page globally..." : "Close the rename popup.");
    case static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow):
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click Rename selected library reticle globally..." : "Close the rename popup.");
    case static_cast<int>(TutorialStepId::InspectDesignExportWorkflow):
        return stepPhase_ == 0 ? "Click File." :
                                 (stepPhase_ == 1 ? "Open Export." :
                                                    (stepPhase_ == 2 ? "Click Export design..." : "Close the export popup."));
    default:
        return {};
    }
}

void EditorTutorialController::SaveProgress() const
{
    std::error_code error;
    std::filesystem::create_directories(progressFile_.parent_path(), error);
    std::ofstream stream(progressFile_, std::ios::trunc);
    if (!stream.good())
    {
        return;
    }

    stream << stepIndex_ << '\n';
}

void EditorTutorialController::ClearProgress()
{
    std::error_code error;
    std::filesystem::remove(progressFile_, error);
}

void EditorTutorialController::CleanupGeneratedFiles()
{
    const auto projectRoot = FindProjectRoot(std::filesystem::current_path());
    if (!projectRoot.has_value())
    {
        return;
    }

    RemoveTutorialGeneratedSourceFiles(*projectRoot);
    RemoveTutorialBuildOutputs(*projectRoot);
    RemoveTutorialStageArtifacts(*projectRoot);
}

void EditorTutorialController::DrawReferenceDocument()
{
    ClampStepIndex();
    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(stepIndex_)];

    ImGui::TextDisabled("Path");
    ImGui::SameLine();
    ImGui::TextUnformatted(step.filePath);

    ImGui::Spacing();
    ImGui::TextWrapped("%s", step.explanation);
    ImGui::Spacing();
    if (AccentButton(step.advanceLabel))
    {
        CompleteStep();
    }
}
