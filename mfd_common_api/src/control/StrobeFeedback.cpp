/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for StrobeFeedback.
 */

#include "mfd/control/StrobeFeedback.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

#include "mfd_feedback.pb.h"

namespace mfd
{
namespace
{
namespace pb = ::mfd::transport;

void FillProtoVec2(const Vec2& value, pb::Vec2* target)
{
    target->set_x(value.x);
    target->set_y(value.y);
}

Vec2 FromProtoVec2(const pb::Vec2& value) noexcept
{
    return Vec2 {value.x(), value.y()};
}

pb::StrobeCaptureShape ToProtoCaptureShape(const StrobeCaptureShape shape) noexcept
{
    switch (shape)
    {
    case StrobeCaptureShape::Circle:
        return pb::STROBE_CAPTURE_SHAPE_CIRCLE;

    case StrobeCaptureShape::Rectangle:
        return pb::STROBE_CAPTURE_SHAPE_RECTANGLE;
    }

    return pb::STROBE_CAPTURE_SHAPE_CIRCLE;
}

StrobeCaptureShape FromProtoCaptureShape(const pb::StrobeCaptureShape shape) noexcept
{
    switch (shape)
    {
    case pb::STROBE_CAPTURE_SHAPE_RECTANGLE:
        return StrobeCaptureShape::Rectangle;

    case pb::STROBE_CAPTURE_SHAPE_CIRCLE:
    default:
        return StrobeCaptureShape::Circle;
    }
}

void FillProtoCaptureConfig(const StrobeCaptureConfig& capture, pb::StrobeCaptureArea* target)
{
    target->set_shape(ToProtoCaptureShape(capture.shape));
    target->set_radius(capture.radius);
    FillProtoVec2(capture.size, target->mutable_size());
}

StrobeCaptureConfig FromProtoCaptureConfig(const pb::StrobeCaptureArea& value)
{
    StrobeCaptureConfig capture;
    capture.shape = FromProtoCaptureShape(value.shape());
    capture.radius = value.radius();
    if (value.has_size())
    {
        capture.size = FromProtoVec2(value.size());
    }
    return capture;
}

void FillProtoMagnet(const StrobeFeedbackMagnet& magnet, pb::StrobeMagnetStatus* target)
{
    target->set_enabled(magnet.enabled);
    target->set_radius(magnet.radius);
    target->set_strength(magnet.strength);
    target->set_magnetized(magnet.magnetized);
    target->set_reticle(magnet.reticleId);
    FillProtoVec2(magnet.targetPosition, target->mutable_target_position());
    target->set_distance(magnet.distance);
}

StrobeFeedbackMagnet FromProtoMagnet(const pb::StrobeMagnetStatus& value)
{
    StrobeFeedbackMagnet magnet;
    magnet.enabled = value.enabled();
    magnet.radius = value.radius();
    magnet.strength = value.strength();
    magnet.magnetized = value.magnetized();
    magnet.reticleId = value.reticle();
    if (value.has_target_position())
    {
        magnet.targetPosition = FromProtoVec2(value.target_position());
    }
    magnet.distance = value.distance();
    return magnet;
}

void FillProtoCaptureResult(const StrobeFeedbackCapture& capture, pb::StrobeCaptureStatus* target)
{
    target->set_reticle(capture.reticleId);
    target->set_template_id(capture.sourceTemplateId);
    target->set_label(capture.label);
    target->set_category(capture.category);
    FillProtoVec2(capture.position, target->mutable_position());
    target->set_distance(capture.distance);

    for (const auto& [key, value] : capture.metadata)
    {
        (*target->mutable_metadata())[key] = value;
    }
}

StrobeFeedbackCapture FromProtoCaptureResult(const pb::StrobeCaptureStatus& value)
{
    StrobeFeedbackCapture capture;
    capture.reticleId = value.reticle();
    capture.sourceTemplateId = value.template_id();
    capture.label = value.label();
    capture.category = value.category();
    if (value.has_position())
    {
        capture.position = FromProtoVec2(value.position());
    }
    capture.distance = value.distance();

    for (const auto& [key, metadataValue] : value.metadata())
    {
        capture.metadata.emplace(key, metadataValue);
    }

    return capture;
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
                pb::StrobeStatusFeedback* message = envelope.mutable_strobe_status();
                message->set_sequence(value.sequence);
                message->set_page(value.pageName);
                message->set_strobe_id(value.strobeId);
                message->set_active(value.active);
                FillProtoVec2(value.position, message->mutable_position());
                FillProtoCaptureConfig(value.capture, message->mutable_capture());
                FillProtoMagnet(value.magnet, message->mutable_magnet());

                if (value.captureResult.has_value())
                {
                    FillProtoCaptureResult(*value.captureResult, message->mutable_capture_result());
                }
            }
            else if constexpr (std::is_same_v<FeedbackType, ActivePageFeedback>)
            {
                pb::ActivePageFeedback* message = envelope.mutable_active_page();
                message->set_sequence(value.sequence);
                message->set_page(value.pageName);
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
            const pb::StrobeStatusFeedback& message = envelope.strobe_status();

            StrobeStatusFeedback feedback;
            feedback.sequence = message.sequence();
            feedback.pageName = message.page();
            feedback.strobeId = message.strobe_id();
            feedback.active = message.active();

            if (message.has_position())
            {
                feedback.position = FromProtoVec2(message.position());
            }

            if (message.has_capture())
            {
                feedback.capture = FromProtoCaptureConfig(message.capture());
            }

            if (message.has_magnet())
            {
                feedback.magnet = FromProtoMagnet(message.magnet());
            }

            if (message.has_capture_result())
            {
                feedback.captureResult = FromProtoCaptureResult(message.capture_result());
            }

            return FeedbackPayload {std::move(feedback)};
        }

        if (envelope.payload_case() == pb::FeedbackEnvelope::kActivePage)
        {
            const pb::ActivePageFeedback& message = envelope.active_page();
            ActivePageFeedback feedback;
            feedback.sequence = message.sequence();
            feedback.pageName = message.page();
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

std::string SerializeStrobeStatusFeedback(const StrobeStatusFeedback& feedback)
{
    return SerializeFeedbackPayload(FeedbackPayload {feedback});
}

std::optional<StrobeStatusFeedback> DeserializeStrobeStatusFeedback(const std::string_view payload, std::string* error)
{
    std::string localError;
    std::string* errorTarget = error == nullptr ? &localError : error;
    errorTarget->clear();

    const auto decoded = DeserializeFeedbackPayload(payload, errorTarget);
    if (!decoded.has_value())
    {
        return std::nullopt;
    }

    if (const auto* feedback = std::get_if<StrobeStatusFeedback>(&(*decoded)); feedback != nullptr)
    {
        return *feedback;
    }

    errorTarget->clear();
    return std::nullopt;
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
} // namespace mfd
