/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for WindowLivenessMonitor.
 */

#include "mfd/client/WindowLivenessMonitor.h"

#include <cmath>

namespace mfd::client
{
namespace
{
constexpr double kMinimumTimeoutSeconds = 0.001;

double SanitizeTimeout(const double timeoutSeconds) noexcept
{
    if (!std::isfinite(timeoutSeconds) || timeoutSeconds < kMinimumTimeoutSeconds)
    {
        return kMinimumTimeoutSeconds;
    }

    return timeoutSeconds;
}
} // namespace

WindowLivenessMonitor::WindowLivenessMonitor(const double heartbeatTimeoutSeconds) noexcept :
    timeoutSeconds_(SanitizeTimeout(heartbeatTimeoutSeconds))
{
}

void WindowLivenessMonitor::RecordActivity(const double nowSeconds) noexcept
{
    if (!std::isfinite(nowSeconds))
    {
        return;
    }

    lastActivitySeconds_ = nowSeconds;
    hasActivity_ = true;

    // A graceful close stays sticky until the caller re-arms the monitor.
    if (state_ == WindowConnectionState::WaitingForFirstContact ||
        state_ == WindowConnectionState::Lost)
    {
        state_ = WindowConnectionState::Connected;
    }
}

void WindowLivenessMonitor::RecordWindowClosing(const double nowSeconds) noexcept
{
    if (std::isfinite(nowSeconds))
    {
        lastActivitySeconds_ = nowSeconds;
        hasActivity_ = true;
    }

    if (state_ != WindowConnectionState::Closed)
    {
        state_ = WindowConnectionState::Closed;
        pendingDisconnect_ = true;
    }
}

WindowConnectionState WindowLivenessMonitor::Observe(const std::uint64_t totalDecodedPackets,
                                                     const bool windowReportedClosing,
                                                     const double nowSeconds) noexcept
{
    if (!hasObservedPackets_ || totalDecodedPackets != lastObservedPackets_)
    {
        if (hasObservedPackets_ && totalDecodedPackets > lastObservedPackets_)
        {
            RecordActivity(nowSeconds);
        }
        else if (!hasObservedPackets_ && totalDecodedPackets > 0)
        {
            RecordActivity(nowSeconds);
        }

        lastObservedPackets_ = totalDecodedPackets;
        hasObservedPackets_ = true;
    }

    if (windowReportedClosing)
    {
        RecordWindowClosing(nowSeconds);
    }

    return Update(nowSeconds);
}

WindowConnectionState WindowLivenessMonitor::Update(const double nowSeconds) noexcept
{
    if (state_ == WindowConnectionState::Connected && hasActivity_ && std::isfinite(nowSeconds) &&
        (nowSeconds - lastActivitySeconds_) > timeoutSeconds_)
    {
        state_ = WindowConnectionState::Lost;
        pendingDisconnect_ = true;
    }

    return state_;
}

WindowConnectionState WindowLivenessMonitor::State() const noexcept
{
    return state_;
}

bool WindowLivenessMonitor::ConsumeDisconnect() noexcept
{
    const bool pending = pendingDisconnect_;
    pendingDisconnect_ = false;
    return pending;
}

void WindowLivenessMonitor::Reset() noexcept
{
    lastActivitySeconds_ = 0.0;
    lastObservedPackets_ = 0;
    state_ = WindowConnectionState::WaitingForFirstContact;
    hasActivity_ = false;
    hasObservedPackets_ = false;
    pendingDisconnect_ = false;
}

double WindowLivenessMonitor::TimeoutSeconds() const noexcept
{
    return timeoutSeconds_;
}
} // namespace mfd::client
