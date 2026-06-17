/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Client-side detector for window shutdown over the connectionless UDP feedback stream.
 */

#include <cstdint>

#include "mfd/client/ClientExport.h"

namespace mfd::client
{
/**
 * @brief Connection state inferred by a `WindowLivenessMonitor`.
 */
enum class WindowConnectionState
{
    /** @brief No feedback has been observed yet; the window is not confirmed alive. */
    WaitingForFirstContact,
    /** @brief Recent feedback confirms the window is alive. */
    Connected,
    /** @brief No feedback arrived within the timeout; the window is assumed gone. */
    Lost,
    /** @brief The window reported a graceful shutdown. */
    Closed
};

/**
 * @brief Detects window shutdown so a client can reset and rebuild its transports.
 *
 * @details The UDP feedback stream is connectionless, so a client cannot rely on
 * a socket error to learn that a window vanished. This monitor combines two
 * complementary signals emitted by the runtime:
 * - the periodic window lifecycle heartbeat, whose absence beyond a timeout
 *   reveals an abrupt termination (crash, kill, or network loss);
 * - the explicit graceful `Closing` lifecycle payload, which triggers an
 *   immediate transition without waiting for the timeout.
 *
 * The monitor keeps no internal clock: the caller supplies a monotonic time in
 * seconds, which keeps the class deterministic and trivially testable.
 *
 * @note The monitor never owns a transport. The caller drains the feedback
 * channel, reports activity, and rebuilds its own connections when a disconnect
 * is detected.
 */
class MFD_CLIENT_API WindowLivenessMonitor
{
public:
    /**
     * @brief Creates a monitor with a heartbeat timeout.
     * @param heartbeatTimeoutSeconds Maximum silence tolerated while connected before the
     * window is considered lost. Non-finite or too-small values are clamped to a small positive value.
     *
     * @note Choose a timeout safely larger than the window heartbeat interval so
     * a single dropped UDP heartbeat does not trigger a false disconnect.
     */
    explicit WindowLivenessMonitor(double heartbeatTimeoutSeconds = 2.0) noexcept;

    /**
     * @brief Records that a feedback packet was received from the window.
     * @param nowSeconds Monotonic timestamp of the reception in seconds.
     *
     * @note Call this for any decoded feedback packet, including unchanged
     * heartbeats. A graceful `Closing` is sticky and is not cleared by later
     * activity until `Reset` is called.
     */
    void RecordActivity(double nowSeconds) noexcept;

    /**
     * @brief Records an explicit graceful window shutdown notification.
     * @param nowSeconds Monotonic timestamp of the notification in seconds.
     */
    void RecordWindowClosing(double nowSeconds) noexcept;

    /**
     * @brief Updates and returns the connection state from a decoded-packet counter.
     * @param totalDecodedPackets Monotonic decoded-packet counter, e.g. from `RuntimeFeedbackState`.
     * @param windowReportedClosing `true` when the window reported a graceful shutdown.
     * @param nowSeconds Monotonic timestamp in seconds.
     * @return Current connection state.
     *
     * @note Convenience entry point for generated clients: it derives activity
     * from increases of `totalDecodedPackets`, records a graceful close when
     * requested, then recomputes the timeout.
     */
    WindowConnectionState Observe(std::uint64_t totalDecodedPackets,
                                  bool windowReportedClosing,
                                  double nowSeconds) noexcept;

    /**
     * @brief Recomputes the connection state by applying the heartbeat timeout.
     * @param nowSeconds Monotonic timestamp in seconds.
     * @return Current connection state.
     */
    WindowConnectionState Update(double nowSeconds) noexcept;

    /**
     * @brief Returns the last computed connection state.
     * @return Current connection state.
     */
    [[nodiscard]] WindowConnectionState State() const noexcept;

    /**
     * @brief Consumes a pending disconnect transition.
     * @return `true` exactly once after the monitor entered `Lost` or `Closed`.
     *
     * @note Use the returned `true` to run the client reset/reconnect sequence a
     * single time per disconnect.
     */
    [[nodiscard]] bool ConsumeDisconnect() noexcept;

    /**
     * @brief Re-arms the monitor for a fresh connection attempt.
     */
    void Reset() noexcept;

    /**
     * @brief Returns the effective heartbeat timeout in seconds.
     * @return Sanitized timeout value.
     */
    [[nodiscard]] double TimeoutSeconds() const noexcept;

private:
    double timeoutSeconds_ = 2.0;
    double lastActivitySeconds_ = 0.0;
    std::uint64_t lastObservedPackets_ = 0;
    WindowConnectionState state_ = WindowConnectionState::WaitingForFirstContact;
    bool hasActivity_ = false;
    bool hasObservedPackets_ = false;
    bool pendingDisconnect_ = false;
};
} // namespace mfd::client
