/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Cadence control deciding when the editor should write a crash-recovery snapshot.
 */

#include <cmath>

namespace editor
{
/**
 * @brief Accumulates frame time and reports when an autosave recovery snapshot is due.
 *
 * @details The scheduler is renderer-agnostic and unit-testable. It only signals an autosave while
 * the document is dirty and enough time has elapsed since the last snapshot; the owner resets the
 * timer when the document becomes clean (saved or reloaded).
 */
class EditorAutosaveScheduler
{
public:
    /** @brief Default seconds between recovery snapshots while the document stays dirty. */
    static constexpr float kDefaultIntervalSeconds = 30.0f;

    /**
     * @brief Accumulates elapsed time.
     * @param deltaSeconds Frame delta; non-finite or non-positive values are ignored.
     */
    void Advance(const float deltaSeconds) noexcept
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        {
            return;
        }

        elapsedSeconds_ += deltaSeconds;
    }

    /**
     * @brief Returns whether a recovery snapshot is due.
     * @param intervalSeconds Minimum seconds between snapshots; clamped to a small positive value.
     * @param dirty Whether the document currently has unsaved changes.
     */
    [[nodiscard]] bool DueForAutosave(const float intervalSeconds, const bool dirty) const noexcept
    {
        return dirty && elapsedSeconds_ >= SanitizeInterval(intervalSeconds);
    }

    /** @brief Resets the timer after a snapshot was written. */
    void MarkAutosaved() noexcept
    {
        elapsedSeconds_ = 0.0f;
    }

    /** @brief Resets the timer, e.g. when the document becomes clean. */
    void Reset() noexcept
    {
        elapsedSeconds_ = 0.0f;
    }

    /** @brief Returns the seconds accumulated since the last reset. */
    [[nodiscard]] float ElapsedSeconds() const noexcept
    {
        return elapsedSeconds_;
    }

private:
    [[nodiscard]] static float SanitizeInterval(const float intervalSeconds) noexcept
    {
        constexpr float kMinIntervalSeconds = 1.0f;
        if (!std::isfinite(intervalSeconds) || intervalSeconds < kMinIntervalSeconds)
        {
            return kMinIntervalSeconds;
        }

        return intervalSeconds;
    }

    float elapsedSeconds_ = 0.0f;
};
} // namespace editor
