/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorInspectorStringEdit.h"
#include "internal/application/EditorLineEndpointMarkers.h"

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

TEST(EditorInspectorEdit, EndpointPinsSeparateCoincidentCornersAndSkipOffscreenAnchors)
{
    for (ImVec2 point : {ImVec2(0, 0), ImVec2(100, 100), ImVec2(50, 50)})
    {
        const auto pins = editor::app::PlaceLineEndpointMarkers({0, 0}, {100, 100}, point, point, 10);
        ASSERT_TRUE(pins[0] && pins[1]);
        EXPECT_GE(std::hypot(pins[0]->x - pins[1]->x, pins[0]->y - pins[1]->y), 20);
        for (const auto& pin : pins)
        {
            EXPECT_GE(pin->x, 10);
            EXPECT_LE(pin->x, 90);
            EXPECT_GE(pin->y, 10);
            EXPECT_LE(pin->y, 90);
        }
    }
    const auto pins = editor::app::PlaceLineEndpointMarkers({0, 0}, {100, 100}, {-1, 0}, {50, 50}, 10);
    EXPECT_FALSE(pins[0]);
    EXPECT_TRUE(pins[1]);
}

TEST(EditorInspectorEdit, CatalogDeletionPreservesTheSelectedPrimitiveAndOrder)
{
    mfd::ReticleGroup reticle;
    reticle.primitives.resize(4);
    for (std::size_t i = 0; i < reticle.primitives.size(); ++i)
    {
        reticle.primitives[i].id = std::to_string(i);
    }

    reticle.clipping.primitiveId = "2";
    using editor::app::EraseLibraryPrimitive;
    EXPECT_EQ(EraseLibraryPrimitive(reticle, -1, 2), 2);
    EXPECT_EQ(EraseLibraryPrimitive(reticle, 4, 2), 2);
    ASSERT_EQ(reticle.primitives.size(), 4U);
    EXPECT_EQ(EraseLibraryPrimitive(reticle, 1, 2), 1);
    ASSERT_EQ(reticle.primitives.size(), 3U);
    EXPECT_EQ(reticle.primitives[0].id, "0");
    EXPECT_EQ(reticle.primitives[1].id, "2");
    EXPECT_EQ(reticle.primitives[2].id, "3");
    EXPECT_EQ(EraseLibraryPrimitive(reticle, 2, 1), 1);
    EXPECT_EQ(EraseLibraryPrimitive(reticle, 0, -1), -1);
    ASSERT_EQ(reticle.primitives.size(), 1U);
    EXPECT_EQ(reticle.primitives.front().id, "2");
    EXPECT_EQ(reticle.clipping.primitiveId, "2");
    EXPECT_EQ(EraseLibraryPrimitive(reticle, 0, 0), -1);
    EXPECT_TRUE(reticle.primitives.empty());
    EXPECT_TRUE(reticle.clipping.primitiveId.empty());
}

TEST(EditorInspectorEdit, BezierControlRemovalStopsAtTheSerializableMinimum)
{
    mfd::BezierGeometry curve;
    for (std::size_t count = 0; count <= 3; ++count)
    {
        curve.controlPoints.resize(count);
        EXPECT_EQ(editor::app::CanRemoveBezierControl(curve), count > 2);
    }
}

TEST(EditorInspectorEdit, AppendSkipsOccupiedNormalizedPrimitiveIds)
{
    mfd::ReticleGroup group;
    group.primitives.resize(2);
    group.primitives[0].id = "PRIMITIVE_3";
    group.primitives[1].id = "primitive_4";
    EXPECT_EQ(editor::app::NextLibraryPrimitiveId(group), "primitive_5");
}

TEST(EditorInspectorEdit, RenameValidatesIdsAndPreservesExposedAndClippingReferences)
{
    mfd::MfdDocument document;
    auto& source = document.reticleLibrary["template"];
    source.id = "template";
    source.primitives.resize(2);
    source.primitives[0].id = "outline";
    source.primitives[0].exposed = true;
    source.primitives[1].id = "other";
    source.clipping.primitiveId = "outline";
    document.pages.emplace_back();
    auto& page = document.pages.back();
    page.staticReticles.push_back(source);
    page.staticReticles.back().sourceTemplateId = source.id;
    page.strobes.emplace_back();
    page.strobes.back().reticle = page.staticReticles.back();
    EXPECT_FALSE(editor::app::RenamePrimitiveInCandidate(document, "template", 0, " OTHER "));
    EXPECT_FALSE(editor::app::RenamePrimitiveInCandidate(document, "template", 0, " "));
    ASSERT_TRUE(editor::app::RenamePrimitiveInCandidate(document, "template", 0, "mask"));
    for (const auto* group : {&source, &page.staticReticles.front(), &page.strobes.front().reticle})
    {
        EXPECT_EQ(group->primitives[0].id, "mask");
        EXPECT_TRUE(group->primitives[0].exposed);
        EXPECT_EQ(group->clipping.primitiveId, "mask");
    }
}
