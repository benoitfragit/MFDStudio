/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal runtime-debug state model used by `mfd_window`.
 */

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/model/Reticle.h"

namespace mfd::window::debug
{
/**
 * @brief Runtime classification of one reticle exposed by the debug overlay.
 */
enum class ReticleKind
{
    Static,
    Dynamic,
    Strobe
};

/**
 * @brief Stable key identifying one reticle inside one page.
 */
struct ReticleKey
{
    /** @brief Owning page name. */
    std::string pageName;
    /** @brief Reticle id unique within the page. */
    std::string reticleId;
    /** @brief Runtime kind used to distinguish authored, dynamic and strobe reticles. */
    ReticleKind kind = ReticleKind::Static;
};

/**
 * @brief Hash helper used by unordered debug indexes keyed by `ReticleKey`.
 */
struct ReticleKeyHash
{
    /**
     * @brief Returns one stable hash for a reticle key.
     * @param key Reticle key to hash.
     * @return Hash value suitable for unordered containers.
     */
    std::size_t operator()(const ReticleKey& key) const noexcept;
};

/**
 * @brief Key identifying one dynamic reticle template visibility override.
 */
struct DynamicTemplateKey
{
    /** @brief Owning page name. */
    std::string pageName;
    /** @brief Template id owning the dynamic-reticle set. */
    std::string templateId;
};

/**
 * @brief Hash helper used by unordered template-visibility indexes.
 */
struct DynamicTemplateKeyHash
{
    /**
     * @brief Returns one stable hash for a dynamic-template key.
     * @param key Dynamic-template key to hash.
     * @return Hash value suitable for unordered containers.
     */
    std::size_t operator()(const DynamicTemplateKey& key) const noexcept;
};

/**
 * @brief Locally bypassed reticle draft stored while the debug overlay owns one reticle.
 */
struct ReticleBypassState
{
    /** @brief Stable identity of the bypassed reticle. */
    ReticleKey key;
    /** @brief Full desired reticle state rendered by the preview scene. */
    ReticleGroup draft;
};

/**
 * @brief Locally bypassed active-strobe selection owned by the debug overlay.
 */
struct StrobeBypassState
{
    /** @brief Owning page name. */
    std::string pageName;
    /** @brief User-facing strobe name forced locally on the preview scene. */
    std::string strobeName;
};

/**
 * @brief Transport telemetry displayed by the runtime debug overlay.
 */
struct TransportState
{
    /** @brief Indicates whether a UDP command receiver is configured. */
    bool commandConfigured = false;
    /** @brief Indicates whether the UDP command receiver is ready. */
    bool commandReady = false;
    /** @brief Indicates whether a UDP feedback sender is configured. */
    bool feedbackConfigured = false;
    /** @brief Indicates whether the UDP feedback sender is ready. */
    bool feedbackReady = false;
    /** @brief Indicates whether at least one command batch has been observed. */
    bool observedCommandTraffic = false;
    /** @brief Total number of observed command batches since the current window load. */
    std::size_t observedBatchCount = 0;
    /** @brief Total number of observed commands since the current window load. */
    std::size_t observedCommandCount = 0;
    /** @brief Latest thread-safe UDP runtime metrics snapshot. */
    UdpRuntimeBridgeMetrics metrics {};
    /** @brief Last command-side status surfaced by the runtime. */
    std::string commandStatus;
    /** @brief Last feedback-side status surfaced by the runtime. */
    std::string feedbackStatus;
};

/**
 * @brief Testable state model backing the integrated runtime debug overlay.
 *
 * @note This class intentionally keeps all mutation rules away from the ImGui
 * view so the behavior can be unit tested without a live window.
 */
class RuntimeDebugState
{
public:
    /** @brief Monotonic clock used by the transport telemetry. */
    using Clock = std::chrono::steady_clock;

    /**
     * @brief Enables the runtime debug mode.
     */
    void Activate();

    /**
     * @brief Disables the runtime debug mode and clears every local override.
     */
    void Deactivate();

    /**
     * @brief Returns whether the runtime debug mode is currently active.
     * @return `true` when the overlay owns the rendered preview scene.
     */
    [[nodiscard]] bool Active() const noexcept;

    /**
     * @brief Resets every interactive override while keeping the transport telemetry.
     *
     * @note This helper is used when the window JSON is reloaded or when the
     * overlay is closed.
     */
    void ResetInteractiveState();

    /**
     * @brief Clears every state derived from previously observed UDP commands.
     */
    void ResetObservedRuntimeState();

