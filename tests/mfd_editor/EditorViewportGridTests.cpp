/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the bounded shared viewport-grid layout.
 */

#include "internal/application/EditorViewportGrid.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

namespace
{
std::int64_t CountMajorLines(const std::int64_t firstIndex,
                             const std::int64_t lastIndex,
                             const int majorLineMultiple)
{
    if (majorLineMultiple <= 0 || lastIndex < firstIndex)
    {
        return 0;
    }

    std::int64_t firstMajorIndex = firstIndex;
    int attempts = 0;
    while ((firstMajorIndex % majorLineMultiple) != 0 && attempts < majorLineMultiple)
    {
        ++firstMajorIndex;
        ++attempts;
    }

    if (firstMajorIndex > lastIndex)
    {
        return 0;
    }

    const long double span = static_cast<long double>(lastIndex) - static_cast<long double>(firstMajorIndex);
    return static_cast<std::int64_t>(std::floor(span / static_cast<long double>(majorLineMultiple))) + 1;
}
} // namespace

TEST(EditorViewportGridTests, SanitizeGridStepLogicalClampsToSupportedRange)
{
    EXPECT_FLOAT_EQ(editor::app::SanitizeGridStepLogical(0.001f), editor::app::kMinGridStepLogical);
    EXPECT_FLOAT_EQ(editor::app::SanitizeGridStepLogical(0.25f), 0.25f);
    EXPECT_FLOAT_EQ(editor::app::SanitizeGridStepLogical(1.0f), editor::app::kMaxGridStepLogical);
    EXPECT_FLOAT_EQ(editor::app::SanitizeGridStepLogical(std::numeric_limits<float>::quiet_NaN()),
                    editor::app::kDefaultGridStepLogical);
}

TEST(EditorViewportGridTests, NormalZoomKeepsMinorLinesVisible)
{
    const editor::app::ViewportGridLayout layout = editor::app::BuildViewportGridLayout(
        editor::app::ViewportGridInput {800, 600, mfd::PageViewState {}, 0.05f});

    EXPECT_TRUE(layout.valid);
    EXPECT_TRUE(layout.showMinorLines);
    EXPECT_EQ(layout.majorLineMultiple, editor::app::kViewportGridMajorLineMultiple);
}

TEST(EditorViewportGridTests, LowZoomHidesMinorLinesAndRarefiesMajorGrid)
{
    mfd::PageViewState view;
    view.zoom = 0.1f;

    const editor::app::ViewportGridLayout layout = editor::app::BuildViewportGridLayout(
        editor::app::ViewportGridInput {800, 600, view, 0.05f});

    EXPECT_TRUE(layout.valid);
    EXPECT_FALSE(layout.showMinorLines);
    EXPECT_GT(layout.majorLineMultiple, editor::app::kViewportGridMajorLineMultiple);
}

TEST(EditorViewportGridTests, MajorGridCountStaysBoundedOnVeryLargeVisibleRange)
{
    mfd::PageViewState view;
    view.zoom = 0.1f;

    const editor::app::ViewportGridLayout layout = editor::app::BuildViewportGridLayout(
        editor::app::ViewportGridInput {4096, 4096, view, 0.01f});

    ASSERT_TRUE(layout.valid);
    const std::int64_t verticalMajorLines =
        CountMajorLines(layout.firstVerticalIndex, layout.lastVerticalIndex, layout.majorLineMultiple);
    const std::int64_t horizontalMajorLines =
        CountMajorLines(layout.firstHorizontalIndex, layout.lastHorizontalIndex, layout.majorLineMultiple);
    EXPECT_LE(verticalMajorLines, editor::app::kViewportGridMaxMajorLinesPerAxis);
    EXPECT_LE(horizontalMajorLines, editor::app::kViewportGridMaxMajorLinesPerAxis);
}
