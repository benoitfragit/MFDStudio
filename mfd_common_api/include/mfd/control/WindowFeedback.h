/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Protocol Buffers runtime feedback payloads describing a window's own
 * state (active page, lifecycle) and the envelope carrying any supported
 * feedback kind.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "mfd/MfdExport.h"
#include "mfd/control/StrobeFeedback.h"

namespace mfd
{
/**
 * @brief Runtime snapshot reporting which page is currently rendered as active.
 */
struct ActivePageFeedback
{
    /** @brief Monotonic sequence set by the window. */
    std::uint32_t sequence = 0;
    /** @brief Non-zero generated transport id of the page currently rendered as active. */
    mfd::TransportId pageId = 0;
};

/**
 * @brief Window lifecycle state reported by a runtime feedback heartbeat.
 */
enum class WindowLifecycleState
{
    /** @brief The window runtime is alive and serving the authored contract. */
    Alive,
    /** @brief The window runtime is shutting down and will stop emitting feedback. */
    Closing
};

/**
 * @brief Liveness heartbeat emitted by a window so clients can detect shutdown.
 *
 * @note The window emits `Alive` payloads on a slow cadence and one final
 * `Closing` payload during a graceful shutdown. Clients combine the explicit
 * `Closing` signal with an absence-of-heartbeat timeout to detect both graceful
 * and abrupt window termination.
 */
struct WindowLifecycleFeedback
{
    /** @brief Monotonic sequence set by the window. */
    std::uint32_t sequence = 0;
    /** @brief Reported lifecycle state. */
    WindowLifecycleState state = WindowLifecycleState::Alive;
};

/**
 * @brief Variant carrying one supported runtime feedback payload.
 */
using FeedbackPayload = std::variant<StrobeStatusFeedback, ActivePageFeedback, WindowLifecycleFeedback>;

/**
 * @brief Serializes any supported runtime feedback payload to Protocol Buffers.
 * @param feedback Feedback payload to serialize.
 * @return Binary payload ready to be sent through UDP.
 * @pre Identifier-bearing feedback variants contain non-zero identifiers.
 */
MFD_API std::string SerializeFeedbackPayload(const FeedbackPayload& feedback);

/**
 * @brief Deserializes a Protocol Buffers runtime feedback payload.
 * @param payload Raw binary Protocol Buffers payload.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed feedback payload, or `std::nullopt` if the payload is invalid.
 */
MFD_API std::optional<FeedbackPayload> DeserializeFeedbackPayload(std::string_view payload,
                                                                  std::string* error = nullptr);

/**
 * @brief Serializes an active-page feedback payload to Protocol Buffers.
 * @param feedback Feedback payload to serialize.
 * @return Binary payload ready to be sent through UDP.
 * @pre `feedback.pageId` is non-zero.
 */
MFD_API std::string SerializeActivePageFeedback(const ActivePageFeedback& feedback);

/**
 * @brief Deserializes an active-page Protocol Buffers feedback payload.
 *
 * @note When the payload contains another supported feedback kind, this
 * decoder returns `std::nullopt` without treating it as an error.
 *
 * @param payload Raw binary Protocol Buffers payload.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed active-page feedback payload, or `std::nullopt` if the
 * payload is invalid or belongs to another supported feedback kind.
 */
MFD_API std::optional<ActivePageFeedback> DeserializeActivePageFeedback(std::string_view payload,
                                                                        std::string* error = nullptr);

/**
 * @brief Serializes a window lifecycle feedback payload to Protocol Buffers.
 * @param feedback Feedback payload to serialize.
 * @return Binary payload ready to be sent through UDP.
 */
MFD_API std::string SerializeWindowLifecycleFeedback(const WindowLifecycleFeedback& feedback);

/**
 * @brief Deserializes a Protocol Buffers window lifecycle feedback payload.
 *
 * @note When the payload contains another supported feedback kind, this
 * decoder returns `std::nullopt` without treating it as an error.
 *
 * @param payload Raw binary Protocol Buffers payload.
 * @param error Optional output string receiving the parsing error.
 * @return Parsed window lifecycle feedback payload, or `std::nullopt` if the
 * payload is invalid or belongs to another supported feedback kind.
 */
MFD_API std::optional<WindowLifecycleFeedback> DeserializeWindowLifecycleFeedback(std::string_view payload,
                                                                                  std::string* error = nullptr);
} // namespace mfd
