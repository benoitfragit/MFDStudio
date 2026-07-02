#pragma once

#include <cstddef>

#include "mfd/control/CommandTypes.h"

namespace mfd::detail
{
/**
 * @brief Serialized Protocol Buffers payload size, in bytes, of one dynamic reticle state.
 *
 * @param state Dynamic reticle state to measure.
 * @return Byte size of the encoded `DynamicReticleState` message, excluding the
 * repeated-field tag and length prefix added by the enclosing command message.
 *
 * @note Used by the client-side command splitter to estimate chunk sizes from one cheap
 * per-entry measurement instead of serializing a full candidate batch per probe.
 */
std::size_t DynamicReticleStateWireSize(const DynamicReticleState& state);
} // namespace mfd::detail
