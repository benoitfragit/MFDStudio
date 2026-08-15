/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal runtime access to CommandProcessor transport maintenance.
 */

#include <chrono>

#include "mfd/control/CommandProcessor.h"

namespace mfd::detail
{
/**
 * @brief Keeps transport lifecycle operations outside the public command API.
 */
struct CommandProcessorInternalAccess
{
    /**
     * @brief Expires stale fragmented batches at a caller-provided monotonic time.
     * @param processor Command processor to maintain.
     * @param now Current monotonic timestamp.
     */
    static void MaintainTransportState(
        CommandProcessor& processor,
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept
    {
        processor.MaintainTransportState(now);
    }

    /**
     * @brief Clears fragment and sequence state after a successful transport session replacement.
     * @param processor Command processor whose transport state is reset.
     */
    static void ResetTransportState(CommandProcessor& processor) noexcept
    {
        processor.ResetTransportState();
    }
};
} // namespace mfd::detail
