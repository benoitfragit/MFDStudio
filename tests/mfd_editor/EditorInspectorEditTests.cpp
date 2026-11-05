/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorInspectorStringEdit.h"

#include "internal/application/EditorApplicationAuthoringSupport.h"

#include <gtest/gtest.h>

TEST(EditorInspectorEdit, OnlyValidEffectiveChangesCreateUndoHistory)
{
    using editor::app::CheckInspectorStringEdit;
    using editor::app::StringEditDecision;
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%H", true), StringEditDecision::Skip);
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%", true), StringEditDecision::RejectFormat);
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%H:%M", true), StringEditDecision::Commit);
    EXPECT_EQ(CheckInspectorStringEdit("changed", "old", "new", false), StringEditDecision::Skip);
    EXPECT_EQ(CheckInspectorStringEdit("title", "title", "50%", false), StringEditDecision::Commit);
}

TEST(EditorInspectorEdit, BothInstanceAndLayerMustBeVisibleOnThePageCanvas)
{
    mfd::PageDefinition page;
    mfd::ReticleGroup reticle;
    reticle.primitives.resize(3);
    for (auto& primitive : reticle.primitives) primitive.geometry = mfd::TextGeometry {};
    reticle.visible = false;
    EXPECT_FALSE(editor::app::IsReticleVisibleInEditor(page, reticle));
    reticle.layerId = "HUD";
    page.editor.layers.push_back({"HUD", true});
    EXPECT_FALSE(editor::app::IsReticleVisibleInEditor(page, reticle));
    reticle.visible = true;
    EXPECT_TRUE(editor::app::IsReticleVisibleInEditor(page, reticle));
    page.editor.layers.front().visible = false;
    EXPECT_FALSE(editor::app::IsReticleVisibleInEditor(page, reticle));
}
