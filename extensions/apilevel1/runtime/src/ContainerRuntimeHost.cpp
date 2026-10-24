/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerRuntimeHost.h"
#include "ContainerRuntimeHostValidation.h"

#include <array>
#include <limits>
#include <sstream>
#include <utility>

#include "mfd/apilevel1/ContainerRuntimeAbi.h"

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
MfdContainerVec2V1 ToAbi(const Vec2 value) noexcept { return {value.x, value.y}; }
MfdContainerInsetsV1 ToAbi(const ContainerInsets& value) noexcept
{
    return {value.left, value.top, value.right, value.bottom};
}
Vec2 FromAbi(const MfdContainerVec2V1 value) noexcept { return {value.x, value.y}; }
ContainerRect FromAbi(const MfdContainerRectV1& value) noexcept
{
    return {FromAbi(value.center), FromAbi(value.size)};
}

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

/** @brief Locates the runtime already retained by this process's session. */
std::filesystem::path LoadedRuntimeFile()
{
#if defined(_WIN32)
    const HMODULE module = GetModuleHandleW(L"mfd_containers_api1.dll");
    if (module == nullptr) return {};
    std::array<wchar_t, 32768U> path {};
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0U || length >= path.size()) return {};
    return std::filesystem::path(std::wstring(path.data(), length));
#else
    return {};
#endif
}
} // namespace

class ContainerRuntimeHost::Impl
{
public:
#if defined(_WIN32)
    HMODULE module = nullptr;
#endif
    MfdContainersRuntimeApiV1 api {};
};

