/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Service interfaces and facade used by hidden editor automation plugins and tests.
 */

#include <memory>
#include <vector>

#include "EditorAutomationTypes.h"

namespace editor::automation
{
class IEditorAutomationEditorBridge;

/**
 * @brief Read-only query service exposing the current editor state to automation plugins.
 */
class IEditorAutomationQueryService
{
public:
    virtual ~IEditorAutomationQueryService() = default;

    /** @brief Builds one full automation-safe snapshot of the current editor state. */
    [[nodiscard]] virtual AutomationResult<DocumentSnapshot> GetSnapshot() const = 0;
};

/**
 * @brief Authoring service applying one semantic automation action to the live editor model.
 */
class IEditorAutomationAuthoringService
{
public:
    virtual ~IEditorAutomationAuthoringService() = default;

    /**
     * @brief Applies one semantic authoring action to the live editor model.
     * @param action Action to execute.
     * @return Structured status describing success or failure.
     */
    virtual AutomationStatus ApplyAction(const AutomationAction& action) = 0;
};

/**
 * @brief Persistence service delegating validation, JSON preview export and save operations to the editor.
 */
class IEditorAutomationPersistenceService
{
public:
    virtual ~IEditorAutomationPersistenceService() = default;

    /** @brief Validates the current live editor state. */
    [[nodiscard]] virtual AutomationResult<ValidationReport> ValidateCurrentState() const = 0;
    /** @brief Saves every dirty asset through the editor-owned serializer. */
    [[nodiscard]] virtual AutomationResult<SaveResult> SaveAll() = 0;
    /** @brief Saves one specific page or reticle asset when that granularity is supported. */
    [[nodiscard]] virtual AutomationResult<SaveResult> SaveAsset(const SaveAssetRequest& request) = 0;
    /** @brief Exports one JSON preview for one automation-visible entity through editor-owned serializers. */
    [[nodiscard]] virtual AutomationResult<ExportJsonPreviewResult> ExportJsonPreview(
        const ExportJsonPreviewRequest& request) const = 0;
};

/**
 * @brief Session service driving preview, validation, commit and rollback for automation-originated edits.
 */
class IEditorAutomationSessionService
{
public:
    virtual ~IEditorAutomationSessionService() = default;

    /** @brief Opens one new preview session when no other session is currently active. */
    [[nodiscard]] virtual AutomationResult<AutomationSessionId> BeginSession(const BeginSessionRequest& request) = 0;
    /** @brief Applies one semantic action to one active preview session. */
    virtual AutomationStatus ApplyAction(const ApplyActionRequest& request) = 0;
    /** @brief Validates one active preview session. */
    [[nodiscard]] virtual AutomationResult<ValidationReport> ValidateSession(const ValidateSessionRequest& request) const = 0;
    /** @brief Commits one active preview session and records one single undo step. */
    virtual AutomationStatus CommitSession(const CommitSessionRequest& request) = 0;
    /** @brief Rolls back one active preview session to its captured pre-session snapshot. */
    virtual AutomationStatus RollbackSession(const RollbackSessionRequest& request) = 0;
    /** @brief Returns the currently active preview session when one exists. */
    [[nodiscard]] virtual std::optional<AutomationSessionId> ActiveSessionId() const = 0;
};

/**
 * @brief Event service collecting structured automation events for automation plugins.
 */
class IEditorAutomationEventService
{
public:
    virtual ~IEditorAutomationEventService() = default;

    /** @brief Returns and clears every queued automation event. */
    [[nodiscard]] virtual std::vector<AutomationEvent> ConsumePendingEvents() = 0;
};

/**
 * @brief Unified facade consumed by future in-process automation plugins and the future optional GUI.
 */
class IEditorAutomationFacade
{
public:
    virtual ~IEditorAutomationFacade() = default;

    /** @brief Returns the read-only automation query service. */
    [[nodiscard]] virtual IEditorAutomationQueryService& QueryService() noexcept = 0;
    /** @brief Returns the authoring automation service. */
    [[nodiscard]] virtual IEditorAutomationAuthoringService& AuthoringService() noexcept = 0;
    /** @brief Returns the persistence automation service. */
    [[nodiscard]] virtual IEditorAutomationPersistenceService& PersistenceService() noexcept = 0;
    /** @brief Returns the session automation service. */
    [[nodiscard]] virtual IEditorAutomationSessionService& SessionService() noexcept = 0;
    /** @brief Returns the event automation service. */
    [[nodiscard]] virtual IEditorAutomationEventService& EventService() noexcept = 0;
};

/**
 * @brief Builds the automation facade bound to one live editor bridge.
 * @param bridge Editor bridge exposing the live editor state.
 * @return Ready-to-use automation facade.
 */
std::unique_ptr<IEditorAutomationFacade> CreateEditorAutomationFacade(IEditorAutomationEditorBridge& bridge);
} // namespace editor::automation
