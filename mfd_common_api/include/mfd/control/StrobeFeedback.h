/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Protocol Buffers feedback payloads emitted by a window for strobe state.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "mfd/MfdExport.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/model/Types.h"

namespace mfd
{
/**
 * @brief Runtime magnetization state reported for one strobe.
 */
struct StrobeFeedbackMagnet
{
    /** @brief Indicates whether magnetization is enabled for this strobe. */
    bool enabled = false;
    /** @brief Attraction radius in logical coordinates. */
    float radius = 0.0f;
    /** @brief Blend factor applied when the strobe snaps to a target. */
    float strength = 1.0f;
    /** @brief Indicates whether a target is currently magnetized. */
    bool magnetized = false;
    /** @brief Public id of the currently magnetized reticle. */
    std::string reticleId;
    /** @brief Target position used by the magnetization logic. */
    Vec2 targetPosition {};
    /** @brief Distance between the strobe and the magnetized target. */
    float distance = 0.0f;
};

/**
 * @brief Capture snapshot reported in a strobe feedback message.
 */
struct StrobeFeedbackCapture
{
    /** @brief Public id of the captured dynamic reticle. */
    std::string reticleId;
    /** @brief Template id originally used to create the captured reticle. */
    std::string sourceTemplateId;
    /** @brief Human-readable label exposed by the captured reticle. */
    std::string label;
    /** @brief Category exposed by the captured reticle. */
    std::string category;
    /** @brief Captured reticle position in logical coordinates. */
    Vec2 position {};
    /** @brief Distance between strobe center and captured reticle. */
    float distance = 0.0f;
    /** @brief Arbitrary metadata copied from the captured reticle. */
    ReticleMetadata metadata;
};

/**
 * @brief Full feedback payload emitted by a window for one strobe.
 */
struct StrobeStatusFeedback
{
    /** @brief Monotonic sequence set by the window. */
    std::uint32_t sequence = 0;
    /** @brief Owning page name. */
    std::string pageName;
    /** @brief Public reticle id of the strobe cursor. */
    std::string strobeId;
    /** @brief Current strobe active state. */
    bool active = true;
    /** @brief Current strobe position in logical coordinates. */
    Vec2 position {};
    /** @brief Capture rules applied by the strobe. */
    StrobeCaptureConfig capture;
    /** @brief Magnetization state applied by the strobe. */
    StrobeFeedbackMagnet magnet;
    /** @brief Optional capture result currently associated with the strobe. */
    std::optional<StrobeFeedbackCapture> captureResult;
};

/**
 * @brief Serializes a strobe feedback payload to Protocol Buffers.
 * @param feedback Feedback payload to serialize.
 * @return Binary payload ready to be sent through UDP.
 */
MFD_API std::string SerializeStrobeStatusFeedback(const StrobeStatusFeedback& feedback);

/**
 * @brief Deserializes a Protocol Buffers strobe feedback payload.
 * @param payload Raw binary Protocol Buffers payload.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed feedback payload, or `std::nullopt` if the payload is invalid.
 */
MFD_API std::optional<StrobeStatusFeedback> DeserializeStrobeStatusFeedback(std::string_view payload,
                                                                            std::string* error = nullptr);
} // namespace mfd
