/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerEditorAbi.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
bool IsFacadeValid(const MfdContainerEditorUiFacadeV1* const ui) noexcept
{
    return ui != nullptr && ui->struct_size >= sizeof(MfdContainerEditorUiFacadeV1) &&
           ui->version == MFD_CONTAINER_EDITOR_UI_FACADE_VERSION && ui->text != nullptr &&
           ui->checkbox != nullptr && ui->drag_float != nullptr && ui->drag_float2 != nullptr &&
           ui->combo != nullptr && ui->color_edit4 != nullptr;
}
bool IsSelectionValid(const MfdContainerEditorSelectionV1* const selection) noexcept
{
    if (selection == nullptr || selection->struct_size < sizeof(MfdContainerEditorSelectionV1) ||
        selection->id == nullptr || selection->type == nullptr || selection->size_mode == nullptr ||
        selection->position_xy == nullptr ||
        selection->size_xy == nullptr || selection->minimum_size_xy == nullptr ||
        selection->visible == nullptr || selection->enabled == nullptr || selection->exposed == nullptr ||
        selection->padding_ltrb == nullptr || selection->spacing == nullptr ||
        selection->orientation == nullptr || selection->alignment == nullptr ||
        selection->rows == nullptr || selection->columns == nullptr ||
        selection->horizontal_scroll_mode == nullptr || selection->vertical_scroll_mode == nullptr ||
        selection->scroll_offset_xy == nullptr || selection->scrollbar_thickness == nullptr ||
        selection->scrollbar_minimum_thumb_size == nullptr || selection->clip_content == nullptr ||
        selection->background_visible == nullptr || selection->border_visible == nullptr ||
        selection->background_rgba == nullptr || selection->border_rgba == nullptr ||
        selection->track_rgba == nullptr || selection->thumb_rgba == nullptr ||
        selection->thumb_hover_rgba == nullptr || selection->thumb_active_rgba == nullptr ||
        selection->border_thickness == nullptr || selection->corner_radius == nullptr ||
        selection->scrollbar_corner_radius == nullptr || selection->scrollbar_margins_ltrb == nullptr)
    {
        return false;
    }
    return selection->has_parent == 0U ||
           (selection->parent_id != nullptr && selection->child_resize_mode != nullptr &&
            selection->child_horizontal_alignment != nullptr &&
            selection->child_vertical_alignment != nullptr && selection->child_row != nullptr &&
            selection->child_column != nullptr && selection->child_row_span != nullptr &&
            selection->child_column_span != nullptr && selection->child_render_order != nullptr);
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawInspector(
    const MfdContainerEditorUiFacadeV1* const ui,
    MfdContainerEditorSelectionV1* const selection) noexcept
{
    if (!IsFacadeValid(ui) || !IsSelectionValid(selection))
    {
        return 0U;
    }

    bool changed = false;
    ui->text(ui->host_context, selection->id);
    const char* const typeLabels[] {"CanvasBox", "LinearBox", "GridBox", "OverlayBox"};
    int32_t type = static_cast<int32_t>(*selection->type);
    changed = ui->combo(ui->host_context, "Container type", &type, typeLabels, 4U) != 0U || changed;
    *selection->type = static_cast<uint32_t>(std::clamp(type, 0, 3));

    const char* const sizeModeLabels[] {"Fixed", "Fit content", "Fit horizontally", "Fit vertically"};
    int32_t sizeMode = static_cast<int32_t>(*selection->size_mode);
    changed = ui->combo(ui->host_context, "Size mode", &sizeMode, sizeModeLabels, 4U) != 0U || changed;
    *selection->size_mode = static_cast<uint32_t>(std::clamp(sizeMode, 0, 3));

    changed = ui->checkbox(ui->host_context, "Visible", selection->visible) != 0U || changed;
    changed = ui->checkbox(ui->host_context, "Input enabled", selection->enabled) != 0U || changed;
    changed = ui->checkbox(ui->host_context, "Expose to generated API", selection->exposed) != 0U || changed;
    ui->text(ui->host_context, "Geometry");
    const bool positionIsAuthored = selection->has_parent == 0U || selection->parent_type == 0U ||
                                    selection->parent_type == 3U;
    if (positionIsAuthored)
    {
        changed = ui->drag_float2(
                      ui->host_context, "Position", selection->position_xy, 0.005f, -8.0f, 8.0f) != 0U ||
                  changed;
    }
    changed = ui->drag_float2(
                  ui->host_context, "Authored size", selection->size_xy, 0.005f, 0.02f, 16.0f) != 0U ||
              changed;
    changed = ui->drag_float2(ui->host_context, "Minimum size", selection->minimum_size_xy, 0.002f, 0.0f, 16.0f) != 0U || changed;
    changed = ui->drag_float2(ui->host_context, "Padding L/T", selection->padding_ltrb, 0.002f, 0.0f, 4.0f) != 0U || changed;
    changed = ui->drag_float2(ui->host_context, "Padding R/B", selection->padding_ltrb + 2, 0.002f, 0.0f, 4.0f) != 0U || changed;

    if (selection->has_parent != 0U)
    {
        ui->text(ui->host_context, "Child placement");
        ui->text(ui->host_context, selection->parent_id);
        if (selection->parent_type == 0U || selection->parent_type == 3U)
        {
            const char* const resizeLabels[] {"Fixed", "Proportional", "Stretch"};
            int32_t resizeMode = static_cast<int32_t>(*selection->child_resize_mode);
            changed = ui->combo(ui->host_context, "Resize mode", &resizeMode, resizeLabels, 3U) != 0U || changed;
            *selection->child_resize_mode = static_cast<uint32_t>(std::clamp(resizeMode, 0, 2));
        }
        if (selection->parent_type == 2U)
        {
            const char* const placementAlignmentLabels[] {"Start", "Center", "End", "Stretch"};
            int32_t horizontalAlignment = static_cast<int32_t>(*selection->child_horizontal_alignment);
            int32_t verticalAlignment = static_cast<int32_t>(*selection->child_vertical_alignment);
            changed = ui->combo(ui->host_context,
                                "Horizontal placement",
                                &horizontalAlignment,
                                placementAlignmentLabels,
                                4U) != 0U || changed;
            changed = ui->combo(ui->host_context,
                                "Vertical placement",
                                &verticalAlignment,
                                placementAlignmentLabels,
                                4U) != 0U || changed;
            *selection->child_horizontal_alignment =
                static_cast<uint32_t>(std::clamp(horizontalAlignment, 0, 3));
            *selection->child_vertical_alignment =
                static_cast<uint32_t>(std::clamp(verticalAlignment, 0, 3));

            float gridCell[] {static_cast<float>(*selection->child_row),
                              static_cast<float>(*selection->child_column)};
            if (ui->drag_float2(
                    ui->host_context, "Grid row / column", gridCell, 1.0f, 0.0f, 4095.0f) != 0U)
            {
                *selection->child_row = static_cast<uint32_t>(std::lround(gridCell[0]));
                *selection->child_column = static_cast<uint32_t>(std::lround(gridCell[1]));
                changed = true;
            }
            float gridSpan[] {static_cast<float>(*selection->child_row_span),
                              static_cast<float>(*selection->child_column_span)};
            if (ui->drag_float2(
                    ui->host_context, "Grid row / column span", gridSpan, 1.0f, 1.0f, 4096.0f) != 0U)
            {
                *selection->child_row_span = static_cast<uint32_t>(std::lround(gridSpan[0]));
                *selection->child_column_span = static_cast<uint32_t>(std::lround(gridSpan[1]));
                changed = true;
            }
        }
        if (selection->parent_type == 3U)
        {
            float renderOrder = static_cast<float>(*selection->child_render_order);
            if (ui->drag_float(ui->host_context,
                               "Overlay render order",
                               &renderOrder,
                               1.0f,
                               -4096.0f,
                               4096.0f) != 0U)
            {
                *selection->child_render_order = static_cast<int32_t>(std::lround(renderOrder));
                changed = true;
            }
        }
    }

    if (type == 1)
    {
        ui->text(ui->host_context, "Linear layout");
        const char* const orientationLabels[] {"Horizontal", "Vertical"};
        int32_t orientation = static_cast<int32_t>(*selection->orientation);
        changed = ui->combo(ui->host_context, "Orientation", &orientation, orientationLabels, 2U) != 0U || changed;
        *selection->orientation = static_cast<uint32_t>(std::clamp(orientation, 0, 1));
        const char* const alignmentLabels[] {"Start", "Center", "End", "Stretch"};
        int32_t alignment = static_cast<int32_t>(*selection->alignment);
        changed = ui->combo(ui->host_context, "Cross-axis alignment", &alignment, alignmentLabels, 4U) != 0U || changed;
        *selection->alignment = static_cast<uint32_t>(std::clamp(alignment, 0, 3));
    }
    if (type == 1 || type == 2)
    {
        changed = ui->drag_float(
                      ui->host_context, "Spacing", selection->spacing, 0.002f, 0.0f, 4.0f) != 0U ||
                  changed;
    }
    if (type == 2)
    {
        ui->text(ui->host_context, "Grid layout");
        float gridSize[] {static_cast<float>(*selection->rows), static_cast<float>(*selection->columns)};
        if (ui->drag_float2(ui->host_context, "Rows / columns", gridSize, 1.0f, 1.0f, 4096.0f) != 0U)
        {
            *selection->rows = static_cast<std::uint32_t>(std::lround(gridSize[0]));
            *selection->columns = static_cast<std::uint32_t>(std::lround(gridSize[1]));
            changed = true;
        }
    }

    const char* const scrollLabels[] {"Disabled", "Automatic", "Always visible"};
    int32_t horizontalScroll = static_cast<int32_t>(*selection->horizontal_scroll_mode);
    int32_t verticalScroll = static_cast<int32_t>(*selection->vertical_scroll_mode);
    changed = ui->combo(ui->host_context, "Horizontal scrolling", &horizontalScroll, scrollLabels, 3U) != 0U || changed;
    changed = ui->combo(ui->host_context, "Vertical scrolling", &verticalScroll, scrollLabels, 3U) != 0U || changed;
    *selection->horizontal_scroll_mode = static_cast<uint32_t>(std::clamp(horizontalScroll, 0, 2));
    *selection->vertical_scroll_mode = static_cast<uint32_t>(std::clamp(verticalScroll, 0, 2));
    changed = ui->checkbox(ui->host_context, "Clip content", selection->clip_content) != 0U || changed;
    changed = ui->drag_float2(ui->host_context, "Scroll offset", selection->scroll_offset_xy, 0.005f, 0.0f, 16.0f) != 0U || changed;
    changed = ui->drag_float(ui->host_context, "Scrollbar thickness", selection->scrollbar_thickness, 0.001f, 0.001f, 1.0f) != 0U || changed;
    changed = ui->drag_float(ui->host_context, "Minimum thumb size", selection->scrollbar_minimum_thumb_size, 0.001f, 0.001f, 4.0f) != 0U || changed;
    ui->text(ui->host_context, "Appearance");
    changed = ui->checkbox(ui->host_context, "Draw background", selection->background_visible) != 0U || changed;
    changed = ui->color_edit4(ui->host_context, "Background color", selection->background_rgba) != 0U || changed;
    changed = ui->checkbox(ui->host_context, "Draw border", selection->border_visible) != 0U || changed;
    changed = ui->color_edit4(ui->host_context, "Border color", selection->border_rgba) != 0U || changed;
    changed = ui->drag_float(ui->host_context, "Border thickness", selection->border_thickness, 0.001f, 0.0f, 1.0f) != 0U || changed;
    changed = ui->drag_float(ui->host_context, "Corner radius", selection->corner_radius, 0.001f, 0.0f, 1.0f) != 0U || changed;
    ui->text(ui->host_context, "Scrollbar style");
    changed = ui->color_edit4(ui->host_context, "Track color", selection->track_rgba) != 0U || changed;
    changed = ui->color_edit4(ui->host_context, "Thumb color", selection->thumb_rgba) != 0U || changed;
    changed = ui->color_edit4(ui->host_context, "Thumb hover color", selection->thumb_hover_rgba) != 0U || changed;
    changed = ui->color_edit4(ui->host_context, "Thumb active color", selection->thumb_active_rgba) != 0U || changed;
    changed = ui->drag_float(ui->host_context, "Scrollbar radius", selection->scrollbar_corner_radius, 0.001f, 0.0f, 1.0f) != 0U || changed;
    changed = ui->drag_float2(ui->host_context, "Scrollbar margins L/T", selection->scrollbar_margins_ltrb, 0.001f, 0.0f, 1.0f) != 0U || changed;
    changed = ui->drag_float2(ui->host_context, "Scrollbar margins R/B", selection->scrollbar_margins_ltrb + 2, 0.001f, 0.0f, 1.0f) != 0U || changed;
    return changed ? 1U : 0U;
}
} // namespace

extern "C" MFD_CONTAINER_EDITOR_EXPORT std::uint32_t MFD_CONTAINER_EDITOR_CALL
mfd_get_container_editor_api_v1(MfdContainerEditorApiV1* const api)
{
    if (api == nullptr || api->struct_size < sizeof(MfdContainerEditorApiV1))
    {
        return 1U;
    }
    api->abi_version = MFD_CONTAINER_EDITOR_ABI_VERSION;
    api->required_ui_facade_version = MFD_CONTAINER_EDITOR_UI_FACADE_VERSION;
    api->draw_inspector = &DrawInspector;
    return 0U;
}
