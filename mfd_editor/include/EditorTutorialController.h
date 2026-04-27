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
#include <string>
#include <string_view>

class EditorApplication;

namespace mfd
{
enum class PrimitiveType;
struct PageDefinition;
struct Primitive;
struct ReticleGroup;
}

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
    /** @brief Returns `true` when closing the reticle menu must reset the guided phase. */
    bool ShouldResetReticleMenuPhaseOnClose() const noexcept;
    /** @brief Returns `true` when closing the reticle-create popup must reset the guided phase. */
    bool ShouldResetReticleCreatePopupOnCancel() const noexcept;
    /** @brief Returns `true` when creating a reticle should advance to an append-primitive micro-step. */
    bool ShouldAdvanceReticleCreatePhase() const noexcept;
    /** @brief Validates the current reticle-create draft against the active tutorial step. */
    bool ValidateNewLibraryReticleDraft(std::string_view reticleId,
                                        mfd::PrimitiveType primitiveType,
                                        std::string& error) const;
    /** @brief Applies tutorial-specific defaults to a freshly created library reticle when required. */
    void ConfigureCreatedLibraryReticle(mfd::ReticleGroup& reticle) const;
    /** @brief Returns `true` when the current tutorial step forbids drag-and-drop instantiation. */
    bool ShouldUseHighlightedAddToPageButton() const noexcept;
    /** @brief Validates the "add to active page" action against the current tutorial context. */
    bool ValidateAddToPage(const mfd::PageDefinition* page,
                           const mfd::ReticleGroup& reticle,
                           std::string& error) const;
    /** @brief Returns the contextual halo reason displayed on the add-to-page button. */
    std::string_view LibraryAddToPageHaloReason() const noexcept;
    /** @brief Validates the primitive append action against the active tutorial step. */
    bool ValidateAppendPrimitive(const mfd::ReticleGroup& reticle,
                                 mfd::PrimitiveType primitiveType,
                                 std::string& error) const;
    /** @brief Applies tutorial-specific defaults to a freshly appended primitive when required. */
    void ConfigureAppendedPrimitive(mfd::Primitive& primitive) const;
    /** @brief Returns the contextual halo reason displayed on the append-primitive button. */
    std::string_view LibraryAppendPrimitiveHaloReason() const noexcept;
    /** @brief Returns `true` when the selected primitive matches the guided exposure step. */
    bool IsExposedPrimitiveTutorialSelection(std::string_view reticleId, std::string_view primitiveId) const noexcept;

private:
    /** @brief Advances to the next tutorial step and persists progress. */
    void AdvanceStep();
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
};
