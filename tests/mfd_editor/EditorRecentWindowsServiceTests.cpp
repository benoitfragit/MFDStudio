/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests for the editor recent-windows resume-hub service.
 */

#include "EditorRecentWindowsService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace
{
class ScopedTempDir
{
public:
    ScopedTempDir()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_editor_recent_windows_tests_" + std::to_string(ticks));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::filesystem::path WriteFile(const std::filesystem::path& path)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << "{}";
    return path;
}
} // namespace

TEST(EditorRecentWindowsServiceTests, RemembersMostRecentFirstAndDeduplicates)
{
    editor::EditorRecentWindowsService service;
    service.Remember("a.json");
    service.Remember("b.json");
    service.Remember("a.json");

    ASSERT_EQ(service.Entries().size(), 2U);
    EXPECT_EQ(service.Entries()[0].generic_string(), "a.json");
    EXPECT_EQ(service.Entries()[1].generic_string(), "b.json");
}

TEST(EditorRecentWindowsServiceTests, IgnoresEmptyPaths)
{
    editor::EditorRecentWindowsService service;
    service.Remember(std::filesystem::path {});
    EXPECT_TRUE(service.Entries().empty());
}

TEST(EditorRecentWindowsServiceTests, CapsHistoryToMaxEntries)
{
    editor::EditorRecentWindowsService service;
    for (std::size_t index = 0; index < editor::EditorRecentWindowsService::kMaxEntries + 4U; ++index)
    {
        service.Remember(std::to_string(index) + ".json");
    }

    ASSERT_EQ(service.Entries().size(), editor::EditorRecentWindowsService::kMaxEntries);
    // The most recently remembered asset stays at the front and the oldest ones fall off.
    EXPECT_EQ(service.Entries().front().generic_string(),
              std::to_string(editor::EditorRecentWindowsService::kMaxEntries + 3U) + ".json");
}

TEST(EditorRecentWindowsServiceTests, ExistingEntriesFiltersMissingPaths)
{
    ScopedTempDir workspace;
    const std::filesystem::path present = WriteFile(workspace.Path() / "present.json");
    const std::filesystem::path missing = workspace.Path() / "missing.json";

    editor::EditorRecentWindowsService service;
    service.Remember(missing);
    service.Remember(present);

    const std::vector<std::filesystem::path> existing = service.ExistingEntries();
    ASSERT_EQ(existing.size(), 1U);
    EXPECT_EQ(existing.front().filename().generic_string(), "present.json");
}

TEST(EditorRecentWindowsServiceTests, SaveAndLoadRoundTripPreservesOrder)
{
    ScopedTempDir workspace;
    const std::filesystem::path storage = workspace.Path() / "recent.json";

    editor::EditorRecentWindowsService saved;
    saved.Remember("first.json");
    saved.Remember("second.json");
    saved.Save(storage);

    editor::EditorRecentWindowsService loaded;
    loaded.Load(storage);

    ASSERT_EQ(loaded.Entries().size(), 2U);
    EXPECT_EQ(loaded.Entries()[0].generic_string(), "second.json");
    EXPECT_EQ(loaded.Entries()[1].generic_string(), "first.json");
}

TEST(EditorRecentWindowsServiceTests, LoadMissingFileLeavesListEmpty)
{
    ScopedTempDir workspace;

    editor::EditorRecentWindowsService service;
    service.Remember("stale.json");
    service.Load(workspace.Path() / "does_not_exist.json");

    EXPECT_TRUE(service.Entries().empty());
}

TEST(EditorRecentWindowsServiceTests, LoadDeduplicatesAndIgnoresNonStringEntries)
{
    ScopedTempDir workspace;
    const std::filesystem::path storage = workspace.Path() / "recent.json";
    {
        std::ofstream stream(storage, std::ios::binary | std::ios::trunc);
        stream << R"(["a.json", 42, "a.json", "b.json"])";
    }

    editor::EditorRecentWindowsService service;
    service.Load(storage);

    ASSERT_EQ(service.Entries().size(), 2U);
    EXPECT_EQ(service.Entries()[0].generic_string(), "a.json");
    EXPECT_EQ(service.Entries()[1].generic_string(), "b.json");
}
