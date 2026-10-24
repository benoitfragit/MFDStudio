/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal batching helpers shared by `UdpRuntimeBridge` and its focused tests.
 */

#include <cstddef>

#include "mfd/control/CommandTypes.h"

namespace mfd::detail
{
/**
 * @brief Coalesces adjacent mergeable state-like commands in place inside one batch.
 * @param batch Batch whose commands may be merged or replaced in place.
 * @return Number of commands removed by coalescing.
 */
std::size_t CoalesceCommandBatch(CommandBatch& batch);

/**
 * @brief Decides whether the UDP worker must leave its idle wait.
 * @param stopRequested Whether shutdown was requested.
 * @param feedbackWakeRequested Whether a producer queued feedback since the last send attempt.
 * @return `true` for shutdown or newly queued outbound feedback.
 */
[[nodiscard]] bool ShouldWakeUdpRuntimeBridgeWorker(
    bool stopRequested,
    bool feedbackWakeRequested) noexcept;
} // namespace mfd::detail
