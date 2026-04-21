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
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
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
constexpr std::array<std::string_view, 2> kTutorialRootTargetRegistrations {{
    "add_subdirectory(examples/mfd_tutorial)",
    "add_subdirectory(examples/client_tutorial)",
}};

struct TutorialBuildContext
{
    std::filesystem::path projectRoot;
    std::filesystem::path buildDirectory;
    std::string configuration;
};

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

std::optional<std::filesystem::path> FindEnclosingBuildDirectory(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        if (std::filesystem::exists(current / "CMakeCache.txt"))
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

std::optional<std::string> DetectBuildConfiguration(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        const std::string component = ToLowerAscii(current.filename().string());
        for (const std::string_view configuration : kBuildConfigurations)
        {
            if (component == ToLowerAscii(std::string(configuration)))
            {
                return std::string(configuration);
            }
        }

        if (current == current.root_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return std::nullopt;
}

std::string DetectBuildPlatformHint(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        const std::string component = ToLowerAscii(current.filename().string());
        if (component == "x64" || component == "win32")
        {
            return component;
        }

        if (current == current.root_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return {};
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

std::optional<TutorialBuildContext> ResolveTutorialBuildContext(const std::filesystem::path& start)
{
    const auto projectRoot = FindProjectRoot(start);
    if (!projectRoot.has_value())
    {
        return std::nullopt;
    }

    TutorialBuildContext context;
    context.projectRoot = *projectRoot;
    context.configuration = DetectBuildConfiguration(start).value_or("Debug");

    if (const auto enclosingBuild = FindEnclosingBuildDirectory(start); enclosingBuild.has_value())
    {
        context.buildDirectory = *enclosingBuild;
        return context;
    }

    const std::vector<std::filesystem::path> buildDirectories = EnumerateConfiguredBuildDirectories(*projectRoot);
    if (buildDirectories.empty())
    {
        return std::nullopt;
    }

    const std::string platformHint = DetectBuildPlatformHint(start);
    int bestScore = std::numeric_limits<int>::min();
    for (const auto& candidate : buildDirectories)
    {
        int score = 0;
        const std::string candidateLower = ToLowerAscii(candidate.string());
        if (!platformHint.empty() && candidateLower.find(platformHint) != std::string::npos)
        {
            score += 4;
        }
        if (candidateLower.find("vs2022") != std::string::npos)
        {
            score += 1;
        }

        if (score > bestScore)
        {
            bestScore = score;
            context.buildDirectory = candidate;
        }
    }

    return context.buildDirectory.empty() ? std::nullopt : std::optional<TutorialBuildContext> {context};
}

std::string QuoteShellArgument(std::string_view value)
{
    std::string quoted;
    quoted.reserve(value.size() + 2U);
    quoted.push_back('"');
    for (const char ch : value)
    {
        if (ch == '"')
        {
            quoted += "\\\"";
        }
        else
        {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('"');
    return quoted;
}

std::string QuoteShellArgument(const std::filesystem::path& value)
{
    const std::string pathString = value.string();
    return QuoteShellArgument(std::string_view {pathString.data(), pathString.size()});
}

void RemovePathQuietly(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

bool IsTutorialTargetRegistrationLine(std::string_view line) noexcept
{
    const std::string_view trimmed = TrimAsciiWhitespace(line);
    return std::find(kTutorialRootTargetRegistrations.begin(),
                     kTutorialRootTargetRegistrations.end(),
                     trimmed) != kTutorialRootTargetRegistrations.end();
}

void RemoveTutorialTargetRegistration(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path cmakeFile = projectRoot / "CMakeLists.txt";
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
    const std::filesystem::path cmakeFile = projectRoot / "CMakeLists.txt";
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

    bool hasMfdTutorial = false;
    bool hasClientTutorial = false;
    std::size_t insertionIndex = std::numeric_limits<std::size_t>::max();
    std::string insertionIndentation;

    for (std::string line; std::getline(stream, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const std::string_view trimmed = TrimAsciiWhitespace(line);
        if (trimmed == kTutorialRootTargetRegistrations[0])
        {
            hasMfdTutorial = true;
        }
        else if (trimmed == kTutorialRootTargetRegistrations[1])
        {
            hasClientTutorial = true;
        }
        else if (trimmed == "add_subdirectory(mfd_editor)" &&
                 insertionIndex == std::numeric_limits<std::size_t>::max())
        {
            insertionIndex = lines.size();
            insertionIndentation = LeadingWhitespace(line);
        }

        lines.push_back(std::move(line));
    }

    if ((hasMfdTutorial && hasClientTutorial) ||
        insertionIndex == std::numeric_limits<std::size_t>::max())
    {
        return;
    }

    std::vector<std::string> insertedLines;
    insertedLines.reserve(2);
    if (!hasMfdTutorial)
    {
        insertedLines.push_back(insertionIndentation + std::string(kTutorialRootTargetRegistrations[0]));
    }
    if (!hasClientTutorial)
    {
        insertedLines.push_back(insertionIndentation + std::string(kTutorialRootTargetRegistrations[1]));
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

std::pair<bool, std::string> BuildTutorialTargets(const TutorialBuildContext& context)
{
    EnsureTutorialTargetRegistration(context.projectRoot);

    std::ostringstream configureCommand;
    configureCommand << "cmake -S " << QuoteShellArgument(context.projectRoot)
                     << " -B " << QuoteShellArgument(context.buildDirectory);

    if (std::system(configureCommand.str().c_str()) != 0)
    {
        return {
            false,
            "Tutorial completed, but CMake reconfiguration failed for '" + context.buildDirectory.string() + "'."};
    }

    std::ostringstream buildCommand;
    buildCommand << "cmake --build " << QuoteShellArgument(context.buildDirectory)
                 << " --config " << context.configuration
                 << " --target mfd_tutorial client_tutorial";

    if (std::system(buildCommand.str().c_str()) != 0)
    {
        return {
            false,
            "Tutorial completed, but building 'mfd_tutorial' and 'client_tutorial' failed in '" +
                context.buildDirectory.string() + "'."};
    }

    return {
        true,
        "Tutorial completed. Built 'mfd_tutorial' and 'client_tutorial' in configuration '" +
            context.configuration + "'."};
}

void RemoveTutorialGeneratedSourceFiles(const std::filesystem::path& projectRoot)
{
    const std::array<std::filesystem::path, 10> generatedFiles {{
        projectRoot / "assets/windows/mfd_tutorial.json",
        projectRoot / "assets/pages/mfd_tutorial_page1.json",
        projectRoot / "assets/pages/mfd_tutorial_page2.json",
        projectRoot / "assets/reticles/mfd_tutorial_radar_track.json",
        projectRoot / "assets/reticles/mfd_tutorial_circle.json",
        projectRoot / "assets/reticles/mfd_tutorial_text.json",
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
        for (const std::string_view targetName : {"mfd_tutorial", "client_tutorial"})
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
        "mfd_tutorial.exe",
        "mfd_tutorial.ilk",
        "mfd_tutorial.pdb",
        "tutorialui.h",
        "tutorialui.cpp",
        "mfdtutorialmockupui.h",
        "mfdtutorialmockupui.cpp",
        "mfd_tutorial.json",
        "mfd_tutorial_page1.json",
        "mfd_tutorial_page2.json",
        "mfd_tutorial_radar_track.json",
        "mfd_tutorial_circle.json",
        "mfd_tutorial_text.json"};

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
    if (buildInProgress_)
    {
        app_.RebuildStatus("Tutorial reset is unavailable while the tutorial targets are still building.", true);
        return;
    }

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
    StartTargetBuild();
}

void EditorTutorialController::PollBuild()
{
    if (!buildInProgress_)
    {
        return;
    }

    using namespace std::chrono_literals;
    if (buildFuture_.wait_for(0ms) != std::future_status::ready)
    {
        return;
    }

    buildInProgress_ = false;
    const TutorialBuildResult result = buildFuture_.get();
    app_.RebuildStatus(result.message, !result.success);
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

void EditorTutorialController::AdvanceStep()
{
    stepIndex_ = std::min(stepIndex_ + 1, editor::tutorial::StepCount() - 1);
    app_.PrepareTutorialStep();
    SaveProgress();
}

void EditorTutorialController::StartTargetBuild()
{
    if (buildInProgress_)
    {
        return;
    }

    const auto buildContext = ResolveTutorialBuildContext(std::filesystem::current_path());
    if (!buildContext.has_value())
    {
        app_.RebuildStatus(
            "Tutorial completed, but no configured CMake build directory was found for automatic tutorial target builds.",
            true);
        return;
    }

    buildInProgress_ = true;
    app_.RebuildStatus(
        "Tutorial completed. Building 'mfd_tutorial' and 'client_tutorial' in '" +
            buildContext->buildDirectory.string() + "'...",
        false);
    buildFuture_ = std::async(std::launch::async, [buildContext]()
                              {
                                  auto [success, message] = BuildTutorialTargets(*buildContext);
                                  return TutorialBuildResult {success, std::move(message)};
                              });
}

std::string_view EditorTutorialController::CurrentTargetId() const noexcept
{
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
    case 0:
        return stepPhase_ == 0 ? "menu_file" : (stepPhase_ == 1 ? "menu_file_new_window" : "popup_window_create");
    case 1:
    case 2:
        return stepPhase_ == 0 ? "menu_reticle" :
                                 (stepPhase_ == 1 ? "menu_reticle_new" : "popup_reticle_create");
    case 3:
    case 7:
        return stepPhase_ == 0 ? "menu_page" : (stepPhase_ == 1 ? "menu_page_new" : "popup_page_create");
    case 4:
        return "library_add_to_page";
    case 5:
        return stepPhase_ == 0 ? "page_preview_clip_source" : "context_clip_outer";
    case 6:
        return stepPhase_ == 0 ? "inspector_add_layer" : "inspector_layer_visibility";
    case 8:
        return stepPhase_ == 0 ? "menu_file" : "menu_file_save";
    default:
        return {};
    }
}

std::string_view EditorTutorialController::CurrentActionLabel() const noexcept
{
    switch (stepIndex_)
    {
    case 0:
        return stepPhase_ == 0 ? "Click File." :
                                 (stepPhase_ == 1 ? "Click New window from scratch." : "Click Create window.");
    case 1:
    case 2:
        return stepPhase_ == 0 ? "Click Reticle." :
                                 (stepPhase_ == 1 ? "Click New library reticle from primitive." : "Click Create reticle.");
    case 3:
    case 7:
        return stepPhase_ == 0 ? "Click Page." :
                                 (stepPhase_ == 1 ? "Click New page." : "Click Create page.");
    case 4:
        return "Click Add to active page.";
    case 5:
        return stepPhase_ == 0 ? "Right-click the circle reticle in the page preview." : "Click Clip outside.";
    case 6:
        return stepPhase_ == 0 ? "Click Add layer." : "Click Visible to hide the new layer.";
    case 8:
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
