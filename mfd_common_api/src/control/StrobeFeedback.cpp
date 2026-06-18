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

#include "StrobeFeedbackProto.h"
#include "mfd/control/WindowFeedback.h"
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
    if (shape == pb::STROBE_CAPTURE_SHAPE_RECTANGLE)
    {
        return StrobeCaptureShape::Rectangle;
    }

    return StrobeCaptureShape::Circle;
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
    target->set_runtime_reticle_id(magnet.runtimeReticleId);
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
    magnet.runtimeReticleId = value.runtime_reticle_id();
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
    target->set_runtime_reticle_id(capture.runtimeReticleId);
    target->set_source_template_transport_id(capture.sourceTemplateTransportId);
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
    capture.runtimeReticleId = value.runtime_reticle_id();
    capture.sourceTemplateTransportId = value.source_template_transport_id();
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

namespace detail
{
void FillStrobeStatusFeedbackProto(const StrobeStatusFeedback& feedback, pb::StrobeStatusFeedback* target)
{
    target->set_sequence(feedback.sequence);
    target->set_page_id(feedback.pageId);
    target->set_page(feedback.pageName);
    target->set_strobe_id(feedback.strobeId);
    target->set_active(feedback.active);
    FillProtoVec2(feedback.position, target->mutable_position());
    FillProtoCaptureConfig(feedback.capture, target->mutable_capture());
    FillProtoMagnet(feedback.magnet, target->mutable_magnet());

    if (feedback.captureResult.has_value())
    {
        FillProtoCaptureResult(*feedback.captureResult, target->mutable_capture_result());
    }
}

StrobeStatusFeedback ParseStrobeStatusFeedbackProto(const pb::StrobeStatusFeedback& message)
{
    if (!IsValidFeedbackIdentifier(message.page()) || !IsValidFeedbackIdentifier(message.strobe_id()))
    {
        throw std::runtime_error("Unsupported feedback payload");
    }

    StrobeStatusFeedback feedback;
    feedback.sequence = message.sequence();
    feedback.pageId = message.page_id();
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

    return feedback;
}
} // namespace detail

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
} // namespace mfd
