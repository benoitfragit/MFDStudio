/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorIcons.h"

/**
 * @file
 * @brief ImDrawList implementation of the editor's white-silhouette action icons.
 */

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "EditorUiTheme.h"

namespace editor::ui
{
namespace
{
/// Neutral white used for the resting glyph color, matched to the theme text tone.
constexpr ImU32 kGlyphRest = IM_COL32(236, 244, 250, 255);

/// Half-turn in radians, used to lay out the arc-based glyphs without the imgui_internal header.
constexpr float kPi = 3.14159265358979323846f;

/**
 * @brief Geometry passed to every glyph painter: the icon's center, radius and stroke width.
 */
struct GlyphMetrics
{
    ImVec2 center;
    float radius;
    float thickness;
};

void PaintAdd(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    drawList.AddLine(ImVec2(m.center.x - m.radius, m.center.y), ImVec2(m.center.x + m.radius, m.center.y), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x, m.center.y - m.radius), ImVec2(m.center.x, m.center.y + m.radius), col, m.thickness);
}

void PaintSearch(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const ImVec2 ringCenter(m.center.x - m.radius * 0.25f, m.center.y - m.radius * 0.25f);
    const float ringRadius = m.radius * 0.62f;
    drawList.AddCircle(ringCenter, ringRadius, col, 0, m.thickness);
    drawList.AddLine(ImVec2(ringCenter.x + ringRadius * 0.70f, ringCenter.y + ringRadius * 0.70f),
                     ImVec2(m.center.x + m.radius, m.center.y + m.radius),
                     col,
                     m.thickness);
}

void PaintRename(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const ImVec2 tip(m.center.x + m.radius, m.center.y - m.radius);
    const ImVec2 heel(m.center.x - m.radius, m.center.y + m.radius);
    drawList.AddLine(heel, tip, col, m.thickness * 1.8f);
    drawList.AddLine(tip, ImVec2(m.center.x + m.radius * 0.45f, m.center.y - m.radius), col, m.thickness);
    drawList.AddLine(tip, ImVec2(m.center.x + m.radius, m.center.y - m.radius * 0.45f), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 1.05f, m.center.y + m.radius * 0.55f),
                     ImVec2(m.center.x - m.radius * 0.55f, m.center.y + m.radius * 1.05f),
                     col,
                     m.thickness);
}

void PaintDelete(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const float top = m.center.y - m.radius * 0.55f;
    const float bottom = m.center.y + m.radius;
    const float halfTop = m.radius * 0.85f;
    const float halfBottom = m.radius * 0.6f;

    drawList.AddLine(ImVec2(m.center.x - m.radius, top), ImVec2(m.center.x + m.radius, top), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.35f, top), ImVec2(m.center.x - m.radius * 0.35f, top - m.radius * 0.35f), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x + m.radius * 0.35f, top), ImVec2(m.center.x + m.radius * 0.35f, top - m.radius * 0.35f), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.35f, top - m.radius * 0.35f), ImVec2(m.center.x + m.radius * 0.35f, top - m.radius * 0.35f), col, m.thickness);

    drawList.AddLine(ImVec2(m.center.x - halfTop, top), ImVec2(m.center.x - halfBottom, bottom), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x + halfTop, top), ImVec2(m.center.x + halfBottom, bottom), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x - halfBottom, bottom), ImVec2(m.center.x + halfBottom, bottom), col, m.thickness);

    const float ribTop = top + m.radius * 0.2f;
    const float ribBottom = bottom - m.radius * 0.15f;
    drawList.AddLine(ImVec2(m.center.x, ribTop), ImVec2(m.center.x, ribBottom), col, m.thickness * 0.8f);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.42f, ribTop), ImVec2(m.center.x - m.radius * 0.38f, ribBottom), col, m.thickness * 0.8f);
    drawList.AddLine(ImVec2(m.center.x + m.radius * 0.42f, ribTop), ImVec2(m.center.x + m.radius * 0.38f, ribBottom), col, m.thickness * 0.8f);
}

void PaintRemoveFromWindow(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const ImVec2 apex(m.center.x, m.center.y - m.radius);
    const ImVec2 left(m.center.x - m.radius * 0.9f, m.center.y + m.radius * 0.25f);
    const ImVec2 right(m.center.x + m.radius * 0.9f, m.center.y + m.radius * 0.25f);
    drawList.AddTriangleFilled(apex, left, right, col);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.9f, m.center.y + m.radius * 0.75f),
                     ImVec2(m.center.x + m.radius * 0.9f, m.center.y + m.radius * 0.75f),
                     col,
                     m.thickness * 1.3f);
}