ContainerRuntimeHost::ContainerRuntimeHost() : impl_(std::make_unique<Impl>()) {}
ContainerRuntimeHost::~ContainerRuntimeHost() { Unload(); }
ContainerRuntimeHost::ContainerRuntimeHost(ContainerRuntimeHost&&) noexcept = default;
ContainerRuntimeHost& ContainerRuntimeHost::operator=(ContainerRuntimeHost&& other) noexcept
{
    if (this != &other)
    {
        Unload();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool ContainerRuntimeHost::Load(const std::filesystem::path& libraryFile, std::string& error)
{
    Unload();
    if (impl_ == nullptr) impl_ = std::make_unique<Impl>();
#if defined(_WIN32)
    impl_->module = LoadLibraryW(libraryFile.c_str());
    if (impl_->module == nullptr)
    {
        error = "Unable to load LEVEL 1 runtime extension '" + libraryFile.string() + "'.";
        return false;
    }
    // Win32 exposes DLL symbols as untyped addresses, so the validated ABI entry
    // point requires this localized conversion to its exact C function type.
    const auto getApi = reinterpret_cast<std::uint32_t(MFD_APILEVEL1_CALL*)(
        MfdContainersRuntimeApiV1*, MfdContainerErrorBufferV1)>(
            GetProcAddress(impl_->module, "mfd_get_containers_runtime_api_v1"));
    if (getApi == nullptr)
    {
        error = "LEVEL 1 runtime extension is missing its ABI entry point.";
        Unload();
        return false;
    }
    impl_->api = {};
    impl_->api.struct_size = sizeof(MfdContainersRuntimeApiV1);
    std::array<char, 512U> diagnostic {};
    if (getApi(&impl_->api, {diagnostic.data(), diagnostic.size()}) != 0U ||
        impl_->api.abi_version != MFD_CONTAINERS_RUNTIME_ABI_VERSION ||
        impl_->api.evaluate_layout == nullptr ||
        (impl_->api.capabilities & (MFD_CONTAINERS_CAPABILITY_LAYOUT |
                                    MFD_CONTAINERS_CAPABILITY_SCROLLING |
                                    MFD_CONTAINERS_CAPABILITY_STATE)) !=
            (MFD_CONTAINERS_CAPABILITY_LAYOUT |
             MFD_CONTAINERS_CAPABILITY_SCROLLING |
             MFD_CONTAINERS_CAPABILITY_STATE))
    {
        error = diagnostic[0] == '\0'
                    ? "LEVEL 1 runtime extension ABI or capabilities are incompatible."
                    : std::string(diagnostic.data());
        Unload();
        return false;
    }
    error.clear();
    return true;
#else
    error = "LEVEL 1 runtime extension loading is unavailable on this platform.";
    return false;
#endif
}

bool ContainerRuntimeHost::LoadForProcess(const std::filesystem::path& windowFile, std::string& error)
{
    // A renderer has no asset path; reuse the session's validated DLL even when
    // it was loaded beside the Window rather than beside the executable.
    if (windowFile.empty())
    {
        const std::filesystem::path loadedFile = LoadedRuntimeFile();
        if (!loadedFile.empty() && Load(loadedFile, error)) return true;
    }
    const std::filesystem::path executableCandidate = ExecutableDirectory() / "mfd_containers_api1.dll";
    if (Load(executableCandidate, error)) return true;
    const std::filesystem::path assetCandidate = windowFile.parent_path() / "mfd_containers_api1.dll";
    if (assetCandidate != executableCandidate && Load(assetCandidate, error)) return true;
    std::ostringstream message;
    message << "API LEVEL 1 requires a compatible mfd_containers_api1.dll. Searched '"
            << executableCandidate.string() << "' and '" << assetCandidate.string() << "'.";
    error = message.str();
    return false;
}

void ContainerRuntimeHost::Unload() noexcept
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
}

bool ContainerRuntimeHost::IsAvailable() const noexcept
{
    return impl_ != nullptr && impl_->api.evaluate_layout != nullptr;
}

bool ContainerRuntimeHost::Evaluate(const std::vector<ContainerDefinition>& definitions,
                                    std::vector<ContainerLayoutNode>& output,
                                    std::string& error) const
{
    if (!IsAvailable())
    {
        error = "LEVEL 1 container runtime extension is not loaded.";
        return false;
    }
    if (definitions.size() > kMaximumContainerCount)
    {
        error = "LEVEL 1 layout exceeds the supported container count.";
        output.clear();
        return false;
    }

    std::vector<MfdContainerChildV1> children;
    std::size_t childCount = 0U;
    for (const ContainerDefinition& definition : definitions)
    {
        if (definition.children.size() > kMaximumContainerCount - childCount)
        {
            error = "LEVEL 1 layout exceeds the supported ownership-edge count.";
            output.clear();
            return false;
        }
        childCount += definition.children.size();
    }
    children.reserve(childCount);
    std::vector<MfdContainerDefinitionV1> abiDefinitions;
    abiDefinitions.reserve(definitions.size());
    for (const ContainerDefinition& definition : definitions)
    {
        const std::size_t firstChild = children.size();
        for (const ContainerChildPlacement& child : definition.children)
        {
            children.push_back({child.childId.c_str(),
                                static_cast<std::uint32_t>(child.resizeMode),
                                static_cast<std::uint32_t>(child.horizontalAlignment),
                                static_cast<std::uint32_t>(child.verticalAlignment),
                                child.row, child.column, child.rowSpan, child.columnSpan, child.renderOrder});
        }
        abiDefinitions.push_back({definition.id.c_str(),
                                  static_cast<std::uint32_t>(definition.type),
                                  static_cast<std::uint32_t>(definition.sizeMode),
                                  ToAbi(definition.geometry.center), ToAbi(definition.geometry.size),
                                  ToAbi(definition.minimumSize), ToAbi(definition.layoutReferenceSize),
                                  static_cast<std::uint8_t>(definition.visible),
                                  static_cast<std::uint8_t>(definition.enabled),
                                  ToAbi(definition.padding), definition.spacing,
                                  static_cast<std::uint32_t>(definition.orientation),
                                  static_cast<std::uint32_t>(definition.alignment),
                                  definition.rows, definition.columns,
                                  static_cast<std::uint32_t>(definition.scrolling.horizontal),
                                  static_cast<std::uint32_t>(definition.scrolling.vertical),
                                  ToAbi(definition.scrolling.offset),
                                  definition.scrolling.appearance.thickness,
                                  definition.scrolling.appearance.minimumThumbSize,
                                  ToAbi(definition.scrolling.appearance.margins),
                                  firstChild, definition.children.size()});
    }

    std::vector<MfdContainerLayoutV1> layouts(definitions.size());
    std::size_t outputCount = 0U;
    std::array<char, 512U> diagnostic {};
    const std::uint32_t status = impl_->api.evaluate_layout(
        abiDefinitions.data(), abiDefinitions.size(), children.data(), children.size(),
        layouts.data(), layouts.size(), &outputCount, {diagnostic.data(), diagnostic.size()});
    if (status != 0U || outputCount > layouts.size())
    {
        error = diagnostic[0] == '\0' ? "LEVEL 1 layout evaluation failed." : std::string(diagnostic.data());
        output.clear();
        return false;
    }
    if (!detail::ValidateContainerRuntimeOutput(layouts.data(), outputCount, definitions.size(), error))
    {
        output.clear();
        return false;
    }

    output.clear();
    output.reserve(outputCount);
    for (std::size_t index = 0U; index < outputCount; ++index)
    {
        const MfdContainerLayoutV1& source = layouts[index];
        if (source.source_index >= definitions.size() ||
            (source.parent_source_index != std::numeric_limits<std::size_t>::max() &&
             source.parent_source_index >= definitions.size()))
        {
            error = "LEVEL 1 runtime extension returned an invalid source index.";
            output.clear();
            return false;
        }
        ContainerLayoutNode node;
        node.sourceIndex = source.source_index;
        node.parentSourceIndex = source.parent_source_index;
        node.id = definitions[source.source_index].id;
        if (source.parent_source_index != std::numeric_limits<std::size_t>::max())
            node.parentId = definitions[source.parent_source_index].id;
        node.bounds = FromAbi(source.bounds);
        node.viewport = FromAbi(source.viewport);
        node.contentSize = FromAbi(source.content_size);
        node.scrollOffset = FromAbi(source.scroll_offset);
        node.effectiveVisible = source.effective_visible != 0U;
        node.effectiveEnabled = source.effective_enabled != 0U;
        node.horizontalScrollbar = {source.horizontal_scrollbar_visible != 0U,
                                    FromAbi(source.horizontal_track), FromAbi(source.horizontal_thumb)};
        node.verticalScrollbar = {source.vertical_scrollbar_visible != 0U,
                                  FromAbi(source.vertical_track), FromAbi(source.vertical_thumb)};
        node.depth = source.depth;
        node.renderOrder = source.render_order;
        output.push_back(std::move(node));
    }
    error.clear();
    return true;
}
} // namespace mfd::api1
