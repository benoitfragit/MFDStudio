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
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
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
constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";
constexpr std::string_view kTutorialProgressBarTemplateId = "mfd_tutorial_progress_bar";
constexpr std::string_view kTutorialProgressBarFillPrimitiveId = "fill_bar";
constexpr std::string_view kTutorialProgressBarFramePrimitiveId = "frame";
constexpr std::array<std::string_view, 1> kTutorialExampleTargetRegistrations {{
    "add_subdirectory(client_tutorial)",
}};

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

std::string_view TrimAsciiWhitespace(std::string_view value) noexcept
{
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}

std::string LeadingWhitespace(std::string_view value)
{
    std::size_t count = 0;
    while (count < value.size() &&
           (value[count] == ' ' || value[count] == '\t'))
    {
        ++count;
    }

    return std::string(value.substr(0, count));
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

bool IsTutorialTargetRegistrationLine(std::string_view line) noexcept
{
    const std::string_view trimmed = TrimAsciiWhitespace(line);
    return std::find(kTutorialExampleTargetRegistrations.begin(),
                     kTutorialExampleTargetRegistrations.end(),
                     trimmed) != kTutorialExampleTargetRegistrations.end();
}

void RemoveTutorialTargetRegistration(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path cmakeFile = projectRoot / "examples" / "CMakeLists.txt";
    std::ifstream input(cmakeFile, std::ios::binary);
    if (!input.is_open())
    {
        return;
    }

    const std::string content {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (content.empty())
    {
        return;
    }

    const std::string newline = content.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    const bool hasTrailingNewline =
        content.size() >= newline.size() &&
        content.compare(content.size() - newline.size(), newline.size(), newline) == 0;

    std::istringstream stream(content);
    std::vector<std::string> keptLines;
    keptLines.reserve(64);

    bool removedLine = false;
    for (std::string line; std::getline(stream, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (IsTutorialTargetRegistrationLine(line))
        {
            removedLine = true;
            continue;
        }

        keptLines.push_back(std::move(line));
    }

    if (!removedLine)
    {
        return;
    }

    std::ofstream output(cmakeFile, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }

    for (std::size_t index = 0; index < keptLines.size(); ++index)
    {
        if (index != 0)
        {
            output << newline;
        }
        output << keptLines[index];
    }

    if (hasTrailingNewline && !keptLines.empty())
    {
        output << newline;
    }
}

void EnsureTutorialTargetRegistration(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path cmakeFile = projectRoot / "examples" / "CMakeLists.txt";
    std::ifstream input(cmakeFile, std::ios::binary);
    if (!input.is_open())
    {
        return;
    }

    const std::string content {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (content.empty())
    {
        return;
    }

    const std::string newline = content.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    const bool hasTrailingNewline =
        content.size() >= newline.size() &&
        content.compare(content.size() - newline.size(), newline.size(), newline) == 0;

    std::istringstream stream(content);
    std::vector<std::string> lines;
    lines.reserve(64);

    std::array<bool, kTutorialExampleTargetRegistrations.size()> hasTargetRegistrations {};
    std::size_t insertionIndex = std::numeric_limits<std::size_t>::max();
    std::string insertionIndentation;
    std::size_t appendIndex = std::numeric_limits<std::size_t>::max();

    for (std::string line; std::getline(stream, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const std::string_view trimmed = TrimAsciiWhitespace(line);
        for (std::size_t index = 0; index < kTutorialExampleTargetRegistrations.size(); ++index)
        {
            if (trimmed == kTutorialExampleTargetRegistrations[index])
            {
                hasTargetRegistrations[index] = true;
                break;
            }
        }

        if (trimmed.rfind("add_subdirectory(", 0) == 0)
        {
            appendIndex = lines.size() + 1U;
            if (insertionIndentation.empty())
            {
                insertionIndentation = LeadingWhitespace(line);
            }
        }

        if (trimmed.rfind("# client_tutorial is intentionally wired", 0) == 0 &&
            insertionIndex == std::numeric_limits<std::size_t>::max())
        {
            insertionIndex = lines.size();
            insertionIndentation = LeadingWhitespace(line);
        }

        lines.push_back(std::move(line));
    }

    if (std::all_of(hasTargetRegistrations.begin(), hasTargetRegistrations.end(), [](const bool present)
                    { return present; }) ||
        insertionIndex == std::numeric_limits<std::size_t>::max())
    {
        if (std::all_of(hasTargetRegistrations.begin(), hasTargetRegistrations.end(), [](const bool present)
                        { return present; }))
        {
            return;
        }

        insertionIndex = appendIndex;
        if (insertionIndex == std::numeric_limits<std::size_t>::max())
        {
            return;
        }
    }

    std::vector<std::string> insertedLines;
    insertedLines.reserve(kTutorialExampleTargetRegistrations.size());
    for (std::size_t index = 0; index < kTutorialExampleTargetRegistrations.size(); ++index)
    {
        if (!hasTargetRegistrations[index])
        {
            insertedLines.push_back(insertionIndentation + std::string(kTutorialExampleTargetRegistrations[index]));
        }
    }

    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertionIndex),
                 insertedLines.begin(),
                 insertedLines.end());

    std::ofstream output(cmakeFile, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }

    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        if (index != 0)
        {
            output << newline;
        }
        output << lines[index];
    }

    if (hasTrailingNewline && !lines.empty())
    {
        output << newline;
    }
}

void RemoveTutorialGeneratedSourceFiles(const std::filesystem::path& projectRoot)
{
    const std::array<std::filesystem::path, 14> generatedFiles {{
        projectRoot / "assets/windows/mfd_tutorial.json",
        projectRoot / "assets/windows/mfd_tutorial.generated.map",
        projectRoot / "assets/pages/mfd_tutorial_page1.json",
        projectRoot / "assets/pages/mfd_tutorial_page2.json",
        projectRoot / "assets/pages/mfd_tutor.json",
        projectRoot / "assets/reticles/mfd_tutorial_radar_track.json",
        projectRoot / "assets/reticles/mfd_tutorial_circle.json",
        projectRoot / "assets/reticles/mfd_tutorial_progress_bar.json",
        projectRoot / "assets/reticles/mfd_tutorial_text.json",
        projectRoot / "assets/reticles/mfd_tutorial_strobe_cursor.json",
        projectRoot / "examples/client_tutorial/generated/TutorialUi.h",
        projectRoot / "examples/client_tutorial/generated/TutorialUi.cpp",
        projectRoot / "examples/client_tutorial/generated/MfdTutorialMockupUi.h",
        projectRoot / "examples/client_tutorial/generated/MfdTutorialMockupUi.cpp",
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

        RemovePathQuietly(buildDirectory / "examples/client_tutorial/generated");
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

template <typename Callback>
void ForEachTutorialLine(std::string_view text, Callback&& callback)
{
    std::size_t lineStart = 0;
    int lineIndex = 0;

    while (lineStart <= text.size())
    {
        const std::size_t lineEnd = text.find('\n', lineStart);
        const std::size_t nextStart = lineEnd == std::string_view::npos ? text.size() : lineEnd;
        callback(lineIndex++, text.substr(lineStart, nextStart - lineStart));
        if (lineEnd == std::string_view::npos)
        {
            break;
        }

        lineStart = lineEnd + 1U;
    }
}

int CountTutorialLines(std::string_view text)
{
    int lineCount = 0;
    ForEachTutorialLine(
        text,
        [&lineCount](const int, std::string_view)
        {
            ++lineCount;
        });
    return std::max(1, lineCount);
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
    if (const auto projectRoot = FindProjectRoot(std::filesystem::current_path()); projectRoot.has_value())
    {
        EnsureTutorialTargetRegistration(*projectRoot);
    }

    app_.RebuildStatus(
        "Tutorial completed. Reconfigure/rebuild the solution manually to generate and compile 'client_tutorial', then launch the authored window with 'Scripts/Start-MfdTutorial.bat' from the repository or the staged 'Start-MfdTutorial.bat' next to mfd_window.",
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
    const ImVec2 defaultSize =
        editor::tutorial::IsFileReviewStep(step) ? ImVec2(1100.0f, 780.0f) : ImVec2(640.0f, 360.0f);
    ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Tutorial coach", &showCoach_))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Step %d / %d", stepIndex_ + 1, editor::tutorial::StepCount());
    ImGui::Separator();
    ImGui::TextUnformatted(step.title);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", step.instruction);
    ImGui::Spacing();

    if (editor::tutorial::IsFileReviewStep(step))
    {
        DrawFileReview();
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
    if (ImGui::Button("Mark complete"))
    {
        Finish();
    }
    ImGui::End();
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

void EditorTutorialController::ResetFileReviewView() noexcept
{
    fileViewZoom_ = 1.0f;
    fileViewScrollX_ = 0.0f;
    fileViewScrollY_ = 0.0f;
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

bool EditorTutorialController::ShouldResetReticleMenuPhaseOnClose() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStepPhase(static_cast<int>(TutorialStepId::CreateRadarTrackReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateCircleReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle), 1) ||
           IsStepPhase(static_cast<int>(TutorialStepId::CreateProgressBarReticle), 1);
}

bool EditorTutorialController::ShouldResetReticleCreatePopupOnCancel() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::CreateRadarTrackReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateCircleReticle)) ||
           IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)) ||
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
}

bool EditorTutorialController::ShouldUseHighlightedAddToPageButton() const noexcept
{
    using editor::tutorial::TutorialStepId;

    return IsStep(static_cast<int>(TutorialStepId::AddCircleReticleToPage1)) ||
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
    if (editor::tutorial::IsFileReviewStep(step))
    {
        return {};
    }

    switch (stepIndex_)
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        return stepPhase_ == 0 ? "menu_file" : (stepPhase_ == 1 ? "menu_file_new_window" : "popup_window_create");
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" : "popup_reticle_create");
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" :
                                                    (stepPhase_ == 2 ? "popup_reticle_create" : "library_append_primitive"));
    case static_cast<int>(TutorialStepId::CreatePage1):
    case static_cast<int>(TutorialStepId::CreatePage2):
        return stepPhase_ == 0 ? "menu_page" : (stepPhase_ == 1 ? "menu_page_new" : "popup_page_create");
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" :
                                                    (stepPhase_ == 2 ? "popup_reticle_create" : "library_append_primitive"));
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        return "primitive_exposed_checkbox";
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        return "library_add_to_page";
    case static_cast<int>(TutorialStepId::AssignPage1StrobeTemplate):
        return "page_strobe_template";
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
        return "library_add_to_page";
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
        return stepPhase_ == 0 ? "page_preview_clip_source" : "context_clip_outer";
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        return stepPhase_ == 0 ? "inspector_add_layer" : "inspector_layer_visibility";
    case static_cast<int>(TutorialStepId::SaveTutorialAssets):
        return stepPhase_ == 0 ? "menu_file" : "menu_file_save";
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
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click New library reticle from primitive." :
                                                    (stepPhase_ == 2 ? "Click Create reticle." : "Click Append primitive."));
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        return "Enable Exposed on the fill_bar primitive.";
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        return "Click Add to active page.";
    case static_cast<int>(TutorialStepId::AssignPage1StrobeTemplate):
        return "Choose mfd_tutorial_strobe_cursor in Strobe template.";
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
        return "Click Add to active page.";
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
        return stepPhase_ == 0 ? "Right-click the circle reticle in the page preview." : "Click Clip outside.";
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        return stepPhase_ == 0 ? "Click Add layer." : "Click Visible to hide the new layer.";
    case static_cast<int>(TutorialStepId::SaveTutorialAssets):
        return stepPhase_ == 0 ? "Click File." : "Click Save.";
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
    RemoveTutorialTargetRegistration(*projectRoot);
}

