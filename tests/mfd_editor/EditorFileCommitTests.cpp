/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/EditorFileCommit.h"
#include "EditorDocumentSerializer.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
using editor::internal::AssetFileWrite;
using editor::internal::CommitAssetFiles;
using editor::internal::FileCommitPoint;
using editor::internal::WriteRecoveryFile;

class EditorFileCommitTests : public testing::Test
{
protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("mfd_file_commit_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);
    }
    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    static void Write(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream stream(path, std::ios::binary);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream << content;
        stream.close();
    }
    static std::string Read(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
    std::filesystem::path root;
};

TEST_F(EditorFileCommitTests, EveryHandledFailureRestoresBytesAndRemovesNewFiles)
{
    const auto first = root / "page.json";
    const auto second = root / "window.json";
    const auto created = root / "renamed.json";
    const auto obsolete = root / "old.json";
    const std::vector<AssetFileWrite> writes {{first, "new page"}, {created, "new template", false}, {second, "new window"}};
    for (const auto point : {FileCommitPoint::Stage, FileCommitPoint::Write, FileCommitPoint::Flush,
                            FileCommitPoint::Close, FileCommitPoint::Verify, FileCommitPoint::Backup, FileCommitPoint::Promote,
                            FileCommitPoint::Delete})
    {
        Write(first, "original page\r\n");
        Write(second, "original window\n");
        Write(obsolete, "original template");
        EXPECT_THROW(CommitAssetFiles(writes, {obsolete},
            [point, second, obsolete](const FileCommitPoint reached, const std::filesystem::path& path)
            {
                if (reached == point && (path == second || path == obsolete))
                {
                    throw std::runtime_error("injected I/O failure");
                }
            }), std::runtime_error);
        EXPECT_EQ(Read(first), "original page\r\n");
        EXPECT_EQ(Read(second), "original window\n");
        EXPECT_EQ(Read(obsolete), "original template");
        EXPECT_FALSE(std::filesystem::exists(created));
        EXPECT_EQ(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator()), 3);
    }
}

TEST_F(EditorFileCommitTests, FailedRollbackReportsAndPreservesOriginalBackup)
{
    const auto first = root / "page.json";
    const auto second = root / "window.json";
    Write(first, "original page");
    Write(second, "original window");
    try
    {
        CommitAssetFiles({{first, "new page"}, {second, "new window"}}, {},
            [first, second](const FileCommitPoint point, const std::filesystem::path& path)
            {
                if ((point == FileCommitPoint::Promote && path == second) ||
                    (point == FileCommitPoint::Rollback && path == first))
                {
                    throw std::runtime_error("injected failure");
                }
            });
        FAIL() << "Expected failed promotion and rollback";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("Rollback failed"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("retained backup:"), std::string::npos);
    }
    bool foundBackup = false;
    for (const auto& entry : std::filesystem::directory_iterator(root))
    {
        if (entry.path().filename().string().find(".mfd_backup_") != std::string::npos)
        {
            EXPECT_EQ(Read(entry.path()), "original page");
            foundBackup = true;
        }
    }
    EXPECT_TRUE(foundBackup);
    EXPECT_EQ(Read(second), "original window");
}

TEST_F(EditorFileCommitTests, SuccessAcrossDirectoriesAndDeletionRollback)
{
    std::filesystem::create_directory(root / "pages");
    const auto page = root / "pages" / "page.json";
    const auto old = root / "old.json";
    const auto other = root / "other.json";
    Write(old, "old");
    Write(other, "other");
    EXPECT_THROW(CommitAssetFiles({{page, "page", false}}, {old, other},
        [other](const FileCommitPoint point, const std::filesystem::path& path)
        {
            if (point == FileCommitPoint::Delete && path == other)
            {
                throw std::runtime_error("second deletion failed");
            }
        }), std::runtime_error);
    EXPECT_EQ(Read(old), "old");
    EXPECT_EQ(Read(other), "other");
    EXPECT_FALSE(std::filesystem::exists(page));
    EXPECT_NO_THROW(CommitAssetFiles({{page, "page", false}}, {old, other}));
    EXPECT_EQ(Read(page), "page");
    EXPECT_FALSE(std::filesystem::exists(old));
    EXPECT_FALSE(std::filesystem::exists(other));
}

