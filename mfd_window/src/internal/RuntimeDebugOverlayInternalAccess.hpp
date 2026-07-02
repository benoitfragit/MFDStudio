/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal-only bridge exposing runtime debug overlay state to repository tests.
 */

#include "mfd/window/debug/RuntimeDebugOverlay.hpp"

namespace mfd::window::debug::internal
{
/**
 * @brief Internal access reserved to repository tests that need to inspect overlay state.
 */
struct RuntimeDebugOverlayInternalAccess
{
    [[nodiscard]] static RuntimeDebugState& State(RuntimeDebugOverlay& overlay) noexcept
    {
        return overlay.state_;
    }

    [[nodiscard]] static const RuntimeDebugState& State(const RuntimeDebugOverlay& overlay) noexcept
    {
        return overlay.state_;
    }
};
} // namespace mfd::window::debug::internal
