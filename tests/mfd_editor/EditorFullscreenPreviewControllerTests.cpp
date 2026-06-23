/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering fullscreen preview layout capture and restoration.
 */

#include "EditorFullscreenPreviewController.h"

#include <gtest/gtest.h>

namespace
{
editor::FullscreenPreviewLayoutState MakeLayoutState()
{
    editor::FullscreenPreviewLayoutState state;
    state.sidebarVisible = true;
    state.inspectorVisible = true;
    state.layerInspectorVisible = true;
    state.minimapVisible = true;
    state.problemsVisible = true;
    state.sidebarWidth = 320.0f;
    state.inspectorWidth = 360.0f;
    return state;
}
} // namespace

TEST(FullscreenPreviewControllerTests, EnterActivatesFullscreenAndHidesEditorPanels)
{
    editor::FullscreenPreviewController controller;
    const editor::FullscreenPreviewLayoutState current = MakeLayoutState();

    const editor::FullscreenPreviewTransition transition = controller.Enter(current);

    EXPECT_TRUE(transition.changed);
    EXPECT_TRUE(controller.IsActive());
    EXPECT_FALSE(transition.state.sidebarVisible);
    EXPECT_FALSE(transition.state.inspectorVisible);
    EXPECT_FALSE(transition.state.layerInspectorVisible);
    EXPECT_FALSE(transition.state.minimapVisible);
    EXPECT_FALSE(transition.state.problemsVisible);
    EXPECT_FLOAT_EQ(transition.state.sidebarWidth, 320.0f);
    EXPECT_FLOAT_EQ(transition.state.inspectorWidth, 360.0f);
}

TEST(FullscreenPreviewControllerTests, ExitRestoresPreviousLayoutExactly)
{
    editor::FullscreenPreviewController controller;
    const editor::FullscreenPreviewLayoutState current = MakeLayoutState();
    const editor::FullscreenPreviewTransition entered = controller.Enter(current);
    EXPECT_TRUE(entered.changed);

    const editor::FullscreenPreviewTransition transition = controller.Exit();

    EXPECT_TRUE(transition.changed);
    EXPECT_FALSE(controller.IsActive());
    EXPECT_TRUE(transition.state.sidebarVisible);
    EXPECT_TRUE(transition.state.inspectorVisible);
    EXPECT_TRUE(transition.state.layerInspectorVisible);
    EXPECT_TRUE(transition.state.minimapVisible);
    EXPECT_TRUE(transition.state.problemsVisible);
    EXPECT_FLOAT_EQ(transition.state.sidebarWidth, 320.0f);
    EXPECT_FLOAT_EQ(transition.state.inspectorWidth, 360.0f);
}

TEST(FullscreenPreviewControllerTests, ToggleAlternatesBetweenFullscreenAndCapturedLayout)
{
    editor::FullscreenPreviewController controller;
    const editor::FullscreenPreviewLayoutState current = MakeLayoutState();

    const editor::FullscreenPreviewTransition entered = controller.Toggle(current);
    ASSERT_TRUE(entered.changed);
    ASSERT_TRUE(controller.IsActive());

    const editor::FullscreenPreviewTransition exited = controller.Toggle(current);
    EXPECT_TRUE(exited.changed);
    EXPECT_FALSE(controller.IsActive());
    EXPECT_TRUE(exited.state.sidebarVisible);
    EXPECT_TRUE(exited.state.inspectorVisible);
}

TEST(FullscreenPreviewControllerTests, RepeatedEnterExitKeepsOriginalCapturedState)
{
    editor::FullscreenPreviewController controller;
    const editor::FullscreenPreviewLayoutState current = MakeLayoutState();

    const editor::FullscreenPreviewTransition firstEnter = controller.Enter(current);
    EXPECT_TRUE(firstEnter.changed);

    const editor::FullscreenPreviewTransition secondEnter = controller.Enter(MakeLayoutState());
    EXPECT_FALSE(secondEnter.changed);
    EXPECT_TRUE(controller.IsActive());

    const editor::FullscreenPreviewTransition exit = controller.Exit();
    EXPECT_TRUE(exit.changed);
    EXPECT_TRUE(exit.state.sidebarVisible);
    EXPECT_TRUE(exit.state.inspectorVisible);
}

TEST(FullscreenPreviewControllerTests, ExitWithoutActiveFullscreenDoesNothing)
{
    editor::FullscreenPreviewController controller;

    const editor::FullscreenPreviewTransition transition = controller.Exit();

    EXPECT_FALSE(transition.changed);
    EXPECT_FALSE(controller.IsActive());
}