void PaintCopy(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const float rounding = m.radius * 0.28f;
    drawList.AddRect(ImVec2(m.center.x - m.radius * 0.4f, m.center.y - m.radius),
                     ImVec2(m.center.x + m.radius, m.center.y + m.radius * 0.4f),
                     col,
                     rounding,
                     0,
                     m.thickness);
    drawList.AddRect(ImVec2(m.center.x - m.radius, m.center.y - m.radius * 0.4f),
                     ImVec2(m.center.x + m.radius * 0.4f, m.center.y + m.radius),
                     col,
                     rounding,
                     0,
                     m.thickness);
}

void PaintPaste(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const float rounding = m.radius * 0.22f;
    drawList.AddRect(ImVec2(m.center.x - m.radius * 0.8f, m.center.y - m.radius * 0.8f),
                     ImVec2(m.center.x + m.radius * 0.8f, m.center.y + m.radius),
                     col,
                     rounding,
                     0,
                     m.thickness);
    drawList.AddRectFilled(ImVec2(m.center.x - m.radius * 0.35f, m.center.y - m.radius),
                           ImVec2(m.center.x + m.radius * 0.35f, m.center.y - m.radius * 0.6f),
                           col,
                           rounding * 0.5f);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.45f, m.center.y - m.radius * 0.05f),
                     ImVec2(m.center.x + m.radius * 0.45f, m.center.y - m.radius * 0.05f),
                     col,
                     m.thickness * 0.8f);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.45f, m.center.y + m.radius * 0.45f),
                     ImVec2(m.center.x + m.radius * 0.45f, m.center.y + m.radius * 0.45f),
                     col,
                     m.thickness * 0.8f);
}

void PaintCut(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const float ringRadius = m.radius * 0.34f;
    const ImVec2 leftRing(m.center.x - m.radius * 0.5f, m.center.y + m.radius * 0.55f);
    const ImVec2 rightRing(m.center.x + m.radius * 0.5f, m.center.y + m.radius * 0.55f);
    drawList.AddCircle(leftRing, ringRadius, col, 0, m.thickness);
    drawList.AddCircle(rightRing, ringRadius, col, 0, m.thickness);
    drawList.AddLine(leftRing, ImVec2(m.center.x + m.radius * 0.7f, m.center.y - m.radius), col, m.thickness);
    drawList.AddLine(rightRing, ImVec2(m.center.x - m.radius * 0.7f, m.center.y - m.radius), col, m.thickness);
}

void PaintDuplicate(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    PaintCopy(drawList, m, col);
    const ImVec2 plusCenter(m.center.x - m.radius * 0.3f, m.center.y + m.radius * 0.3f);
    const float plusRadius = m.radius * 0.26f;
    drawList.AddLine(ImVec2(plusCenter.x - plusRadius, plusCenter.y), ImVec2(plusCenter.x + plusRadius, plusCenter.y), col, m.thickness);
    drawList.AddLine(ImVec2(plusCenter.x, plusCenter.y - plusRadius), ImVec2(plusCenter.x, plusCenter.y + plusRadius), col, m.thickness);
}

void PaintOverflow(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    const float dotRadius = std::max(1.0f, m.radius * 0.16f);
    drawList.AddCircleFilled(ImVec2(m.center.x - m.radius * 0.6f, m.center.y), dotRadius, col);
    drawList.AddCircleFilled(m.center, dotRadius, col);
    drawList.AddCircleFilled(ImVec2(m.center.x + m.radius * 0.6f, m.center.y), dotRadius, col);
}

void PaintTutorial(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Graduation cap: a rhombus board, a small cap base, and a tassel.
    const ImVec2 top(m.center.x, m.center.y - m.radius * 0.6f);
    const ImVec2 right(m.center.x + m.radius, m.center.y - m.radius * 0.05f);
    const ImVec2 bottom(m.center.x, m.center.y + m.radius * 0.5f);
    const ImVec2 left(m.center.x - m.radius, m.center.y - m.radius * 0.05f);
    drawList.AddQuadFilled(top, right, bottom, left, col);

    const float baseTop = m.center.y + m.radius * 0.15f;
    const float baseBottom = m.center.y + m.radius * 0.7f;
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.45f, baseTop), ImVec2(m.center.x - m.radius * 0.45f, baseBottom), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x + m.radius * 0.45f, baseTop), ImVec2(m.center.x + m.radius * 0.45f, baseBottom), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x - m.radius * 0.45f, baseBottom), ImVec2(m.center.x + m.radius * 0.45f, baseBottom), col, m.thickness);

    drawList.AddLine(right, ImVec2(right.x, right.y + m.radius * 0.75f), col, m.thickness * 0.7f);
    drawList.AddCircleFilled(ImVec2(right.x, right.y + m.radius * 0.75f), std::max(1.0f, m.radius * 0.1f), col);
}