    /**
     * @brief Updates the selected page shown by the overlay tree.
     * @param pageName Page currently selected by the user.
     */
    void SelectPage(std::string pageName);

    /**
     * @brief Returns the selected page name.
     * @return Selected page name, or an empty string when nothing is selected.
     */
    [[nodiscard]] const std::string& SelectedPage() const noexcept;

    /**
     * @brief Updates the selected reticle shown by the inspector.
     * @param key Selected reticle key.
     */
    void SelectReticle(ReticleKey key);

    /**
     * @brief Clears the current reticle selection.
     */
    void ClearReticleSelection();

    /**
     * @brief Returns the selected reticle when one exists.
     * @return Optional reticle key.
     */
    [[nodiscard]] const std::optional<ReticleKey>& SelectedReticle() const noexcept;

    /**
     * @brief Forces the preview scene to keep one authored page active.
     * @param pageName Page name selected by the user.
     */
    void EnablePageBypass(std::string pageName);

    /**
     * @brief Clears the forced active-page override.
     */
    void DisablePageBypass();

    /**
     * @brief Returns whether the active page is currently bypassed.
     * @return `true` when the preview scene ignores UDP page activation.
     */
    [[nodiscard]] bool PageBypassed() const noexcept;

    /**
     * @brief Returns the page forced by the debug overlay.
     * @return Forced page name, or an empty string when page bypass is disabled.
     */
    [[nodiscard]] const std::string& ForcedActivePage() const noexcept;

    /**
     * @brief Returns whether one reticle is currently bypassed by the overlay.
     * @param key Reticle to inspect.
     * @return `true` when one local draft exists for that reticle.
     */
    [[nodiscard]] bool ReticleBypassed(const ReticleKey& key) const noexcept;

    /**
     * @brief Returns whether one page currently bypasses its active strobe selection.
     * @param pageName Page to inspect.
     * @return `true` when the preview scene ignores live strobe-selection updates on that page.
     */
    [[nodiscard]] bool StrobeBypassed(std::string_view pageName) const noexcept;

    /**
     * @brief Returns the locally forced strobe name of one page when it exists.
     * @param pageName Page to inspect.
     * @return Pointer to the forced strobe name, or `nullptr` when the page follows live selection.
     */
    [[nodiscard]] const std::string* ForcedActiveStrobe(std::string_view pageName) const noexcept;

    /**
     * @brief Forces the preview scene to use one named strobe on a page.
     * @param pageName Page owning the selected strobe.
     * @param strobeName User-facing strobe name to force locally.
     */
    void EnableStrobeBypass(std::string pageName, std::string strobeName);

    /**
     * @brief Clears the forced strobe-selection override of one page.
     * @param pageName Page to release.
     */
    void DisableStrobeBypass(std::string_view pageName);

    /**
     * @brief Clears every forced strobe-selection override.
     */
    void ReleaseAllStrobeBypasses();

    /**
     * @brief Returns every page-level strobe-selection bypass currently owned by the overlay.
     * @return Flat list of page/strobe overrides.
     */
    [[nodiscard]] std::vector<StrobeBypassState> BypassedStrobes() const;

    /**
     * @brief Returns the current bypass draft of one reticle when it exists.
     * @param key Reticle to inspect.
     * @return Pointer to the bypass state, or `nullptr` when no bypass exists.
     */
    [[nodiscard]] const ReticleBypassState* FindReticleBypass(const ReticleKey& key) const noexcept;

    /**
     * @brief Returns mutable access to the bypass draft of one reticle.
     * @param key Reticle to inspect.
     * @return Pointer to the bypass state, or `nullptr` when no bypass exists.
     */
    [[nodiscard]] ReticleBypassState* FindReticleBypass(const ReticleKey& key) noexcept;

    /**
     * @brief Creates or returns one reticle bypass draft.
     * @param key Reticle to bypass.
     * @param seed Current runtime reticle state copied into the draft on first use.
     * @return Mutable bypass state.
     */
    ReticleBypassState& EnsureReticleBypass(ReticleKey key, const ReticleGroup& seed);

    /**
     * @brief Releases one reticle bypass and hands control back to the UDP scene.
     * @param key Reticle to release.
     */
    void ReleaseReticleBypass(const ReticleKey& key);

    /**
     * @brief Releases every reticle bypass currently owned by the overlay.
     */
    void ReleaseAllReticleBypasses();

    /**
     * @brief Returns every bypassed reticle key.
     * @return Flat list of currently bypassed reticles.
     */
    [[nodiscard]] std::vector<ReticleKey> BypassedReticles() const;

