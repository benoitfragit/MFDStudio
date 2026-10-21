/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Window API capability levels supported by MFDStudio assets.
 */

#include <cstdint>

namespace mfd
{
/**
 * @brief Maximum API capability level enabled for one Window asset.
 *
 * @note LEVEL 0 preserves the original MFDStudio feature set and behavior.
 */
enum class ApiLevel : std::uint32_t
{
    Legacy = 0
};
} // namespace mfd
