/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorSidebarFilter.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
using editor::ParseSidebarFilter;
using editor::PagePreviewProblem;
using editor::SidebarFilterScope;
using editor::SummarizeSidebarProblems;

TEST(EditorSidebarFilterTests, PlainTextStaysScopeAll)
{
    const editor::SidebarFilterQuery query = ParseSidebarFilter("radar");
    EXPECT_EQ(query.scope, SidebarFilterScope::All);
    EXPECT_EQ(query.text, "radar");
}

TEST(EditorSidebarFilterTests, EmptyFilterIsNeutral)
{
    const editor::SidebarFilterQuery query = ParseSidebarFilter("   ");
    EXPECT_EQ(query.scope, SidebarFilterScope::All);
    EXPECT_TRUE(query.text.empty());
    EXPECT_FALSE(editor::SidebarFilterIsActive(query));
}

TEST(EditorSidebarFilterTests, ParsesPageReticleAndProblemTokens)
{
    EXPECT_EQ(ParseSidebarFilter("page:nav").scope, SidebarFilterScope::Pages);
    EXPECT_EQ(ParseSidebarFilter("reticle:track").scope, SidebarFilterScope::Reticles);
    EXPECT_EQ(ParseSidebarFilter("problem:").scope, SidebarFilterScope::Problems);
}

TEST(EditorSidebarFilterTests, TokenMatchingIsCaseInsensitiveAndTrimmed)
{
    const editor::SidebarFilterQuery query = ParseSidebarFilter("  PAGE:  Radar  ");
    EXPECT_EQ(query.scope, SidebarFilterScope::Pages);
    EXPECT_EQ(query.text, "Radar");
}

TEST(EditorSidebarFilterTests, TokenWithoutQueryKeepsActiveScope)
{
    const editor::SidebarFilterQuery query = ParseSidebarFilter("problem:");
    EXPECT_EQ(query.scope, SidebarFilterScope::Problems);
    EXPECT_TRUE(query.text.empty());
    EXPECT_TRUE(editor::SidebarFilterIsActive(query));
}

TEST(EditorSidebarFilterTests, UnknownPrefixStaysFreeText)
{
    const editor::SidebarFilterQuery query = ParseSidebarFilter("layer:top");
    EXPECT_EQ(query.scope, SidebarFilterScope::All);
    EXPECT_EQ(query.text, "layer:top");
}

TEST(EditorSidebarFilterTests, ScopeVisibilityPredicates)
{
    EXPECT_TRUE(editor::SidebarFilterShowsPages(SidebarFilterScope::Pages));
    EXPECT_FALSE(editor::SidebarFilterShowsReticles(SidebarFilterScope::Pages));
    EXPECT_FALSE(editor::SidebarFilterShowsPages(SidebarFilterScope::Reticles));
    EXPECT_TRUE(editor::SidebarFilterShowsReticles(SidebarFilterScope::Reticles));
    EXPECT_TRUE(editor::SidebarFilterShowsPages(SidebarFilterScope::Problems));
    EXPECT_TRUE(editor::SidebarFilterShowsReticles(SidebarFilterScope::Problems));
}

TEST(EditorSidebarFilterTests, SummaryFoldsContextsIntoEntitySets)
{
    const std::vector<std::string> normalizedPageIds {"radar", "nav"};
    const std::vector<PagePreviewProblem> problems {
        PagePreviewProblem {"page name", "page/radar"},
        PagePreviewProblem {"page reticle", "page/nav/reticle/track"},
        PagePreviewProblem {"library reticle", "reticle/crosshair"},
        PagePreviewProblem {"window layout", "window"},
    };

    const editor::SidebarProblemSummary summary = SummarizeSidebarProblems(problems, normalizedPageIds);
    EXPECT_EQ(summary.total, 4U);
    EXPECT_TRUE(editor::SidebarProblemSummaryHasPage(summary, 0));
    EXPECT_TRUE(editor::SidebarProblemSummaryHasPage(summary, 1));
    EXPECT_TRUE(editor::SidebarProblemSummaryHasReticle(summary, "crosshair"));
    EXPECT_FALSE(editor::SidebarProblemSummaryHasReticle(summary, "track"));
}

TEST(EditorSidebarFilterTests, SummaryIgnoresUnknownPagesButCountsTotal)
{
    const std::vector<std::string> normalizedPageIds {"radar"};
    const std::vector<PagePreviewProblem> problems {
        PagePreviewProblem {"missing page", "page/ghost"},
    };

    const editor::SidebarProblemSummary summary = SummarizeSidebarProblems(problems, normalizedPageIds);
    EXPECT_EQ(summary.total, 1U);
    EXPECT_TRUE(summary.pages.empty());
    EXPECT_TRUE(summary.libraryReticles.empty());
}
} // namespace
