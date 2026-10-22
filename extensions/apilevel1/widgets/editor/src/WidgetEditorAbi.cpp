#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
namespace
{
uint32_t MFD_WIDGET_EDITOR_CALL Draw(const MfdWidgetEditorUiV1* ui, MfdWidgetEditorDraftV1* draft)
{
    if (!ui || !draft || ui->struct_size < sizeof(*ui) || draft->struct_size < sizeof(*draft) ||
        ui->version != 1U || !ui->text || !ui->input_text || !ui->checkbox || !ui->number || !ui->combo || !ui->button) return 0U;
    void* context = ui->context;
    uint32_t result = 0U;
    ui->text(context, "Instance (independent overrides)");
    result |= ui->input_text(context, "Instance ID", draft->id, sizeof(draft->id));
    result |= ui->input_text(context, "Parent (empty = root)", draft->parent, sizeof(draft->parent));
    result |= ui->checkbox(context, "Visible", &draft->visible);
    result |= ui->checkbox(context, "Enabled", &draft->enabled);
    result |= ui->checkbox(context, "Exposed C++ handle", &draft->exposed);
    result |= ui->number(context, "Position X", &draft->x, -10000, 10000);
    result |= ui->number(context, "Position Y", &draft->y, -10000, 10000);
    result |= ui->checkbox(context, "Override size", &draft->override_size);
    if (draft->override_size)
    {
        result |= ui->number(context, "Width", &draft->width, 0.00001, 10000);
        result |= ui->number(context, "Height", &draft->height, 0.00001, 10000);
    }
    result |= ui->checkbox(context, "Override checked", &draft->override_checked);
    if (draft->override_checked) result |= ui->checkbox(context, "Checked", &draft->checked);
    result |= ui->checkbox(context, "Override warning", &draft->override_warning);
    if (draft->override_warning) result |= ui->checkbox(context, "Warning", &draft->warning);
    result |= ui->checkbox(context, "Override value", &draft->override_value);
    if (draft->override_value) result |= ui->number(context, "Value", &draft->value, draft->minimum, draft->maximum);
    result |= ui->checkbox(context, "Override text", &draft->override_text);
    if (draft->override_text) result |= ui->input_text(context, "Text", draft->text, sizeof(draft->text));
    if (draft->parent[0])
    {
        const char* resizing[] = {"Fixed", "Proportional", "Stretch"};
        const char* alignment[] = {"Start", "Center", "End", "Stretch"};
        result |= ui->combo(context, "Resize", &draft->resize_mode, resizing, 3);
        result |= ui->combo(context, "Horizontal alignment", &draft->horizontal_alignment, alignment, 4);
        result |= ui->combo(context, "Vertical alignment", &draft->vertical_alignment, alignment, 4);
        result |= ui->number(context, "Grid row", &draft->row, 0, 4095);
        result |= ui->number(context, "Grid column", &draft->column, 0, 4095);
        result |= ui->number(context, "Row span", &draft->row_span, 1, 4096);
        result |= ui->number(context, "Column span", &draft->column_span, 1, 4096);
        result |= ui->number(context, "Render order", &draft->render_order, -1000000, 1000000);
    }
    if (ui->button(context, "Reset instance overrides")) result |= 16U;
    ui->checkbox(context, "Edit shared definition", &draft->edit_definition);
    if (!draft->edit_definition) return result;
    ui->text(context, "Shared definition: changes affect every referencing instance.");
    uint32_t definitionChanged = ui->input_text(context, "Definition ID", draft->definition_id, sizeof(draft->definition_id));
    const char* types[] = {"Button", "Toggle", "Checkbox", "Radio", "Slider", "Knob", "Gauge", "ProgressBar", "Indicator", "NumericDisplay", "Label"};
    definitionChanged |= ui->combo(context, "Semantic type", &draft->type, types, 11);
    definitionChanged |= ui->number(context, "Minimum", &draft->minimum, -1.0e12, 1.0e12);
    definitionChanged |= ui->number(context, "Maximum", &draft->maximum, -1.0e12, 1.0e12);
    definitionChanged |= ui->number(context, "Step (0 = continuous)", &draft->step, 0, 1.0e12);
    definitionChanged |= ui->number(context, "Decimal places", &draft->decimals, 0, 9);
    definitionChanged |= ui->number(context, "Hit center X", &draft->hit_x, -10000, 10000);
    definitionChanged |= ui->number(context, "Hit center Y", &draft->hit_y, -10000, 10000);
    definitionChanged |= ui->number(context, "Hit width", &draft->hit_width, 0.00001, 10000);
    definitionChanged |= ui->number(context, "Hit height", &draft->hit_height, 0.00001, 10000);
    if (definitionChanged) result |= 2U;
    const char* states[] = {"Normal", "Hover", "Pressed", "Checked", "Disabled", "Warning", "Pending"};
    ui->combo(context, "Visual state / preview", &draft->visual_state, states, 7);
    if (ui->button(context, "Edit visual in Reticle studio")) result |= 4U;
    if (ui->button(context, "Apply edited visual to definition")) result |= 8U;
    return result;
}
}
uint32_t MFD_WIDGET_EDITOR_CALL mfd_get_widgets_editor_api_v1(MfdWidgetEditorApiV1* output)
{
    if (!output || output->struct_size < sizeof(*output)) return 1U;
    *output = {sizeof(*output), MFD_WIDGET_EDITOR_ABI_VERSION, MFD_WIDGET_EDITOR_CAPABILITY_DESIGNER, Draw};
    return 0U;
}
