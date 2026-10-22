/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/EditorFileCommit.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
using editor::internal::CommitAssetFiles;
using editor::internal::FileCommitPoint;

class EditorFileCommitFailureTests : public testing::Test
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
        EXPECT_FALSE(error) << error.message();
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

}

TEST_F(EditorFileCommitFailureTests, CleanupFailureAfterCommitDoesNotRollBackDeletedBackups)
{
    const auto first = root / "page.json";
    const auto second = root / "window.json";
    Write(first, "original page");
    Write(second, "original window");
    try
    {
        CommitAssetFiles({{first, "new page"}, {second, "new window"}}, {},
            [second](const FileCommitPoint point, const std::filesystem::path& path)
            {
                if (point == FileCommitPoint::Cleanup &&
                    path.string().find(second.string() + ".mfd_backup_") == 0U)
                    throw std::runtime_error("backup locked");
            });
        FAIL() << "Expected cleanup diagnostic";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("Assets committed"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("backup locked"), std::string::npos);
    }
    EXPECT_EQ(Read(first), "new page");
    EXPECT_EQ(Read(second), "new window");
    EXPECT_NO_THROW(CommitAssetFiles({{first, "retry page"}, {second, "retry window"}}, {}));
    EXPECT_EQ(Read(second), "retry window");
}

TEST_F(EditorFileCommitFailureTests, SecondaryRestoreAndRemovalFailuresAreBothReported)
{
    const auto original = root / "page.json";
    const auto created = root / "new.json";
    const auto last = root / "window.json";
    Write(original, "original bytes");
    try
    {
        CommitAssetFiles({{original, "new bytes"}, {created, "created"}, {last, "last"}}, {},
            [last](const FileCommitPoint point, const std::filesystem::path& path)
            {
                if (point == FileCommitPoint::Promote && path == last)
                    throw std::runtime_error("primary promotion failure");
                if (point == FileCommitPoint::RestoreBackup)
                    throw std::runtime_error("restore denied");
                if (point == FileCommitPoint::RemovePromoted)
                    throw std::runtime_error("removal denied");
            });
        FAIL() << "Expected rollback diagnostics";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("primary promotion failure"), std::string::npos);
        EXPECT_NE(message.find("restore denied"), std::string::npos);
        EXPECT_NE(message.find("removal denied"), std::string::npos);
    }
    bool originalRecoverable = false;
    for (const auto& entry : std::filesystem::directory_iterator(root))
        if (entry.path().string().find(".mfd_backup_") != std::string::npos)
            originalRecoverable = originalRecoverable || Read(entry.path()) == "original bytes";
    EXPECT_TRUE(originalRecoverable);
    EXPECT_TRUE(std::filesystem::exists(created));
    EXPECT_FALSE(std::filesystem::exists(last));
}
