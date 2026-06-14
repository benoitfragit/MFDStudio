/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorAssetWatcher.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
class ScopedTempDir
{
public:
    ScopedTempDir()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_asset_watcher_tests_" + std::to_string(ticks));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

std::filesystem::path WriteFile(const std::filesystem::path& path, const char* content)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << content;
    return path;
}

void BumpModificationTime(const std::filesystem::path& path)
{
    const std::filesystem::file_time_type current = std::filesystem::last_write_time(path);
    std::filesystem::last_write_time(path, current + std::chrono::seconds(10));
}

TEST(EditorAssetWatcherTests, ReportsNoChangeRightAfterWatching)
{
    ScopedTempDir dir;
    const auto file = WriteFile(dir.Path() / "page.json", "{}");

    editor::EditorAssetWatcher watcher;
    watcher.Watch({file});
    EXPECT_EQ(watcher.WatchedCount(), 1U);
    EXPECT_FALSE(watcher.DetectExternalChange());
}

TEST(EditorAssetWatcherTests, DetectsAnExternalModificationOnce)
{
    ScopedTempDir dir;
    const auto file = WriteFile(dir.Path() / "page.json", "{}");

    editor::EditorAssetWatcher watcher;
    watcher.Watch({file});

    BumpModificationTime(file);
    EXPECT_TRUE(watcher.DetectExternalChange());
    // The change is re-baselined, so it is not reported again.
    EXPECT_FALSE(watcher.DetectExternalChange());
}

TEST(EditorAssetWatcherTests, DetectsChangeInAnyWatchedFile)
{
    ScopedTempDir dir;
    const auto first = WriteFile(dir.Path() / "window.json", "{}");
    const auto second = WriteFile(dir.Path() / "reticle.json", "{}");

    editor::EditorAssetWatcher watcher;
    watcher.Watch({first, second});

    BumpModificationTime(second);
    EXPECT_TRUE(watcher.DetectExternalChange());
}

TEST(EditorAssetWatcherTests, SkipsMissingFilesAndClearResets)
{
    ScopedTempDir dir;
    const auto file = WriteFile(dir.Path() / "page.json", "{}");

    editor::EditorAssetWatcher watcher;
    watcher.Watch({file, dir.Path() / "does_not_exist.json"});
    EXPECT_EQ(watcher.WatchedCount(), 1U);

    watcher.Clear();
    EXPECT_EQ(watcher.WatchedCount(), 0U);
    EXPECT_FALSE(watcher.DetectExternalChange());
}
} // namespace
