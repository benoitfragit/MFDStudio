/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Detects external edits to the authored files currently open in the editor.
 */

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor
{
/**
 * @brief Tracks the last-write timestamps of a set of files and reports external modifications.
 *
 * @details The watcher records each file's existence and modification time when @ref Watch is called
 * (after a load or a save) and reports a change when a later poll observes a different timestamp, a
 * watched file that disappeared, or one that reappeared. It is the editor's responsibility to re-arm the
 * watcher after it writes those files itself, so only third-party edits surface. The component performs
 * no throttling; callers poll at whatever cadence they prefer.
 */
class EditorAssetWatcher
{
public:
    /**
     * @brief Records the current existence and modification time of each file as the new baseline.
     * @param files Files to track. Missing or unreadable files are recorded as absent so that a later
     * reappearance is reported as an external change.
     */
    void Watch(const std::vector<std::filesystem::path>& files)
    {
        states_.clear();
        for (const std::filesystem::path& file : files)
        {
            std::error_code error;
            const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(file, error);
            WatchedFileState state;
            state.present = !error;
            if (!error)
            {
                state.writeTime = writeTime;
            }

            states_[file.lexically_normal().generic_string()] = state;
        }
    }

    /**
     * @brief Returns whether any watched file changed on disk since the last baseline.
     * @return `true` when at least one tracked file has a newer-or-different timestamp, has disappeared,
     * or has reappeared since the baseline was taken.
     * @note Detected files are re-baselined so a single external edit, deletion, or reappearance is
     * reported once.
     */
    [[nodiscard]] bool DetectExternalChange()
    {
        bool changed = false;
        for (auto& [path, baseline] : states_)
        {
            std::error_code error;
            const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
            const bool present = !error;

            if (present != baseline.present || (present && writeTime != baseline.writeTime))
            {
                baseline.present = present;
                baseline.writeTime = present ? writeTime : std::filesystem::file_time_type {};
                changed = true;
            }
        }

        return changed;
    }

    /** @brief Stops tracking every file. */
    void Clear() noexcept
    {
        states_.clear();
    }

    /** @brief Returns the number of files currently tracked. */
    [[nodiscard]] std::size_t WatchedCount() const noexcept
    {
        return states_.size();
    }

private:
    struct WatchedFileState
    {
        bool present = false;
        std::filesystem::file_time_type writeTime {};
    };

    std::unordered_map<std::string, WatchedFileState> states_;
};
} // namespace editor
