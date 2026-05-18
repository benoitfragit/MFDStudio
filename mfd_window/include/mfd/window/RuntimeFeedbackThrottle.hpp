/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Lightweight cadence controller for runtime UDP feedback emission.
 */

namespace mfd::window
{
/**
 * @brief Tracks fast changed-state feedback and slower unchanged-state heartbeat cadence.
 */
class RuntimeFeedbackThrottle
{
public:
    /**
     * @brief Clears elapsed time and marks the next changed feedback as immediately sendable.
     */
    void Reset() noexcept;

    /**
     * @brief Advances the throttle clocks.
     * @param deltaSeconds Elapsed frame time in seconds. Non-finite or negative values are ignored.
     */
    void Advance(float deltaSeconds) noexcept;

    /**
     * @brief Returns whether changed feedback may be sent on the fast cadence.
     * @param fastIntervalSeconds Minimum interval between changed-state emissions.
     * @return `true` when changed feedback should be emitted now.
     */
    [[nodiscard]] bool ShouldSendChanged(float fastIntervalSeconds) const noexcept;

    /**
     * @brief Returns whether unchanged feedback may be sent as a heartbeat.
     * @param heartbeatIntervalSeconds Minimum interval between unchanged-state emissions.
     * @return `true` when unchanged feedback should be emitted now.
     */
    [[nodiscard]] bool ShouldSendHeartbeat(float heartbeatIntervalSeconds) const noexcept;

    /**
     * @brief Marks that one or more changed feedback payloads were emitted.
     * @param fastIntervalSeconds Configured fast interval used to retain elapsed remainder.
     */
    void MarkChangedSent(float fastIntervalSeconds) noexcept;

    /**
     * @brief Marks that one or more unchanged heartbeat payloads were emitted.
     * @param heartbeatIntervalSeconds Configured heartbeat interval used to retain elapsed remainder.
     */
    void MarkHeartbeatSent(float heartbeatIntervalSeconds) noexcept;

private:
    float fastElapsedSeconds_ = 0.0f;
    float heartbeatElapsedSeconds_ = 0.0f;
    bool firstFeedbackPending_ = true;
};
} // namespace mfd::window
