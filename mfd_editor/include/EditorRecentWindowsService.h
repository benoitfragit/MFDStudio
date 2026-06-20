/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Small editor-local persistence of the most recently opened window assets.
 */

#include <cstddef>
#include <filesystem>
#include <vector>

namespace editor
{
/**
 * @brief Tracks the recently opened window assets so the empty workspace can offer quick reopen.
 *
 * @details The list keeps the most recent asset first, de-duplicates entries by their normalized
 * path, and is capped to a small bound. Persistence is a best-effort JSON file local to the editor;
 * load/save never throw so authoring is never interrupted by a corrupt or unreadable list. The
 * service owns no UI and no document state, keeping the recent-assets concern isolated and testable.
 */
class EditorRecentWindowsService
{
public:
    /** @brief Maximum number of remembered window assets. */
    static constexpr std::size_t kMaxEntries = 8U;

    /**
     * @brief Records one window asset as the most recently opened one.
     * @param windowFile Window asset path; empty paths are ignored.
     * @note Re-opening an already tracked asset moves it back to the front without duplicating it.
     */
    void Remember(const std::filesystem::path& windowFile);

    /** @brief Returns the remembered assets, most recent first. */
    [[nodiscard]] const std::vector<std::filesystem::path>& Entries() const noexcept;

    /** @brief Returns the remembered assets that still exist on disk, most recent first. */
    [[nodiscard]] std::vector<std::filesystem::path> ExistingEntries() const;

    /**
     * @brief Loads the remembered assets from a JSON list, replacing the current content.
     * @param storageFile JSON file previously written by @ref Save; missing or invalid files leave
     *        the list empty.
     */
    void Load(const std::filesystem::path& storageFile);

    /**
     * @brief Persists the remembered assets to a JSON list.
     * @param storageFile Destination file; parent directories are created when needed.
     */
    void Save(const std::filesystem::path& storageFile) const;

private:
    std::vector<std::filesystem::path> entries_;
};
} // namespace editor
