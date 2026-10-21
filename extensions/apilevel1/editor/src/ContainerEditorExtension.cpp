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
    constexpr std::size_t kRequiredFacadeSize = offsetof(MfdContainerEditorUiFacadeV1, begin_section);
    return ui != nullptr && ui->struct_size >= kRequiredFacadeSize &&
           ui->version == MFD_CONTAINER_EDITOR_UI_FACADE_VERSION && ui->text != nullptr &&
           ui->checkbox != nullptr && ui->drag_float != nullptr &&
           ui->drag_float2 != nullptr && ui->combo != nullptr && ui->color_edit4 != nullptr;
}

bool IsSelectionValid(const MfdContainerEditorSelectionV1* const selection) noexcept
{
    if (selection == nullptr || selection->struct_size < sizeof(MfdContainerEditorSelectionV1) ||
        selection->id == nullptr || selection->type == nullptr || selection->size_mode == nullptr ||
        selection->position_xy == nullptr || selection->size_xy == nullptr ||
        selection->minimum_size_xy == nullptr || selection->visible == nullptr ||
        selection->enabled == nullptr || selection->exposed == nullptr ||
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

void MergeChange(bool& changed, const std::uint8_t callbackResult) noexcept
{
    changed = callbackResult != 0U || changed;
}

bool BeginSection(const MfdContainerEditorUiFacadeV1& ui,
                  const char* const id,
                  const char* const label,
                  const bool defaultOpen) noexcept
{
    if (ui.struct_size < sizeof(MfdContainerEditorUiFacadeV1) || ui.begin_section == nullptr)
    {
        ui.text(ui.host_context, label);
        return true;
    }
    return ui.begin_section(ui.host_context, id, label, defaultOpen ? 1U : 0U) != 0U;
}

bool DrawGeneralSection(const MfdContainerEditorUiFacadeV1& ui,
                        MfdContainerEditorSelectionV1& selection) noexcept
{
    if (!BeginSection(ui, "api1_container_general", "Container", true))
    {
        return false;
    }

    bool changed = false;
    const char* const typeLabels[] {"CanvasBox", "LinearBox", "GridBox", "OverlayBox"};
    std::int32_t type = static_cast<std::int32_t>(*selection.type);
    MergeChange(changed, ui.combo(ui.host_context, "Container type", &type, typeLabels, 4U));
    *selection.type = static_cast<std::uint32_t>(std::clamp(type, 0, 3));

    const char* const sizeModeLabels[] {"Fixed", "Fit content", "Fit horizontally", "Fit vertically"};
    std::int32_t sizeMode = static_cast<std::int32_t>(*selection.size_mode);
    MergeChange(changed, ui.combo(ui.host_context, "Size mode", &sizeMode, sizeModeLabels, 4U));
    *selection.size_mode = static_cast<std::uint32_t>(std::clamp(sizeMode, 0, 3));

    MergeChange(changed, ui.checkbox(ui.host_context, "Visible", selection.visible));
    MergeChange(changed, ui.checkbox(ui.host_context, "Input enabled", selection.enabled));
    MergeChange(changed, ui.checkbox(ui.host_context, "Generated API", selection.exposed));
    return changed;
}

bool DrawGeometrySection(const MfdContainerEditorUiFacadeV1& ui,
                         MfdContainerEditorSelectionV1& selection) noexcept
{
    if (!BeginSection(ui, "api1_container_geometry", "Geometry", true))
    {
        return false;
    }

    bool changed = false;
    const bool positionIsAuthored = selection.has_parent == 0U || selection.parent_type == 0U ||
                                    selection.parent_type == 3U;
    if (positionIsAuthored)
    {
        MergeChange(changed,
                    ui.drag_float2(
                        ui.host_context, "Position", selection.position_xy, 0.005f, -8.0f, 8.0f));
    }
    MergeChange(changed,
                ui.drag_float2(
                    ui.host_context, "Authored size", selection.size_xy, 0.005f, 0.02f, 16.0f));
    MergeChange(changed,
                ui.drag_float2(
                    ui.host_context, "Minimum size", selection.minimum_size_xy, 0.002f, 0.0f, 16.0f));
    MergeChange(changed,
                ui.drag_float2(
                    ui.host_context, "Padding L/T", selection.padding_ltrb, 0.002f, 0.0f, 4.0f));
    MergeChange(changed,
                ui.drag_float2(
                    ui.host_context, "Padding R/B", selection.padding_ltrb + 2, 0.002f, 0.0f, 4.0f));
    return changed;
}

bool DrawChildPlacementSection(const MfdContainerEditorUiFacadeV1& ui,
                               MfdContainerEditorSelectionV1& selection) noexcept
{
    if (selection.has_parent == 0U ||
        !BeginSection(ui, "api1_container_child_placement", "Child placement", true))
    {
        return false;
    }

    bool changed = false;
    if (selection.parent_type == 0U || selection.parent_type == 3U)
    {
        const char* const resizeLabels[] {"Fixed", "Proportional", "Stretch"};
        std::int32_t resizeMode = static_cast<std::int32_t>(*selection.child_resize_mode);
        MergeChange(changed,
                    ui.combo(ui.host_context, "Resize mode", &resizeMode, resizeLabels, 3U));
        *selection.child_resize_mode = static_cast<std::uint32_t>(std::clamp(resizeMode, 0, 2));
    }

    if (selection.parent_type == 2U)
    {
        const char* const alignmentLabels[] {"Start", "Center", "End", "Stretch"};
        std::int32_t horizontalAlignment =
            static_cast<std::int32_t>(*selection.child_horizontal_alignment);
        std::int32_t verticalAlignment =
            static_cast<std::int32_t>(*selection.child_vertical_alignment);
        MergeChange(changed,
                    ui.combo(ui.host_context,
                             "Horizontal placement",
                             &horizontalAlignment,
                             alignmentLabels,
                             4U));
        MergeChange(changed,
                    ui.combo(ui.host_context,
                             "Vertical placement",
                             &verticalAlignment,
                             alignmentLabels,
                             4U));
        *selection.child_horizontal_alignment =
            static_cast<std::uint32_t>(std::clamp(horizontalAlignment, 0, 3));
        *selection.child_vertical_alignment =
            static_cast<std::uint32_t>(std::clamp(verticalAlignment, 0, 3));

        float gridCell[] {static_cast<float>(*selection.child_row),
                          static_cast<float>(*selection.child_column)};
        if (ui.drag_float2(ui.host_context, "Grid row / column", gridCell, 1.0f, 0.0f, 4095.0f) != 0U)
        {
            *selection.child_row = static_cast<std::uint32_t>(std::lround(gridCell[0]));
            *selection.child_column = static_cast<std::uint32_t>(std::lround(gridCell[1]));
            changed = true;
        }

        float gridSpan[] {static_cast<float>(*selection.child_row_span),
                          static_cast<float>(*selection.child_column_span)};
        if (ui.drag_float2(
                ui.host_context, "Grid row / column span", gridSpan, 1.0f, 1.0f, 4096.0f) != 0U)
        {
            *selection.child_row_span = static_cast<std::uint32_t>(std::lround(gridSpan[0]));
            *selection.child_column_span = static_cast<std::uint32_t>(std::lround(gridSpan[1]));
            changed = true;
        }
    }

    if (selection.parent_type == 3U)
    {
        float renderOrder = static_cast<float>(*selection.child_render_order);
        if (ui.drag_float(
                ui.host_context, "Overlay render order", &renderOrder, 1.0f, -4096.0f, 4096.0f) != 0U)
        {
            *selection.child_render_order = static_cast<std::int32_t>(std::lround(renderOrder));
            changed = true;
        }
    }
    return changed;
}

bool DrawLayoutSection(const MfdContainerEditorUiFacadeV1& ui,
                       MfdContainerEditorSelectionV1& selection) noexcept
{
    const std::uint32_t type = *selection.type;
    if ((type != 1U && type != 2U) ||
        !BeginSection(ui, "api1_container_layout", "Layout", true))
    {
        return false;
    }

    bool changed = false;
    if (type == 1U)
    {
        const char* const orientationLabels[] {"Horizontal", "Vertical"};
        std::int32_t orientation = static_cast<std::int32_t>(*selection.orientation);
        MergeChange(changed,
                    ui.combo(ui.host_context, "Orientation", &orientation, orientationLabels, 2U));
        *selection.orientation = static_cast<std::uint32_t>(std::clamp(orientation, 0, 1));

        const char* const alignmentLabels[] {"Start", "Center", "End", "Stretch"};
        std::int32_t alignment = static_cast<std::int32_t>(*selection.alignment);
        MergeChange(changed,
                    ui.combo(ui.host_context, "Cross-axis alignment", &alignment, alignmentLabels, 4U));
        *selection.alignment = static_cast<std::uint32_t>(std::clamp(alignment, 0, 3));
    }

    MergeChange(changed,
                ui.drag_float(ui.host_context, "Spacing", selection.spacing, 0.002f, 0.0f, 4.0f));
    if (type == 2U)
    {
        float gridSize[] {static_cast<float>(*selection.rows), static_cast<float>(*selection.columns)};
        if (ui.drag_float2(ui.host_context, "Rows / columns", gridSize, 1.0f, 1.0f, 4096.0f) != 0U)
        {
            *selection.rows = static_cast<std::uint32_t>(std::lround(gridSize[0]));
            *selection.columns = static_cast<std::uint32_t>(std::lround(gridSize[1]));
            changed = true;
        }
    }
    return changed;
}

bool DrawScrollingSection(const MfdContainerEditorUiFacadeV1& ui,
                          MfdContainerEditorSelectionV1& selection) noexcept
{
    if (!BeginSection(ui, "api1_container_scrolling", "Scrolling", false))
    {
        return false;
    }

    bool changed = false;
    const char* const scrollLabels[] {"Disabled", "Automatic", "Always visible"};
    std::int32_t horizontalScroll = static_cast<std::int32_t>(*selection.horizontal_scroll_mode);
    std::int32_t verticalScroll = static_cast<std::int32_t>(*selection.vertical_scroll_mode);
    MergeChange(changed,
                ui.combo(ui.host_context,
                         "Horizontal scroll",
                         &horizontalScroll,
                         scrollLabels,
                         3U));
    MergeChange(changed,
                ui.combo(ui.host_context, "Vertical scroll", &verticalScroll, scrollLabels, 3U));
    *selection.horizontal_scroll_mode =
        static_cast<std::uint32_t>(std::clamp(horizontalScroll, 0, 2));
    *selection.vertical_scroll_mode =
        static_cast<std::uint32_t>(std::clamp(verticalScroll, 0, 2));

    MergeChange(changed, ui.checkbox(ui.host_context, "Clip content", selection.clip_content));
    MergeChange(changed,
                ui.drag_float2(
                    ui.host_context, "Scroll offset", selection.scroll_offset_xy, 0.005f, 0.0f, 16.0f));
    MergeChange(changed,
                ui.drag_float(ui.host_context,
                              "Scrollbar thickness",
                              selection.scrollbar_thickness,
                              0.001f,
                              0.001f,
                              1.0f));
    MergeChange(changed,
                ui.drag_float(ui.host_context,
                              "Minimum thumb size",
                              selection.scrollbar_minimum_thumb_size,
                              0.001f,
                              0.001f,
                              4.0f));
    return changed;
}

bool DrawAppearanceSection(const MfdContainerEditorUiFacadeV1& ui,
                           MfdContainerEditorSelectionV1& selection) noexcept
{
    if (!BeginSection(ui, "api1_container_appearance", "Appearance", false))
    {
        return false;
    }

    bool changed = false;
    MergeChange(changed,
                ui.checkbox(ui.host_context, "Draw background", selection.background_visible));
    MergeChange(changed,
                ui.color_edit4(ui.host_context, "Background color", selection.background_rgba));
    MergeChange(changed, ui.checkbox(ui.host_context, "Draw border", selection.border_visible));
    MergeChange(changed, ui.color_edit4(ui.host_context, "Border color", selection.border_rgba));
    MergeChange(changed,
                ui.drag_float(ui.host_context,
                              "Border thickness",
                              selection.border_thickness,
                              0.001f,
                              0.0f,
                              1.0f));
    MergeChange(changed,
                ui.drag_float(ui.host_context,
                              "Corner radius",
                              selection.corner_radius,
                              0.001f,
                              0.0f,
                              1.0f));
    return changed;
}

bool DrawScrollbarStyleSection(const MfdContainerEditorUiFacadeV1& ui,
                               MfdContainerEditorSelectionV1& selection) noexcept
{
    if (!BeginSection(ui, "api1_container_scrollbar_style", "Scrollbar style", false))
    {
        return false;
    }

    bool changed = false;
    MergeChange(changed, ui.color_edit4(ui.host_context, "Track color", selection.track_rgba));
    MergeChange(changed, ui.color_edit4(ui.host_context, "Thumb color", selection.thumb_rgba));
    MergeChange(changed,
                ui.color_edit4(ui.host_context, "Thumb hover color", selection.thumb_hover_rgba));
    MergeChange(changed,
                ui.color_edit4(ui.host_context, "Thumb active color", selection.thumb_active_rgba));
    MergeChange(changed,
                ui.drag_float(ui.host_context,
                              "Scrollbar radius",
                              selection.scrollbar_corner_radius,
                              0.001f,
                              0.0f,
                              1.0f));
    MergeChange(changed,
                ui.drag_float2(ui.host_context,
                               "Scrollbar margins L/T",
                               selection.scrollbar_margins_ltrb,
                               0.001f,
                               0.0f,
                               1.0f));
    MergeChange(changed,
                ui.drag_float2(ui.host_context,
                               "Scrollbar margins R/B",
                               selection.scrollbar_margins_ltrb + 2,
                               0.001f,
                               0.0f,
                               1.0f));
    return changed;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawInspector(
    const MfdContainerEditorUiFacadeV1* const ui,
    MfdContainerEditorSelectionV1* const selection) noexcept
{
    if (!IsFacadeValid(ui) || !IsSelectionValid(selection))
    {
        return 0U;
    }

    bool changed = DrawGeneralSection(*ui, *selection);
    changed = DrawGeometrySection(*ui, *selection) || changed;
    changed = DrawChildPlacementSection(*ui, *selection) || changed;
    changed = DrawLayoutSection(*ui, *selection) || changed;
    changed = DrawScrollingSection(*ui, *selection) || changed;
    changed = DrawAppearanceSection(*ui, *selection) || changed;
    changed = DrawScrollbarStyleSection(*ui, *selection) || changed;
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
