/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief API LEVEL 1 container command types. */

#include <optional>
#include <string>

#include "mfd/model/Types.h"
#include "mfd/runtime/GeneratedTransportMap.h"

namespace mfd::api1
{
/** @brief Stable page-local target used by container commands. */
struct ContainerHandle
{
    std::string page;
    std::string containerId;
    TransportId pageId = 0U;
    TransportId containerTransportId = 0U;
};

/** @brief Partial state update for one exposed container. */
struct ContainerStatePatch
{
    std::optional<bool> visible;
    std::optional<bool> enabled;
    std::optional<Vec2> scrollOffset;
};

/** @brief Applies visibility, interaction or scroll state to one container. */
struct UpdateContainerCommand
{
    ContainerHandle target;
    ContainerStatePatch patch;
};
} // namespace mfd::api1