void PaintRecent(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Clock: a ring with an hour and a minute hand.
    drawList.AddCircle(m.center, m.radius, col, 0, m.thickness);
    drawList.AddLine(m.center, ImVec2(m.center.x, m.center.y - m.radius * 0.55f), col, m.thickness);
    drawList.AddLine(m.center, ImVec2(m.center.x + m.radius * 0.45f, m.center.y), col, m.thickness);
}

void PaintHelp(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Question mark: an open hook resting over a short stem and a dot.
    const ImVec2 hookCenter(m.center.x, m.center.y - m.radius * 0.45f);
    const float hookRadius = m.radius * 0.6f;
    const float hookEndAngle = kPi * 2.55f;
    drawList.PathArcTo(hookCenter, hookRadius, kPi * 1.05f, hookEndAngle, 20);
    drawList.PathStroke(col, ImDrawFlags_None, m.thickness);

    const ImVec2 hookEnd(hookCenter.x + hookRadius * std::cos(hookEndAngle),
                         hookCenter.y + hookRadius * std::sin(hookEndAngle));
    drawList.AddLine(hookEnd, ImVec2(m.center.x, m.center.y + m.radius * 0.35f), col, m.thickness);
    drawList.AddCircleFilled(ImVec2(m.center.x, m.center.y + m.radius * 0.95f), std::max(1.2f, m.thickness * 0.65f), col);
}

void PaintRecenter(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Recenter camera: a crosshair ring with axis ticks and a center dot.
    const float ring = m.radius * 0.72f;
    drawList.AddCircle(m.center, ring, col, 0, m.thickness);
    drawList.AddCircleFilled(m.center, std::max(1.2f, m.thickness * 0.7f), col);
    drawList.AddLine(ImVec2(m.center.x - m.radius, m.center.y), ImVec2(m.center.x - ring, m.center.y), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x + ring, m.center.y), ImVec2(m.center.x + m.radius, m.center.y), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x, m.center.y - m.radius), ImVec2(m.center.x, m.center.y - ring), col, m.thickness);
    drawList.AddLine(ImVec2(m.center.x, m.center.y + ring), ImVec2(m.center.x, m.center.y + m.radius), col, m.thickness);
}

void PaintMoveUp(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Upward chevron over a short stem, hinting "move one slot up".
    const float halfWidth = m.radius * 0.85f;
    const ImVec2 apex(m.center.x, m.center.y - m.radius * 0.65f);
    drawList.AddLine(ImVec2(m.center.x - halfWidth, m.center.y), apex, col, m.thickness);
    drawList.AddLine(apex, ImVec2(m.center.x + halfWidth, m.center.y), col, m.thickness);
    drawList.AddLine(apex, ImVec2(m.center.x, m.center.y + m.radius * 0.85f), col, m.thickness);
}

void PaintMoveDown(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Downward chevron under a short stem, hinting "move one slot down".
    const float halfWidth = m.radius * 0.85f;
    const ImVec2 apex(m.center.x, m.center.y + m.radius * 0.65f);
    drawList.AddLine(ImVec2(m.center.x - halfWidth, m.center.y), apex, col, m.thickness);
    drawList.AddLine(apex, ImVec2(m.center.x + halfWidth, m.center.y), col, m.thickness);
    drawList.AddLine(apex, ImVec2(m.center.x, m.center.y - m.radius * 0.85f), col, m.thickness);
}

