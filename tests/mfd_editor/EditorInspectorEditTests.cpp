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

TEST(EditorInspectorEdit, PageOverrideResolvesTheNamedPrimitiveAfterReordering)
{
    mfd::ReticleGroup reticle;
    for (const char* id : {"one", "two", "three"})
    {
        mfd::Primitive primitive;
        primitive.id = id;
        primitive.geometry = mfd::TextGeometry {};
        reticle.primitives.push_back(primitive);
    }
    EXPECT_EQ(editor::app::FindPageTextOverride(reticle, "two"), 1);
    std::swap(reticle.primitives[0], reticle.primitives[1]);
    EXPECT_EQ(editor::app::FindPageTextOverride(reticle, "two"), 0);
    reticle.primitives[0].geometry = mfd::LineGeometry {};
    EXPECT_EQ(editor::app::FindPageTextOverride(reticle, "two"), -1);
    reticle.primitives[1].id.clear();
    reticle.primitives[2].geometry = mfd::TimeGeometry {};
    EXPECT_EQ(editor::app::FindPageTextOverride(reticle, {}), 2);
}

TEST(EditorInspectorEdit, RemovingAnyPolylinePointPreservesOrderAndClosure)
{
    for (bool closed : {false, true})
    {
        for (std::size_t removed : {0U, 1U, 3U})
        {
            mfd::PolylineGeometry path;
            path.closed = closed;
            path.points = {{0, 0}, {1, 2}, {2, 4}, {3, 6}};
            ASSERT_TRUE(editor::app::RemovePolylinePoint(path, removed));
            ASSERT_EQ(path.points.size(), 3U);
            EXPECT_EQ(path.closed, closed);
            for (std::size_t i = 0; i < path.points.size(); ++i)
            {
                const float expected = static_cast<float>(i < removed ? i : i + 1U);
                EXPECT_EQ(path.points[i].x, expected);
                EXPECT_EQ(path.points[i].y, 2 * expected);
            }
            EXPECT_FALSE(editor::app::RemovePolylinePoint(path, 3));
            EXPECT_TRUE(editor::app::RemovePolylinePoint(path, 1));
            EXPECT_FALSE(editor::app::RemovePolylinePoint(path, 0));
            EXPECT_EQ(path.points.size(), 2U);
        }
    }
}
