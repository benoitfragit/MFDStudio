/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering persisted editor-only grid preferences.
 */

#include "EditorUiStatePersistence.h"
#include "internal/application/EditorViewportGrid.h"

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
                std::filesystem::path("mfd_editor_ui_state_tests_" + std::to_string(ticks));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};
} // namespace

TEST(EditorUiStatePersistenceTests, SaveAndLoadRoundTripGridPreferences)
{
    ScopedTempDir tempDir;
    const std::filesystem::path file = tempDir.Path() / "editor_ui_state.json";

    editor::EditorUiPersistentState saved;
    saved.sidebarWidth = 320.0f;
    saved.inspectorWidth = 420.0f;
    saved.libraryStudioPageWidth = 540.0f;
    saved.showGrid = false;
    saved.snapToGrid = true;
    saved.gridStepLogical = 0.125f;
    saved.sectionOpen["page.preview"] = true;

    editor::SaveEditorUiState(file, saved);
    const editor::EditorUiPersistentState loaded = editor::LoadEditorUiState(file);

    ASSERT_TRUE(loaded.sidebarWidth.has_value());
    ASSERT_TRUE(loaded.inspectorWidth.has_value());
    ASSERT_TRUE(loaded.libraryStudioPageWidth.has_value());
    ASSERT_TRUE(loaded.showGrid.has_value());
    ASSERT_TRUE(loaded.snapToGrid.has_value());
    ASSERT_TRUE(loaded.gridStepLogical.has_value());
    EXPECT_FLOAT_EQ(*loaded.sidebarWidth, 320.0f);
    EXPECT_FLOAT_EQ(*loaded.inspectorWidth, 420.0f);
    EXPECT_FLOAT_EQ(*loaded.libraryStudioPageWidth, 540.0f);
    EXPECT_FALSE(*loaded.showGrid);
    EXPECT_TRUE(*loaded.snapToGrid);
    EXPECT_FLOAT_EQ(*loaded.gridStepLogical, 0.125f);
    ASSERT_EQ(loaded.sectionOpen.size(), 1U);
    EXPECT_TRUE(loaded.sectionOpen.at("page.preview"));
}

TEST(EditorUiStatePersistenceTests, LoadIgnoresInvalidGridFlagsAndClampsGridStep)
{
    ScopedTempDir tempDir;
    const std::filesystem::path file = tempDir.Path() / "editor_ui_state.json";

    std::ofstream stream(file, std::ios::trunc);
    ASSERT_TRUE(stream.is_open());
    stream << R"json({
  "sidebarWidth": 280.0,
  "libraryStudioPageWidth": -5.0,
  "showGrid": "yes",
  "snapToGrid": false,
  "gridStepLogical": 2.0,
  "sections": {
    "visible": true,
    "invalid": "no"
  }
})json";
    stream.close();

    const editor::EditorUiPersistentState loaded = editor::LoadEditorUiState(file);

    ASSERT_TRUE(loaded.sidebarWidth.has_value());
    EXPECT_FLOAT_EQ(*loaded.sidebarWidth, 280.0f);
    EXPECT_FALSE(loaded.libraryStudioPageWidth.has_value());
    EXPECT_FALSE(loaded.showGrid.has_value());
    ASSERT_TRUE(loaded.snapToGrid.has_value());
    EXPECT_FALSE(*loaded.snapToGrid);
    ASSERT_TRUE(loaded.gridStepLogical.has_value());
    EXPECT_FLOAT_EQ(*loaded.gridStepLogical, editor::app::kMaxGridStepLogical);
    ASSERT_EQ(loaded.sectionOpen.size(), 1U);
    EXPECT_TRUE(loaded.sectionOpen.at("visible"));
}