void PaintZoomBox(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Drag-a-box-to-zoom: a square lens carrying a '+' with a short magnifier handle.
    const float lens = m.radius * 0.72f;
    const ImVec2 lensCenter(m.center.x - m.radius * 0.18f, m.center.y - m.radius * 0.18f);
    drawList.AddRect(ImVec2(lensCenter.x - lens, lensCenter.y - lens),
                     ImVec2(lensCenter.x + lens, lensCenter.y + lens),
                     col,
                     m.radius * 0.16f,
                     0,
                     m.thickness);
    const float plus = lens * 0.5f;
    drawList.AddLine(ImVec2(lensCenter.x - plus, lensCenter.y), ImVec2(lensCenter.x + plus, lensCenter.y), col, m.thickness);
    drawList.AddLine(ImVec2(lensCenter.x, lensCenter.y - plus), ImVec2(lensCenter.x, lensCenter.y + plus), col, m.thickness);
    drawList.AddLine(ImVec2(lensCenter.x + lens * 0.78f, lensCenter.y + lens * 0.78f),
                     ImVec2(m.center.x + m.radius, m.center.y + m.radius),
                     col,
                     m.thickness * 1.25f);
}

void PaintSmartSelect(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Rectangular picker: a dashed marquee with a small pointer cursor resting inside it.
    const float r = m.radius * 0.92f;
    const float dash = r * 0.5f;
    const ImVec2 topLeft(m.center.x - r, m.center.y - r);
    const ImVec2 topRight(m.center.x + r, m.center.y - r);
    const ImVec2 bottomRight(m.center.x + r, m.center.y + r);
    const ImVec2 bottomLeft(m.center.x - r, m.center.y + r);
    drawList.AddLine(topLeft, ImVec2(topLeft.x + dash, topLeft.y), col, m.thickness);
    drawList.AddLine(ImVec2(topRight.x - dash, topRight.y), topRight, col, m.thickness);
    drawList.AddLine(topRight, ImVec2(topRight.x, topRight.y + dash), col, m.thickness);
    drawList.AddLine(ImVec2(bottomRight.x, bottomRight.y - dash), bottomRight, col, m.thickness);
    drawList.AddLine(bottomRight, ImVec2(bottomRight.x - dash, bottomRight.y), col, m.thickness);
    drawList.AddLine(ImVec2(bottomLeft.x + dash, bottomLeft.y), bottomLeft, col, m.thickness);
    drawList.AddLine(bottomLeft, ImVec2(bottomLeft.x, bottomLeft.y - dash), col, m.thickness);
    drawList.AddLine(ImVec2(topLeft.x, topLeft.y + dash), topLeft, col, m.thickness);

    const ImVec2 pointer(m.center.x - m.radius * 0.12f, m.center.y - m.radius * 0.12f);
    drawList.AddTriangleFilled(pointer,
                               ImVec2(pointer.x, pointer.y + m.radius * 0.82f),
                               ImVec2(pointer.x + m.radius * 0.58f, pointer.y + m.radius * 0.5f),
                               col);
}

void PaintInwardCornerArrow(ImDrawList& drawList,
                            const ImVec2 corner,
                            const float signX,
                            const float signY,
                            const float inset,
                            const float length,
                            const ImU32 col,
                            const float thickness)
{
    const ImVec2 tip(corner.x + signX * inset, corner.y + signY * inset);
    drawList.AddLine(tip, ImVec2(tip.x + signX * length, tip.y), col, thickness);
    drawList.AddLine(tip, ImVec2(tip.x, tip.y + signY * length), col, thickness);
}

void PaintFitToPage(ImDrawList& drawList, const GlyphMetrics& m, const ImU32 col)
{
    // Scale-content-to-fit: a page frame with four inward-pointing corner arrows.
    const float r = m.radius;
    drawList.AddRect(ImVec2(m.center.x - r, m.center.y - r),
                     ImVec2(m.center.x + r, m.center.y + r),
                     col,
                     r * 0.12f,
                     0,
                     m.thickness);
    const float inset = r * 0.30f;
    const float length = r * 0.46f;
    PaintInwardCornerArrow(drawList, ImVec2(m.center.x - r, m.center.y - r), 1.0f, 1.0f, inset, length, col, m.thickness);
    PaintInwardCornerArrow(drawList, ImVec2(m.center.x + r, m.center.y - r), -1.0f, 1.0f, inset, length, col, m.thickness);
    PaintInwardCornerArrow(drawList, ImVec2(m.center.x + r, m.center.y + r), -1.0f, -1.0f, inset, length, col, m.thickness);
    PaintInwardCornerArrow(drawList, ImVec2(m.center.x - r, m.center.y + r), 1.0f, -1.0f, inset, length, col, m.thickness);
}

