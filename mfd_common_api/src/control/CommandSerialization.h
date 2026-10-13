#pragma once

#include <limits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "mfd/control/CommandTypes.h"
#include "mfd/core/ArrayView.h"

namespace mfd::detail
{
/**
 * @brief Borrowed batch used synchronously while its caller-owned commands remain alive.
 * @note Transport metadata can change without copying or mutating the commands.
 * This header is private to the implementation and is not installed with the SDK.
 */
struct CommandBatchView
{
    ArrayView<const UserCommand> commands;
    std::uint32_t sequence = 0;
    std::string_view mappingHash;
    std::optional<CommandBatchFragment> fragment;
};

/** @brief Borrows a batch; all referenced storage must outlive the returned view. */
inline CommandBatchView ViewBatch(const CommandBatch& batch) noexcept
{
    return {ArrayView<const UserCommand>(batch.commands), batch.sequence, batch.mappingHash, batch.fragment};
}

/** @brief Validates and encodes borrowed commands; returns empty when the encoded size exceeds the limit.
 *  @param batch Synchronous borrowed input.
 *  @param maxBytes Output budget. Oversized input is still fully validated before returning.
 */
std::string SerializeBatch(const CommandBatchView& batch,
                           std::size_t maxBytes = std::numeric_limits<std::size_t>::max());
/** @brief Exact encoded envelope size without allocating a serialized output buffer. */
std::size_t BatchWireSize(const CommandBatchView& batch);
/** @brief Exact envelope framing size; intentionally permits an empty command view. */
std::size_t EnvelopeMetadataWireSize(const CommandBatchView& batch);
/** @brief Exact contribution of one command, including its envelope tag and length. */
std::size_t CommandFieldWireSize(const UserCommand& command);
}
