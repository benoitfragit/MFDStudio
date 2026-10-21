/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Undo-friendly editor domain mutation tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerEditorModel.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"

#include <string>

#include <gtest/gtest.h>

TEST(ContainerEditorModelTests, SupportsAddRenameDuplicateReparentAndDelete)
{
    mfd::PageDefinition page;
    const std::string root = mfd::api1::ContainerEditorModel::AddRoot(page, mfd::api1::ContainerType::Linear);
    std::string error;
    const std::string child =
        mfd::api1::ContainerEditorModel::AddChild(page, root, mfd::api1::ContainerType::Grid, error);
    ASSERT_FALSE(child.empty()) << error;
    ASSERT_EQ(page.containers.size(), 2U);
    ASSERT_TRUE(mfd::api1::ContainerEditorModel::Rename(page, child, "renamed", error)) << error;

    const std::string duplicate = mfd::api1::ContainerEditorModel::Duplicate(page, root, error);
    ASSERT_FALSE(duplicate.empty()) << error;
    ASSERT_EQ(page.containers.size(), 4U);
    EXPECT_FALSE(mfd::api1::ContainerEditorModel::Reparent(page, root, "renamed", error));
    EXPECT_NE(error.find("cycle"), std::string::npos);
    ASSERT_TRUE(mfd::api1::ContainerEditorModel::Reparent(page, "renamed", {}, error)) << error;
    ASSERT_TRUE(mfd::api1::ContainerEditorModel::Delete(page, root, error)) << error;
    EXPECT_EQ(page.containers.size(), 3U);
}

TEST(ContainerEditorModelTests, RejectedMutationLeavesHierarchyUnchanged)
{
    mfd::PageDefinition page;
    const std::string root = mfd::api1::ContainerEditorModel::AddRoot(page, mfd::api1::ContainerType::Canvas);
    std::string error;
    const std::string child =
        mfd::api1::ContainerEditorModel::AddChild(page, root, mfd::api1::ContainerType::Overlay, error);
    ASSERT_FALSE(child.empty());
    const mfd::PageDefinition before = page;
    EXPECT_FALSE(mfd::api1::ContainerEditorModel::Reparent(page, root, child, error));
    ASSERT_EQ(page.containers.size(), before.containers.size());
    EXPECT_EQ(page.containers.front().children.front().childId,
              before.containers.front().children.front().childId);
}

TEST(ContainerEditorModelTests, NewContainersAreVisibleAndNormalizedRenameCollisionsAreRejected)
{
    mfd::PageDefinition page;
    const std::string first =
        mfd::api1::ContainerEditorModel::AddRoot(page, mfd::api1::ContainerType::Canvas);
    const std::string second =
        mfd::api1::ContainerEditorModel::AddRoot(page, mfd::api1::ContainerType::Canvas);
    ASSERT_EQ(page.containers.size(), 2U);
    EXPECT_TRUE(page.containers.front().appearance.backgroundVisible);
    EXPECT_TRUE(page.containers.front().appearance.borderVisible);
    EXPECT_GT(page.containers.front().appearance.backgroundColor.a, 0U);

    std::string error;
    ASSERT_TRUE(mfd::api1::ContainerEditorModel::Rename(page, first, "MainPanel", error)) << error;
    EXPECT_FALSE(mfd::api1::ContainerEditorModel::Rename(page, second, " mainpanel ", error));
    EXPECT_NE(error.find("already"), std::string::npos);
}

TEST(ContainerEditorModelTests, ExcessiveDepthRollsBackAddAndReparent)
{
    mfd::PageDefinition page;
    for (std::size_t index = 0U; index < mfd::api1::kMaximumContainerDepth; ++index)
    {
        mfd::api1::ContainerDefinition node;
        node.id = "node" + std::to_string(index);
        if (index > 0U) page.containers.back().children.push_back({node.id});
        page.containers.push_back(std::move(node));
    }
    const std::string leafId = page.containers.back().id;
    std::string error;
    EXPECT_TRUE(mfd::api1::ContainerEditorModel::AddChild(
        page, leafId, mfd::api1::ContainerType::Canvas, error).empty());
    EXPECT_EQ(page.containers.size(), mfd::api1::kMaximumContainerDepth);
    EXPECT_TRUE(page.containers.back().children.empty());
    const std::string extraRoot =
        mfd::api1::ContainerEditorModel::AddRoot(page, mfd::api1::ContainerType::Canvas);
    EXPECT_FALSE(mfd::api1::ContainerEditorModel::Reparent(page, extraRoot, leafId, error));
    EXPECT_TRUE(page.containers[mfd::api1::kMaximumContainerDepth - 1U].children.empty());
    EXPECT_TRUE(mfd::api1::ContainerLayoutEngine {}.Validate(page.containers, error)) << error;
}