void PaintGlyph(ImDrawList& drawList, const EditorIcon icon, const ImVec2& topLeft, const float boxSize, const ImU32 col)
{
    GlyphMetrics metrics {};
    metrics.center = ImVec2(topLeft.x + boxSize * 0.5f, topLeft.y + boxSize * 0.5f);
    metrics.radius = boxSize * 0.30f;
    metrics.thickness = std::max(1.4f, boxSize * 0.072f);

    switch (icon)
    {
    case EditorIcon::Rename:
        PaintRename(drawList, metrics, col);
        return;
    case EditorIcon::Delete:
        PaintDelete(drawList, metrics, col);
        return;
    case EditorIcon::RemoveFromWindow:
        PaintRemoveFromWindow(drawList, metrics, col);
        return;
    case EditorIcon::Copy:
        PaintCopy(drawList, metrics, col);
        return;
    case EditorIcon::Paste:
        PaintPaste(drawList, metrics, col);
        return;
    case EditorIcon::Cut:
        PaintCut(drawList, metrics, col);
        return;
    case EditorIcon::Duplicate:
        PaintDuplicate(drawList, metrics, col);
        return;
    case EditorIcon::Add:
        PaintAdd(drawList, metrics, col);
        return;
    case EditorIcon::Search:
        PaintSearch(drawList, metrics, col);
        return;
    case EditorIcon::Overflow:
        PaintOverflow(drawList, metrics, col);
        return;
    case EditorIcon::Tutorial:
        PaintTutorial(drawList, metrics, col);
        return;
    case EditorIcon::Recent:
        PaintRecent(drawList, metrics, col);
        return;
    case EditorIcon::Help:
        PaintHelp(drawList, metrics, col);
        return;
    case EditorIcon::Recenter:
        PaintRecenter(drawList, metrics, col);
        return;
    case EditorIcon::MoveUp:
        PaintMoveUp(drawList, metrics, col);
        return;
    case EditorIcon::MoveDown:
        PaintMoveDown(drawList, metrics, col);
        return;
    case EditorIcon::ZoomBox:
        PaintZoomBox(drawList, metrics, col);
        return;
    case EditorIcon::SmartSelect:
        PaintSmartSelect(drawList, metrics, col);
        return;
    case EditorIcon::FitToPage:
        PaintFitToPage(drawList, metrics, col);
        return;
    }
}
} // namespace

bool IconButton(const char* const strId,
                const EditorIcon icon,
                const char* const tooltip,
                const bool enabled,
                const float sizeOverride)
{
    const float size = sizeOverride > 0.0f ? sizeOverride : ImGui::GetFrameHeight();
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();

    bool pressed = false;
    bool hovered = false;
    if (enabled)
    {
        pressed = ImGui::InvisibleButton(strId, ImVec2(size, size));
        hovered = ImGui::IsItemHovered();
    }
    else
    {
        ImGui::Dummy(ImVec2(size, size));
    }

    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    if (hovered)
    {
        drawList->AddRectFilled(topLeft,
                                ImVec2(topLeft.x + size, topLeft.y + size),
                                ImGui::GetColorU32(ImGuiCol_ButtonHovered),
                                ImGui::GetStyle().FrameRounding);
    }

    const ImU32 glyphColor =
        !enabled ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                 : (hovered ? ImGui::GetColorU32(ImVec4(0.33f, 0.86f, 0.78f, 1.0f)) : kGlyphRest);
    PaintGlyph(*drawList, icon, topLeft, size, glyphColor);

    if (enabled)
    {
        ShowItemTooltip(tooltip);
    }
    return pressed;
}

void IconText(const EditorIcon icon)
{
    const float size = ImGui::GetFrameHeight();
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    PaintGlyph(*ImGui::GetWindowDrawList(), icon, topLeft, size, ImGui::GetColorU32(ImGuiCol_TextDisabled));
}

float BeginRowIconStrip(const ImVec2& rowMin, const ImVec2& rowMax, const int iconCount)
{
    const float rowHeight = rowMax.y - rowMin.y;
    // Fit the icons to the row height so a frame-height button never overflows the line
    // and pushes neighboring rows, while keeping a sane floor on very short rows.
    const float iconSize = std::max(12.0f, std::min(ImGui::GetFrameHeight(), rowHeight));
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int clampedCount = std::max(1, iconCount);
    const float stripWidth =
        static_cast<float>(clampedCount) * iconSize + static_cast<float>(clampedCount - 1) * spacing;

    const float originX = rowMax.x - stripWidth;
    const float originY = rowMin.y + (rowHeight - iconSize) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(originX, originY));
    return iconSize;
}
} // namespace editor::ui