TEST_F(EditorFileCommitTests, RejectsCollisionsMissingParentsAndExistingNewTargets)
{
    const auto target = root / "asset.json";
    Write(target, "original");
    EXPECT_THROW(CommitAssetFiles({{target, "new", false}}, {}), std::runtime_error);
    EXPECT_THROW(CommitAssetFiles({{target, "first"}, {target, "second"}}, {}), std::runtime_error);
    EXPECT_THROW(CommitAssetFiles({{target, "first"}}, {target}), std::runtime_error);
    EXPECT_THROW(CommitAssetFiles({{target, "first"}, {root / "missing" / "asset.json", "new"}}, {}), std::exception);
    EXPECT_EQ(Read(target), "original");
}

TEST_F(EditorFileCommitTests, RecoveryFailuresPreserveCommittedSnapshotAndIgnoreInterruptedStaging)
{
    const auto target = root / "recovery.json";
    Write(target, "previous valid snapshot");
    Write(root / "recovery.json.mfd_tmp_interrupted", "incomplete bytes");
    for (const auto point : {FileCommitPoint::Stage, FileCommitPoint::Write, FileCommitPoint::Flush,
                            FileCommitPoint::Close, FileCommitPoint::Verify, FileCommitPoint::Promote})
    {
        EXPECT_THROW(WriteRecoveryFile(target, "next snapshot",
            [point](const FileCommitPoint reached, const std::filesystem::path&)
            {
                if (point == reached)
                {
                    throw std::runtime_error("injected recovery failure");
                }
            }), std::runtime_error);
        EXPECT_EQ(Read(target), "previous valid snapshot");
        EXPECT_EQ(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator()), 2);
    }
    WriteRecoveryFile(target, "next valid snapshot");
    EXPECT_EQ(Read(target), "next valid snapshot");
}

TEST_F(EditorFileCommitTests, ReadOnlyDestinationPreservesSnapshotAndEarlierAssets)
{
    const auto first = root / "page.json";
    const auto locked = root / "window.json";
    Write(first, "page");
    Write(locked, "window");
    std::filesystem::permissions(locked, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);
#if defined(_WIN32)
    EXPECT_THROW(CommitAssetFiles({{first, "changed page"}, {locked, "changed window"}}, {}), std::exception);
    EXPECT_THROW(WriteRecoveryFile(locked, "changed recovery"), std::exception);
    EXPECT_EQ(Read(first), "page");
    EXPECT_EQ(Read(locked), "window");
#endif
    std::filesystem::permissions(locked, std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add);
}
}

TEST_F(EditorFileCommitTests, TruncatedStagingIsDetectedBeforeRecoveryReplacement)
{
    const auto target = root / "recovery.json";
    Write(target, "previous snapshot");
    EXPECT_THROW(WriteRecoveryFile(target, "complete next snapshot",
        [this](const FileCommitPoint point, const std::filesystem::path&)
        {
            if (point == FileCommitPoint::Verify)
            {
                for (const auto& entry : std::filesystem::directory_iterator(root))
                {
                    if (entry.path().filename().string().find(".mfd_tmp_") != std::string::npos)
                    {
                        std::filesystem::resize_file(entry.path(), 2);
                    }
                }
            }
        }), std::exception);
    EXPECT_EQ(Read(target), "previous snapshot");
}

TEST_F(EditorFileCommitTests, SerializedSnapshotStillRestoresThroughExistingReader)
{
    mfd::LoadedWindowConfiguration loaded;
    loaded.window.sourceFile = root / "window.json";
    loaded.window.title = "Recovery";
    loaded.window.width = 800;
    loaded.window.height = 600;
    loaded.window.reticleLibraryFolder = root / "reticles";
    editor::EditorFileLayout layout;
    const auto target = root / "recovery.json";
    const auto bundle = editor::SerializeRecoveryBundleToJsonString(loaded, layout);
    WriteRecoveryFile(target, bundle);
    std::filesystem::path restored;
    std::string error;
    EXPECT_TRUE(editor::RestoreRecoveryBundle(Read(target), root, restored, &error)) << error;
    EXPECT_EQ(restored, loaded.window.sourceFile);
    loaded.window.title = std::string(1, static_cast<char>(0xFF)); // Invalid UTF-8 fails JSON serialization.
    EXPECT_THROW(WriteRecoveryFile(target, editor::SerializeRecoveryBundleToJsonString(loaded, layout)), std::exception);
    EXPECT_EQ(Read(target), bundle);
}
