/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for CommandTypes.
 */

#include "mfd/control/CommandTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mfd_commands.pb.h"
#include "mfd/model/PageName.h"
#include "mfd/control/internal/CommandWireSizeHelpers.h"
#include "mfd/model/RuntimeBudgets.h"

namespace mfd
{
namespace
{
namespace pb = ::mfd::transport;

// Runtime safety budgets and validity predicates are owned by mfd_common_api so the command path
// shares one source of truth with the JSON/scene paths and cannot drift away from them.
using runtime_validation::DaysInMonth;
using runtime_validation::HasVisibleTimeField;
using runtime_validation::IsFiniteVec2;
using runtime_validation::kMaxAbsAngleDegrees;
using runtime_validation::kMaxAbsCoordinate;
using runtime_validation::kMaxAbsScale;
using runtime_validation::kMaxFilledPolygonPoints;
using runtime_validation::kMaxLogicalSize;
using runtime_validation::kMaxPrimitivePoints;
using runtime_validation::kMaxPrimitiveSegments;
using runtime_validation::kMaxTextBytes;
using runtime_validation::kMaxThickness;
using runtime_validation::kMaxTimeYear;
using runtime_validation::kMaxZoom;
using runtime_validation::kMinTimeYear;

// Command-transport-specific budgets that have no JSON/scene equivalent and therefore stay local.
constexpr std::size_t kMaxCommandPayloadBytes = 1024ULL * 1024ULL;
constexpr std::size_t kMaxCommandsPerEnvelope = 1024U;
constexpr std::size_t kMaxPatchEntryCount = 2048U;
constexpr std::size_t kMaxDynamicReticlesPerBatch = 4096U;

void ValidateFiniteAbs(const float value, const char* fieldName, const float maxAbs)
{
    if (!std::isfinite(value))
    {
        throw std::runtime_error(std::string(fieldName) + " must be finite");
    }

    if (std::abs(value) > maxAbs)
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidatePositiveFinite(const float value, const char* fieldName, const float maxValue)
{
    if (!std::isfinite(value) || value <= 0.0f)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive and finite");
    }

    if (value > maxValue)
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateVec2(const Vec2& value, const char* fieldName, const float maxAbs = kMaxAbsCoordinate)
{
    if (!IsFiniteVec2(value))
    {
        throw std::runtime_error(std::string(fieldName) + " must contain finite coordinates");
    }

    if (std::abs(value.x) > maxAbs || std::abs(value.y) > maxAbs)
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateScale(const Vec2& value, const char* fieldName)
{
    ValidateVec2(value, fieldName, kMaxAbsScale);
}

void ValidateContainerSize(const std::size_t size, const std::size_t maxSize, const char* fieldName)
{
    if (size > maxSize)
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateTextPayload(const std::string& value, const char* fieldName)
{
    if (value.size() > kMaxTextBytes)
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateSegmentCount(const int value, const char* fieldName)
{
    if (value < 2 || value > kMaxPrimitiveSegments)
    {
        throw std::runtime_error(std::string(fieldName) + " must stay in [2, " +
                                 std::to_string(kMaxPrimitiveSegments) + "]");
    }
}

void ValidateTimeValue(const TimeValue& value, const char* fieldName)
{
    if (value.year < kMinTimeYear || value.year > kMaxTimeYear)
    {
        throw std::runtime_error(std::string(fieldName) + ".year must stay in [1, 9999]");
    }

    if (value.month < 1 || value.month > 12)
    {
        throw std::runtime_error(std::string(fieldName) + ".month must stay in [1, 12]");
    }

    const int maxDay = DaysInMonth(value.year, value.month);
    if (value.day < 1 || value.day > maxDay)
    {
        throw std::runtime_error(std::string(fieldName) + ".day is outside the selected month");
    }

    if (value.hour < 0 || value.hour > 23)
    {
        throw std::runtime_error(std::string(fieldName) + ".hour must stay in [0, 23]");
    }

    if (value.minute < 0 || value.minute > 59)
    {
        throw std::runtime_error(std::string(fieldName) + ".minute must stay in [0, 59]");
    }

    if (value.second < 0 || value.second > 59)
    {
        throw std::runtime_error(std::string(fieldName) + ".second must stay in [0, 59]");
    }
}

void ValidateTimeFieldVisibility(const TimeFieldVisibility& fields, const char* fieldName)
{
    if (!HasVisibleTimeField(fields))
    {
        throw std::runtime_error(std::string(fieldName) + " must show at least one field");
    }
}

void ValidatePointList(const std::vector<Vec2>& points, const char* fieldName)
{
    ValidateContainerSize(points.size(), kMaxPrimitivePoints, fieldName);
    if (points.size() == 1U)
    {
        throw std::runtime_error(std::string(fieldName) + " must contain at least 2 points");
    }

    for (const Vec2& point : points)
    {
        ValidateVec2(point, "PrimitivePatch point");
    }
}

void ValidatePrimitivePatch(const PrimitivePatch& patch)
{
    if (patch.position.has_value())
    {
        ValidateVec2(*patch.position, "PrimitivePatch.position");
    }

    if (patch.rotationDegrees.has_value())
    {
        ValidateFiniteAbs(*patch.rotationDegrees, "PrimitivePatch.rotationDegrees", kMaxAbsAngleDegrees);
    }

    if (patch.scale.has_value())
    {
        ValidateScale(*patch.scale, "PrimitivePatch.scale");
    }

    if (patch.thickness.has_value())
    {
        ValidatePositiveFinite(*patch.thickness, "PrimitivePatch.thickness", kMaxThickness);
    }

    if (patch.text.has_value())
    {
        ValidateTextPayload(*patch.text, "PrimitivePatch.text");
    }

    if (patch.letterSpacing.has_value())
    {
        ValidateFiniteAbs(*patch.letterSpacing, "PrimitivePatch.letterSpacing", kMaxLogicalSize);
    }

    if (patch.lineStart.has_value())
    {
        ValidateVec2(*patch.lineStart, "PrimitivePatch.lineStart");
    }

    if (patch.lineEnd.has_value())
    {
        ValidateVec2(*patch.lineEnd, "PrimitivePatch.lineEnd");
    }

    if (patch.radius.has_value())
    {
        ValidateFiniteAbs(*patch.radius, "PrimitivePatch.radius", kMaxLogicalSize);
    }

    if (patch.innerRadius.has_value())
    {
        ValidateFiniteAbs(*patch.innerRadius, "PrimitivePatch.innerRadius", kMaxLogicalSize);
    }

    if (patch.outerRadius.has_value())
    {
        ValidateFiniteAbs(*patch.outerRadius, "PrimitivePatch.outerRadius", kMaxLogicalSize);
    }

    if (patch.width.has_value())
    {
        ValidateFiniteAbs(*patch.width, "PrimitivePatch.width", kMaxLogicalSize);
    }

    if (patch.height.has_value())
    {
        ValidateFiniteAbs(*patch.height, "PrimitivePatch.height", kMaxLogicalSize);
    }

    if (patch.size.has_value())
    {
        ValidateVec2(*patch.size, "PrimitivePatch.size", kMaxLogicalSize);
    }

    if (patch.points.has_value())
    {
        ValidatePointList(*patch.points, "PrimitivePatch.points");
        if (patch.closed.value_or(false) && patch.points->size() > kMaxFilledPolygonPoints)
        {
            throw std::runtime_error("PrimitivePatch.points exceeds filled polygon runtime safety limits");
        }
    }

    if (patch.segments.has_value())
    {
        ValidateSegmentCount(*patch.segments, "PrimitivePatch.segments");
    }

    if (patch.startAngleDegrees.has_value())
    {
        ValidateFiniteAbs(*patch.startAngleDegrees, "PrimitivePatch.startAngleDegrees", kMaxAbsAngleDegrees);
    }

    if (patch.endAngleDegrees.has_value())
    {
        ValidateFiniteAbs(*patch.endAngleDegrees, "PrimitivePatch.endAngleDegrees", kMaxAbsAngleDegrees);
    }

    if (patch.clearTimeValue && patch.timeValue.has_value())
    {
        throw std::runtime_error("PrimitivePatch cannot set and clear timeValue in the same patch");
    }

    if (patch.timeValue.has_value())
    {
        ValidateTimeValue(*patch.timeValue, "PrimitivePatch.timeValue");
    }

    if (patch.timeFields.has_value())
    {
        ValidateTimeFieldVisibility(*patch.timeFields, "PrimitivePatch.timeFields");
    }
}

void ValidateReticlePatch(const ReticlePatch& patch)
{
    if (patch.position.has_value())
    {
        ValidateVec2(*patch.position, "ReticlePatch.position");
    }

    if (patch.rotationDegrees.has_value())
    {
        ValidateFiniteAbs(*patch.rotationDegrees, "ReticlePatch.rotationDegrees", kMaxAbsAngleDegrees);
    }

    if (patch.scale.has_value())
    {
        ValidateScale(*patch.scale, "ReticlePatch.scale");
    }

    if (patch.thickness.has_value())
    {
        ValidatePositiveFinite(*patch.thickness, "ReticlePatch.thickness", kMaxThickness);
    }

    if (patch.text.has_value())
    {
        ValidateTextPayload(*patch.text, "ReticlePatch.text");
    }

    if (patch.letterSpacing.has_value())
    {
        ValidateFiniteAbs(*patch.letterSpacing, "ReticlePatch.letterSpacing", kMaxLogicalSize);
    }

    ValidateContainerSize(patch.texts.size(), kMaxPatchEntryCount, "ReticlePatch.texts");
    for (const auto& textEntry : patch.texts)
    {
        ValidateTextPayload(textEntry.second, "ReticlePatch.texts[]");
    }

    ValidateContainerSize(patch.textsById.size(), kMaxPatchEntryCount, "ReticlePatch.textsById");
    for (const auto& textEntry : patch.textsById)
    {
        ValidateTextPayload(textEntry.second, "ReticlePatch.textsById[]");
    }

    ValidateContainerSize(patch.letterSpacings.size(), kMaxPatchEntryCount, "ReticlePatch.letterSpacings");
    for (const auto& spacingEntry : patch.letterSpacings)
    {
        ValidateFiniteAbs(spacingEntry.second, "ReticlePatch.letterSpacings[]", kMaxLogicalSize);
    }

    ValidateContainerSize(patch.letterSpacingsById.size(), kMaxPatchEntryCount, "ReticlePatch.letterSpacingsById");
    for (const auto& spacingEntry : patch.letterSpacingsById)
    {
        ValidateFiniteAbs(spacingEntry.second, "ReticlePatch.letterSpacingsById[]", kMaxLogicalSize);
    }

    ValidateContainerSize(patch.primitivePatches.size(), kMaxPatchEntryCount, "ReticlePatch.primitivePatches");
    for (const auto& primitivePatchEntry : patch.primitivePatches)
    {
        ValidatePrimitivePatch(primitivePatchEntry.second);
    }

    ValidateContainerSize(patch.primitivePatchesById.size(), kMaxPatchEntryCount, "ReticlePatch.primitivePatchesById");
    for (const auto& primitivePatchEntry : patch.primitivePatchesById)
    {
        ValidatePrimitivePatch(primitivePatchEntry.second);
    }
}

void ValidatePageViewState(const PageViewState& view)
{
    ValidateVec2(view.center, "SetPageViewCommand.center");
    ValidatePositiveFinite(view.zoom, "SetPageViewCommand.zoom", kMaxZoom);
}

void ValidateWindowDisplayPatch(const WindowDisplayPatch& patch)
{
    if (patch.brightness.has_value() && !std::isfinite(*patch.brightness))
    {
        throw std::runtime_error("WindowDisplayPatch.brightness must be finite");
    }
}

void ValidateDynamicReticleState(const DynamicReticleState& state)
{
    if (state.runtimeReticleId == 0 && state.reticleId.empty())
    {
        throw std::runtime_error("Dynamic reticle updates require a runtimeReticleId or a local reticleId");
    }

    ValidateReticlePatch(state.patch);
}

// Validates semantic command preconditions with one explicit overload per command type.
// This visitor intentionally has no generic fallback: adding a new UserCommand alternative
// must fail to compile here until its validation rule is written.
struct UserCommandValidator
{
    void operator()(const ActivatePageCommand& command) const
    {
        if (command.pageId == 0 && command.page.empty())
        {
            throw std::runtime_error("ActivatePageCommand requires a pageId or page name");
        }
    }

    void operator()(const SetPageViewCommand& command) const
    {
        if (command.pageId == 0 && command.page.empty())
        {
            throw std::runtime_error("SetPageViewCommand requires a pageId or page name");
        }

        ValidatePageViewState(command.view);
    }

    void operator()(const UpdateWindowDisplayCommand& command) const
    {
        ValidateWindowDisplayPatch(command.patch);
    }

    void operator()(const UpdateReticleCommand& command) const
    {
        if ((command.target.pageId == 0 && command.target.page.empty()) ||
            (command.target.reticleId == 0 && command.target.reticle.empty()))
        {
            throw std::runtime_error("UpdateReticleCommand requires a target page and reticle");
        }

        ValidateReticlePatch(command.patch);
    }

    void operator()(const UpdateStrobeCommand& command) const
    {
        if (command.pageId == 0 && command.page.empty())
        {
            throw std::runtime_error("UpdateStrobeCommand requires a pageId or page name");
        }

        if (command.strobeId == 0 && !command.strobe.empty())
        {
            if (PageNameIsBlank(command.strobe))
            {
                throw std::runtime_error("UpdateStrobeCommand.strobe must not be empty");
            }
        }

        if (command.position.has_value())
        {
            ValidateVec2(*command.position, "UpdateStrobeCommand.position");
        }
    }

    void operator()(const UpsertDynamicReticleCommand& command) const
    {
        if ((command.target.pageId == 0 && command.target.page.empty()) ||
            (command.target.runtimeReticleId == 0 && command.target.reticleId.empty()) ||
            (command.templateTransportId == 0 && command.templateId.empty()))
        {
            throw std::runtime_error(
                "UpsertDynamicReticleCommand requires page, target reticle and template identifiers");
        }

        ValidateReticlePatch(command.patch);
    }

    void operator()(const UpsertDynamicReticlesCommand& command) const
    {
        if ((command.pageId == 0 && command.page.empty()) ||
            (command.templateTransportId == 0 && command.templateId.empty()))
        {
            throw std::runtime_error(
                "UpsertDynamicReticlesCommand requires page and template identifiers");
        }

        ValidateContainerSize(
            command.reticles.size(),
            kMaxDynamicReticlesPerBatch,
            "UpsertDynamicReticlesCommand.reticles");
        for (const DynamicReticleState& state : command.reticles)
        {
            ValidateDynamicReticleState(state);
        }
    }

    void operator()(const SetDynamicReticleSetVisibilityCommand& command) const
    {
        if ((command.pageId == 0 && command.page.empty()) ||
            (command.templateTransportId == 0 && command.templateId.empty()))
        {
            throw std::runtime_error(
                "SetDynamicReticleSetVisibilityCommand requires page and template identifiers");
        }
    }

    void operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        if ((command.pageId == 0 && command.page.empty()) ||
            (command.templateTransportId == 0 && command.templateId.empty()))
        {
            throw std::runtime_error(
                "SetDynamicReticleSetStrobeMagnetEnabledCommand requires page and template identifiers");
        }
    }

    void operator()(const RemoveDynamicReticleCommand& command) const
    {
        if ((command.target.pageId == 0 && command.target.page.empty()) ||
            (command.target.runtimeReticleId == 0 && command.target.reticleId.empty()))
        {
            throw std::runtime_error("RemoveDynamicReticleCommand requires a target page and reticle");
        }
    }

    void operator()(const ResetWindowCommand&) const noexcept
    {
        // A window reset carries no payload: there is nothing to validate.
    }
};

void ValidateUserCommand(const UserCommand& command)
{
    std::visit(UserCommandValidator {}, command);
}

void ValidateCommandBatch(const CommandBatch& batch)
{
    ValidateContainerSize(batch.commands.size(), kMaxCommandsPerEnvelope, "CommandEnvelope.commands");
    if (batch.commands.empty())
    {
        throw std::runtime_error("Protocol Buffers command payload does not contain any command");
    }

    for (const UserCommand& command : batch.commands)
    {
        ValidateUserCommand(command);
    }

    if (batch.fragment.has_value())
    {
        const CommandBatchFragment& fragment = *batch.fragment;
        if (fragment.clientId == 0U || fragment.sessionEpoch == 0U || fragment.batchId == 0U ||
            fragment.chunkCount == 0U || fragment.chunkIndex >= fragment.chunkCount)
        {
            throw std::runtime_error("Command batch fragment metadata is incomplete or inconsistent");
        }
    }
}

std::uint32_t PackColor(const ColorRgba& color) noexcept
{
    return static_cast<std::uint32_t>(color.r) |
           (static_cast<std::uint32_t>(color.g) << 8U) |
           (static_cast<std::uint32_t>(color.b) << 16U) |
           (static_cast<std::uint32_t>(color.a) << 24U);
}

ColorRgba UnpackColor(const std::uint32_t packed) noexcept
{
    return ColorRgba {
        static_cast<std::uint8_t>(packed & 0xFFU),
        static_cast<std::uint8_t>((packed >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((packed >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((packed >> 24U) & 0xFFU)};
}

void FillProtoVec2(const Vec2& value, pb::Vec2* target)
{
    target->set_x(value.x);
    target->set_y(value.y);
}

// Trivially small proto conversions below stay by-value. Coordinate and time payloads are
// validated exactly once per command by ValidateUserCommand() after in-place deserialization.
Vec2 FromProtoVec2(const pb::Vec2& value)
{
    return Vec2 {value.x(), value.y()};
}

void FillProtoTimeValue(const TimeValue& value, pb::TimeValue* target)
{
    target->set_year(value.year);
    target->set_month(value.month);
    target->set_day(value.day);
    target->set_hour(value.hour);
    target->set_minute(value.minute);
    target->set_second(value.second);
}

TimeValue FromProtoTimeValue(const pb::TimeValue& value)
{
    TimeValue result;
    result.year = value.year();
    result.month = value.month();
    result.day = value.day();
    result.hour = value.hour();
    result.minute = value.minute();
    result.second = value.second();
    return result;
}

void FillProtoTimeFieldVisibility(const TimeFieldVisibility& value, pb::TimeFieldVisibility* target)
{
    target->set_year(value.year);
    target->set_month(value.month);
    target->set_day(value.day);
    target->set_hour(value.hour);
    target->set_minute(value.minute);
    target->set_second(value.second);
}

TimeFieldVisibility FromProtoTimeFieldVisibility(const pb::TimeFieldVisibility& value)
{
    TimeFieldVisibility result;
    result.year = value.year();
    result.month = value.month();
    result.day = value.day();
    result.hour = value.hour();
    result.minute = value.minute();
    result.second = value.second();
    return result;
}

pb::PrimitiveLineStyle ToProtoLineStyle(const LineStyle value) noexcept
{
    if (value == LineStyle::Dotted)
    {
        return pb::PRIMITIVE_LINE_STYLE_DOTTED;
    }

    if (value == LineStyle::Dashed)
    {
        return pb::PRIMITIVE_LINE_STYLE_DASHED;
    }

    return pb::PRIMITIVE_LINE_STYLE_SOLID;
}

LineStyle FromProtoLineStyle(const pb::PrimitiveLineStyle value)
{
    switch (value)
    {
    case pb::PRIMITIVE_LINE_STYLE_DOTTED:
        return LineStyle::Dotted;
    case pb::PRIMITIVE_LINE_STYLE_DASHED:
        return LineStyle::Dashed;
    case pb::PRIMITIVE_LINE_STYLE_SOLID:
        return LineStyle::Solid;
    case pb::PRIMITIVE_LINE_STYLE_UNSPECIFIED:
        throw std::runtime_error("PrimitivePatch.lineStyle cannot be explicitly set to UNSPECIFIED");
    default:
        throw std::runtime_error("PrimitivePatch.lineStyle enum value is unknown");
    }
}

void FillProtoPrimitivePatch(const PrimitivePatch& patch, pb::PrimitivePatch* target)
{
    if (patch.visible.has_value())
    {
        target->set_visible(*patch.visible);
    }

    if (patch.position.has_value())
    {
        FillProtoVec2(*patch.position, target->mutable_position());
    }

    if (patch.rotationDegrees.has_value())
    {
        target->set_rotation_degrees(*patch.rotationDegrees);
    }

    if (patch.scale.has_value())
    {
        FillProtoVec2(*patch.scale, target->mutable_scale());
    }

    if (patch.color.has_value())
    {
        target->set_packed_rgba(PackColor(*patch.color));
    }

    if (patch.fillColor.has_value())
    {
        target->set_fill_packed_rgba(PackColor(*patch.fillColor));
    }

    if (patch.filled.has_value())
    {
        target->set_filled(*patch.filled);
    }

    if (patch.thickness.has_value())
    {
        target->set_thickness(*patch.thickness);
    }

    if (patch.lineStyle.has_value())
    {
        target->set_line_style(ToProtoLineStyle(*patch.lineStyle));
    }

    if (patch.text.has_value())
    {
        target->set_text(*patch.text);
    }

    if (patch.letterSpacing.has_value())
    {
        target->set_letter_spacing(*patch.letterSpacing);
    }

    if (patch.lineStart.has_value())
    {
        FillProtoVec2(*patch.lineStart, target->mutable_line_start());
    }

    if (patch.lineEnd.has_value())
    {
        FillProtoVec2(*patch.lineEnd, target->mutable_line_end());
    }

    if (patch.radius.has_value())
    {
        target->set_radius(*patch.radius);
    }

    if (patch.innerRadius.has_value())
    {
        target->set_inner_radius(*patch.innerRadius);
    }

    if (patch.outerRadius.has_value())
    {
        target->set_outer_radius(*patch.outerRadius);
    }

    if (patch.width.has_value())
    {
        target->set_width(*patch.width);
    }

    if (patch.height.has_value())
    {
        target->set_height(*patch.height);
    }

    if (patch.size.has_value())
    {
        FillProtoVec2(*patch.size, target->mutable_size());
    }

    if (patch.points.has_value())
    {
        for (const Vec2& point : *patch.points)
        {
            FillProtoVec2(point, target->add_points());
        }
    }

    if (patch.closed.has_value())
    {
        target->set_closed(*patch.closed);
    }

    if (patch.segments.has_value())
    {
        target->set_segments(*patch.segments);
    }

    if (patch.startAngleDegrees.has_value())
    {
        target->set_start_angle_degrees(*patch.startAngleDegrees);
    }

    if (patch.endAngleDegrees.has_value())
    {
        target->set_end_angle_degrees(*patch.endAngleDegrees);
    }

    if (patch.timeValue.has_value())
    {
        FillProtoTimeValue(*patch.timeValue, target->mutable_time_value());
    }

    if (patch.clearTimeValue)
    {
        target->set_clear_time_value(true);
    }

    if (patch.timeUtc.has_value())
    {
        target->set_time_utc(*patch.timeUtc);
    }

    if (patch.timeFields.has_value())
    {
        FillProtoTimeFieldVisibility(*patch.timeFields, target->mutable_time_fields());
    }
}

void FillPrimitivePatchFromProto(const pb::PrimitivePatch& value, PrimitivePatch& patch)
{
    if (value.has_visible())
    {
        patch.visible = value.visible();
    }

    if (value.has_position())
    {
        patch.position = FromProtoVec2(value.position());
    }

    if (value.has_rotation_degrees())
    {
        patch.rotationDegrees = value.rotation_degrees();
    }

    if (value.has_scale())
    {
        patch.scale = FromProtoVec2(value.scale());
    }

    if (value.has_packed_rgba())
    {
        patch.color = UnpackColor(value.packed_rgba());
    }

    if (value.has_fill_packed_rgba())
    {
        patch.fillColor = UnpackColor(value.fill_packed_rgba());
    }

    if (value.has_filled())
    {
        patch.filled = value.filled();
    }

    if (value.has_thickness())
    {
        patch.thickness = value.thickness();
    }

    if (value.has_line_style())
    {
        patch.lineStyle = FromProtoLineStyle(value.line_style());
    }

    if (value.has_text())
    {
        patch.text = value.text();
    }

    if (value.has_letter_spacing())
    {
        patch.letterSpacing = value.letter_spacing();
    }

    if (value.has_line_start())
    {
        patch.lineStart = FromProtoVec2(value.line_start());
    }

    if (value.has_line_end())
    {
        patch.lineEnd = FromProtoVec2(value.line_end());
    }

    if (value.has_radius())
    {
        patch.radius = value.radius();
    }

    if (value.has_inner_radius())
    {
        patch.innerRadius = value.inner_radius();
    }

    if (value.has_outer_radius())
    {
        patch.outerRadius = value.outer_radius();
    }

    if (value.has_width())
    {
        patch.width = value.width();
    }

    if (value.has_height())
    {
        patch.height = value.height();
    }

    if (value.has_size())
    {
        patch.size = FromProtoVec2(value.size());
    }

    if (value.points_size() > 0)
    {
        ValidateContainerSize(
            static_cast<std::size_t>(value.points_size()),
            kMaxPrimitivePoints,
            "Protocol Buffers PrimitivePatch.points");
        patch.points.emplace();
        patch.points->reserve(static_cast<std::size_t>(value.points_size()));
        for (const pb::Vec2& point : value.points())
        {
            patch.points->push_back(FromProtoVec2(point));
        }
    }

    if (value.has_closed())
    {
        patch.closed = value.closed();
    }

    if (value.has_segments())
    {
        patch.segments = value.segments();
    }

    if (value.has_start_angle_degrees())
    {
        patch.startAngleDegrees = value.start_angle_degrees();
    }

    if (value.has_end_angle_degrees())
    {
        patch.endAngleDegrees = value.end_angle_degrees();
    }

    if (value.has_time_value())
    {
        patch.timeValue = FromProtoTimeValue(value.time_value());
    }

    if (value.has_clear_time_value())
    {
        patch.clearTimeValue = value.clear_time_value();
    }

    if (value.has_time_utc())
    {
        patch.timeUtc = value.time_utc();
    }

    if (value.has_time_fields())
    {
        patch.timeFields = FromProtoTimeFieldVisibility(value.time_fields());
    }
}

void FillProtoWindowDisplayPatch(const WindowDisplayPatch& patch, pb::WindowDisplayPatch* target)
{
    if (patch.invertColors.has_value())
    {
        target->set_invert_colors(*patch.invertColors);
    }

    if (patch.brightness.has_value())
    {
        target->set_brightness(*patch.brightness);
    }

    if (patch.disabled.has_value())
    {
        target->set_disabled(*patch.disabled);
    }
}

void FillWindowDisplayPatchFromProto(const pb::WindowDisplayPatch& value, WindowDisplayPatch& patch)
{
    if (value.has_invert_colors())
    {
        patch.invertColors = value.invert_colors();
    }

    if (value.has_brightness())
    {
        patch.brightness = value.brightness();
    }

    if (value.has_disabled())
    {
        patch.disabled = value.disabled();
    }
}

void FillProtoReticlePatch(const ReticlePatch& patch, pb::ReticlePatch* target)
{
    if (patch.visible.has_value())
    {
        target->set_visible(*patch.visible);
    }

    if (patch.blinkEnabled.has_value())
    {
        target->set_blink_enabled(*patch.blinkEnabled);
    }

    if (patch.blinkType.has_value() && !patch.blinkTypeId.has_value())
    {
        throw std::runtime_error("ReticlePatch serialization requires blinkTypeId when blinkType is set");
    }

    if (patch.blinkTypeId.has_value())
    {
        target->set_blink_type_id(*patch.blinkTypeId);
    }

    if (patch.position.has_value())
    {
        FillProtoVec2(*patch.position, target->mutable_position());
    }

    if (patch.rotationDegrees.has_value())
    {
        target->set_rotation_degrees(*patch.rotationDegrees);
    }

    if (patch.scale.has_value())
    {
        FillProtoVec2(*patch.scale, target->mutable_scale());
    }

    if (patch.color.has_value())
    {
        target->set_packed_rgba(PackColor(*patch.color));
    }

    if (patch.thickness.has_value())
    {
        target->set_thickness(*patch.thickness);
    }

    if (patch.text.has_value())
    {
        target->set_text(*patch.text);
    }

    if (!patch.texts.empty())
    {
        throw std::runtime_error("ReticlePatch serialization requires textsById instead of named texts");
    }

    for (const auto& [primitiveId, text] : patch.textsById)
    {
        (*target->mutable_texts_by_id())[primitiveId] = text;
    }

    if (patch.letterSpacing.has_value())
    {
        target->set_letter_spacing(*patch.letterSpacing);
    }

    if (!patch.letterSpacings.empty())
    {
        throw std::runtime_error(
            "ReticlePatch serialization requires letterSpacingsById instead of named letterSpacings");
    }

    for (const auto& [primitiveId, spacing] : patch.letterSpacingsById)
    {
        (*target->mutable_letter_spacings_by_id())[primitiveId] = spacing;
    }

    if (!patch.primitivePatches.empty())
    {
        throw std::runtime_error(
            "ReticlePatch serialization requires primitivePatchesById instead of named primitivePatches");
    }

    for (const auto& [primitiveId, primitivePatch] : patch.primitivePatchesById)
    {
        FillProtoPrimitivePatch(primitivePatch, &(*target->mutable_primitive_patches_by_id())[primitiveId]);
    }
}

void FillReticlePatchFromProto(const pb::ReticlePatch& value, ReticlePatch& patch)
{
    if (value.has_visible())
    {
        patch.visible = value.visible();
    }

    if (value.has_blink_enabled())
    {
        patch.blinkEnabled = value.blink_enabled();
    }

    if (value.has_blink_type_id())
    {
        patch.blinkTypeId = value.blink_type_id();
    }

    if (value.has_position())
    {
        patch.position = FromProtoVec2(value.position());
    }

    if (value.has_rotation_degrees())
    {
        patch.rotationDegrees = value.rotation_degrees();
    }

    if (value.has_scale())
    {
        patch.scale = FromProtoVec2(value.scale());
    }

    if (value.has_packed_rgba())
    {
        patch.color = UnpackColor(value.packed_rgba());
    }

    if (value.has_thickness())
    {
        patch.thickness = value.thickness();
    }

    if (value.has_text())
    {
        patch.text = value.text();
    }

    ValidateContainerSize(value.texts_by_id().size(), kMaxPatchEntryCount, "Protocol Buffers ReticlePatch.textsById");
    patch.textsById.reserve(static_cast<std::size_t>(value.texts_by_id().size()));
    for (const auto& [primitiveId, text] : value.texts_by_id())
    {
        patch.textsById.emplace(primitiveId, text);
    }

    if (value.has_letter_spacing())
    {
        patch.letterSpacing = value.letter_spacing();
    }

    ValidateContainerSize(
        value.letter_spacings_by_id().size(),
        kMaxPatchEntryCount,
        "Protocol Buffers ReticlePatch.letterSpacingsById");
    patch.letterSpacingsById.reserve(static_cast<std::size_t>(value.letter_spacings_by_id().size()));
    for (const auto& [primitiveId, spacing] : value.letter_spacings_by_id())
    {
        patch.letterSpacingsById.emplace(primitiveId, spacing);
    }

    ValidateContainerSize(
        value.primitive_patches_by_id().size(),
        kMaxPatchEntryCount,
        "Protocol Buffers ReticlePatch.primitivePatchesById");
    patch.primitivePatchesById.reserve(static_cast<std::size_t>(value.primitive_patches_by_id().size()));
    for (const auto& [primitiveId, primitivePatch] : value.primitive_patches_by_id())
    {
        PrimitivePatch& primitiveTarget = patch.primitivePatchesById.try_emplace(primitiveId).first->second;
        FillPrimitivePatchFromProto(primitivePatch, primitiveTarget);
    }
}

void FillProtoStaticHandle(const StaticReticleHandle& handle, pb::StaticReticleHandle* target)
{
    if (handle.pageId == 0 || handle.reticleId == 0)
    {
        throw std::runtime_error("Static reticle serialization requires generated pageId and reticleId");
    }

    target->set_page_id(handle.pageId);
    target->set_reticle_id(handle.reticleId);
}

StaticReticleHandle FromProtoStaticHandle(const pb::StaticReticleHandle& value)
{
    StaticReticleHandle handle;
    handle.pageId = value.page_id();
    handle.reticleId = value.reticle_id();
    return handle;
}

void FillProtoDynamicHandle(const DynamicReticleHandle& handle, pb::DynamicReticleHandle* target)
{
    if (handle.pageId == 0 || handle.runtimeReticleId == 0)
    {
        throw std::runtime_error("Dynamic reticle serialization requires generated pageId and runtimeReticleId");
    }

    target->set_page_id(handle.pageId);
    target->set_runtime_reticle_id(handle.runtimeReticleId);
}

DynamicReticleHandle FromProtoDynamicHandle(const pb::DynamicReticleHandle& value)
{
    DynamicReticleHandle handle;
    handle.pageId = value.page_id();
    handle.runtimeReticleId = value.runtime_reticle_id();
    return handle;
}

void FillProtoDynamicReticleState(const DynamicReticleState& state, pb::DynamicReticleState* target)
{
    if (state.runtimeReticleId == 0)
    {
        throw std::runtime_error("Dynamic reticle serialization requires runtimeReticleId");
    }

    target->set_runtime_reticle_id(state.runtimeReticleId);
    FillProtoReticlePatch(state.patch, target->mutable_patch());
}

void FillDynamicReticleStateFromProto(const pb::DynamicReticleState& value, DynamicReticleState& state)
{
    state.runtimeReticleId = value.runtime_reticle_id();
    if (value.has_patch())
    {
        FillReticlePatchFromProto(value.patch(), state.patch);
    }
}

// Serializes one typed command into its proto message with one explicit overload per command
// type. This visitor intentionally has no generic fallback: adding a new UserCommand
// alternative must fail to compile here until its wire mapping is written.
struct UserCommandProtoWriter
{
    pb::UserCommand* target = nullptr;

    void operator()(const ActivatePageCommand& command) const
    {
        auto* message = target->mutable_activate_page();
        if (command.pageId == 0)
        {
            throw std::runtime_error("ActivatePageCommand serialization requires generated pageId");
        }

        message->set_page_id(command.pageId);
    }

    void operator()(const SetPageViewCommand& command) const
    {
        auto* message = target->mutable_set_page_view();
        FillProtoVec2(command.view.center, message->mutable_center());
        message->set_zoom(command.view.zoom);
        if (command.pageId == 0)
        {
            throw std::runtime_error("SetPageViewCommand serialization requires generated pageId");
        }

        message->set_page_id(command.pageId);
    }

    void operator()(const UpdateWindowDisplayCommand& command) const
    {
        FillProtoWindowDisplayPatch(command.patch, target->mutable_update_window_display()->mutable_patch());
    }

    void operator()(const UpdateReticleCommand& command) const
    {
        auto* message = target->mutable_update_reticle();
        FillProtoStaticHandle(command.target, message->mutable_target());
        FillProtoReticlePatch(command.patch, message->mutable_patch());
    }

    void operator()(const UpdateStrobeCommand& command) const
    {
        auto* message = target->mutable_update_strobe();
        if (command.pageId == 0)
        {
            throw std::runtime_error("UpdateStrobeCommand serialization requires generated pageId");
        }

        message->set_page_id(command.pageId);

        if (command.active.has_value())
        {
            message->set_active(*command.active);
        }

        if (command.position.has_value())
        {
            FillProtoVec2(*command.position, message->mutable_position());
        }

        if (command.strobeId != 0)
        {
            message->set_strobe_id(command.strobeId);
        }
    }

    void operator()(const UpsertDynamicReticleCommand& command) const
    {
        auto* message = target->mutable_upsert_dynamic_reticle();
        FillProtoDynamicHandle(command.target, message->mutable_target());
        FillProtoReticlePatch(command.patch, message->mutable_patch());
        if (command.templateTransportId == 0)
        {
            throw std::runtime_error(
                "UpsertDynamicReticleCommand serialization requires templateTransportId");
        }

        message->set_template_transport_id(command.templateTransportId);
    }

    void operator()(const UpsertDynamicReticlesCommand& command) const
    {
        auto* message = target->mutable_upsert_dynamic_reticles();
        if (command.pageId == 0)
        {
            throw std::runtime_error(
                "UpsertDynamicReticlesCommand serialization requires generated pageId");
        }

        if (command.templateTransportId == 0)
        {
            throw std::runtime_error(
                "UpsertDynamicReticlesCommand serialization requires templateTransportId");
        }

        message->set_page_id(command.pageId);
        message->set_template_transport_id(command.templateTransportId);

        for (const DynamicReticleState& reticle : command.reticles)
        {
            FillProtoDynamicReticleState(reticle, message->add_reticles());
        }
    }

    void operator()(const SetDynamicReticleSetVisibilityCommand& command) const
    {
        auto* message = target->mutable_set_dynamic_reticle_set_visibility();
        message->set_visible(command.visible);
        if (command.pageId == 0)
        {
            throw std::runtime_error(
                "SetDynamicReticleSetVisibilityCommand serialization requires generated pageId");
        }

        if (command.templateTransportId == 0)
        {
            throw std::runtime_error(
                "SetDynamicReticleSetVisibilityCommand serialization requires templateTransportId");
        }

        message->set_page_id(command.pageId);
        message->set_template_transport_id(command.templateTransportId);
    }

    void operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        auto* message = target->mutable_set_dynamic_reticle_set_strobe_magnet_enabled();
        message->set_enabled(command.enabled);
        if (command.pageId == 0)
        {
            throw std::runtime_error(
                "SetDynamicReticleSetStrobeMagnetEnabledCommand serialization requires generated pageId");
        }

        if (command.templateTransportId == 0)
        {
            throw std::runtime_error(
                "SetDynamicReticleSetStrobeMagnetEnabledCommand serialization requires templateTransportId");
        }

        message->set_page_id(command.pageId);
        message->set_template_transport_id(command.templateTransportId);
    }

    void operator()(const RemoveDynamicReticleCommand& command) const
    {
        FillProtoDynamicHandle(command.target, target->mutable_remove_dynamic_reticle()->mutable_target());
    }

    void operator()(const ResetWindowCommand&) const
    {
        target->mutable_reset_window();
    }
};

void FillProtoUserCommand(const UserCommand& command, pb::UserCommand* target)
{
    std::visit(UserCommandProtoWriter {target}, command);
}

// Deserializes one proto command directly into a new element of the target vector, without
// building command temporaries. Appends exactly one command or throws without appending.
void AppendParsedUserCommandFromProto(const pb::UserCommand& value, std::vector<UserCommand>& target)
{
    switch (value.command_case())
    {
    case pb::UserCommand::kActivatePage:
    {
        auto& command = std::get<ActivatePageCommand>(target.emplace_back(std::in_place_type<ActivatePageCommand>));
        command.pageId = value.activate_page().page_id();
        return;
    }

    case pb::UserCommand::kSetPageView:
    {
        auto& command = std::get<SetPageViewCommand>(target.emplace_back(std::in_place_type<SetPageViewCommand>));
        if (value.set_page_view().has_center())
        {
            command.view.center = FromProtoVec2(value.set_page_view().center());
        }
        command.view.zoom = value.set_page_view().zoom();
        command.pageId = value.set_page_view().page_id();
        return;
    }

    case pb::UserCommand::kUpdateWindowDisplay:
    {
        auto& command = std::get<UpdateWindowDisplayCommand>(
            target.emplace_back(std::in_place_type<UpdateWindowDisplayCommand>));
        if (value.update_window_display().has_patch())
        {
            FillWindowDisplayPatchFromProto(value.update_window_display().patch(), command.patch);
        }
        return;
    }

    case pb::UserCommand::kUpdateReticle:
    {
        auto& command = std::get<UpdateReticleCommand>(target.emplace_back(std::in_place_type<UpdateReticleCommand>));
        command.target = FromProtoStaticHandle(value.update_reticle().target());
        if (value.update_reticle().has_patch())
        {
            FillReticlePatchFromProto(value.update_reticle().patch(), command.patch);
        }
        return;
    }

    case pb::UserCommand::kUpdateStrobe:
    {
        auto& command = std::get<UpdateStrobeCommand>(target.emplace_back(std::in_place_type<UpdateStrobeCommand>));
        command.pageId = value.update_strobe().page_id();
        command.strobeId = value.update_strobe().strobe_id();
        if (value.update_strobe().has_active())
        {
            command.active = value.update_strobe().active();
        }
        if (value.update_strobe().has_position())
        {
            command.position = FromProtoVec2(value.update_strobe().position());
        }
        return;
    }

    case pb::UserCommand::kUpsertDynamicReticle:
    {
        auto& command = std::get<UpsertDynamicReticleCommand>(
            target.emplace_back(std::in_place_type<UpsertDynamicReticleCommand>));
        command.target = FromProtoDynamicHandle(value.upsert_dynamic_reticle().target());
        command.templateTransportId = value.upsert_dynamic_reticle().template_transport_id();
        if (value.upsert_dynamic_reticle().has_patch())
        {
            FillReticlePatchFromProto(value.upsert_dynamic_reticle().patch(), command.patch);
        }
        return;
    }

    case pb::UserCommand::kUpsertDynamicReticles:
    {
        auto& command = std::get<UpsertDynamicReticlesCommand>(
            target.emplace_back(std::in_place_type<UpsertDynamicReticlesCommand>));
        command.pageId = value.upsert_dynamic_reticles().page_id();
        command.templateTransportId = value.upsert_dynamic_reticles().template_transport_id();
        ValidateContainerSize(
            static_cast<std::size_t>(value.upsert_dynamic_reticles().reticles_size()),
            kMaxDynamicReticlesPerBatch,
            "Protocol Buffers UpsertDynamicReticlesCommand.reticles");
        command.reticles.reserve(static_cast<std::size_t>(value.upsert_dynamic_reticles().reticles_size()));

        for (const pb::DynamicReticleState& reticle : value.upsert_dynamic_reticles().reticles())
        {
            DynamicReticleState& state = command.reticles.emplace_back();
            FillDynamicReticleStateFromProto(reticle, state);
        }

        return;
    }

    case pb::UserCommand::kSetDynamicReticleSetVisibility:
    {
        auto& command = std::get<SetDynamicReticleSetVisibilityCommand>(
            target.emplace_back(std::in_place_type<SetDynamicReticleSetVisibilityCommand>));
        command.visible = value.set_dynamic_reticle_set_visibility().visible();
        command.pageId = value.set_dynamic_reticle_set_visibility().page_id();
        command.templateTransportId = value.set_dynamic_reticle_set_visibility().template_transport_id();
        return;
    }

    case pb::UserCommand::kSetDynamicReticleSetStrobeMagnetEnabled:
    {
        auto& command = std::get<SetDynamicReticleSetStrobeMagnetEnabledCommand>(
            target.emplace_back(std::in_place_type<SetDynamicReticleSetStrobeMagnetEnabledCommand>));
        command.enabled = value.set_dynamic_reticle_set_strobe_magnet_enabled().enabled();
        command.pageId = value.set_dynamic_reticle_set_strobe_magnet_enabled().page_id();
        command.templateTransportId = value.set_dynamic_reticle_set_strobe_magnet_enabled().template_transport_id();
        return;
    }

    case pb::UserCommand::kRemoveDynamicReticle:
    {
        auto& command = std::get<RemoveDynamicReticleCommand>(
            target.emplace_back(std::in_place_type<RemoveDynamicReticleCommand>));
        command.target = FromProtoDynamicHandle(value.remove_dynamic_reticle().target());
        return;
    }

    case pb::UserCommand::kResetWindow:
        target.emplace_back(std::in_place_type<ResetWindowCommand>);
        return;

    case pb::UserCommand::COMMAND_NOT_SET:
        break;
    }

    throw std::runtime_error("Protocol Buffers command payload is empty");
}

void AppendUserCommandFromProto(const pb::UserCommand& value, std::vector<UserCommand>& target)
{
    AppendParsedUserCommandFromProto(value, target);
    ValidateUserCommand(target.back());
}

pb::CommandEnvelope BuildEnvelope(const CommandBatch& batch)
{
    pb::CommandEnvelope envelope;
    envelope.set_sequence(batch.sequence);
    envelope.set_mapping_hash(batch.mappingHash);
    if (batch.fragment.has_value())
    {
        const CommandBatchFragment& fragment = *batch.fragment;
        envelope.set_client_id(fragment.clientId);
        envelope.set_session_epoch(fragment.sessionEpoch);
        envelope.set_batch_id(fragment.batchId);
        envelope.set_chunk_index(fragment.chunkIndex);
        envelope.set_chunk_count(fragment.chunkCount);
    }

    for (const UserCommand& command : batch.commands)
    {
        FillProtoUserCommand(command, envelope.add_commands());
    }

    return envelope;
}
} // namespace

std::string SerializeUserCommand(const UserCommand& command)
{
    CommandBatch batch;
    batch.commands.push_back(command);
    return SerializeCommandBatch(batch);
}

std::string SerializeCommandBatch(const CommandBatch& batch)
{
    ValidateCommandBatch(batch);
    const pb::CommandEnvelope envelope = BuildEnvelope(batch);
    std::string payload;
    if (!envelope.SerializeToString(&payload))
    {
        throw std::runtime_error("Unable to serialize Protocol Buffers command payload");
    }

    return payload;
}

std::optional<UserCommand> DeserializeUserCommand(const std::string_view payload, std::string* error)
{
    const auto batch = DeserializeCommandBatch(payload, error);
    if (!batch.has_value())
    {
        return std::nullopt;
    }

    if (batch->commands.size() != 1U)
    {
        if (error != nullptr)
        {
            *error = "Command payload contains more than one command";
        }
        return std::nullopt;
    }

    return batch->commands.front();
}

std::optional<CommandBatch> DeserializeCommandBatch(const std::string_view payload, std::string* error)
{
    if (payload.empty())
    {
        if (error != nullptr)
        {
            *error = "Protocol Buffers command payload is empty";
        }

        return std::nullopt;
    }

    if (payload.size() > kMaxCommandPayloadBytes)
    {
        if (error != nullptr)
        {
            *error = "Protocol Buffers command payload exceeds safety size limit";
        }

        return std::nullopt;
    }

    try
    {
        pb::CommandEnvelope envelope;
        if (!envelope.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
        {
            throw std::runtime_error("Unable to parse Protocol Buffers command payload");
        }

        CommandBatch batch;
        batch.sequence = envelope.sequence();
        batch.mappingHash = envelope.mapping_hash();
        const bool hasFragmentMetadata = envelope.client_id() != 0U || envelope.session_epoch() != 0U ||
                                         envelope.batch_id() != 0U || envelope.chunk_index() != 0U ||
                                         envelope.chunk_count() != 0U;
        if (hasFragmentMetadata)
        {
            batch.fragment = CommandBatchFragment {
                envelope.client_id(),
                envelope.session_epoch(),
                envelope.batch_id(),
                envelope.chunk_index(),
                envelope.chunk_count()};
        }
        if (envelope.commands_size() == 0)
        {
            throw std::runtime_error("Protocol Buffers command payload is empty");
        }

        ValidateContainerSize(
            static_cast<std::size_t>(envelope.commands_size()),
            kMaxCommandsPerEnvelope,
            "Protocol Buffers CommandEnvelope.commands");
        batch.commands.reserve(static_cast<std::size_t>(envelope.commands_size()));

        for (const pb::UserCommand& command : envelope.commands())
        {
            AppendUserCommandFromProto(command, batch.commands);
        }

        ValidateCommandBatch(batch);

        return batch;
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

std::optional<std::vector<UserCommand>> DeserializeUserCommands(const std::string_view payload, std::string* error)
{
    const auto batch = DeserializeCommandBatch(payload, error);
    if (!batch.has_value())
    {
        return std::nullopt;
    }

    return batch->commands;
}

std::size_t detail::DynamicReticleStateWireSize(const DynamicReticleState& state)
{
    pb::DynamicReticleState message;
    FillProtoDynamicReticleState(state, &message);
    return message.ByteSizeLong();
}
} // namespace mfd
