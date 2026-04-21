/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private controller that owns the guided tutorial workflow for the editor shell.
 */

#include <filesystem>
#include <future>
#include <string>
#include <string_view>

class EditorApplication;

/**
 * @brief Encapsulates tutorial state, persistence, cleanup and coach UI orchestration.
 *
 * @details `EditorTutorialController` is a private collaborator of
 * `EditorApplication`. It keeps the tutorial-specific lifecycle out of the main
 * editor object while still being allowed to drive internal editor actions such
 * as draft preparation, document reloads and status updates.
 */
class EditorTutorialController
{
public:
    /**
     * @brief Binds the controller to one editor shell.
     * @param application Editor shell hosting the tutorial workflow.
     */
    explicit EditorTutorialController(EditorApplication& application);

    /** @brief Loads persisted tutorial progress from disk. */
    void LoadProgress();
    /** @brief Opens the tutorial flow from the Help menu. */
    void OpenFlow();
    /** @brief Resumes the tutorial from the progress already loaded on disk. */
    void ResumeFromSavedProgress();
    /** @brief Restarts the tutorial and cleans generated tutorial files. */
    void RestartFromScratch();
    /** @brief Marks the current tutorial step as completed and advances the flow. */
    void CompleteStep();
    /** @brief Ends the tutorial and clears persisted progress. */
    void Finish();
    /** @brief Polls the asynchronous tutorial build and refreshes the footer status once it completes. */
    void PollBuild();
    /** @brief Draws the guided tutorial coach panel. */
    void DrawCoach();
    /** @brief Draws a persistent callout and halo around the current tutorial target. */
    void DrawHalo(const char* targetId, const char* title, const char* reason) const;

    /**
     * @brief Returns `true` when the current tutorial target matches the provided id.
     * @param targetId ImGui target identifier to compare with the current step.
     */
    bool MatchesTarget(std::string_view targetId) const noexcept;

    /** @brief Returns `true` when the tutorial coach panel is currently visible. */
    bool IsCoachVisible() const noexcept;
    /** @brief Returns `true` when the guided tutorial is active on the requested step. */
    bool IsStep(int stepIndex) const noexcept;
    /** @brief Returns `true` when the guided tutorial is active on the requested step phase. */
    bool IsStepPhase(int stepIndex, int stepPhase) const noexcept;
    /** @brief Returns the current tutorial step index. */
    int StepIndex() const noexcept;
    /** @brief Advances the current micro-step inside the active tutorial step. */
    void AdvancePhase() noexcept;
    /** @brief Resets the current micro-step to its initial value. */
    void ResetPhase() noexcept;
    /** @brief Clamps the stored step index inside the valid tutorial range. */
    void ClampStepIndex() noexcept;
    /** @brief Resets the shared file-review zoom and scroll values. */
    void ResetFileReviewView() noexcept;
    /** @brief Clears the tracked reticle id used by multi-step tutorial actions. */
    void ClearTrackedReticle() noexcept;
    /** @brief Updates the tracked reticle id used by multi-step tutorial actions. */
    void SetTrackedReticleId(std::string trackedReticleId);
    /** @brief Returns the tracked reticle id used by multi-step tutorial actions. */
    std::string_view TrackedReticleId() const noexcept;
    /** @brief Clears the highlighted editor-layer id used by the tutorial. */
    void ClearFocusLayer() noexcept;
    /** @brief Updates the highlighted editor-layer id used by the tutorial. */
    void SetFocusLayerId(std::string focusLayerId);
    /** @brief Returns the highlighted editor-layer id used by the tutorial. */
    std::string_view FocusLayerId() const noexcept;
    /** @brief Consumes the one-shot request that opens the resume/restart popup. */
    bool ConsumeResumePopupRequest() noexcept;

private:
    /** @brief Result payload returned by the asynchronous tutorial build worker. */
    struct TutorialBuildResult
    {
        bool success = false;
        std::string message {};
    };

    /** @brief Advances to the next tutorial step and persists progress. */
    void AdvanceStep();
    /** @brief Starts the asynchronous build of the tutorial runtime and client targets. */
    void StartTargetBuild();
    /** @brief Returns the currently expected tutorial target id for UI-driven steps. */
    std::string_view CurrentTargetId() const noexcept;
    /** @brief Returns a short summary of the current tutorial action. */
    std::string_view CurrentActionLabel() const noexcept;
    /** @brief Saves persisted tutorial progress to disk. */
    void SaveProgress() const;
    /** @brief Clears persisted tutorial progress from disk. */
    void ClearProgress();
    /** @brief Cleans generated tutorial files plus root-project tutorial registrations. */
    void CleanupGeneratedFiles();
    /** @brief Draws the synchronized file review UI used by tutorial file steps. */
    void DrawFileReview();

    /** @brief Editor shell hosting the tutorial workflow. */
    EditorApplication& app_;
    /** @brief Popup visibility flag for tutorial resume/restart choice. */
    bool showResumePopup_ = false;
    /** @brief Indicates whether the tutorial coach panel is visible. */
    bool showCoach_ = false;
    /** @brief Indicates whether guided tutorial mode is active. */
    bool active_ = false;
    /** @brief Index of the current tutorial step. */
    int stepIndex_ = 0;
    /** @brief Micro-step index inside the current tutorial step. */
    int stepPhase_ = 0;
    /** @brief Small progress file used to resume tutorial state across launches. */
    std::filesystem::path progressFile_ {"assets/tutorial/.editor_tutorial_progress"};
    /** @brief Reticle instance created by the tutorial and reused by later guidance. */
    std::string trackedReticleId_ {};
    /** @brief Editor layer currently highlighted by the tutorial, when applicable. */
    std::string focusLayerId_ {};
    /** @brief Shared zoom used by both tutorial file panes. */
    float fileViewZoom_ = 1.0f;
    /** @brief Shared horizontal scroll used by both tutorial file panes. */
    float fileViewScrollX_ = 0.0f;
    /** @brief Shared vertical scroll used by both tutorial file panes. */
    float fileViewScrollY_ = 0.0f;
    /** @brief Background future used to build the tutorial targets after the walkthrough finishes. */
    std::future<TutorialBuildResult> buildFuture_ {};
    /** @brief Indicates whether the tutorial target build is currently running. */
    bool buildInProgress_ = false;
};
