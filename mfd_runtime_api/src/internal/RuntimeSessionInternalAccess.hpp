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

#include <cstddef>
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
    /**
     * @brief Returns the batches applied during the last `Advance` call.
     * @note The list stays empty while command telemetry is disabled; use
     * `SetCommandTelemetryEnabled` to opt into the per-frame batch retention.
     */
    [[nodiscard]] static const std::vector<CommandBatch>& AppliedCommandBatches(const RuntimeSession& session) noexcept;
    /**
     * @brief Enables or disables per-frame command telemetry.
     *
     * Telemetry retains deep copies of every applied command batch and enriches the
     * command status with queue-depth details. It is disabled by default so the
     * `Advance` hot path stays allocation-free when no debug tooling consumes it.
     */
    static void SetCommandTelemetryEnabled(RuntimeSession& session, bool enabled) noexcept;
    /** @brief Returns the number of batches applied during the last `Advance` call. */
    [[nodiscard]] static std::size_t LastAppliedBatchCount(const RuntimeSession& session) noexcept;
    /** @brief Returns the number of commands applied during the last `Advance` call. */
    [[nodiscard]] static std::size_t LastAppliedCommandCount(const RuntimeSession& session) noexcept;
};
} // namespace mfd::runtime_api::internal
