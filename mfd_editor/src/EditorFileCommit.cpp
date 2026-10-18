/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/EditorFileCommit.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace editor::internal
{
namespace
{
void Checkpoint(const FileCommitCheckpoint& checkpoint, const FileCommitPoint point,
                const std::filesystem::path& target)
{
    if (checkpoint)
    {
        checkpoint(point, target);
    }
}

void RemoveTemporary(const std::filesystem::path& path) noexcept
{
    if (!path.empty())
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        if (error)
        {
            // A backup may inherit the original's read-only attribute. Only
            // owned temporary files reach this helper; never alter live assets.
            std::filesystem::permissions(path, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::add, error);
            std::filesystem::remove(path, error);
        }
    }
}

std::filesystem::path ReserveSibling(const std::filesystem::path& target, const char* suffix)
{
    static std::atomic<unsigned long long> sequence {0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned int attempt = 0; attempt < 100U; ++attempt)
    {
        std::filesystem::path candidate = target;
        candidate += std::string(suffix) + std::to_string(ticks) + "_" + std::to_string(sequence++);
#if defined(_WIN32)
        const HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr,
                                        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            if (!CloseHandle(file))
            {
                const auto error = GetLastError();
                RemoveTemporary(candidate);
                throw std::system_error(static_cast<int>(error), std::system_category(), "Close temporary file");
            }
            return candidate;
        }
        const auto error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
        {
            throw std::system_error(static_cast<int>(error), std::system_category(), candidate.string());
        }
#else
        const int file = ::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (file >= 0)
        {
            if (::close(file) != 0)
            {
                const int error = errno;
                RemoveTemporary(candidate);
                throw std::system_error(error, std::generic_category(), "Close temporary file");
            }
            return candidate;
        }
        if (errno != EEXIST)
        {
            throw std::system_error(errno, std::generic_category(), candidate.string());
        }
#endif
    }
    throw std::runtime_error("Unable to reserve temporary file beside " + target.string());
}

void Promote(const std::filesystem::path& source, const std::filesystem::path& target,
             const bool replaceExisting)
{
#if defined(_WIN32)
    if (!MoveFileExW(source.c_str(), target.c_str(), replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0))
    {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "Unable to promote " + source.string() + " to " + target.string());
    }
#else
    if (replaceExisting)
    {
        std::filesystem::rename(source, target);
    }
    else
    {
        // Link creation fails if another writer has created the destination.
        std::filesystem::create_hard_link(source, target);
        RemoveTemporary(source);
    }
#endif
}

void WriteStaged(const std::filesystem::path& staged, const std::filesystem::path& target,
                 const std::string_view content, const FileCommitCheckpoint& checkpoint)
{
    Checkpoint(checkpoint, FileCommitPoint::Stage, target);
    std::ofstream stream;
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.open(staged, std::ios::binary | std::ios::trunc);
    Checkpoint(checkpoint, FileCommitPoint::Write, target);
    stream << content;
    Checkpoint(checkpoint, FileCommitPoint::Flush, target);
    stream.flush();
    Checkpoint(checkpoint, FileCommitPoint::Close, target);
    stream.close();
    Checkpoint(checkpoint, FileCommitPoint::Verify, target);
    std::ifstream verification(staged, std::ios::binary);
    verification.exceptions(std::ios::badbit);
    const std::string actual {std::istreambuf_iterator<char>(verification), std::istreambuf_iterator<char>()};
    if (!verification.is_open() || actual != content)
    {
        throw std::runtime_error("Staged file verification failed: " + staged.string());
    }
}

struct CommitEntry
{
    std::filesystem::path target;
    std::filesystem::path staged;
    std::filesystem::path backup;
    bool changed = false;
};

