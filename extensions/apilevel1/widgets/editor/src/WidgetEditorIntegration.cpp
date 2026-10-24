#include "EditorApplication.h"
#include "EditorFileDialogs.h"
#include "EditorIcons.h"
#include "mfd/apilevel1/widgets/WidgetAssetService.h"
#include "EditorUiTheme.h"
#include "mfd/apilevel1/widgets/WidgetJson.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/model/PageName.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"
#include "ContainerPointerQuery.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace
{
void CopyWidgetText(char* destination, const std::size_t capacity, const std::string& value)
{
    const auto count = std::min(value.size(), capacity - 1);
    std::memcpy(destination, value.data(), count);
    destination[count] = 0;
}
std::string WidgetParent(const mfd::PageDefinition& page, const std::string& id)
{
    for (const auto& container : page.containers)
        for (const auto& edge : container.children)
            if (edge.kind == mfd::api1::ContainerChildKind::Widget && edge.childId == id) return container.id;
    return {};
}

const mfd::api1::ContainerChildPlacement* WidgetPlacement(const mfd::PageDefinition& page,
                                                          const std::string_view id) noexcept
{
    for (const mfd::api1::ContainerDefinition& container : page.containers)
    {
        for (const mfd::api1::ContainerChildPlacement& edge : container.children)
        {
            if (edge.kind == mfd::api1::ContainerChildKind::Widget && edge.childId == id)
            {
                return &edge;
            }
        }
    }
    return nullptr;
}

bool SameOptionalSize(const std::optional<mfd::Vec2>& first,
                      const std::optional<mfd::Vec2>& second) noexcept
{
    return first.has_value() == second.has_value() &&
           (!first.has_value() || (first->x == second->x && first->y == second->y));
}

bool SamePlacement(const mfd::api1::ContainerChildPlacement& first,
                   const mfd::api1::ContainerChildPlacement& second) noexcept
{
    return first.resizeMode == second.resizeMode &&
           first.horizontalAlignment == second.horizontalAlignment &&
           first.verticalAlignment == second.verticalAlignment && first.row == second.row &&
           first.column == second.column && first.rowSpan == second.rowSpan &&
           first.columnSpan == second.columnSpan && first.renderOrder == second.renderOrder;
}

bool HasValidStateOnlyOverrides(const mfd::api1::WidgetInstance& widget) noexcept
{
    if (!widget.definition)
    {
        return false;
    }
    if (widget.overrides.value &&
        (!std::isfinite(*widget.overrides.value) || *widget.overrides.value < widget.definition->range.minimum ||
         *widget.overrides.value > widget.definition->range.maximum))
    {
        return false;
    }
    return !widget.overrides.text ||
           (widget.overrides.text->size() <= mfd::api1::kMaximumWidgetTextBytes &&
            widget.overrides.text->find('\0') == std::string::npos);
}

bool IsStateOnlyWidgetEdit(const mfd::PageDefinition& page,
                           const mfd::api1::WidgetInstance& current,
                           const mfd::api1::WidgetInstance& replacement,
                           const std::string_view parent,
                           const mfd::api1::ContainerChildPlacement& placement)
{
    if (current.id != replacement.id || current.position.x != replacement.position.x ||
        current.position.y != replacement.position.y || !SameOptionalSize(current.size, replacement.size) ||
        WidgetParent(page, current.id) != parent || !HasValidStateOnlyOverrides(replacement))
    {
        return false;
    }
    if (parent.empty())
    {
        return true;
    }
    const mfd::api1::ContainerChildPlacement* currentPlacement = WidgetPlacement(page, current.id);
    return currentPlacement != nullptr && SamePlacement(*currentPlacement, placement);
}

void ApplyWidgetStateOnlyEdit(mfd::api1::WidgetInstance& target,
                              const mfd::api1::WidgetInstance& replacement)
{
    target.visible = replacement.visible;
    target.enabled = replacement.enabled;
    target.exposed = replacement.exposed;
    target.overrides = replacement.overrides;
}
}
mfd::api1::WidgetInstance* EditorApplication::SelectedWidget() noexcept
{
    auto* page = ActivePage();
    return page && documentState_.selection.kind == SelectionKind::Widget ?
        mfd::api1::WidgetEditorModel::Find(*page, documentState_.selection.widgetId) : nullptr;
}
void EditorApplication::SelectWidget(const int pageIndex, std::string id)
{
    if (pageIndex < 0 || static_cast<std::size_t>(pageIndex) >= documentState_.loaded.document.pages.size() ||
        !mfd::api1::WidgetEditorModel::Find(documentState_.loaded.document.pages[pageIndex], id)) return;
    const bool selectionChanged = documentState_.selection.kind != SelectionKind::Widget ||
                                  documentState_.selection.pageIndex != pageIndex ||
                                  documentState_.selection.widgetId != id;
    documentState_.selection = {};
    documentState_.selection.kind = SelectionKind::Widget;
    documentState_.selection.pageIndex = pageIndex;
    documentState_.selection.widgetId = std::move(id);
    layoutState_.focusedStudioFitPending = selectionChanged;
}

