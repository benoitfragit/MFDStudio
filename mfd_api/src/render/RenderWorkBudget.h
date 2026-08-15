/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Deterministic work budget shared by one render pass.
 */

#include <cstddef>

namespace mfd::detail
{
/**
 * @brief Maximum logical render work emitted by one canvas.
 *
 * Tessellated geometry consumes triangle or segment units, text consumes units
 * proportional to its UTF-8 byte count, and direct image/background operations
 * consume at least one unit.
 */
constexpr std::size_t kMaxCanvasDrawWorkUnits = 16384U;

/** @brief Maximum number of reticle visits performed by layer-local restoration in one frame. */
constexpr std::size_t kMaxLayerRestoreVisits = 16384U;

/**
 * @brief Monotonic counter preventing nested render loops from multiplying independent caps.
 */
class RenderWorkBudget
{
public:
    /**
     * @brief Creates a budget with a fixed amount of work.
     * @param availableUnits Maximum accepted work units.
     */
    explicit constexpr RenderWorkBudget(const std::size_t availableUnits) noexcept
        : remainingUnits_(availableUnits)
    {
    }

    /**
     * @brief Reserves work units when they remain available.
     * @param requestedUnits Number of units required by the operation.
     * @return `true` when the reservation succeeded.
     */
    [[nodiscard]] constexpr bool TryConsume(const std::size_t requestedUnits = 1U) noexcept
    {
        if (requestedUnits > remainingUnits_)
        {
            return false;
        }

        remainingUnits_ -= requestedUnits;
        return true;
    }

    /** @brief Returns the work units that can still be reserved. */
    [[nodiscard]] constexpr std::size_t RemainingUnits() const noexcept
    {
        return remainingUnits_;
    }

private:
    std::size_t remainingUnits_ = 0U;
};
} // namespace mfd::detail
