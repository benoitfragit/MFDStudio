/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerRuntimeAbi.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "mfd/apilevel1/ContainerLayoutEngine.h"

namespace mfd::api1
{
namespace
{

void WriteError(const MfdContainerErrorBufferV1 buffer, const std::string& message) noexcept
{
    if (buffer.data == nullptr || buffer.capacity == 0U)
    {
        return;
    }
    const std::size_t copied = std::min(message.size(), buffer.capacity - 1U);
    std::memcpy(buffer.data, message.data(), copied);
    buffer.data[copied] = '\0';
}
mfd::Vec2 ToVec2(const MfdContainerVec2V1 value) noexcept
{
    return {value.x, value.y};
}

ContainerInsets ToInsets(const MfdContainerInsetsV1 value) noexcept
{
    return {value.left, value.top, value.right, value.bottom};
}

MfdContainerVec2V1 ToAbi(const mfd::Vec2 value) noexcept
{
    return {value.x, value.y};
}

MfdContainerRectV1 ToAbi(const ContainerRect& value) noexcept
{
    return {ToAbi(value.center), ToAbi(value.size)};
}

bool IsKnownContainerType(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(ContainerType::Overlay);
}

bool IsKnownContainerSizeMode(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(ContainerSizeMode::FitVertical);
}

bool IsKnownAlignment(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(ContainerAlignment::Stretch);
}

bool IsKnownResizeMode(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(ChildResizeMode::Stretch);
}

bool IsKnownScrollMode(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(ScrollMode::AlwaysVisible);
}

bool BuildDefinitions(const MfdContainerDefinitionV1* definitions,
                      const std::size_t definitionCount,
                      const MfdContainerChildV1* children,
                      const std::size_t childCount,
                      std::vector<ContainerDefinition>& result,
                      std::string& error)
{
    if ((definitionCount > 0U && definitions == nullptr) || (childCount > 0U && children == nullptr))
    {
        error = "Container ABI input pointers do not match their counts.";
        return false;
    }
    if (definitionCount > kMaximumContainerCount || childCount > kMaximumContainerCount)
    {
        error = "Container ABI input exceeds the supported container count.";
        return false;
    }

    result.clear();
    result.reserve(definitionCount);
    std::size_t remainingEdges = kMaximumContainerCount;
    for (std::size_t index = 0U; index < definitionCount; ++index)
    {
        const MfdContainerDefinitionV1& source = definitions[index];
        if (source.id == nullptr || !IsKnownContainerType(source.type) ||
            !IsKnownContainerSizeMode(source.size_mode) ||
            source.orientation > static_cast<std::uint32_t>(LinearOrientation::Vertical) ||
            !IsKnownAlignment(source.alignment) ||
            !IsKnownScrollMode(source.horizontal_scroll_mode) ||
            !IsKnownScrollMode(source.vertical_scroll_mode) ||
            source.first_child > childCount || source.child_count > childCount - source.first_child ||
            source.child_count > remainingEdges)
        {
            error = "Container ABI definition contains an invalid enum, string or child range.";
            return false;
        }

        ContainerDefinition destination;
        remainingEdges -= source.child_count;
        destination.id = source.id;
        destination.name = source.id;
        destination.type = static_cast<ContainerType>(source.type);
        destination.sizeMode = static_cast<ContainerSizeMode>(source.size_mode);
        destination.geometry = {ToVec2(source.position), ToVec2(source.size)};
        destination.minimumSize = ToVec2(source.minimum_size);
        destination.layoutReferenceSize = ToVec2(source.layout_reference_size);
        destination.visible = source.visible != 0U;
        destination.enabled = source.enabled != 0U;
        destination.padding = ToInsets(source.padding);
        destination.spacing = source.spacing;
        destination.orientation = static_cast<LinearOrientation>(source.orientation);
        destination.alignment = static_cast<ContainerAlignment>(source.alignment);
        destination.rows = source.rows;
        destination.columns = source.columns;
        destination.scrolling.horizontal = static_cast<ScrollMode>(source.horizontal_scroll_mode);
        destination.scrolling.vertical = static_cast<ScrollMode>(source.vertical_scroll_mode);
        destination.scrolling.offset = ToVec2(source.scroll_offset);
        destination.scrolling.appearance.thickness = source.scrollbar_thickness;
        destination.scrolling.appearance.minimumThumbSize = source.scrollbar_minimum_thumb_size;
        destination.scrolling.appearance.margins = ToInsets(source.scrollbar_margins);
        destination.children.reserve(source.child_count);
        for (std::size_t childOffset = 0U; childOffset < source.child_count; ++childOffset)
        {
            const MfdContainerChildV1& child = children[source.first_child + childOffset];
            if (child.child_id == nullptr || !IsKnownResizeMode(child.resize_mode) ||
                !IsKnownAlignment(child.horizontal_alignment) || !IsKnownAlignment(child.vertical_alignment))
            {
                error = "Container ABI child placement contains an invalid enum or string.";
                return false;
            }
            destination.children.push_back(ContainerChildPlacement {
                child.child_id,
                static_cast<ChildResizeMode>(child.resize_mode),
                static_cast<ContainerAlignment>(child.horizontal_alignment),
                static_cast<ContainerAlignment>(child.vertical_alignment),
                child.row,
                child.column,
                child.row_span,
                child.column_span,
                child.render_order});
        }
        result.push_back(std::move(destination));
    }
    return true;
}

std::uint32_t MFD_APILEVEL1_CALL EvaluateLayout(const MfdContainerDefinitionV1* definitions,
                                                const std::size_t definitionCount,
                                                const MfdContainerChildV1* children,
                                                const std::size_t childCount,
                                                MfdContainerLayoutV1* output,
                                                const std::size_t outputCapacity,
                                                std::size_t* const outputCount,
                                                const MfdContainerErrorBufferV1 errorBuffer) noexcept
{
    if (outputCount == nullptr)
    {
        WriteError(errorBuffer, "Container ABI requires a non-null output count.");
        return 1U;
    }
    *outputCount = 0U;
    try
    {
        std::vector<ContainerDefinition> converted;
        std::string error;
        if (!BuildDefinitions(definitions, definitionCount, children, childCount, converted, error))
        {
            WriteError(errorBuffer, error);
            return 2U;
        }

        std::vector<ContainerLayoutNode> layouts;
        if (!ContainerLayoutEngine {}.Evaluate(converted, layouts, error))
        {
            WriteError(errorBuffer, error);
            return 3U;
        }
        *outputCount = layouts.size();
        if (layouts.size() > outputCapacity || (layouts.size() > 0U && output == nullptr))
        {
            WriteError(errorBuffer, "Container ABI output buffer is too small.");
            return 4U;
        }

        std::unordered_map<std::string, std::size_t> sourceIndices;
        sourceIndices.reserve(converted.size());
        for (std::size_t index = 0U; index < converted.size(); ++index)
        {
            sourceIndices.emplace(converted[index].id, index);
        }
        for (std::size_t index = 0U; index < layouts.size(); ++index)
        {
            const ContainerLayoutNode& source = layouts[index];
            MfdContainerLayoutV1& destination = output[index];
            destination = {};
            destination.source_index = sourceIndices.at(source.id);
            destination.parent_source_index = source.parentId.empty()
                                                  ? std::numeric_limits<std::size_t>::max()
                                                  : sourceIndices.at(source.parentId);
            destination.bounds = ToAbi(source.bounds);
            destination.viewport = ToAbi(source.viewport);
            destination.content_size = ToAbi(source.contentSize);
            destination.scroll_offset = ToAbi(source.scrollOffset);
            destination.effective_visible = source.effectiveVisible ? 1U : 0U;
            destination.effective_enabled = source.effectiveEnabled ? 1U : 0U;
            destination.horizontal_scrollbar_visible = source.horizontalScrollbar.visible ? 1U : 0U;
            destination.vertical_scrollbar_visible = source.verticalScrollbar.visible ? 1U : 0U;
            destination.horizontal_track = ToAbi(source.horizontalScrollbar.track);
            destination.horizontal_thumb = ToAbi(source.horizontalScrollbar.thumb);
            destination.vertical_track = ToAbi(source.verticalScrollbar.track);
            destination.vertical_thumb = ToAbi(source.verticalScrollbar.thumb);
            destination.depth = static_cast<std::uint32_t>(source.depth);
            destination.render_order = source.renderOrder;
        }
        return 0U;
    }
    catch (const std::exception& exception)
    {
        WriteError(errorBuffer, exception.what());
        return 5U;
    }
    catch (...)
    {
        WriteError(errorBuffer, "Unknown container runtime extension failure.");
        return 6U;
    }
}
} // namespace
} // namespace mfd::api1

extern "C" MFD_APILEVEL1_EXPORT std::uint32_t MFD_APILEVEL1_CALL mfd_get_containers_runtime_api_v1(
    MfdContainersRuntimeApiV1* const api,
    const MfdContainerErrorBufferV1 error)
{
    if (api == nullptr || api->struct_size < sizeof(MfdContainersRuntimeApiV1))
    {
        mfd::api1::WriteError(error, "Incompatible mfd_containers_api1 API structure size.");
        return 1U;
    }
    api->abi_version = MFD_CONTAINERS_RUNTIME_ABI_VERSION;
    api->capabilities = MFD_CONTAINERS_CAPABILITY_LAYOUT |
                        MFD_CONTAINERS_CAPABILITY_SCROLLING |
                        MFD_CONTAINERS_CAPABILITY_STATE;
    api->evaluate_layout = &mfd::api1::EvaluateLayout;
    return 0U;
}
