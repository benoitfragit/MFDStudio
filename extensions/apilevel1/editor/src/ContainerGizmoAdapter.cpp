/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerGizmoAdapter.h"

#include <algorithm>
#include <cmath>

namespace mfd::api1
{
ContainerRect ContainerGizmoAdapter::ResizeAroundCenter(const ContainerRect initial,
                                                        const Vec2 minimumSize,
                                                        const float factor,
                                                        const ContainerSizeMode sizeMode) const noexcept
{
    if (!std::isfinite(factor) || factor <= 0.0f ||
        !std::isfinite(initial.size.x) || !std::isfinite(initial.size.y) ||
        !std::isfinite(minimumSize.x) || !std::isfinite(minimumSize.y))
    {
        return initial;
    }
    ContainerRect result = initial;
    result.size = {
        std::max(std::max(0.0f, minimumSize.x), initial.size.x * factor),
        std::max(std::max(0.0f, minimumSize.y), initial.size.y * factor)};
    if (!std::isfinite(result.size.x) || !std::isfinite(result.size.y)) return initial;
    if (sizeMode == ContainerSizeMode::FitContent || sizeMode == ContainerSizeMode::FitHorizontal)
        result.size.x = initial.size.x;
    if (sizeMode == ContainerSizeMode::FitContent || sizeMode == ContainerSizeMode::FitVertical)
        result.size.y = initial.size.y;
    return result;
}
} // namespace mfd::api1
