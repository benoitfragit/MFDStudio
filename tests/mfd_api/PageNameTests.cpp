/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <gtest/gtest.h>

#include "mfd/model/PageName.h"

namespace
{
TEST(PageNameTests, PageNamesEqualIgnoresCaseAndSurroundingWhitespace)
{
    EXPECT_TRUE(mfd::PageNamesEqual("Main Page", "main page"));
    EXPECT_TRUE(mfd::PageNamesEqual("  Main Page  ", "MAIN PAGE"));
    EXPECT_TRUE(mfd::PageNamesEqual("\tMain Page\n", " main page "));
}

TEST(PageNameTests, PageNamesEqualKeepsInnerWhitespaceSignificant)
{
    EXPECT_FALSE(mfd::PageNamesEqual("main page", "mainpage"));
    EXPECT_FALSE(mfd::PageNamesEqual("main  page", "main page"));
}

TEST(PageNameTests, PageNamesEqualRejectsDifferentNames)
{
    EXPECT_FALSE(mfd::PageNamesEqual("main", "menu"));
    EXPECT_FALSE(mfd::PageNamesEqual("main", "main2"));
    EXPECT_FALSE(mfd::PageNamesEqual("main", ""));
}

TEST(PageNameTests, PageNamesEqualMatchesNormalizePageNameSemantics)
{
    const char* const samples[] = {"Main", "  MAIN  ", "menu", "", "  ", "a b", "A  B"};
    for (const char* lhs : samples)
    {
        for (const char* rhs : samples)
        {
            EXPECT_EQ(mfd::PageNamesEqual(lhs, rhs),
                      mfd::NormalizePageName(lhs) == mfd::NormalizePageName(rhs))
                << "lhs='" << lhs << "' rhs='" << rhs << "'";
        }
    }
}

TEST(PageNameTests, PageNameIsBlankMatchesEmptyNormalization)
{
    EXPECT_TRUE(mfd::PageNameIsBlank(""));
    EXPECT_TRUE(mfd::PageNameIsBlank("   "));
    EXPECT_TRUE(mfd::PageNameIsBlank("\t\r\n"));
    EXPECT_FALSE(mfd::PageNameIsBlank("main"));
    EXPECT_FALSE(mfd::PageNameIsBlank("  x  "));
}
} // namespace
