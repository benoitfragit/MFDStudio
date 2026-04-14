/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for ArrayView.
 */

#include <array>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "mfd/core/ArrayView.h"

TEST(ArrayViewTests, SupportsRawPointerConstruction)
{
    const std::array<int, 4> values {1, 2, 3, 4};
    const mfd::ArrayView<const int> view(values.data(), values.size());

    ASSERT_EQ(view.size(), values.size());
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[3], 4);
}

TEST(ArrayViewTests, SupportsVectorConstruction)
{
    std::vector<int> values {10, 20, 30};
    mfd::ArrayView<int> mutableView(values);
    mfd::ArrayView<const int> constView(values);

    ASSERT_EQ(mutableView.size(), values.size());
    EXPECT_FALSE(mutableView.empty());
    mutableView[1] = 99;
    EXPECT_EQ(values[1], 99);

    ASSERT_EQ(constView.size(), values.size());
    EXPECT_EQ(constView[2], 30);
}

TEST(ArrayViewTests, SupportsArrayConstruction)
{
    const std::array<std::byte, 8> bytes {};
    const mfd::ByteView byteView(bytes);

    EXPECT_EQ(byteView.size(), bytes.size());
    EXPECT_EQ(byteView.data(), bytes.data());
}