    /**
     * @brief Returns whether the overlay currently owns any local runtime override.
     * @return `true` when one page or reticle bypass is active.
     */
    [[nodiscard]] bool HasInteractiveOverrides() const noexcept;

    /**
     * @brief Stores the last known visibility of one dynamic-reticle template set.
     * @param pageName Owning page name.
     * @param templateId Template id identifying the set.
     * @param visible Visibility currently enforced by the runtime scene.
     */
    void SetDynamicTemplateVisibility(std::string pageName, std::string templateId, bool visible);

    /**
     * @brief Clears every tracked dynamic-template visibility override.
     */
    void ClearDynamicTemplateVisibility();

    /**
     * @brief Returns one dynamic-template visibility override when it exists.
     * @param pageName Owning page name.
     * @param templateId Template id identifying the set.
     * @return Optional visibility override.
     */
    [[nodiscard]] std::optional<bool> DynamicTemplateVisibility(std::string_view pageName,
                                                                std::string_view templateId) const;

    /**
     * @brief Returns every dynamic-template visibility override currently tracked.
     * @return Flat copy of the visibility map.
     */
    [[nodiscard]] std::vector<std::pair<DynamicTemplateKey, bool>> DynamicTemplateVisibilityEntries() const;

    /**
     * @brief Updates the transport telemetry displayed by the overlay.
     * @param commandConfigured Whether the command receiver is configured.
     * @param commandReady Whether the command receiver is ready.
     * @param feedbackConfigured Whether the feedback sender is configured.
     * @param feedbackReady Whether the feedback sender is ready.
     * @param metrics Latest UDP runtime metrics snapshot.
     * @param commandStatus Last command-side runtime status.
     * @param feedbackStatus Last feedback-side runtime status.
     */
    void UpdateTransportState(bool commandConfigured,
                              bool commandReady,
                              bool feedbackConfigured,
                              bool feedbackReady,
                              const UdpRuntimeBridgeMetrics& metrics,
                              std::string commandStatus,
                              std::string feedbackStatus);

    /**
     * @brief Marks one batch burst as observed by the UDP transport.
     * @param batchCount Number of batches drained during the current frame.
     * @param commandCount Number of commands contained in that burst.
     * @param now Observation time.
     */
    void NoteCommandTraffic(std::size_t batchCount, std::size_t commandCount, Clock::time_point now = Clock::now());

    /**
     * @brief Returns the transport telemetry snapshot.
     * @return Transport telemetry.
     */
    [[nodiscard]] const TransportState& Transport() const noexcept;

    /**
     * @brief Returns the elapsed time since the last observed command traffic.
     * @param now Reference time.
     * @return Elapsed seconds, or a negative value when no traffic was seen yet.
     */
    [[nodiscard]] double SecondsSinceLastCommandTraffic(Clock::time_point now = Clock::now()) const noexcept;

    /**
     * @brief Updates the status message displayed by the manual test panel.
     * @param message Human-readable status string.
     */
    void SetTestPanelStatus(std::string message);

    /**
     * @brief Returns the manual test-panel status.
     * @return Human-readable status string.
     */
    [[nodiscard]] const std::string& TestPanelStatus() const noexcept;

private:
    bool active_ = false;
    bool pageBypassed_ = false;
    std::string forcedActivePage_ {};
    std::string selectedPage_ {};
    std::optional<ReticleKey> selectedReticle_ {};
    std::unordered_map<std::string, std::string> strobeBypasses_ {};
    std::unordered_map<ReticleKey, ReticleBypassState, ReticleKeyHash> reticleBypasses_ {};
    std::unordered_map<DynamicTemplateKey, bool, DynamicTemplateKeyHash> dynamicTemplateVisibility_ {};
    TransportState transport_ {};
    std::optional<Clock::time_point> lastCommandTrafficAt_ {};
    std::string testPanelStatus_ {};
};

/**
 * @brief Returns whether two reticle keys refer to the same runtime reticle.
 * @param lhs Left-hand key.
 * @param rhs Right-hand key.
 * @return `true` when both keys are identical.
 */
bool operator==(const ReticleKey& lhs, const ReticleKey& rhs) noexcept;

/**
 * @brief Returns whether two dynamic-template keys refer to the same set.
 * @param lhs Left-hand key.
 * @param rhs Right-hand key.
 * @return `true` when both keys are identical.
 */
bool operator==(const DynamicTemplateKey& lhs, const DynamicTemplateKey& rhs) noexcept;
} // namespace mfd::window::debug