void EditorTutorialController::DrawFileReview()
{
    ClampStepIndex();
    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(stepIndex_)];

    struct TutorialPaneResult
    {
        bool hovered = false;
        float scrollX = 0.0f;
        float scrollY = 0.0f;
    };

    auto drawPane = [&](const char* childId,
                        const char* label,
                        const ImVec4 color,
                        const char* content,
                        const int firstLine,
                        const ImVec2 size) -> TutorialPaneResult
    {
        ImGui::TextColored(color, "%s", label);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.08f, 0.11f, 1.0f));
        ImGui::BeginChild(childId, size, true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::SetWindowFontScale(fileViewZoom_);
        ImGui::SetScrollX(fileViewScrollX_);
        ImGui::SetScrollY(fileViewScrollY_);

        const std::string_view contentView = content == nullptr ? std::string_view {} : std::string_view(content);
        const int lineCount = CountTutorialLines(contentView);
        const int lastLine = std::max(firstLine, firstLine + lineCount - 1);
        char numberWidthBuffer[32] {};
        std::snprintf(numberWidthBuffer, sizeof(numberWidthBuffer), "%d", lastLine);
        const float lineNumberWidth = ImGui::CalcTextSize(numberWidthBuffer).x + 20.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 2.0f));
        ForEachTutorialLine(
            contentView,
            [&](const int lineOffset, const std::string_view line)
            {
                const int lineNumber = firstLine + lineOffset;
                char numberBuffer[32] {};
                std::snprintf(numberBuffer, sizeof(numberBuffer), "%d", lineNumber);
                ImGui::TextDisabled("%s", numberBuffer);
                ImGui::SameLine(lineNumberWidth);
                if (line.empty())
                {
                    ImGui::TextUnformatted("");
                }
                else
                {
                    ImGui::TextUnformatted(line.data(), line.data() + line.size());
                }
            });
        ImGui::PopStyleVar();

        TutorialPaneResult result {};
        result.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        result.scrollX = ImGui::GetScrollX();
        result.scrollY = ImGui::GetScrollY();

        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return result;
    };

    ImGui::TextDisabled("Path");
    ImGui::SameLine();
    ImGui::TextUnformatted(step.filePath);
    ImGui::SameLine();
    if (ImGui::Button("A-"))
    {
        fileViewZoom_ = std::max(0.75f, fileViewZoom_ - 0.10f);
    }
    ImGui::SameLine();
    if (ImGui::Button("A+"))
    {
        fileViewZoom_ = std::min(2.25f, fileViewZoom_ + 0.10f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset view"))
    {
        ResetFileReviewView();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Zoom %.0f%%", fileViewZoom_ * 100.0f);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float paneHeight = std::max(260.0f, available.y - 110.0f);

    TutorialPaneResult beforePane {};
    TutorialPaneResult afterPane {};
    if (ImGui::BeginTable(
            "##tutorial_file_review",
            2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        beforePane = drawPane(
            "##tutorial_before",
            "Before",
            ImVec4(0.95f, 0.68f, 0.28f, 1.0f),
            step.beforeText,
            std::max(1, step.beforeFirstLine),
            ImVec2(0.0f, paneHeight));

        ImGui::TableSetColumnIndex(1);
        afterPane = drawPane(
            "##tutorial_after",
            "After",
            ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
            step.afterText,
            std::max(1, step.afterFirstLine),
            ImVec2(0.0f, paneHeight));
        ImGui::EndTable();
    }

    if (beforePane.hovered)
    {
        fileViewScrollX_ = beforePane.scrollX;
        fileViewScrollY_ = beforePane.scrollY;
    }
    else if (afterPane.hovered)
    {
        fileViewScrollX_ = afterPane.scrollX;
        fileViewScrollY_ = afterPane.scrollY;
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", step.explanation);
    ImGui::Spacing();
    if (AccentButton(step.advanceLabel))
    {
        CompleteStep();
    }
}
