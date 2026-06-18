/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for WindowFeedback.
 */

#include "mfd/control/WindowFeedback.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

#include "StrobeFeedbackProto.h"
#include "mfd_feedback.pb.h"

namespace mfd
{
namespace
{
namespace pb = ::mfd::transport;

bool ContainsControlCharacters(const std::string_view value) noexcept
{
    for (const unsigned char character : value)
    {
        if (character < 0x20U || character == 0x7FU)
        {
            return true;
        }
    }

    return false;
}

bool IsValidFeedbackIdentifier(const std::string_view value) noexcept
{
    return !value.empty() && !ContainsControlCharacters(value);
}

pb::WindowLifecycleState ToProtoLifecycleState(const WindowLifecycleState state) noexcept
{
    switch (state)
    {
    case WindowLifecycleState::Alive:
        return pb::WINDOW_LIFECYCLE_STATE_ALIVE;

    case WindowLifecycleState::Closing:
        return pb::WINDOW_LIFECYCLE_STATE_CLOSING;
    }

    return pb::WINDOW_LIFECYCLE_STATE_ALIVE;
}

WindowLifecycleState FromProtoLifecycleState(const pb::WindowLifecycleState state) noexcept
{
    if (state == pb::WINDOW_LIFECYCLE_STATE_CLOSING)
    {
        return WindowLifecycleState::Closing;
    }

    return WindowLifecycleState::Alive;
}
} // namespace

std::string SerializeFeedbackPayload(const FeedbackPayload& feedback)
{
    pb::FeedbackEnvelope envelope;
    std::visit(
        [&envelope](const auto& value)
        {
            using FeedbackType = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<FeedbackType, StrobeStatusFeedback>)
            {
                detail::FillStrobeStatusFeedbackProto(value, envelope.mutable_strobe_status());
            }
            else if constexpr (std::is_same_v<FeedbackType, ActivePageFeedback>)
            {
                pb::ActivePageFeedback* message = envelope.mutable_active_page();
                message->set_sequence(value.sequence);
                message->set_page(value.pageName);
            }
            else if constexpr (std::is_same_v<FeedbackType, WindowLifecycleFeedback>)
            {
                pb::WindowLifecycleFeedback* message = envelope.mutable_window_lifecycle();
                message->set_sequence(value.sequence);
                message->set_state(ToProtoLifecycleState(value.state));
            }
        },
        feedback);

    std::string payload;
    if (!envelope.SerializeToString(&payload))
    {
        throw std::runtime_error("Unable to serialize Protocol Buffers runtime feedback payload");
    }

    return payload;
}

std::optional<FeedbackPayload> DeserializeFeedbackPayload(const std::string_view payload, std::string* error)
{
    try
    {
        pb::FeedbackEnvelope envelope;
        if (!envelope.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
        {
            throw std::runtime_error("Unable to parse Protocol Buffers runtime feedback payload");
        }

        if (envelope.payload_case() == pb::FeedbackEnvelope::kStrobeStatus)
        {
            return FeedbackPayload {detail::ParseStrobeStatusFeedbackProto(envelope.strobe_status())};
        }

        if (envelope.payload_case() == pb::FeedbackEnvelope::kActivePage)
        {
            const pb::ActivePageFeedback& message = envelope.active_page();
            if (!IsValidFeedbackIdentifier(message.page()))
            {
                throw std::runtime_error("Unsupported feedback payload");
            }

            ActivePageFeedback feedback;
            feedback.sequence = message.sequence();
            feedback.pageName = message.page();
            return FeedbackPayload {std::move(feedback)};
        }

        if (envelope.payload_case() == pb::FeedbackEnvelope::kWindowLifecycle)
        {
            const pb::WindowLifecycleFeedback& message = envelope.window_lifecycle();

            WindowLifecycleFeedback feedback;
            feedback.sequence = message.sequence();
            feedback.state = FromProtoLifecycleState(message.state());
            return FeedbackPayload {std::move(feedback)};
        }

        throw std::runtime_error("Unsupported feedback payload");
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }

        return std::nullopt;
    }
}

std::string SerializeActivePageFeedback(const ActivePageFeedback& feedback)
{
    return SerializeFeedbackPayload(FeedbackPayload {feedback});
}

std::optional<ActivePageFeedback> DeserializeActivePageFeedback(const std::string_view payload, std::string* error)
{
    std::string localError;
    std::string* errorTarget = error == nullptr ? &localError : error;
    errorTarget->clear();

    const auto decoded = DeserializeFeedbackPayload(payload, errorTarget);
    if (!decoded.has_value())
    {
        return std::nullopt;
    }

    if (const auto* feedback = std::get_if<ActivePageFeedback>(&(*decoded)); feedback != nullptr)
    {
        return *feedback;
    }

    errorTarget->clear();
    return std::nullopt;
}

std::string SerializeWindowLifecycleFeedback(const WindowLifecycleFeedback& feedback)
{
    return SerializeFeedbackPayload(FeedbackPayload {feedback});
}

std::optional<WindowLifecycleFeedback> DeserializeWindowLifecycleFeedback(const std::string_view payload,
                                                                          std::string* error)
{
    std::string localError;
    std::string* errorTarget = error == nullptr ? &localError : error;
    errorTarget->clear();

    const auto decoded = DeserializeFeedbackPayload(payload, errorTarget);
    if (!decoded.has_value())
    {
        return std::nullopt;
    }

    if (const auto* feedback = std::get_if<WindowLifecycleFeedback>(&(*decoded)); feedback != nullptr)
    {
        return *feedback;
    }

    errorTarget->clear();
    return std::nullopt;
}
} // namespace mfd
