/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Dedicated libFuzzer entry point stressing Protocol Buffers transport decoding.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/WindowFeedback.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (data == nullptr)
    {
        return 0;
    }

    // The transport decoders operate on one raw byte payload and must reject
    // malformed or hostile inputs without hanging or corrupting state.
    const std::string_view payload(reinterpret_cast<const char*>(data), size);

    std::string error;
    mfd::DeserializeCommandBatch(payload, &error);
    error.clear();
    mfd::DeserializeFeedbackPayload(payload, &error);
    error.clear();
    mfd::DeserializeStrobeStatusFeedback(payload, &error);

    return 0;
}
