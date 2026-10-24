#include "mfd/apilevel1/widgets/WidgetEditorHost.h"
#include "EditorUiTheme.h"
#include "WidgetEditorHostInternal.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace mfd::api1
{
namespace internal
{
namespace
{
bool HasNullTerminator(const char* value, const std::size_t capacity) noexcept
{
    return std::memchr(value, '\0', capacity) != nullptr;
}
} // namespace

bool IsWidgetEditorDraftValidAfterDraw(const MfdWidgetEditorDraftV1& draft) noexcept
{
    return draft.struct_size == sizeof(MfdWidgetEditorDraftV1) &&
           HasNullTerminator(draft.id, sizeof(draft.id)) &&
           HasNullTerminator(draft.parent, sizeof(draft.parent)) &&
           HasNullTerminator(draft.text, sizeof(draft.text)) &&
           HasNullTerminator(draft.definition_id, sizeof(draft.definition_id)) &&
           HasNullTerminator(draft.default_text, sizeof(draft.default_text)) &&
           HasNullTerminator(draft.binding_primitive, sizeof(draft.binding_primitive));
}
} // namespace internal

namespace
{
void MFD_WIDGET_EDITOR_CALL Text(void*, const char* label) { ImGui::TextWrapped("%s", label); }
std::uint8_t MFD_WIDGET_EDITOR_CALL Input(void*, const char* label, char* value, std::size_t capacity)
{
    if (!value || capacity == 0 || capacity > 4097) return 0;
    editor::ui::BeginInspectorField(label);
    const bool changed = ImGui::InputText("##value", value, capacity, ImGuiInputTextFlags_EnterReturnsTrue);
    editor::ui::EndInspectorField();
    return changed;
}
std::uint8_t MFD_WIDGET_EDITOR_CALL Checkbox(void*, const char* label, std::uint8_t* value)
{
    bool checked = *value != 0;
    editor::ui::BeginInspectorField(label);
    const bool changed = ImGui::Checkbox("##value", &checked);
    editor::ui::EndInspectorField();
    *value = checked;
    return changed;
}
std::uint8_t MFD_WIDGET_EDITOR_CALL Number(void*, const char* label, double* value, double minimum, double maximum)
{
    editor::ui::BeginInspectorField(label);
    const bool changed = ImGui::InputDouble("##value", value, 0, 0, "%.6g");
    editor::ui::EndInspectorField();
    if (changed && std::isfinite(*value) && minimum <= maximum) *value = std::clamp(*value, minimum, maximum);
    return changed;
}
std::uint8_t MFD_WIDGET_EDITOR_CALL Combo(void*, const char* label, std::int32_t* value, const char* const* labels, std::size_t count)
{
    if (!count || count > 64) return 0;
    const auto previous = *value;
    editor::ui::BeginInspectorField(label);
    if (ImGui::BeginCombo("##value", labels[std::clamp<std::int32_t>(*value, 0, static_cast<std::int32_t>(count - 1))]))
    {
        for (std::size_t index = 0; index < count; ++index)
            if (ImGui::Selectable(labels[index], *value == static_cast<std::int32_t>(index))) *value = static_cast<std::int32_t>(index);
        ImGui::EndCombo();
    }
    editor::ui::EndInspectorField();
    return previous != *value;
}
std::uint8_t MFD_WIDGET_EDITOR_CALL Button(void*, const char* label) { return ImGui::Button(label); }
}
class WidgetEditorHost::Impl
{
public:
#if defined(_WIN32)
    HMODULE module = nullptr;
#endif
    MfdWidgetEditorApiV1 api {};
};
WidgetEditorHost::WidgetEditorHost() : impl_(std::make_unique<Impl>()) {}
WidgetEditorHost::~WidgetEditorHost() { Unload(); }
void WidgetEditorHost::Unload() noexcept
{
    impl_->api = {};
#if defined(_WIN32)
    if (impl_->module) { FreeLibrary(impl_->module); impl_->module = nullptr; }
#endif
}
bool WidgetEditorHost::IsLoaded() const noexcept { return impl_->api.draw != nullptr; }
const std::string& WidgetEditorHost::Diagnostic() const noexcept { return diagnostic_; }
bool WidgetEditorHost::Load(const std::filesystem::path& file)
{
    Unload();
#if defined(_WIN32)
    impl_->module = LoadLibraryW(file.c_str());
    if (impl_->module)
    {
        // Win32 resolves an untyped symbol; convert only to this exact C ABI query signature.
        const auto query = reinterpret_cast<uint32_t(MFD_WIDGET_EDITOR_CALL*)(MfdWidgetEditorApiV1*)>(
            GetProcAddress(impl_->module, "mfd_get_widgets_editor_api_v1"));
        impl_->api.struct_size = sizeof(impl_->api);
        if (query && query(&impl_->api) == 0U && impl_->api.struct_size >= sizeof(impl_->api) &&
            impl_->api.abi_version == 1U && (impl_->api.capabilities & 1U) != 0 && impl_->api.draw)
        { diagnostic_.clear(); return true; }
    }
#endif
    diagnostic_ = "Missing/incompatible API LEVEL 1 Widgets editor: " + file.string();
    Unload();
    return false;
}
bool WidgetEditorHost::EnsureLoaded(const std::filesystem::path& windowFile)
{
    if (IsLoaded()) return true;
#if defined(_WIN32)
    std::array<wchar_t, 32768> path {};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length && length < path.size() && Load(std::filesystem::path(std::wstring(path.data(), length)).parent_path() /
                                             "mfd_editor_widgets_api1.dll")) return true;
#endif
    return Load(windowFile.parent_path() / "mfd_editor_widgets_api1.dll");
}
std::uint32_t WidgetEditorHost::Draw(MfdWidgetEditorDraftV1& draft) const
{
    if (!IsLoaded()) return 0;
    const MfdWidgetEditorUiV1 ui {sizeof(ui), 1U, nullptr, Text, Input, Checkbox, Number, Combo, Button};
    const std::uint32_t actions = impl_->api.draw(&ui, &draft);
    return internal::IsWidgetEditorDraftValidAfterDraw(draft) ? actions & 127U : 0U;
}
}
