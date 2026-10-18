/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Internal, recoverable asset writes and atomic recovery-file replacement. */

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor::internal
{
/** @brief One fully serialized asset, prepared before changing live files. */
struct AssetFileWrite
{
    std::filesystem::path target;
    std::string content;
    bool replaceExisting = true;
};

/** @brief I/O boundaries available to deterministic failure-injection tests. */
enum class FileCommitPoint { Stage, Write, Flush, Close, Verify, Backup, Promote, Delete, Rollback };
using FileCommitCheckpoint = std::function<void(FileCommitPoint, const std::filesystem::path&)>;

/**
 * @brief Stages all bytes, backs up originals, then promotes writes and removes obsolete assets.
 * @param writes Serialized assets; targets must be distinct from each other and deletions.
 * @param deletions Obsolete files removed only after all writes have been promoted.
 * @param checkpoint Optional internal I/O fault injection; empty in production.
 * @throws std::exception On failure, including retained backup paths if rollback fails.
 * @note Provides handled-failure rollback, not multi-file crash atomicity. No concurrent writers.
 */
void CommitAssetFiles(const std::vector<AssetFileWrite>& writes,
                      const std::vector<std::filesystem::path>& deletions,
                      const FileCommitCheckpoint& checkpoint = {});

/**
 * @brief Replaces one recovery file only after an adjacent temporary file is fully written.
 * @param target Existing recovery filename, preserved on staging/promotion failure.
 * @param content Complete serialized recovery bundle.
 * @param checkpoint Optional internal I/O fault injection.
 * @note Atomic visibility on the same filesystem; no power-loss durability guarantee.
 */
void WriteRecoveryFile(const std::filesystem::path& target, std::string_view content,
                       const FileCommitCheckpoint& checkpoint = {});
}
