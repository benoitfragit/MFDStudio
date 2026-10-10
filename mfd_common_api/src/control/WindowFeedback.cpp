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
#include <limits>
#include <type_traits>
#include <utility>

#include "StrobeFeedbackProto.h"
#include "mfd_feedback.pb.h"

namespace mfd
{
namespace
{
namespace pb = ::mfd::transport;

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
                if (value.pageId == 0U)
                {
                    throw std::runtime_error("Unsupported feedback payload");
                }

                pb::ActivePageFeedback* message = envelope.mutable_active_page();
                message->set_sequence(value.sequence);
                message->set_page_id(value.pageId);
            }
            else if constexpr (std::is_same_v<FeedbackType, WindowLifecycleFeedback>)
            {
                pb::WindowLifecycleFeedback* message = envelope.mutable_window_lifecycle();
                message->set_sequence(value.sequence);
                message->set_state(ToProtoLifecycleState(value.state));
            }
            else if constexpr (std::is_same_v<FeedbackType, ExternalStreamFeedback>)
            {
                if (!IsValidStreamId(value.streamId) || !IsValidStreamId(value.instance) ||
                    value.status.diagnostic.size() > 512 || static_cast<std::uint32_t>(value.status.playback) > 2 ||
                    static_cast<std::uint32_t>(value.status.result) > static_cast<std::uint32_t>(StreamResult::Error))
                    throw std::runtime_error("Invalid external stream feedback");
                auto* message = envelope.mutable_external_stream();
                message->set_sequence(value.sequence);
                message->set_stream_id(value.streamId); message->set_instance(value.instance);
                message->set_producer_connected(value.status.producerConnected);
                message->set_has_image(value.status.hasImage); message->set_fresh(value.status.fresh);
                message->set_playback(static_cast<std::uint32_t>(value.status.playback));
                message->set_result(static_cast<std::uint32_t>(value.status.result));
                message->set_consumed_frame(value.status.consumedFrame);
                message->set_generation(value.status.generation); message->set_diagnostic(value.status.diagnostic);
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
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Feedback payload exceeds Protobuf parser size range");
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
            if (message.page_id() == 0U)
            {
                throw std::runtime_error("Unsupported feedback payload");
            }

            ActivePageFeedback feedback;
            feedback.sequence = message.sequence();
            feedback.pageId = message.page_id();
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

        if (envelope.payload_case() == pb::FeedbackEnvelope::kExternalStream)
        {
            const auto& message = envelope.external_stream();
            if (!IsValidStreamId(message.stream_id()) || !IsValidStreamId(message.instance()) ||
                message.diagnostic().size() > 512 || message.playback() > 2 ||
                message.result() > static_cast<std::uint32_t>(StreamResult::Error))
                throw std::runtime_error("Invalid external stream feedback");
            ExternalStreamFeedback feedback;
            feedback.sequence = message.sequence(); feedback.streamId = message.stream_id(); feedback.instance = message.instance();
            feedback.status.producerConnected = message.producer_connected(); feedback.status.hasImage = message.has_image();
            feedback.status.fresh = message.fresh(); feedback.status.playback = static_cast<StreamPlayback>(message.playback());
            feedback.status.result = static_cast<StreamResult>(message.result());
            feedback.status.consumedFrame = message.consumed_frame(); feedback.status.generation = message.generation();
            feedback.status.diagnostic = message.diagnostic();
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