void EditorApplication::SelectWidgetDefinition(const std::size_t index)
{
    const auto& library = documentState_.loaded.document.widgetLibrary;
    if (index >= library.size()) return;
    const bool selectionChanged = documentState_.selection.kind != SelectionKind::WidgetDefinition ||
                                  widgetLibrarySelected_ != index;
    const int activePageIndex = documentState_.selection.pageIndex;
    widgetLibrarySelected_ = index;
    documentState_.selection = {};
    documentState_.selection.kind = SelectionKind::WidgetDefinition;
    documentState_.selection.pageIndex = activePageIndex;
    documentState_.selection.widgetDefinitionIndex = index;
    layoutState_.focusedStudioFitPending = selectionChanged;
}
void EditorApplication::DrawSidebarWidgetsSection(const float height)
{
    ImGui::BeginChild("SidebarWidgetsSection", ImVec2(0, std::max(1.0f, height)), false);
    auto* page = ActivePage();
    const auto count = page ? page->widgets.size() : 0;
    const std::string title = "Widgets (" + std::to_string(count) + ")###SidebarWidgetsHeader";
    if (editor::ui::BeginNavigationSection(title.c_str(), false, layoutState_.sidebarWidgetsExpanded))
    {
        DrawWidgetActionToolbar();
        ImGui::Spacing();
        ImGui::BeginChild(
            "SidebarWidgetsScrollRegion",
            ImVec2(0, std::max(1.0f, ImGui::GetContentRegionAvail().y)),
            false);
        page = ActivePage();
        if (!page || page->widgets.empty()) ImGui::TextDisabled("No widget. Use + or the definition library.");
        if (page)
        {
            const auto filter = mfd::NormalizePageName(ResolveSidebarFilter().text);
            for (const auto& widget : page->widgets)
            {
                if (!filter.empty() && mfd::NormalizePageName(widget.id).find(filter) == std::string::npos) continue;
                const auto parent = WidgetParent(*page, widget.id);
                const std::string label = widget.id + (parent.empty() ? " (root)" : " / " + parent);
                if (ImGui::Selectable(label.c_str(), documentState_.selection.kind == SelectionKind::Widget &&
                                      documentState_.selection.widgetId == widget.id))
                {
                    SelectWidget(documentState_.selection.pageIndex, widget.id);
                }
                if (widget.definition != nullptr)
                {
                    const mfd::api1::ContainerRect previewBounds {
                        {}, widget.size.value_or(widget.definition->preferredSize)};
                    const mfd::ColorRgba& background = page->backgroundColor;
                    DrawWidgetHoverPreviewTooltip(
                        widget,
                        previewBounds,
                        widget.id,
                        Color {background.r, background.g, background.b, background.a});
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

void EditorApplication::DrawWidgetActionToolbar()
{
    using editor::ui::EditorIcon;
    using editor::ui::IconButton;

    if (IconButton("##sidebar_add_widget_definition",
                   EditorIcon::Add,
                   "Create a widget definition and open its dedicated library editor."))
    {
        AddWidgetDefinition(false);
    }

    ImGui::SameLine();
    if (IconButton("##sidebar_open_widget_library",
                   EditorIcon::Library,
                   "Open the widget definition library and editor."))
    {
        widgetLibraryOpen_ = true;
        ScanWidgetLibrary();
    }

    const bool hasSelection = SelectedWidget() != nullptr;
    ImGui::SameLine();
    if (IconButton("##sidebar_duplicate_widget",
                   EditorIcon::Duplicate,
                   "Duplicate the selected widget instance.",
                   hasSelection))
    {
        DuplicateWidget();
    }

    ImGui::SameLine();
    if (IconButton("##sidebar_remove_widget",
                   EditorIcon::Delete,
                   "Remove the selected widget instance from this Page.",
                   hasSelection))
    {
        DeleteWidget();
    }

    if (!widgetVisualTemplate_.empty())
    {
        ImGui::SameLine();
        if (IconButton("##sidebar_apply_widget_visual",
                       EditorIcon::Apply,
                       "Apply the visual edited in Reticle studio to its widget definition."))
        {
            ApplyWidgetVisual();
        }
    }
}
void EditorApplication::ScanWidgetLibrary()
{
    try
    {
        const auto root = mfd::api1::WidgetAssetRoot(documentState_.loaded.window.sourceFile);
        const auto folder = root / "widgets";
        if (!std::filesystem::exists(folder)) return;
        std::size_t count = 0;
        auto candidate = documentState_.loaded.document.widgetLibrary;
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            if (++count > 4096) throw std::runtime_error("Widget library exceeds 4096 entries.");
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const auto file = std::filesystem::weakly_canonical(entry.path());
            if (std::any_of(candidate.begin(), candidate.end(), [&file](const auto& value) { return value->sourceFile == file; })) continue;
            candidate.push_back(documentState_.loader.LoadWidgetAsset(file, root, documentState_.loaded.document.reticleLibrary));
        }
        documentState_.loaded.document.widgetLibrary = std::move(candidate);
    }
    catch (const std::exception& error) { RebuildStatus(error.what(), true); }
}
void EditorApplication::DrawWidgetLibrary()
{
    if (!widgetLibraryOpen_) return;
    if (!ImGui::Begin("Widget definition library", &widgetLibraryOpen_)) { ImGui::End(); return; }
    if (ImGui::Button("New definition")) AddWidgetDefinition(false);
    ImGui::SameLine();
    if (ImGui::Button("Import")) ImportWidgetDefinition();
    ImGui::InputText("Search", widgetLibraryFilter_.data(), widgetLibraryFilter_.size());
    const auto& library = documentState_.loaded.document.widgetLibrary;
    for (std::size_t index = 0; index < library.size(); ++index)
    {
        if (mfd::NormalizePageName(library[index]->id).find(mfd::NormalizePageName(widgetLibraryFilter_.data())) == std::string::npos) continue;
        ImGui::PushID(static_cast<int>(index));
        const bool selected = documentState_.selection.kind == SelectionKind::WidgetDefinition &&
                              documentState_.selection.widgetDefinitionIndex == index;
        if (ImGui::Selectable(library[index]->id.c_str(), selected)) SelectWidgetDefinition(index);
        mfd::api1::WidgetInstance previewWidget;
        previewWidget.id = library[index]->id;
        previewWidget.definition = library[index];
        DrawWidgetHoverPreviewTooltip(
            previewWidget,
            {{}, library[index]->preferredSize},
            library[index]->id,
            Color {10, 18, 24, 255});
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("MFD_WIDGET_DEFINITION", &index, sizeof(index));
            ImGui::TextUnformatted(library[index]->id.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    if (widgetLibrarySelected_ < library.size())
    {
        ImGui::TextWrapped("%s", library[widgetLibrarySelected_]->sourceFile.string().c_str());
        ImGui::TextWrapped("%zu state visuals. Shared definition edits preserve instance overrides.", library[widgetLibrarySelected_]->visuals.size());
        if (ImGui::Button("Insert into active Page")) InsertWidgetDefinition(widgetLibrarySelected_);
        if (ImGui::Button("Duplicate definition")) AddWidgetDefinition(true);
        ImGui::SameLine();
        if (ImGui::Button("Export definition and resources")) ExportWidgetDefinition();
        if (ImGui::Button("Save library and document")) SaveAll();
        ImGui::TextDisabled("Select the definition to edit it in Widget Studio and the inspector.");
    }
    if (!widgetVisualTemplate_.empty() && ImGui::Button("Apply edited visual")) ApplyWidgetVisual();
    ImGui::End();
}
void EditorApplication::AddWidgetDefinition(const bool duplicate)
{
    if (documentState_.loaded.window.apiLevel != mfd::ApiLevel::Level1 || !ActivePage()) return;
    if (!widgetEditorHost_.EnsureLoaded(documentState_.loaded.window.sourceFile))
    { RebuildStatus(widgetEditorHost_.Diagnostic(), true); return; }
    try
    {
        auto& document = documentState_.loaded.document;
        if (document.widgetLibrary.size() >= 4096) throw std::runtime_error("Widget library is full.");
        const auto folder = mfd::api1::WidgetAssetRoot(documentState_.loaded.window.sourceFile) / "widgets";
        std::string id;
        std::filesystem::path file;
        for (std::size_t index = 1; index <= 8193; ++index)
        {
            id = "Widget" + std::to_string(index);
            file = std::filesystem::weakly_canonical(folder / (id + ".json"));
            if (!std::filesystem::exists(file) && std::none_of(document.widgetLibrary.begin(), document.widgetLibrary.end(),
                [&file](const auto& value) { return value->sourceFile == file; })) break;
        }
        auto definition = mfd::api1::WidgetEditorModel::CreateDefinition(file, id, mfd::api1::WidgetType::Button);
        if (duplicate && widgetLibrarySelected_ < document.widgetLibrary.size())
        {
            auto copy = std::make_shared<mfd::api1::WidgetDefinition>(*document.widgetLibrary[widgetLibrarySelected_]);
            copy->id = id; copy->sourceFile = file; definition = std::move(copy);
        }
        auto before = CaptureCurrentSnapshot();
        std::string error;
        if (!mfd::api1::WidgetEditorModel::ReplaceDefinition(document, definition, error))
        { RebuildStatus(error, true); return; }
        widgetLibrarySelected_ = document.widgetLibrary.size() - 1;
        PushUndoSnapshot(std::move(before));
        widgetLibraryOpen_ = true;
        SelectWidgetDefinition(widgetLibrarySelected_);
        RebuildStatus("Widget definition created. Edit it in Widget Studio, then insert independent instances.", false);
    }
    catch (const std::exception& error) { RebuildStatus(error.what(), true); }
}
void EditorApplication::InsertWidgetDefinition(const std::size_t index)
{
    auto* page = ActivePage();
    const auto& library = documentState_.loaded.document.widgetLibrary;
    if (!page || index >= library.size() || documentState_.loaded.window.apiLevel != mfd::ApiLevel::Level1) return;
    if (!widgetEditorHost_.EnsureLoaded(documentState_.loaded.window.sourceFile))
    { RebuildStatus(widgetEditorHost_.Diagnostic(), true); return; }
    const auto before = CaptureCurrentSnapshot();
    const std::string parent = documentState_.selection.kind == SelectionKind::Container ? documentState_.selection.containerId : "";
    const auto reference = std::filesystem::relative(library[index]->sourceFile,
        documentState_.files.pageFiles[documentState_.selection.pageIndex].parent_path()).generic_string();
    std::string error;
    const auto id = mfd::api1::WidgetEditorModel::Insert(*page, library[index], reference, parent, error);
    if (id.empty()) { RebuildStatus(error, true); return; }
    PushUndoSnapshot(before);
    SelectWidget(documentState_.selection.pageIndex, id);
}
void EditorApplication::DuplicateWidget()
{
    auto* page = ActivePage(); if (!page || !SelectedWidget()) return;
    const auto before = CaptureCurrentSnapshot();
    std::string error;
    const auto id = mfd::api1::WidgetEditorModel::Duplicate(*page, documentState_.selection.widgetId, error);
    if (id.empty()) { RebuildStatus(error, true); return; }
    PushUndoSnapshot(before);
    SelectWidget(documentState_.selection.pageIndex, id);
}
void EditorApplication::DeleteWidget()
{
    auto* page = ActivePage(); if (!page || !SelectedWidget()) return;
    const auto before = CaptureCurrentSnapshot();
    std::string error;
    if (!mfd::api1::WidgetEditorModel::Remove(*page, documentState_.selection.widgetId, error))
    { RebuildStatus(error, true); return; }
    PushUndoSnapshot(before);
    SelectPage(documentState_.selection.pageIndex, false);
}
std::shared_ptr<const mfd::api1::WidgetDefinition> EditorApplication::SelectedWidgetDefinition(const bool definitionOnly)
{
    if (definitionOnly)
    {
        const auto& library = documentState_.loaded.document.widgetLibrary;
        const std::size_t definitionIndex = documentState_.selection.kind == SelectionKind::WidgetDefinition
                                                ? documentState_.selection.widgetDefinitionIndex
                                                : widgetLibrarySelected_;
        return definitionIndex < library.size() ? library[definitionIndex] : nullptr;
    }
    const auto* widget = SelectedWidget();
    return widget ? widget->definition : nullptr;
}
void EditorApplication::DrawWidgetInspector(const bool definitionOnly)
{
    mfd::api1::WidgetInstance preview;
    preview.definition = SelectedWidgetDefinition(definitionOnly);
    if (!preview.definition) return;
    preview.id = preview.definition->id;
    const auto* widget = definitionOnly ? &preview : SelectedWidget();
    const auto* page = ActivePage();
    if (!widget || !page) return;
    if (!widgetEditorHost_.EnsureLoaded(documentState_.loaded.window.sourceFile))
    { ImGui::TextWrapped("%s", widgetEditorHost_.Diagnostic().c_str()); return; }
    auto& draft = widgetDraft_;
    const auto& definition = *widget->definition;
    const auto value = mfd::api1::InitialWidgetState(*widget).confirmed;
    draft.struct_size = sizeof(draft);
    draft.definition_only = definitionOnly;
    CopyWidgetText(draft.id, sizeof(draft.id), widget->id);
    CopyWidgetText(draft.parent, sizeof(draft.parent), WidgetParent(*page, widget->id));
    CopyWidgetText(draft.text, sizeof(draft.text), value.text);
    CopyWidgetText(draft.definition_id, sizeof(draft.definition_id), definition.id);
    draft.visible = widget->visible; draft.enabled = widget->enabled; draft.exposed = widget->exposed;
    draft.checked = value.checked; draft.warning = value.warning; draft.value = value.value;
    draft.override_checked = widget->overrides.checked.has_value(); draft.override_warning = widget->overrides.warning.has_value();
    draft.override_value = widget->overrides.value.has_value(); draft.override_text = widget->overrides.text.has_value();
    draft.x = widget->position.x; draft.y = widget->position.y;
    draft.override_size = widget->size.has_value();
    const auto size = widget->size.value_or(definition.preferredSize);
    draft.width = size.x; draft.height = size.y;
    draft.preferred_width = definition.preferredSize.x; draft.preferred_height = definition.preferredSize.y;
    draft.default_checked = definition.defaults.checked; draft.default_warning = definition.defaults.warning;
    draft.default_value = definition.defaults.value;
    CopyWidgetText(draft.default_text, sizeof(draft.default_text), definition.defaults.text);
    draft.minimum = definition.range.minimum; draft.maximum = definition.range.maximum;
    draft.step = definition.range.step; draft.decimals = definition.range.decimals;
    draft.type = static_cast<int>(definition.type);
    draft.hit_x = definition.hitRegion.center.x; draft.hit_y = definition.hitRegion.center.y;
    draft.hit_width = definition.hitRegion.size.x; draft.hit_height = definition.hitRegion.size.y;
    for (const auto& container : page->containers)
        for (const auto& edge : container.children)
            if (edge.kind == mfd::api1::ContainerChildKind::Widget && edge.childId == widget->id)
            {
                draft.resize_mode = static_cast<int>(edge.resizeMode);
                draft.horizontal_alignment = static_cast<int>(edge.horizontalAlignment);
                draft.vertical_alignment = static_cast<int>(edge.verticalAlignment);
                draft.row = edge.row; draft.column = edge.column; draft.row_span = edge.rowSpan; draft.column_span = edge.columnSpan;
                draft.render_order = edge.renderOrder;
            }
    const auto actions = widgetEditorHost_.Draw(draft);
    if (actions & 19U) ApplyWidgetInspector(actions, definitionOnly);
    if (actions & 4U) EditWidgetVisual(definitionOnly);
    if (actions & 8U) ApplyWidgetVisual();
    if (actions & 32U) ApplyWidgetBinding(false, definitionOnly);
    if (actions & 64U) ApplyWidgetBinding(true, definitionOnly);
}
void EditorApplication::ApplyWidgetInspector(const std::uint32_t actions, const bool definitionOnly)
{
    mfd::api1::WidgetInstance preview;
    preview.definition = SelectedWidgetDefinition(definitionOnly);
    if (!preview.definition) return;
    preview.id = preview.definition->id;
    auto* page = ActivePage();
    const auto* widget = definitionOnly ? &preview : SelectedWidget();
    if (!page || !widget) return;
    const auto& draft = widgetDraft_;
    const std::array<double, 21> numbers {draft.x, draft.y, draft.width, draft.height, draft.value,
        draft.row, draft.column, draft.row_span, draft.column_span, draft.render_order,
        draft.minimum, draft.maximum, draft.step, draft.decimals, draft.hit_x, draft.hit_y, draft.hit_width,
        draft.hit_height, draft.preferred_width, draft.preferred_height, draft.default_value};
    if (!std::all_of(numbers.begin(), numbers.end(), [](const double value) { return std::isfinite(value); }) ||
        draft.row < 0 || draft.row > 4095 || draft.column < 0 || draft.column > 4095 ||
        draft.row_span < 0 || draft.row_span > 4096 || draft.column_span < 0 || draft.column_span > 4096 ||
        std::abs(draft.render_order) > 1000000 || draft.decimals < 0 || draft.decimals > 9 ||
        draft.type < 0 || draft.type > static_cast<int>(mfd::api1::WidgetType::Label) ||
        draft.resize_mode < 0 || draft.resize_mode > static_cast<int>(mfd::api1::ChildResizeMode::Stretch) ||
        draft.horizontal_alignment < 0 ||
        draft.horizontal_alignment > static_cast<int>(mfd::api1::ContainerAlignment::Stretch) ||
        draft.vertical_alignment < 0 ||
        draft.vertical_alignment > static_cast<int>(mfd::api1::ContainerAlignment::Stretch) ||
        std::floor(draft.row) != draft.row || std::floor(draft.column) != draft.column ||
        std::floor(draft.row_span) != draft.row_span || std::floor(draft.column_span) != draft.column_span ||
        std::floor(draft.render_order) != draft.render_order || std::floor(draft.decimals) != draft.decimals)
    { RebuildStatus("Widget numeric fields require finite values and integer grid/format fields.", true); return; }
    mfd::api1::WidgetInstance replacement = *widget;
    const std::string previousId = widget->id;
    std::string error;
    if (actions & 1U)
    {
        replacement.id = draft.id;
        replacement.visible = draft.visible != 0; replacement.enabled = draft.enabled != 0; replacement.exposed = draft.exposed != 0;
        replacement.position = {static_cast<float>(draft.x), static_cast<float>(draft.y)};
        replacement.size = draft.override_size ? std::optional<mfd::Vec2>({static_cast<float>(draft.width), static_cast<float>(draft.height)}) : std::nullopt;
        replacement.overrides = {};
        if (draft.override_checked) replacement.overrides.checked = draft.checked != 0;
        if (draft.override_warning) replacement.overrides.warning = draft.warning != 0;
        if (draft.override_value) replacement.overrides.value = draft.value;
        if (draft.override_text) replacement.overrides.text = draft.text;
    }
    if (actions & 16U) { replacement.overrides = {}; replacement.size.reset(); }
    mfd::api1::ContainerChildPlacement edge;
    edge.resizeMode = static_cast<mfd::api1::ChildResizeMode>(draft.resize_mode);
    edge.horizontalAlignment = static_cast<mfd::api1::ContainerAlignment>(draft.horizontal_alignment);
    edge.verticalAlignment = static_cast<mfd::api1::ContainerAlignment>(draft.vertical_alignment);
    edge.row = static_cast<std::uint32_t>(draft.row); edge.column = static_cast<std::uint32_t>(draft.column);
    edge.rowSpan = std::max(1U, static_cast<std::uint32_t>(draft.row_span)); edge.columnSpan = std::max(1U, static_cast<std::uint32_t>(draft.column_span));
    edge.renderOrder = static_cast<int>(draft.render_order);

    const auto before = CaptureCurrentSnapshot();
    const bool changesDefinition = (actions & 2U) != 0U;
    if (!definitionOnly && !changesDefinition)
    {
        if (IsStateOnlyWidgetEdit(*page, *widget, replacement, draft.parent, edge))
        {
            mfd::api1::WidgetInstance* liveWidget = mfd::api1::WidgetEditorModel::Find(*page, previousId);
            if (liveWidget == nullptr)
            {
                RebuildStatus("The selected widget no longer exists.", true);
                return;
            }
            ApplyWidgetStateOnlyEdit(*liveWidget, replacement);
        }
        else if (!mfd::api1::WidgetEditorModel::Replace(*page, previousId, replacement, draft.parent, &edge, error))
        {
            RebuildStatus(error, true);
            return;
        }
        PushUndoSnapshot(before);
        SelectWidget(documentState_.selection.pageIndex, replacement.id);
        return;
    }

    auto candidate = documentState_.loaded.document;
    auto& candidatePage = candidate.pages[documentState_.selection.pageIndex];
    if (!definitionOnly && !mfd::api1::WidgetEditorModel::Replace(
            candidatePage, previousId, replacement, draft.parent, &edge, error))
    {
        RebuildStatus(error, true);
        return;
    }
    if (actions & 2U)
    {
        auto definition = std::make_shared<mfd::api1::WidgetDefinition>(*replacement.definition);
        definition->id = draft.definition_id; definition->type = static_cast<mfd::api1::WidgetType>(draft.type);
        definition->preferredSize = {static_cast<float>(draft.preferred_width), static_cast<float>(draft.preferred_height)};
        definition->defaults = {draft.default_checked != 0, draft.default_warning != 0, draft.default_value, draft.default_text};
        definition->range = {draft.minimum, draft.maximum, draft.step, static_cast<std::uint32_t>(draft.decimals)};
        definition->hitRegion = {{static_cast<float>(draft.hit_x), static_cast<float>(draft.hit_y)},
                                 {static_cast<float>(draft.hit_width), static_cast<float>(draft.hit_height)}};
        if (!mfd::api1::WidgetEditorModel::ReplaceDefinition(candidate, definition, error))
        { RebuildStatus(error, true); return; }
    }
    documentState_.loaded.document = std::move(candidate);
    PushUndoSnapshot(before);
    if (!definitionOnly)
    {
        SelectWidget(documentState_.selection.pageIndex, replacement.id);
    }
    else
    {
        SelectWidgetDefinition(widgetLibrarySelected_);
    }
}
void EditorApplication::EditWidgetVisual(const bool definitionOnly)
{
    const auto selected = SelectedWidgetDefinition(definitionOnly); if (!selected) return;
    const auto& definition = *selected;
    const auto state = static_cast<mfd::api1::WidgetVisualState>(widgetDraft_.visual_state);
    const auto visual = std::find_if(definition.visuals.begin(), definition.visuals.end(),
        [state](const auto& value) { return value.state == state; });
    const auto normal = std::find_if(definition.visuals.begin(), definition.visuals.end(),
        [](const auto& value) { return value.state == mfd::api1::WidgetVisualState::Normal; });
    auto composition = (visual == definition.visuals.end() ? normal : visual)->composition;
    const auto before = CaptureCurrentSnapshot();
    std::string id = "WidgetVisual_" + definition.id;
    for (std::size_t suffix = 2; documentState_.loaded.document.reticleLibrary.count(id) && suffix < 8192; ++suffix)
        id = "WidgetVisual_" + definition.id + std::to_string(suffix);
    composition.id = id;
    composition.sourceTemplateId.clear();
    documentState_.loaded.document.reticleLibrary.emplace(id, std::move(composition));
    widgetVisualTemplate_ = id; widgetVisualDefinition_ = definition.sourceFile; widgetVisualState_ = state;
    PushUndoSnapshot(before);
    SelectLibraryReticle(id);
    RebuildStatus("Edit primitives in Reticle studio, then use Apply edited visual in Widgets.", false);
}
void EditorApplication::ApplyWidgetVisual()
{
    const auto source = documentState_.loaded.document.reticleLibrary.find(widgetVisualTemplate_);
    if (source == documentState_.loaded.document.reticleLibrary.end()) return;
    const auto& library = documentState_.loaded.document.widgetLibrary;
    const auto found = std::find_if(library.begin(), library.end(), [this](const auto& definition)
        { return definition->sourceFile == widgetVisualDefinition_; });
    if (found == library.end()) { RebuildStatus("The edited widget definition is no longer in this document.", true); return; }
    auto replacement = std::make_shared<mfd::api1::WidgetDefinition>(**found);
    auto visual = std::find_if(replacement->visuals.begin(), replacement->visuals.end(), [this](const auto& value)
        { return value.state == widgetVisualState_; });
    if (visual == replacement->visuals.end())
    { replacement->visuals.push_back({widgetVisualState_, source->second, {}}); }
    else visual->composition = source->second;
    const auto before = CaptureCurrentSnapshot();
    const std::size_t definitionIndex = static_cast<std::size_t>(found - library.begin());
    std::string error;
    if (!mfd::api1::WidgetEditorModel::ReplaceDefinition(documentState_.loaded.document, replacement, error))
    { RebuildStatus(error, true); return; }
    PushUndoSnapshot(before);
    SelectWidgetDefinition(definitionIndex);
    RebuildStatus("Shared widget visual updated; instance overrides retained.", false);
}

void EditorApplication::ImportWidgetDefinition()
{
    if (!widgetEditorHost_.EnsureLoaded(documentState_.loaded.window.sourceFile))
    { RebuildStatus(widgetEditorHost_.Diagnostic(), true); return; }
    std::string error;
    const auto file = editor::OpenPageAssetFileDialog({}, &error);
    if (!file) { if (!error.empty()) RebuildStatus(error, true); return; }
    try
    {
        const auto sourceRoot = mfd::api1::WidgetAssetRoot(*file);
        const auto library = documentState_.loader.LoadReticleAssets(sourceRoot / "reticles");
        const auto definition = documentState_.loader.LoadWidgetAsset(*file, sourceRoot, library);
        auto candidate = documentState_.loaded.document;
        auto files = documentState_.files;
        const auto imported = editor::WidgetAssetService::Stage(*definition, sourceRoot,
            mfd::api1::WidgetAssetRoot(documentState_.loaded.window.sourceFile), candidate, files);
        const auto before = CaptureCurrentSnapshot();
        documentState_.loaded.document = std::move(candidate);
        documentState_.files = std::move(files);
        const auto& definitions = documentState_.loaded.document.widgetLibrary;
        widgetLibrarySelected_ = static_cast<std::size_t>(std::find(definitions.begin(), definitions.end(), imported) - definitions.begin());
        PushUndoSnapshot(before);
        SelectWidgetDefinition(widgetLibrarySelected_);
        RebuildStatus("Widget and resources imported. Save commits all authored assets together.", false);
    }
    catch (const std::exception& failure) { RebuildStatus(failure.what(), true); }
}
void EditorApplication::ExportWidgetDefinition()
{
    const auto& library = documentState_.loaded.document.widgetLibrary;
    if (widgetLibrarySelected_ >= library.size()) return;
    std::string error;
    const auto folder = editor::OpenFolderDialog({}, "Export widget package", &error);
    if (!folder) { if (!error.empty()) RebuildStatus(error, true); return; }
    try
    {
        editor::WidgetAssetService::Export(*library[widgetLibrarySelected_],
            mfd::api1::WidgetAssetRoot(documentState_.loaded.window.sourceFile), *folder, documentState_.files);
        RebuildStatus("Widget package exported with its resources.", false);
    }
    catch (const std::exception& failure) { RebuildStatus(failure.what(), true); }
}

void EditorApplication::DrawWidgetPreviewGizmo(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    if (!mfd::api1::EvaluateWidgetLayout(page.containers, page.widgets, layouts, error)) return;
    for (const auto& layout : layouts)
    {
        if (layout.sourceIndex < page.containers.size()) continue;
        const auto& widget = page.widgets[layout.sourceIndex - page.containers.size()];
        if (widget.id != documentState_.selection.widgetId || !layout.effectiveVisible) continue;
        const auto a = viewport.ToScreen({layout.bounds.center.x - layout.bounds.size.x * 0.5f,
                                         layout.bounds.center.y + layout.bounds.size.y * 0.5f});
        const auto b = viewport.ToScreen({layout.bounds.center.x + layout.bounds.size.x * 0.5f,
                                         layout.bounds.center.y - layout.bounds.size.y * 0.5f});
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRect(a, b, IM_COL32(100, 220, 255, 255), 2, 0, 2);
        draw->AddRectFilled({b.x - 6, b.y - 6}, {b.x + 6, b.y + 6}, IM_COL32(100, 220, 255, 255));
    }
}
bool EditorApplication::BeginWidgetPreviewGesture(const ViewportState& viewport,
                                                  const ImVec2 mouse,
                                                  const bool selectedOnly)
{
    auto* page = ActivePage(); if (!page || page->widgets.empty()) return false;
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    if (!mfd::api1::EvaluateWidgetLayout(page->containers, page->widgets, layouts, error)) return false;
    const auto nodes = mfd::api1::ProjectWidgetLayoutNodes(page->containers, page->widgets);
    const auto pointer = viewport.ToLogical(mouse);
    const mfd::api1::detail::ContainerPointerQuery query(nodes, layouts, pointer);
    for (auto layout = layouts.rbegin(); layout != layouts.rend(); ++layout)
    {
        if (layout->sourceIndex < page->containers.size() ||
            (!selectedOnly && !query.VisibleAt(*layout, false)))
        {
            continue;
        }
        const auto& widget = page->widgets[layout->sourceIndex - page->containers.size()];
        if (selectedOnly && (documentState_.selection.kind != SelectionKind::Widget ||
                             widget.id != documentState_.selection.widgetId))
            continue;
        const auto corner = viewport.ToScreen({layout->bounds.center.x + layout->bounds.size.x * 0.5f,
                                               layout->bounds.center.y - layout->bounds.size.y * 0.5f});
        const bool resize = documentState_.selection.kind == SelectionKind::Widget && widget.id == documentState_.selection.widgetId &&
                            std::abs(mouse.x - corner.x) <= 10 && std::abs(mouse.y - corner.y) <= 10;
        const auto hit = mfd::api1::WidgetHitRegion(widget, layout->bounds);
        if (!resize && (std::abs(pointer.x - hit.center.x) > hit.size.x * 0.5f || std::abs(pointer.y - hit.center.y) > hit.size.y * 0.5f)) continue;
        SelectWidget(documentState_.selection.pageIndex, widget.id);
        widgetGestureBefore_ = CaptureCurrentSnapshot();
        widgetGestureStart_ = widget;
        widgetGesturePointer_ = pointer;
        widgetGestureResize_ = resize;
        widgetGestureChanged_ = false;
        return true;
    }
    return false;
}
bool EditorApplication::UpdateWidgetPreviewGesture(const ViewportState& viewport)
{
    if (!widgetGestureBefore_) return false;
    if (IsKeyPressed(KEY_ESCAPE) || !IsWindowFocused())
    {
        const auto snapshot = std::move(*widgetGestureBefore_);
        widgetGestureBefore_.reset();
        RestoreSnapshot(snapshot);
        return true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        if (widgetGestureChanged_) PushUndoSnapshot(std::move(*widgetGestureBefore_));
        widgetGestureBefore_.reset();
        return true;
    }
    auto* page = ActivePage(); auto* live = SelectedWidget();
    if (!page || !live || live->id != widgetGestureStart_.id ||
        documentState_.selection.pageIndex != widgetGestureBefore_->selection.pageIndex)
    {
        const auto snapshot = std::move(*widgetGestureBefore_);
        widgetGestureBefore_.reset();
        RestoreSnapshot(snapshot);
        return true;
    }
    const auto pointer = viewport.ToLogical(ImGui::GetMousePos());
    const mfd::Vec2 delta {pointer.x - widgetGesturePointer_.x, pointer.y - widgetGesturePointer_.y};
    std::string error;
    if (mfd::api1::WidgetEditorModel::ApplyGesture(*page, widgetGestureStart_, delta, widgetGestureResize_, error))
    {
        widgetGestureChanged_ = true;
        InvalidateFocusedLevel1StudioLayout();
    }
    else if (!error.empty()) RebuildStatus(error, true);
    return true;
}

bool EditorApplication::HandleWidgetPreviewTestInteraction(const mfd::PageDefinition& page,
                                                           const ViewportState& viewport,
                                                           const ImVec2 mousePosition,
                                                           const bool pointerBlocked)
{
    if (!widgetPreviewController_.Enabled())
    {
        return false;
    }

    if (!previewState_.widgetInteractionLayoutValid)
    {
        widgetPreviewController_.CancelPointerInteraction();
        return true;
    }

    editor::app::WidgetPreviewPointerSample sample;
    sample.pointer = viewport.ToLogical(mousePosition);
    sample.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    sample.began = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    sample.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    sample.cancel = IsKeyPressed(KEY_ESCAPE) || !IsWindowFocused();
    sample.blocked = pointerBlocked;
    widgetPreviewController_.Update(
        page.containers, previewState_.widgetInteractionLayouts, page.widgets, sample);
    return true;
}

void EditorApplication::ApplyWidgetBinding(const bool remove, const bool definitionOnly)
{
    const auto selected = SelectedWidgetDefinition(definitionOnly); if (!selected) return;
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>(*selected);
    const auto state = static_cast<mfd::api1::WidgetVisualState>(widgetDraft_.visual_state);
    auto visual = std::find_if(definition->visuals.begin(), definition->visuals.end(),
        [state](const auto& value) { return value.state == state; });
    if (visual == definition->visuals.end())
    { RebuildStatus("Author this state visual before adding its bindings.", true); return; }
    const mfd::api1::WidgetBinding binding {widgetDraft_.binding_primitive,
        static_cast<mfd::api1::WidgetBindingTarget>(widgetDraft_.binding_target), widgetDraft_.binding_from, widgetDraft_.binding_to};
    const auto previous = std::find_if(visual->bindings.begin(), visual->bindings.end(),
        [&binding](const auto& value) { return value.primitiveId == binding.primitiveId && value.target == binding.target; });
    if (remove)
    { if (previous == visual->bindings.end()) return; visual->bindings.erase(previous); }
    else if (previous != visual->bindings.end()) *previous = binding;
    else visual->bindings.push_back(binding);
    const auto before = CaptureCurrentSnapshot(); std::string error;
    if (!mfd::api1::WidgetEditorModel::ReplaceDefinition(documentState_.loaded.document, definition, error))
    { RebuildStatus(error, true); return; }
    PushUndoSnapshot(before);
}
