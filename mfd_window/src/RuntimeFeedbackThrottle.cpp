/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of runtime UDP feedback cadence control.
 */

#include "mfd/window/RuntimeFeedbackThrottle.hpp"

#include <cmath>

namespace mfd::window
{
namespace
{
constexpr float kMinimumIntervalSeconds = 0.001f;

float SanitizeInterval(const float intervalSeconds) noexcept
{
    if (!std::isfinite(intervalSeconds) || intervalSeconds < kMinimumIntervalSeconds)
    {
        return kMinimumIntervalSeconds;
    }

    return intervalSeconds;
}

float RetainElapsedRemainder(const float elapsedSeconds, const float intervalSeconds) noexcept
{
    const float interval = SanitizeInterval(intervalSeconds);
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < interval)
    {
        return 0.0f;
    }

    return std::fmod(elapsedSeconds, interval);
}
} // namespace

void RuntimeFeedbackThrottle::Reset() noexcept
{
    fastElapsedSeconds_ = 0.0f;
    heartbeatElapsedSeconds_ = 0.0f;
    firstFeedbackPending_ = true;
}

void RuntimeFeedbackThrottle::Advance(const float deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        return;
    }

    fastElapsedSeconds_ += deltaSeconds;
    heartbeatElapsedSeconds_ += deltaSeconds;
}

bool RuntimeFeedbackThrottle::ShouldSendChanged(const float fastIntervalSeconds) const noexcept
{
    return firstFeedbackPending_ || fastElapsedSeconds_ >= SanitizeInterval(fastIntervalSeconds);
}

bool RuntimeFeedbackThrottle::ShouldSendHeartbeat(const float heartbeatIntervalSeconds) const noexcept
{
    return !firstFeedbackPending_ && heartbeatElapsedSeconds_ >= SanitizeInterval(heartbeatIntervalSeconds);
}

void RuntimeFeedbackThrottle::MarkChangedSent(const float fastIntervalSeconds) noexcept
{
    firstFeedbackPending_ = false;
    fastElapsedSeconds_ = RetainElapsedRemainder(fastElapsedSeconds_, fastIntervalSeconds);
    heartbeatElapsedSeconds_ = 0.0f;
}

void RuntimeFeedbackThrottle::MarkHeartbeatSent(const float heartbeatIntervalSeconds) noexcept
{
    firstFeedbackPending_ = false;
    heartbeatElapsedSeconds_ = RetainElapsedRemainder(heartbeatElapsedSeconds_, heartbeatIntervalSeconds);
}
} // namespace mfd::window
