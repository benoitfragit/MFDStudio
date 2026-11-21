#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
#include <cstddef>
namespace
{
constexpr uint32_t kInstanceEdit = 1U;
constexpr uint32_t kDefinitionEdit = 2U;
constexpr uint32_t kEditVisual = 4U;
constexpr uint32_t kApplyVisual = 8U;
constexpr uint32_t kResetOverrides = 16U;
constexpr uint32_t kApplyBinding = 32U;
constexpr uint32_t kRemoveBinding = 64U;

constexpr int32_t kParentCanvas = 0;
constexpr int32_t kParentLinear = 1;
constexpr int32_t kParentGrid = 2;
constexpr int32_t kParentOverlay = 3;

bool IsUiValid(const MfdWidgetEditorUiV1* ui) noexcept
{
    return ui && ui->struct_size >= offsetof(MfdWidgetEditorUiV1, begin_section) && ui->version == 1U &&
           ui->text && ui->input_text && ui->checkbox && ui->number && ui->combo && ui->button;
}

bool IsDraftValid(const MfdWidgetEditorDraftV1* draft) noexcept
{
    return draft && draft->struct_size >= offsetof(MfdWidgetEditorDraftV1, parent_type);
}

/** @brief Collapsible section when the host offers one, otherwise a plain caption with an always-visible body. */
bool BeginSection(const MfdWidgetEditorUiV1& ui, const char* id, const char* label, const bool defaultOpen)
{
    if (ui.struct_size < offsetof(MfdWidgetEditorUiV1, parent_picker) || !ui.begin_section)
    {
        ui.text(ui.context, label);
        return true;
    }
    return ui.begin_section(ui.context, id, label, defaultOpen ? 1U : 0U) != 0U;
}

uint8_t DrawParentField(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (ui.struct_size >= sizeof(MfdWidgetEditorUiV1) && ui.parent_picker)
        return ui.parent_picker(ui.context, "Parent", draft.parent, sizeof(draft.parent));
    return ui.input_text(ui.context, "Parent (empty = root)", draft.parent, sizeof(draft.parent));
}

/** @brief Parent container type, or unknown for a legacy host that does not report it. */
int32_t ParentType(const MfdWidgetEditorDraftV1& draft) noexcept
{
    if (draft.struct_size < sizeof(MfdWidgetEditorDraftV1)) return MFD_WIDGET_EDITOR_PARENT_TYPE_UNKNOWN;
    return draft.parent_type;
}

uint32_t DrawInstanceSection(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (!BeginSection(ui, "api1_widget_instance", "Instance", true)) return 0U;
    uint32_t result = 0U;
    result |= ui.input_text(ui.context, "Instance ID", draft.id, sizeof(draft.id));
    result |= DrawParentField(ui, draft);
    result |= ui.checkbox(ui.context, "Visible", &draft.visible);
    result |= ui.checkbox(ui.context, "Enabled", &draft.enabled);
    result |= ui.checkbox(ui.context, "Exposed C++ handle", &draft.exposed);
    return result;
}

uint32_t DrawGeometrySection(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (!BeginSection(ui, "api1_widget_geometry", "Position and size", true)) return 0U;
    uint32_t result = 0U;
    const int32_t parentType = ParentType(draft);
    const bool layoutOwnsPosition = draft.parent[0] && (parentType == kParentLinear || parentType == kParentGrid);
    if (layoutOwnsPosition)
    {
        ui.text(ui.context, parentType == kParentGrid ? "Position is set by the parent Grid cell."
                                                       : "Position is set by the parent Linear layout order.");
    }
    else
    {
        result |= ui.number(ui.context, "Position X", &draft.x, -10000, 10000);
        result |= ui.number(ui.context, "Position Y", &draft.y, -10000, 10000);
    }
    result |= ui.checkbox(ui.context, "Override size", &draft.override_size);
    if (draft.override_size)
    {
        result |= ui.number(ui.context, "Width", &draft.width, 0.00001, 10000);
        result |= ui.number(ui.context, "Height", &draft.height, 0.00001, 10000);
    }
    return result;
}

uint32_t DrawPlacementSection(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (!draft.parent[0] || !BeginSection(ui, "api1_widget_placement", "Placement in parent", true)) return 0U;
    const int32_t parentType = ParentType(draft);
    const bool unknown = parentType < kParentCanvas || parentType > kParentOverlay;
    const char* resizing[] = {"Fixed", "Proportional", "Stretch"};
    const char* alignment[] = {"Start", "Center", "End", "Stretch"};
    uint32_t result = 0U;
    if (unknown || parentType == kParentCanvas || parentType == kParentOverlay)
        result |= ui.combo(ui.context, "Resize", &draft.resize_mode, resizing, 3);
    if (unknown || parentType == kParentGrid)
    {
        result |= ui.combo(ui.context, "Horizontal alignment", &draft.horizontal_alignment, alignment, 4);
        result |= ui.combo(ui.context, "Vertical alignment", &draft.vertical_alignment, alignment, 4);
        result |= ui.number(ui.context, "Grid row", &draft.row, 0, 4095);
        result |= ui.number(ui.context, "Grid column", &draft.column, 0, 4095);
        result |= ui.number(ui.context, "Row span", &draft.row_span, 1, 4096);
        result |= ui.number(ui.context, "Column span", &draft.column_span, 1, 4096);
    }
    if (unknown || parentType == kParentOverlay)
        result |= ui.number(ui.context, "Render order", &draft.render_order, -1000000, 1000000);
    if (parentType == kParentLinear)
        ui.text(ui.context, "The Linear parent places children in sibling order; there is nothing to set here.");
    return result;
}

uint32_t DrawTextOverride(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    uint32_t result = 0U;
    if (draft.override_text)
    {
        ui.text(ui.context, "Text: local override");
        result |= ui.input_text(ui.context, "Text", draft.text, sizeof(draft.text));
        if (ui.button(ui.context, "Reset text to inherited"))
        {
            draft.override_text = 0U;
            result |= kInstanceEdit;
        }
        return result;
    }
    ui.text(ui.context, "Text: inherited from the shared definition");
    ui.text(ui.context, draft.default_text[0] ? draft.default_text : "(empty)");
    if (ui.button(ui.context, "Override text locally"))
    {
        draft.override_text = 1U;
        result |= kInstanceEdit;
    }
    return result;
}

uint32_t DrawStateSection(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (!BeginSection(ui, "api1_widget_state", "State overrides", true)) return 0U;
    uint32_t result = 0U;
    result |= ui.checkbox(ui.context, "Override checked", &draft.override_checked);
    if (draft.override_checked) result |= ui.checkbox(ui.context, "Checked", &draft.checked);
    result |= ui.checkbox(ui.context, "Override warning", &draft.override_warning);
    if (draft.override_warning) result |= ui.checkbox(ui.context, "Warning", &draft.warning);
    result |= ui.checkbox(ui.context, "Override value", &draft.override_value);
    if (draft.override_value) result |= ui.number(ui.context, "Value", &draft.value, draft.minimum, draft.maximum);
    result |= DrawTextOverride(ui, draft);
    const bool hasOverrides = draft.override_size || draft.override_checked || draft.override_warning ||
                              draft.override_value || draft.override_text;
    if (hasOverrides && ui.button(ui.context, "Reset all instance overrides")) result |= kResetOverrides;
    return result;
}

uint32_t DrawDefinitionFields(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    ui.text(ui.context, "Shared definition: changes affect every instance without a local override.");
    uint32_t changed = ui.input_text(ui.context, "Definition ID", draft.definition_id, sizeof(draft.definition_id));
    const char* types[] = {"Button", "Toggle", "Checkbox", "Radio", "Slider", "Knob", "Gauge", "ProgressBar", "Indicator", "NumericDisplay", "Label"};
    changed |= ui.combo(ui.context, "Semantic type", &draft.type, types, 11);
    changed |= ui.number(ui.context, "Minimum", &draft.minimum, -1.0e12, 1.0e12);
    changed |= ui.number(ui.context, "Maximum", &draft.maximum, -1.0e12, 1.0e12);
    changed |= ui.number(ui.context, "Step (0 = continuous)", &draft.step, 0, 1.0e12);
    changed |= ui.number(ui.context, "Decimal places", &draft.decimals, 0, 9);
    changed |= ui.number(ui.context, "Hit center X", &draft.hit_x, -10000, 10000);
    changed |= ui.number(ui.context, "Hit center Y", &draft.hit_y, -10000, 10000);
    changed |= ui.number(ui.context, "Hit width", &draft.hit_width, 0.00001, 10000);
    changed |= ui.number(ui.context, "Hit height", &draft.hit_height, 0.00001, 10000);
    changed |= ui.number(ui.context, "Preferred width", &draft.preferred_width, 0.00001, 10000);
    changed |= ui.number(ui.context, "Preferred height", &draft.preferred_height, 0.00001, 10000);
    changed |= ui.checkbox(ui.context, "Default checked", &draft.default_checked);
    changed |= ui.checkbox(ui.context, "Default warning", &draft.default_warning);
    changed |= ui.number(ui.context, "Default value", &draft.default_value, draft.minimum, draft.maximum);
    changed |= ui.input_text(ui.context, "Default text", draft.default_text, sizeof(draft.default_text));
    return changed ? kDefinitionEdit : 0U;
}

uint32_t DrawVisualSection(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    if (!BeginSection(ui, "api1_widget_visuals", "Visual states and bindings", false)) return 0U;
    uint32_t result = 0U;
    const char* states[] = {"Normal", "Hover", "Pressed", "Checked", "Disabled", "Warning", "Pending"};
    ui.combo(ui.context, "Visual state / preview", &draft.visual_state, states, 7);
    if (ui.button(ui.context, "Edit visual in Reticle studio")) result |= kEditVisual;
    if (ui.button(ui.context, "Apply edited visual to definition")) result |= kApplyVisual;
    ui.text(ui.context, "Value binding for the selected visual state");
    ui.input_text(ui.context, "Primitive ID", draft.binding_primitive, sizeof(draft.binding_primitive));
    const char* targets[] = {"Text", "Rotation", "Scale X", "Scale Y", "Position X", "Position Y", "Visible"};
    ui.combo(ui.context, "Binding target", &draft.binding_target, targets, 7);
    ui.number(ui.context, "At minimum", &draft.binding_from, -10000, 10000);
    ui.number(ui.context, "At maximum", &draft.binding_to, -10000, 10000);
    if (ui.button(ui.context, "Apply value binding")) result |= kApplyBinding;
    if (ui.button(ui.context, "Remove value binding")) result |= kRemoveBinding;
    return result;
}

/** @brief Instance inspector: the shared definition opens explicitly and collapsed, never by default. */
void DrawSharedDefinitionGate(const MfdWidgetEditorUiV1& ui, MfdWidgetEditorDraftV1& draft)
{
    const bool sectionsAvailable = ui.struct_size >= offsetof(MfdWidgetEditorUiV1, parent_picker) && ui.begin_section;
    if (sectionsAvailable)
    {
        draft.edit_definition = ui.begin_section(ui.context, "api1_widget_shared_definition", "Shared definition", 0U) != 0U;
    }
    else
    {
        ui.checkbox(ui.context, "Edit shared definition", &draft.edit_definition);
    }
}

uint32_t MFD_WIDGET_EDITOR_CALL Draw(const MfdWidgetEditorUiV1* ui, MfdWidgetEditorDraftV1* draft)
{
    if (!IsUiValid(ui) || !IsDraftValid(draft)) return 0U;
    uint32_t result = 0U;
    if (!draft->definition_only)
    {
        result |= DrawInstanceSection(*ui, *draft);
        result |= DrawGeometrySection(*ui, *draft);
        result |= DrawPlacementSection(*ui, *draft);
        result |= DrawStateSection(*ui, *draft);
        DrawSharedDefinitionGate(*ui, *draft);
        if (!draft->edit_definition) return result;
    }
    result |= DrawDefinitionFields(*ui, *draft);
    result |= DrawVisualSection(*ui, *draft);
    return result;
}
}
uint32_t MFD_WIDGET_EDITOR_CALL mfd_get_widgets_editor_api_v1(MfdWidgetEditorApiV1* output)
{
    if (!output || output->struct_size < sizeof(*output)) return 1U;
    *output = {sizeof(*output), MFD_WIDGET_EDITOR_ABI_VERSION, MFD_WIDGET_EDITOR_CAPABILITY_DESIGNER, Draw};
    return 0U;
}
