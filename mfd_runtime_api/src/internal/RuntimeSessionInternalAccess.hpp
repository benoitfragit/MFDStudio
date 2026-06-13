/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal-only bridge exposing runtime implementation details to repository hosts.
 */

#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/runtime_api/RuntimeSession.h"

namespace mfd::runtime_api::internal
{
/**
 * @brief Internal accessors reserved to repository host applications such as `mfd_window`.
 */
struct MFD_RUNTIME_API RuntimeSessionInternalAccess
{
    [[nodiscard]] static SceneRegistry& Scene(RuntimeSession& session) noexcept;
    [[nodiscard]] static const SceneRegistry& Scene(const RuntimeSession& session) noexcept;
    [[nodiscard]] static const WindowAssetDefinition& WindowDefinition(const RuntimeSession& session) noexcept;
    [[nodiscard]] static UdpRuntimeBridge* RuntimeBridge(RuntimeSession& session) noexcept;
    [[nodiscard]] static const UdpRuntimeBridge* RuntimeBridge(const RuntimeSession& session) noexcept;
    [[nodiscard]] static const std::vector<CommandBatch>& AppliedCommandBatches(const RuntimeSession& session) noexcept;
};
} // namespace mfd::runtime_api::internal
