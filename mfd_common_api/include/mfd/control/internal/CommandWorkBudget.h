/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared command work estimation and atomic batch safety limit.
 */

#include <algorithm>
#include <cstddef>
#include <limits>
#include <variant>

#include "mfd/control/CommandTypes.h"
#include "mfd/core/ArrayView.h"

namespace mfd::detail
{
/** @brief Hard upper bound for work applied by one atomic command batch. */
constexpr std::size_t kMaxAtomicCommandWorkUnits = 512U;

/** @brief Stable diagnostic returned when one logical batch exceeds the atomic work limit. */
constexpr char kAtomicCommandWorkLimitError[] =
    "Command batch exceeds the atomic work limit of 512 units";

/** @brief Estimates one command using the runtime mutation unit contract. */
struct CommandWorkUnitEstimator
{
    std::size_t operator()(const ActivatePageCommand&) const noexcept { return 1U; }
    std::size_t operator()(const SetPageViewCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateWindowDisplayCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateReticleCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateStrobeCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpsertDynamicReticleCommand&) const noexcept { return 1U; }

    std::size_t operator()(const UpsertDynamicReticlesCommand& command) const noexcept
    {
        return std::max<std::size_t>(1U, command.reticles.size());
    }

    std::size_t operator()(const SetDynamicReticleSetVisibilityCommand&) const noexcept { return 1U; }
    std::size_t operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand&) const noexcept { return 1U; }
    std::size_t operator()(const RemoveDynamicReticleCommand&) const noexcept { return 1U; }
    std::size_t operator()(const ResetWindowCommand&) const noexcept { return 1U; }
};

/**
 * @brief Estimates the mutation work represented by one command.
 * @param command Command whose logical work is estimated.
 * @return Command work-unit count.
 */
inline std::size_t EstimateCommandWorkUnits(const UserCommand& command) noexcept
{
    return std::visit(CommandWorkUnitEstimator {}, command);
}

/**
 * @brief Estimates the mutation work represented by a command view.
 * @param commands Commands whose logical work is estimated.
 * @return Saturating work-unit count.
 */
inline std::size_t EstimateCommandWorkUnits(const ArrayView<const UserCommand> commands) noexcept
{
    std::size_t workUnits = 0U;
    for (const UserCommand& command : commands)
    {
        const std::size_t commandWorkUnits = EstimateCommandWorkUnits(command);
        if (workUnits > std::numeric_limits<std::size_t>::max() - commandWorkUnits)
        {
            return std::numeric_limits<std::size_t>::max();
        }

        workUnits += commandWorkUnits;
    }

    return workUnits;
}

/**
 * @brief Estimates the mutation work represented by one command batch.
 * @param batch Batch whose logical work is estimated.
 * @return Saturating work-unit count.
 */
inline std::size_t EstimateCommandWorkUnits(const CommandBatch& batch) noexcept
{
    return EstimateCommandWorkUnits(ArrayView<const UserCommand>(batch.commands));
}
} // namespace mfd::detail
