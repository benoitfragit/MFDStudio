#include "mfd/apilevel1/widgets/WidgetRuntimeHost.h"
#include <array>
#include <cmath>
#include <utility>
#if defined(_WIN32)
#include <Windows.h>
#endif
namespace mfd::api1
{
class WidgetRuntimeHost::Impl
{
public:
#if defined(_WIN32)
    HMODULE module = nullptr;
#endif
    MfdWidgetRuntimeApiV1 api {};
};
WidgetRuntimeHost::WidgetRuntimeHost() : impl_(std::make_unique<Impl>()) {}
WidgetRuntimeHost::~WidgetRuntimeHost() { Unload(); }
WidgetRuntimeHost::WidgetRuntimeHost(WidgetRuntimeHost&&) noexcept = default;
WidgetRuntimeHost& WidgetRuntimeHost::operator=(WidgetRuntimeHost&& other) noexcept
{
    if (this != &other) { Unload(); impl_ = std::move(other.impl_); }
    return *this;
}
void WidgetRuntimeHost::Unload() noexcept
{
    if (!impl_) return;
    impl_->api = {};
#if defined(_WIN32)
    if (impl_->module) { FreeLibrary(impl_->module); impl_->module = nullptr; }
#endif
}
bool WidgetRuntimeHost::IsLoaded() const noexcept { return impl_ && impl_->api.evaluate_input != nullptr; }
bool WidgetRuntimeHost::Load(const std::filesystem::path& libraryFile, std::string& error)
{
    Unload();
    if (!impl_) impl_ = std::make_unique<Impl>();
#if defined(_WIN32)
    impl_->module = LoadLibraryW(libraryFile.c_str());
    if (impl_->module)
    {
        // Win32 symbol lookup requires one localized conversion to the exact versioned C signature.
        const auto query = reinterpret_cast<uint32_t(MFD_WIDGET_CALL*)(MfdWidgetRuntimeApiV1*)>(
            GetProcAddress(impl_->module, "mfd_get_widgets_runtime_api_v1"));
        impl_->api.struct_size = sizeof(impl_->api);
        if (query && query(&impl_->api) == 0U && impl_->api.struct_size >= sizeof(impl_->api) &&
            impl_->api.abi_version == MFD_WIDGET_ABI_VERSION &&
            (impl_->api.capabilities & MFD_WIDGET_CAPABILITY_INPUT) != 0U && impl_->api.evaluate_input)
        {
            error.clear();
            return true;
        }
    }
#endif
    error = "Missing or incompatible API LEVEL 1 Widgets runtime: " + libraryFile.string();
    Unload();
    return false;
}
bool WidgetRuntimeHost::LoadForProcess(const std::filesystem::path& windowFile, std::string& error)
{
#if defined(_WIN32)
    std::array<wchar_t, 32768> path {};
    const HMODULE loaded = GetModuleHandleW(L"mfd_widgets_api1.dll");
    const DWORD length = GetModuleFileNameW(loaded, path.data(), static_cast<DWORD>(path.size()));
    if (length != 0U && length < path.size())
    {
        const auto file = std::filesystem::path(std::wstring(path.data(), length));
        if (Load(loaded ? file : file.parent_path() / "mfd_widgets_api1.dll", error)) return true;
    }
#endif
    return Load(windowFile.parent_path() / "mfd_widgets_api1.dll", error);
}
bool WidgetRuntimeHost::Evaluate(const MfdWidgetInputV1& input, MfdWidgetOutputV1& output) const noexcept
{
    if (!IsLoaded()) return false;
    MfdWidgetOutputV1 candidate {};
    candidate.struct_size = sizeof(candidate);
    if (impl_->api.evaluate_input(&input, &candidate) != 0U || candidate.struct_size < sizeof(candidate) ||
        candidate.event_count > 6U || candidate.pressed > 1U || candidate.pending > 1U || candidate.proposed_checked > 1U ||
        !std::isfinite(candidate.proposed_value) || candidate.proposed_value < input.minimum || candidate.proposed_value > input.maximum) return false;
    for (std::uint32_t index = 0; index < candidate.event_count; ++index) if (candidate.events[index] > 9U) return false;
    output = candidate;
    return true;
}
}
