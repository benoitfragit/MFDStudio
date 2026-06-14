/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorProblemNavigation.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
using editor::MatchProblemPageIndex;

const std::vector<std::string> kPages {"radar", "tactical", "nav"};

TEST(EditorProblemNavigationTests, MatchesPageScopedContext)
{
    EXPECT_EQ(MatchProblemPageIndex(kPages, "page/radar"), 0);
    EXPECT_EQ(MatchProblemPageIndex(kPages, "page/nav"), 2);
}

TEST(EditorProblemNavigationTests, MatchesPageOfReticleScopedContext)
{
    EXPECT_EQ(MatchProblemPageIndex(kPages, "page/tactical/reticle/ownship"), 1);
}

TEST(EditorProblemNavigationTests, ReturnsMinusOneForUnknownPage)
{
    EXPECT_EQ(MatchProblemPageIndex(kPages, "page/missing"), -1);
}

TEST(EditorProblemNavigationTests, ReturnsMinusOneForNonPageContext)
{
    EXPECT_EQ(MatchProblemPageIndex(kPages, "window"), -1);
    EXPECT_EQ(MatchProblemPageIndex(kPages, "reticle/track"), -1);
    EXPECT_EQ(MatchProblemPageIndex(kPages, ""), -1);
    EXPECT_EQ(MatchProblemPageIndex(kPages, "page/"), -1);
}
} // namespace
