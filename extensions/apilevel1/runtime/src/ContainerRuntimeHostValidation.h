#pragma once

/**
 * @file
 * @brief Defensive validation for layout records returned by the LEVEL 1 runtime DLL.
 */

#include "mfd/apilevel1/ContainerRuntimeAbi.h"
#include "mfd/apilevel1/ContainerTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace mfd::api1::detail
{
inline bool IsFiniteContainerAbiVector(const MfdContainerVec2V1 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

inline bool IsValidContainerAbiSize(const MfdContainerVec2V1 value) noexcept
{
    return IsFiniteContainerAbiVector(value) && value.x >= 0.0f && value.y >= 0.0f;
}

inline bool IsValidContainerAbiRectangle(const MfdContainerRectV1 rectangle) noexcept
{
    return IsFiniteContainerAbiVector(rectangle.center) && IsValidContainerAbiSize(rectangle.size);
}

inline bool IsValidContainerAbiScrollOffset(const MfdContainerVec2V1 contentSize,
                                            const MfdContainerVec2V1 viewportSize,
                                            const MfdContainerVec2V1 scrollOffset) noexcept
{
    if (!IsValidContainerAbiSize(contentSize) || !IsValidContainerAbiSize(viewportSize) ||
        !IsValidContainerAbiSize(scrollOffset))
    {
        return false;
    }
    const float maximumX = std::max(0.0f, contentSize.x - viewportSize.x);
    const float maximumY = std::max(0.0f, contentSize.y - viewportSize.y);
    return scrollOffset.x <= maximumX && scrollOffset.y <= maximumY;
}

/**
 * @brief Validates every untrusted layout field returned across the DLL boundary.
 * @param layouts Returned records, or null only when both counts are zero.
 * @param layoutCount Number of returned records.
 * @param definitionCount Number of input definitions supplied by the host.
 * @param error Receives a stable diagnostic on failure.
 * @return `true` when the records form one finite, bounded hierarchy.
 */
inline bool ValidateContainerRuntimeOutput(const MfdContainerLayoutV1* layouts,
                                           const std::size_t layoutCount,
                                           const std::size_t definitionCount,
                                           std::string& error)
{
    if (definitionCount > kMaximumContainerCount || layoutCount != definitionCount ||
        (layoutCount != 0U && layouts == nullptr))
    {
        error = "LEVEL 1 runtime extension returned an invalid layout count.";
        return false;
    }

    std::array<const MfdContainerLayoutV1*, kMaximumContainerCount> layoutsBySource {};
    for (std::size_t index = 0U; index < layoutCount; ++index)
    {
        const MfdContainerLayoutV1& layout = layouts[index];
        if (layout.source_index >= definitionCount || layoutsBySource[layout.source_index] != nullptr ||
            (layout.parent_source_index != std::numeric_limits<std::size_t>::max() &&
             layout.parent_source_index >= definitionCount))
        {
            error = "LEVEL 1 runtime extension returned an invalid or duplicate source index.";
            return false;
        }
        if (layout.effective_visible > 1U || layout.effective_enabled > 1U ||
            layout.horizontal_scrollbar_visible > 1U || layout.vertical_scrollbar_visible > 1U ||
            layout.depth >= kMaximumContainerDepth ||
            !IsValidContainerAbiRectangle(layout.bounds) ||
            !IsValidContainerAbiRectangle(layout.viewport) ||
            !IsValidContainerAbiSize(layout.content_size) ||
            !IsValidContainerAbiScrollOffset(
                layout.content_size, layout.viewport.size, layout.scroll_offset) ||
            !IsValidContainerAbiRectangle(layout.horizontal_track) ||
            !IsValidContainerAbiRectangle(layout.horizontal_thumb) ||
            !IsValidContainerAbiRectangle(layout.vertical_track) ||
            !IsValidContainerAbiRectangle(layout.vertical_thumb))
        {
            error = "LEVEL 1 runtime extension returned non-finite, negative or out-of-range layout fields.";
            return false;
        }
        layoutsBySource[layout.source_index] = &layout;
    }

    for (std::size_t sourceIndex = 0U; sourceIndex < definitionCount; ++sourceIndex)
    {
        const MfdContainerLayoutV1& layout = *layoutsBySource[sourceIndex];
        if (layout.parent_source_index == std::numeric_limits<std::size_t>::max())
        {
            if (layout.depth != 0U)
            {
                error = "LEVEL 1 runtime extension returned a root with invalid depth.";
                return false;
            }
            continue;
        }

        if (layout.parent_source_index == sourceIndex)
        {
            error = "LEVEL 1 runtime extension returned a self-parented layout node.";
            return false;
        }
        const MfdContainerLayoutV1& parent = *layoutsBySource[layout.parent_source_index];
        if (layout.depth != parent.depth + 1U ||
            (layout.effective_visible != 0U && parent.effective_visible == 0U) ||
            (layout.effective_enabled != 0U && parent.effective_enabled == 0U))
        {
            error = "LEVEL 1 runtime extension returned an inconsistent hierarchy.";
            return false;
        }
    }

    error.clear();
    return true;
}
} // namespace mfd::api1::detail