std::wstring TargetKey(const std::filesystem::path& target)
{
    std::wstring key = std::filesystem::weakly_canonical(target).wstring();
#if defined(_WIN32)
    std::transform(key.begin(), key.end(), key.begin(),
                   [](const wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
#endif
    return key;
}

void ValidateTargets(const std::vector<CommitEntry>& entries)
{
    std::vector<std::wstring> keys;
    keys.reserve(entries.size());
    for (const CommitEntry& entry : entries)
    {
        keys.push_back(TargetKey(entry.target));
        if (std::filesystem::exists(entry.target) && !std::filesystem::is_regular_file(entry.target))
        {
            throw std::runtime_error("Asset target is not a regular file: " + entry.target.string());
        }
    }
    std::sort(keys.begin(), keys.end());
    if (std::adjacent_find(keys.begin(), keys.end()) != keys.end())
    {
        throw std::runtime_error("Duplicate or colliding asset transaction targets");
    }
}

std::string Rollback(std::vector<CommitEntry>& entries, const FileCommitCheckpoint& checkpoint)
{
    std::string failures;
    for (auto iterator = entries.rbegin(); iterator != entries.rend(); ++iterator)
    {
        CommitEntry& entry = *iterator;
        try
        {
            if (entry.changed)
            {
                Checkpoint(checkpoint, FileCommitPoint::Rollback, entry.target);
                if (!entry.backup.empty())
                {
                    Promote(entry.backup, entry.target, true);
                }
                else
                {
                    std::filesystem::remove(entry.target);
                }
            }
            RemoveTemporary(entry.backup);
        }
        catch (const std::exception& exception)
        {
            failures += "\nRollback failed for " + entry.target.string() + ": " + exception.what() +
                        "; retained backup: " + entry.backup.string();
        }
        RemoveTemporary(entry.staged);
    }
    return failures;
}
}

void CommitAssetFiles(const std::vector<AssetFileWrite>& writes,
                      const std::vector<std::filesystem::path>& deletions,
                      const FileCommitCheckpoint& checkpoint)
{
    std::vector<CommitEntry> entries;
    entries.reserve(writes.size() + deletions.size());
    for (const AssetFileWrite& write : writes)
    {
        entries.push_back({write.target, {}, {}, false});
        if (!write.replaceExisting && std::filesystem::exists(write.target))
        {
            throw std::runtime_error("Asset destination already exists: " + write.target.string());
        }
    }
    for (const auto& deletion : deletions)
    {
        entries.push_back({deletion, {}, {}, false});
    }
    ValidateTargets(entries);
    try
    {
        for (std::size_t index = 0; index < writes.size(); ++index)
        {
            CommitEntry& entry = entries[index];
            entry.staged = ReserveSibling(entry.target, ".mfd_tmp_");
            WriteStaged(entry.staged, entry.target, writes[index].content, checkpoint);
        }
        for (CommitEntry& entry : entries)
        {
            if (std::filesystem::exists(entry.target))
            {
                Checkpoint(checkpoint, FileCommitPoint::Backup, entry.target);
                entry.backup = ReserveSibling(entry.target, ".mfd_backup_");
                std::filesystem::copy_file(entry.target, entry.backup, std::filesystem::copy_options::overwrite_existing);
            }
        }
        for (std::size_t index = 0; index < writes.size(); ++index)
        {
            CommitEntry& entry = entries[index];
            Checkpoint(checkpoint, FileCommitPoint::Promote, entry.target);
            Promote(entry.staged, entry.target, writes[index].replaceExisting);
            entry.changed = true;
        }
        for (std::size_t index = writes.size(); index < entries.size(); ++index)
        {
            CommitEntry& entry = entries[index];
            Checkpoint(checkpoint, FileCommitPoint::Delete, entry.target);
            entry.changed = std::filesystem::remove(entry.target);
        }
    }
    catch (const std::exception& exception)
    {
        const std::string failure = exception.what();
        const std::string rollbackFailures = Rollback(entries, checkpoint);
        throw std::runtime_error(failure + rollbackFailures);
    }
    for (const CommitEntry& entry : entries)
    {
        RemoveTemporary(entry.backup);
        RemoveTemporary(entry.staged);
    }
}

void WriteRecoveryFile(const std::filesystem::path& target, const std::string_view content,
                       const FileCommitCheckpoint& checkpoint)
{
    if (target.has_parent_path())
    {
        std::filesystem::create_directories(target.parent_path());
    }
    const std::filesystem::path staged = ReserveSibling(target, ".mfd_tmp_");
    try
    {
        WriteStaged(staged, target, content, checkpoint);
        Checkpoint(checkpoint, FileCommitPoint::Promote, target);
        Promote(staged, target, true);
    }
    catch (...)
    {
        RemoveTemporary(staged);
        throw;
    }
}
}
