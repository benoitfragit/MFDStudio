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
 * @details The watcher records each file's modification time when @ref Watch is called (after a load
 * or a save) and reports a change when a later poll observes a different timestamp. It is the editor's
 * responsibility to re-arm the watcher after it writes those files itself, so only third-party edits
 * surface. The component performs no throttling; callers poll at whatever cadence they prefer.
 */
class EditorAssetWatcher
{
public:
    /**
     * @brief Records the current modification time of each existing file as the new baseline.
     * @param files Files to track. Missing or unreadable files are skipped.
     */
    void Watch(const std::vector<std::filesystem::path>& files)
    {
        timestamps_.clear();
        for (const std::filesystem::path& file : files)
        {
            std::error_code error;
            const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(file, error);
            if (!error)
            {
                timestamps_[file.lexically_normal().generic_string()] = writeTime;
            }
        }
    }

    /**
     * @brief Returns whether any watched file changed on disk since the last baseline.
     * @return `true` when at least one tracked file has a newer-or-different timestamp.
     * @note Detected files are re-baselined so a single external edit is reported once.
     */
    [[nodiscard]] bool DetectExternalChange()
    {
        bool changed = false;
        for (auto& [path, baseline] : timestamps_)
        {
            std::error_code error;
            const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
            if (error)
            {
                continue;
            }

            if (writeTime != baseline)
            {
                baseline = writeTime;
                changed = true;
            }
        }

        return changed;
    }

    /** @brief Stops tracking every file. */
    void Clear() noexcept
    {
        timestamps_.clear();
    }

    /** @brief Returns the number of files currently tracked. */
    [[nodiscard]] std::size_t WatchedCount() const noexcept
    {
        return timestamps_.size();
    }

private:
    std::unordered_map<std::string, std::filesystem::file_time_type> timestamps_;
};
} // namespace editor
