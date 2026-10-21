/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerEditorHost.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <imgui.h>

#include "EditorUiTheme.h"
#include "mfd/apilevel1/ContainerEditorAbi.h"
#include "mfd/apilevel1/ContainerRuntimeHost.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace mfd::api1
{
namespace
{
std::filesystem::path ExecutableDirectory()
{
#if defined(_WIN32)
    std::array<wchar_t, 32768U> path {};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0U || length >= path.size()) return {};
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
#else
    return {};
#endif
}

void MFD_CONTAINER_EDITOR_CALL DrawText(void*, const char* const text)
{
    ImGui::TextDisabled("%s", text == nullptr ? "" : text);
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL BeginSection(void*,
                                                   const char* const sectionId,
                                                   const char* const label,
                                                   const std::uint8_t defaultOpen)
{
    if (sectionId == nullptr || label == nullptr)
    {
        return 0U;
    }

    return editor::ui::BeginInspectorSection(sectionId, label, defaultOpen != 0U, false) ? 1U : 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawCheckbox(void*, const char* const label, std::uint8_t* const value)
{
    if (value == nullptr) return 0U;
    bool checked = *value != 0U;
    editor::ui::BeginInspectorField(label);
    const bool changed = ImGui::Checkbox("##value", &checked);
    editor::ui::EndInspectorField();
    *value = checked ? 1U : 0U;
    return changed ? 1U : 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawFloat(void*, const char* const label, float* const value,
                                                const float speed, const float minimum, const float maximum)
{
    if (value == nullptr) return 0U;
    const float previous = *value;
    editor::ui::BeginInspectorField(label);
    const bool changed =
        ImGui::DragFloat("##value", value, speed, minimum, maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    editor::ui::EndInspectorField();
    if (!changed) return 0U;
    *value = std::isfinite(*value) ? std::clamp(*value, minimum, maximum) : previous;
    return *value != previous ? 1U : 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawFloat2(void*, const char* const label, float* const values,
                                                 const float speed, const float minimum, const float maximum)
{
    if (values == nullptr) return 0U;
    const std::array<float, 2U> previous {values[0], values[1]};
    editor::ui::BeginInspectorField(label);
    const bool changed =
        ImGui::DragFloat2("##value", values, speed, minimum, maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    editor::ui::EndInspectorField();
    if (!changed) return 0U;
    for (std::size_t axis = 0U; axis < previous.size(); ++axis)
    {
        values[axis] = std::isfinite(values[axis]) ? std::clamp(values[axis], minimum, maximum) : previous[axis];
    }
    return values[0] != previous[0] || values[1] != previous[1] ? 1U : 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawCombo(void*, const char* const label, std::int32_t* const current,
                                                const char* const* const labels, const std::size_t labelCount)
{
    if (current == nullptr || labels == nullptr || labelCount == 0U) return 0U;
    bool changed = false;
    const std::size_t selected = static_cast<std::size_t>(std::clamp<std::int32_t>(
        *current, 0, static_cast<std::int32_t>(labelCount - 1U)));
    editor::ui::BeginInspectorField(label);
    if (ImGui::BeginCombo("##value", labels[selected]))
    {
        for (std::size_t index = 0U; index < labelCount; ++index)
        {
            if (ImGui::Selectable(labels[index], index == selected))
            {
                *current = static_cast<std::int32_t>(index);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    editor::ui::EndInspectorField();
    return changed ? 1U : 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL DrawColor(void*, const char* const label, float* const rgba)
{
    if (rgba == nullptr) return 0U;
    editor::ui::BeginInspectorField(label);
    const bool changed = ImGui::ColorEdit4("##value", rgba);
    editor::ui::EndInspectorField();
    return changed ? 1U : 0U;
}

void ToFloatColor(const ColorRgba color, float* const destination) noexcept
{
    destination[0] = static_cast<float>(color.r) / 255.0f;
    destination[1] = static_cast<float>(color.g) / 255.0f;
    destination[2] = static_cast<float>(color.b) / 255.0f;
    destination[3] = static_cast<float>(color.a) / 255.0f;
}

ColorRgba FromFloatColor(const float* const source) noexcept
{
    return {static_cast<std::uint8_t>(std::lround(std::clamp(source[0], 0.0f, 1.0f) * 255.0f)),
            static_cast<std::uint8_t>(std::lround(std::clamp(source[1], 0.0f, 1.0f) * 255.0f)),
            static_cast<std::uint8_t>(std::lround(std::clamp(source[2], 0.0f, 1.0f) * 255.0f)),
            static_cast<std::uint8_t>(std::lround(std::clamp(source[3], 0.0f, 1.0f) * 255.0f))};
}
} // namespace

class ContainerEditorHost::Impl
{
public:
#if defined(_WIN32)
    HMODULE module = nullptr;
#endif
    MfdContainerEditorApiV1 api {};
    ContainerRuntimeHost runtime {};
    std::string diagnostic {};
    bool attempted = false;
};

ContainerEditorHost::ContainerEditorHost() : impl_(std::make_unique<Impl>()) {}
ContainerEditorHost::~ContainerEditorHost() { Unload(); }

bool ContainerEditorHost::EnsureLoaded(const std::filesystem::path& windowFile)
{
    if (IsAvailable()) return true;
    if (impl_->attempted) return false;
    impl_->attempted = true;
    if (!impl_->runtime.LoadForProcess(windowFile, impl_->diagnostic)) return false;
#if defined(_WIN32)
    const std::filesystem::path library = ExecutableDirectory() / "mfd_editor_containers_api1.dll";
    impl_->module = LoadLibraryW(library.c_str());
    if (impl_->module == nullptr)
    {
        impl_->diagnostic = "API LEVEL 1 authoring requires mfd_editor_containers_api1.dll beside the editor.";
        impl_->runtime.Unload();
        return false;
    }
    // Win32 exposes DLL symbols as untyped addresses, so the validated ABI entry
    // point requires this localized conversion to its exact C function type.
    const auto getApi = reinterpret_cast<std::uint32_t(MFD_CONTAINER_EDITOR_CALL*)(MfdContainerEditorApiV1*)>(
        GetProcAddress(impl_->module, "mfd_get_container_editor_api_v1"));
    impl_->api = {};
    impl_->api.struct_size = sizeof(MfdContainerEditorApiV1);
    if (getApi == nullptr || getApi(&impl_->api) != 0U ||
        impl_->api.abi_version != MFD_CONTAINER_EDITOR_ABI_VERSION ||
        impl_->api.required_ui_facade_version != MFD_CONTAINER_EDITOR_UI_FACADE_VERSION ||
        impl_->api.draw_inspector == nullptr)
    {
        impl_->diagnostic = "mfd_editor_containers_api1.dll has an incompatible editor UI ABI.";
        Unload();
        impl_->attempted = true;
        return false;
    }
    impl_->diagnostic.clear();
    return true;
#else
    impl_->diagnostic = "LEVEL 1 editor extension loading is unavailable on this platform.";
    impl_->runtime.Unload();
    return false;
#endif
}

void ContainerEditorHost::Unload() noexcept
{
    if (impl_ == nullptr) return;
    impl_->api = {};
#if defined(_WIN32)
    if (impl_->module != nullptr)
    {
        FreeLibrary(impl_->module);
        impl_->module = nullptr;
    }
#endif
    impl_->runtime.Unload();
    impl_->attempted = false;
}

bool ContainerEditorHost::IsAvailable() const noexcept
{
    return impl_ != nullptr && impl_->api.draw_inspector != nullptr && impl_->runtime.IsAvailable();
}

const std::string& ContainerEditorHost::Diagnostic() const noexcept { return impl_->diagnostic; }

bool ContainerEditorHost::DrawInspector(ContainerDefinition& container,
                                        ContainerChildPlacement* const placement,
                                        const ContainerDefinition* const parent) const
{
    if (!IsAvailable()) return false;
    std::uint32_t type = static_cast<std::uint32_t>(container.type);
    std::uint32_t sizeMode = static_cast<std::uint32_t>(container.sizeMode);
    std::uint32_t orientation = static_cast<std::uint32_t>(container.orientation);
    std::uint32_t alignment = static_cast<std::uint32_t>(container.alignment);
    std::uint32_t horizontalScroll = static_cast<std::uint32_t>(container.scrolling.horizontal);
    std::uint32_t verticalScroll = static_cast<std::uint32_t>(container.scrolling.vertical);
    std::uint32_t childResizeMode = placement == nullptr ? 0U : static_cast<std::uint32_t>(placement->resizeMode);
    std::uint32_t childHorizontalAlignment =
        placement == nullptr ? 0U : static_cast<std::uint32_t>(placement->horizontalAlignment);
    std::uint32_t childVerticalAlignment =
        placement == nullptr ? 0U : static_cast<std::uint32_t>(placement->verticalAlignment);
    std::uint32_t childRow = placement == nullptr ? 0U : placement->row;
    std::uint32_t childColumn = placement == nullptr ? 0U : placement->column;
    std::uint32_t childRowSpan = placement == nullptr ? 1U : placement->rowSpan;
    std::uint32_t childColumnSpan = placement == nullptr ? 1U : placement->columnSpan;
    std::int32_t childRenderOrder = placement == nullptr ? 0 : placement->renderOrder;
    std::uint8_t visible = container.visible ? 1U : 0U;
    std::uint8_t enabled = container.enabled ? 1U : 0U;
    std::uint8_t exposed = container.exposed ? 1U : 0U;
    std::uint8_t clipContent = container.scrolling.clipContent ? 1U : 0U;
    std::uint8_t backgroundVisible = container.appearance.backgroundVisible ? 1U : 0U;
    std::uint8_t borderVisible = container.appearance.borderVisible ? 1U : 0U;
    float position[] {container.geometry.center.x, container.geometry.center.y};
    float size[] {container.geometry.size.x, container.geometry.size.y};
    float minimum[] {container.minimumSize.x, container.minimumSize.y};
    float padding[] {container.padding.left, container.padding.top, container.padding.right, container.padding.bottom};
    float offset[] {container.scrolling.offset.x, container.scrolling.offset.y};
    float margins[] {container.scrolling.appearance.margins.left, container.scrolling.appearance.margins.top,
                     container.scrolling.appearance.margins.right, container.scrolling.appearance.margins.bottom};
    float background[4] {}, border[4] {}, track[4] {}, thumb[4] {}, hover[4] {}, active[4] {};
    ToFloatColor(container.appearance.backgroundColor, background);
    ToFloatColor(container.appearance.borderColor, border);
    ToFloatColor(container.scrolling.appearance.trackColor, track);
    ToFloatColor(container.scrolling.appearance.thumbColor, thumb);
    ToFloatColor(container.scrolling.appearance.thumbHoverColor, hover);
    ToFloatColor(container.scrolling.appearance.thumbActiveColor, active);

    MfdContainerEditorUiFacadeV1 ui {sizeof(MfdContainerEditorUiFacadeV1),
        MFD_CONTAINER_EDITOR_UI_FACADE_VERSION, nullptr, &DrawText, &DrawCheckbox,
        &DrawFloat, &DrawFloat2, &DrawCombo, &DrawColor, &BeginSection};
    const char* const parentId = parent == nullptr ? "" : parent->id.c_str();
    const std::uint32_t parentType =
        parent == nullptr ? 0U : static_cast<std::uint32_t>(parent->type);
    MfdContainerEditorSelectionV1 selection {sizeof(MfdContainerEditorSelectionV1), container.id.c_str(),
        &type, &sizeMode, position, size, minimum, &visible, &enabled, &exposed, padding, &container.spacing,
        &orientation, &alignment, &container.rows, &container.columns, &horizontalScroll, &verticalScroll,
        offset, &container.scrolling.appearance.thickness, &container.scrolling.appearance.minimumThumbSize,
        &clipContent, &backgroundVisible, &borderVisible, background, border, track, thumb, hover, active,
        &container.appearance.borderThickness, &container.appearance.cornerRadius,
        &container.scrolling.appearance.cornerRadius, margins,
        parentId, placement == nullptr ? 0U : 1U, parentType,
        &childResizeMode, &childHorizontalAlignment, &childVerticalAlignment,
        &childRow, &childColumn, &childRowSpan, &childColumnSpan, &childRenderOrder};
    if (impl_->api.draw_inspector(&ui, &selection) == 0U) return false;

    container.type = static_cast<ContainerType>(type);
    container.sizeMode = static_cast<ContainerSizeMode>(sizeMode);
    container.geometry.center = {position[0], position[1]};
    container.geometry.size = {std::max(size[0], minimum[0]), std::max(size[1], minimum[1])};
    container.minimumSize = {minimum[0], minimum[1]};
    container.visible = visible != 0U;
    container.enabled = enabled != 0U;
    container.exposed = exposed != 0U;
    container.padding = {padding[0], padding[1], padding[2], padding[3]};
    container.orientation = static_cast<LinearOrientation>(orientation);
    container.alignment = static_cast<ContainerAlignment>(alignment);
    container.scrolling.horizontal = static_cast<ScrollMode>(horizontalScroll);
    container.scrolling.vertical = static_cast<ScrollMode>(verticalScroll);
    container.scrolling.offset = {offset[0], offset[1]};
    container.scrolling.clipContent = clipContent != 0U;
    container.appearance.backgroundVisible = backgroundVisible != 0U;
    container.appearance.borderVisible = borderVisible != 0U;
    container.appearance.backgroundColor = FromFloatColor(background);
    container.appearance.borderColor = FromFloatColor(border);
    container.scrolling.appearance.trackColor = FromFloatColor(track);
    container.scrolling.appearance.thumbColor = FromFloatColor(thumb);
    container.scrolling.appearance.thumbHoverColor = FromFloatColor(hover);
    container.scrolling.appearance.thumbActiveColor = FromFloatColor(active);
    container.scrolling.appearance.margins = {margins[0], margins[1], margins[2], margins[3]};
    if (placement != nullptr)
    {
        placement->resizeMode = static_cast<ChildResizeMode>(childResizeMode);
        placement->horizontalAlignment = static_cast<ContainerAlignment>(childHorizontalAlignment);
        placement->verticalAlignment = static_cast<ContainerAlignment>(childVerticalAlignment);
        placement->row = childRow;
        placement->column = childColumn;
        placement->rowSpan = childRowSpan;
        placement->columnSpan = childColumnSpan;
        placement->renderOrder = childRenderOrder;
    }
    return true;
}
} // namespace mfd::api1
