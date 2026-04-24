/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Per-frame guard preventing the runtime debug inspector from using stale snapshot pointers.
 */

#include <string_view>

namespace mfd::window::debug
{
/**
 * @brief Tracks whether the current inspector snapshot can still be consumed safely.
 *
 * @note The runtime debug inspector captures pointers into the current preview
 * snapshot at the beginning of one ImGui frame. Any local action rebuilding the
 * preview invalidates those pointers. This helper makes that state explicit so
 * the remaining widgets can stop using the stale snapshot until the next frame.
 */
class RuntimeDebugInspectorFrameState
{
public:
    /**
     * @brief Returns whether the current frame still owns a valid preview snapshot.
     * @return `true` while the inspector may continue reading captured pointers.
     */
    [[nodiscard]] bool SnapshotValid() const noexcept
    {
        return snapshotValid_;
    }

    /**
     * @brief Marks the current preview snapshot as invalid for the rest of the frame.
     */
    void InvalidateSnapshot() noexcept
    {
        snapshotValid_ = false;
    }

    /**
     * @brief Returns the status string shown after one local mutation rebuilds the preview.
     * @return Human-readable refresh notice.
     */
    [[nodiscard]] static constexpr std::string_view RefreshNotice() noexcept
    {
        return "The inspector state changed and will be refreshed on the next frame.";
    }

private:
    bool snapshotValid_ = true;
};
} // namespace mfd::window::debug
