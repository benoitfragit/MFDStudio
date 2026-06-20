/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for EditorRecentWindowsService.
 */

#include "EditorRecentWindowsService.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

namespace editor
{
namespace
{
std::filesystem::path NormalizeEntry(const std::filesystem::path& windowFile)
{
    return windowFile.lexically_normal();
}

bool SameEntry(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return lhs.generic_string() == rhs.generic_string();
}
} // namespace

void EditorRecentWindowsService::Remember(const std::filesystem::path& windowFile)
{
    if (windowFile.empty())
    {
        return;
    }

    const std::filesystem::path normalized = NormalizeEntry(windowFile);
    entries_.erase(
        std::remove_if(entries_.begin(),
                       entries_.end(),
                       [&normalized](const std::filesystem::path& entry) { return SameEntry(entry, normalized); }),
        entries_.end());

    entries_.insert(entries_.begin(), normalized);
    if (entries_.size() > kMaxEntries)
    {
        entries_.resize(kMaxEntries);
    }
}

const std::vector<std::filesystem::path>& EditorRecentWindowsService::Entries() const noexcept
{
    return entries_;
}

std::vector<std::filesystem::path> EditorRecentWindowsService::ExistingEntries() const
{
    std::vector<std::filesystem::path> existing;
    existing.reserve(entries_.size());
    for (const std::filesystem::path& entry : entries_)
    {
        std::error_code existsError;
        if (std::filesystem::exists(entry, existsError) && !existsError)
        {
            existing.push_back(entry);
        }
    }

    return existing;
}

void EditorRecentWindowsService::Load(const std::filesystem::path& storageFile)
{
    entries_.clear();

    std::ifstream stream(storageFile, std::ios::binary);
    if (!stream.is_open())
    {
        return;
    }

    try
    {
        nlohmann::json document;
        stream >> document;
        if (!document.is_array())
        {
            return;
        }

        for (const nlohmann::json& value : document)
        {
            if (!value.is_string())
            {
                continue;
            }

            const std::filesystem::path entry = NormalizeEntry(std::filesystem::path(value.get<std::string>()));
            if (entry.empty())
            {
                continue;
            }

            const bool alreadyTracked =
                std::any_of(entries_.begin(),
                            entries_.end(),
                            [&entry](const std::filesystem::path& tracked) { return SameEntry(tracked, entry); });
            if (!alreadyTracked)
            {
                entries_.push_back(entry);
            }

            if (entries_.size() >= kMaxEntries)
            {
                break;
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        // Best-effort load: a corrupt recents file must never interrupt authoring.
        entries_.clear();
    }
}

void EditorRecentWindowsService::Save(const std::filesystem::path& storageFile) const
{
    try
    {
        if (storageFile.has_parent_path())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(storageFile.parent_path(), directoryError);
        }

        nlohmann::json document = nlohmann::json::array();
        for (const std::filesystem::path& entry : entries_)
        {
            document.push_back(entry.generic_string());
        }

        std::ofstream stream(storageFile, std::ios::binary | std::ios::trunc);
        if (stream.is_open())
        {
            stream << document.dump(2);
        }
    }
    catch (const std::exception&)
    {
        // Best-effort save: never interrupt authoring if the recents list cannot be written.
    }
}
} // namespace editor
