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
#include <type_traits>
#include <vector>

#include "mfd_commands.pb.h"
#include "mfd/model/PageName.h"
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

template <typename>
struct AlwaysFalse : std::false_type
{
};

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

void ValidateUserCommand(const UserCommand& command)
{
    std::visit(
        [](const auto& value)
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand>)
            {
                if (value.pageId == 0 && value.page.empty())
                {
                    throw std::runtime_error("ActivatePageCommand requires a pageId or page name");
                }
            }
            else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
            {
                if (value.pageId == 0 && value.page.empty())
                {
                    throw std::runtime_error("SetPageViewCommand requires a pageId or page name");
                }

                ValidatePageViewState(value.view);
            }
            else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand>)
            {
                ValidateWindowDisplayPatch(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                if ((value.target.pageId == 0 && value.target.page.empty()) ||
                    (value.target.reticleId == 0 && value.target.reticle.empty()))
                {
                    throw std::runtime_error("UpdateReticleCommand requires a target page and reticle");
                }

                ValidateReticlePatch(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                if (value.pageId == 0 && value.page.empty())
                {
                    throw std::runtime_error("UpdateStrobeCommand requires a pageId or page name");
                }

                if (value.strobeId == 0 && !value.strobe.empty())
                {
                    if (NormalizePageName(value.strobe).empty())
                    {
                        throw std::runtime_error("UpdateStrobeCommand.strobe must not be empty");
                    }
                }

                if (value.position.has_value())
                {
                    ValidateVec2(*value.position, "UpdateStrobeCommand.position");
                }
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                if ((value.target.pageId == 0 && value.target.page.empty()) ||
                    (value.target.runtimeReticleId == 0 && value.target.reticleId.empty()) ||
                    (value.templateTransportId == 0 && value.templateId.empty()))
                {
                    throw std::runtime_error(
                        "UpsertDynamicReticleCommand requires page, target reticle and template identifiers");
                }

                ValidateReticlePatch(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if ((value.pageId == 0 && value.page.empty()) ||
                    (value.templateTransportId == 0 && value.templateId.empty()))
                {
                    throw std::runtime_error(
                        "UpsertDynamicReticlesCommand requires page and template identifiers");
                }

                ValidateContainerSize(
                    value.reticles.size(),
                    kMaxDynamicReticlesPerBatch,
                    "UpsertDynamicReticlesCommand.reticles");
                for (const DynamicReticleState& state : value.reticles)
                {
                    ValidateDynamicReticleState(state);
                }
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                if ((value.pageId == 0 && value.page.empty()) ||
                    (value.templateTransportId == 0 && value.templateId.empty()))
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetVisibilityCommand requires page and template identifiers");
                }
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetStrobeMagnetEnabledCommand>)
            {
                if ((value.pageId == 0 && value.page.empty()) ||
                    (value.templateTransportId == 0 && value.templateId.empty()))
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetStrobeMagnetEnabledCommand requires page and template identifiers");
                }
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                if ((value.target.pageId == 0 && value.target.page.empty()) ||
                    (value.target.runtimeReticleId == 0 && value.target.reticleId.empty()))
                {
                    throw std::runtime_error("RemoveDynamicReticleCommand requires a target page and reticle");
                }
            }
        },
        command);
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

Vec2 FromProtoVec2(const pb::Vec2& value)
{
    const Vec2 result {value.x(), value.y()};
    ValidateVec2(result, "Protocol Buffers Vec2");
    return result;
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
    ValidateTimeValue(result, "Protocol Buffers TimeValue");
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
    ValidateTimeFieldVisibility(result, "Protocol Buffers TimeFieldVisibility");
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

PrimitivePatch FromProtoPrimitivePatch(const pb::PrimitivePatch& value)
{
    PrimitivePatch patch;

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
        if (value.line_style() == pb::PRIMITIVE_LINE_STYLE_UNSPECIFIED)
        {
            throw std::runtime_error("PrimitivePatch.lineStyle cannot be explicitly set to UNSPECIFIED");
        }

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
        std::vector<Vec2> points;
        points.reserve(static_cast<std::size_t>(value.points_size()));
        for (const pb::Vec2& point : value.points())
        {
            points.push_back(FromProtoVec2(point));
        }
        patch.points = std::move(points);
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

    ValidatePrimitivePatch(patch);
    return patch;
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

WindowDisplayPatch FromProtoWindowDisplayPatch(const pb::WindowDisplayPatch& value)
{
    WindowDisplayPatch patch;

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

    ValidateWindowDisplayPatch(patch);
    return patch;
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

ReticlePatch FromProtoReticlePatch(const pb::ReticlePatch& value)
{
    ReticlePatch patch;

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
    for (const auto& [primitiveId, spacing] : value.letter_spacings_by_id())
    {
        patch.letterSpacingsById.emplace(primitiveId, spacing);
    }

    ValidateContainerSize(
        value.primitive_patches_by_id().size(),
        kMaxPatchEntryCount,
        "Protocol Buffers ReticlePatch.primitivePatchesById");
    for (const auto& [primitiveId, primitivePatch] : value.primitive_patches_by_id())
    {
        patch.primitivePatchesById.emplace(primitiveId, FromProtoPrimitivePatch(primitivePatch));
    }

    ValidateReticlePatch(patch);
    return patch;
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

DynamicReticleState FromProtoDynamicReticleState(const pb::DynamicReticleState& value)
{
    DynamicReticleState state;
    state.runtimeReticleId = value.runtime_reticle_id();
    if (value.has_patch())
    {
        state.patch = FromProtoReticlePatch(value.patch());
    }
    ValidateDynamicReticleState(state);
    return state;
}

void FillProtoUserCommand(const UserCommand& command, pb::UserCommand* target)
{
    std::visit(
        [target](const auto& value)
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand>)
            {
                auto* message = target->mutable_activate_page();
                if (value.pageId == 0)
                {
                    throw std::runtime_error("ActivatePageCommand serialization requires generated pageId");
                }

                message->set_page_id(value.pageId);
            }
            else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
            {
                auto* message = target->mutable_set_page_view();
                FillProtoVec2(value.view.center, message->mutable_center());
                message->set_zoom(value.view.zoom);
                if (value.pageId == 0)
                {
                    throw std::runtime_error("SetPageViewCommand serialization requires generated pageId");
                }

                message->set_page_id(value.pageId);
            }
            else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand>)
            {
                FillProtoWindowDisplayPatch(value.patch, target->mutable_update_window_display()->mutable_patch());
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                auto* message = target->mutable_update_reticle();
                FillProtoStaticHandle(value.target, message->mutable_target());
                FillProtoReticlePatch(value.patch, message->mutable_patch());
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                auto* message = target->mutable_update_strobe();
                if (value.pageId == 0)
                {
                    throw std::runtime_error("UpdateStrobeCommand serialization requires generated pageId");
                }

                message->set_page_id(value.pageId);

                if (value.active.has_value())
                {
                    message->set_active(*value.active);
                }

                if (value.position.has_value())
                {
                    FillProtoVec2(*value.position, message->mutable_position());
                }

                if (value.strobeId != 0)
                {
                    message->set_strobe_id(value.strobeId);
                }
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                auto* message = target->mutable_upsert_dynamic_reticle();
                FillProtoDynamicHandle(value.target, message->mutable_target());
                FillProtoReticlePatch(value.patch, message->mutable_patch());
                if (value.templateTransportId == 0)
                {
                    throw std::runtime_error(
                        "UpsertDynamicReticleCommand serialization requires templateTransportId");
                }

                message->set_template_transport_id(value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                auto* message = target->mutable_upsert_dynamic_reticles();
                if (value.pageId == 0)
                {
                    throw std::runtime_error(
                        "UpsertDynamicReticlesCommand serialization requires generated pageId");
                }

                if (value.templateTransportId == 0)
                {
                    throw std::runtime_error(
                        "UpsertDynamicReticlesCommand serialization requires templateTransportId");
                }

                message->set_page_id(value.pageId);
                message->set_template_transport_id(value.templateTransportId);

                for (const DynamicReticleState& reticle : value.reticles)
                {
                    FillProtoDynamicReticleState(reticle, message->add_reticles());
                }
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                auto* message = target->mutable_set_dynamic_reticle_set_visibility();
                message->set_visible(value.visible);
                if (value.pageId == 0)
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetVisibilityCommand serialization requires generated pageId");
                }

                if (value.templateTransportId == 0)
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetVisibilityCommand serialization requires templateTransportId");
                }

                message->set_page_id(value.pageId);
                message->set_template_transport_id(value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetStrobeMagnetEnabledCommand>)
            {
                auto* message = target->mutable_set_dynamic_reticle_set_strobe_magnet_enabled();
                message->set_enabled(value.enabled);
                if (value.pageId == 0)
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetStrobeMagnetEnabledCommand serialization requires generated pageId");
                }

                if (value.templateTransportId == 0)
                {
                    throw std::runtime_error(
                        "SetDynamicReticleSetStrobeMagnetEnabledCommand serialization requires templateTransportId");
                }

                message->set_page_id(value.pageId);
                message->set_template_transport_id(value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                FillProtoDynamicHandle(value.target, target->mutable_remove_dynamic_reticle()->mutable_target());
            }
            else if constexpr (std::is_same_v<Command, ResetWindowCommand>)
            {
                target->mutable_reset_window();
            }
            else
            {
                static_assert(AlwaysFalse<Command>::value, "Unsupported user command type");
            }
        },
        command);
}

UserCommand FromProtoUserCommand(const pb::UserCommand& value)
{
    switch (value.command_case())
    {
    case pb::UserCommand::kActivatePage:
    {
        ActivatePageCommand command;
        command.pageId = value.activate_page().page_id();
        return command;
    }

    case pb::UserCommand::kSetPageView:
    {
        SetPageViewCommand command;
        if (value.set_page_view().has_center())
        {
            command.view.center = FromProtoVec2(value.set_page_view().center());
        }
        command.view.zoom = value.set_page_view().zoom();
        command.pageId = value.set_page_view().page_id();
        ValidatePageViewState(command.view);
        return command;
    }

    case pb::UserCommand::kUpdateWindowDisplay:
    {
        UpdateWindowDisplayCommand command;
        if (value.update_window_display().has_patch())
        {
            command.patch = FromProtoWindowDisplayPatch(value.update_window_display().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpdateReticle:
    {
        UpdateReticleCommand command;
        command.target = FromProtoStaticHandle(value.update_reticle().target());
        if (value.update_reticle().has_patch())
        {
            command.patch = FromProtoReticlePatch(value.update_reticle().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpdateStrobe:
    {
        UpdateStrobeCommand command;
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
        return command;
    }

    case pb::UserCommand::kUpsertDynamicReticle:
    {
        UpsertDynamicReticleCommand command;
        command.target = FromProtoDynamicHandle(value.upsert_dynamic_reticle().target());
        command.templateTransportId = value.upsert_dynamic_reticle().template_transport_id();
        if (value.upsert_dynamic_reticle().has_patch())
        {
            command.patch = FromProtoReticlePatch(value.upsert_dynamic_reticle().patch());
        }
        return command;
    }

    case pb::UserCommand::kUpsertDynamicReticles:
    {
        UpsertDynamicReticlesCommand command;
        command.pageId = value.upsert_dynamic_reticles().page_id();
        command.templateTransportId = value.upsert_dynamic_reticles().template_transport_id();
        ValidateContainerSize(
            static_cast<std::size_t>(value.upsert_dynamic_reticles().reticles_size()),
            kMaxDynamicReticlesPerBatch,
            "Protocol Buffers UpsertDynamicReticlesCommand.reticles");
        command.reticles.reserve(value.upsert_dynamic_reticles().reticles_size());

        for (const pb::DynamicReticleState& reticle : value.upsert_dynamic_reticles().reticles())
        {
            command.reticles.push_back(FromProtoDynamicReticleState(reticle));
        }

        return command;
    }

    case pb::UserCommand::kSetDynamicReticleSetVisibility:
    {
        SetDynamicReticleSetVisibilityCommand command;
        command.visible = value.set_dynamic_reticle_set_visibility().visible();
        command.pageId = value.set_dynamic_reticle_set_visibility().page_id();
        command.templateTransportId = value.set_dynamic_reticle_set_visibility().template_transport_id();
        return command;
    }

    case pb::UserCommand::kSetDynamicReticleSetStrobeMagnetEnabled:
    {
        SetDynamicReticleSetStrobeMagnetEnabledCommand command;
        command.enabled = value.set_dynamic_reticle_set_strobe_magnet_enabled().enabled();
        command.pageId = value.set_dynamic_reticle_set_strobe_magnet_enabled().page_id();
        command.templateTransportId = value.set_dynamic_reticle_set_strobe_magnet_enabled().template_transport_id();
        return command;
    }

    case pb::UserCommand::kRemoveDynamicReticle:
        return RemoveDynamicReticleCommand {FromProtoDynamicHandle(value.remove_dynamic_reticle().target())};

    case pb::UserCommand::kResetWindow:
        return ResetWindowCommand {};

    case pb::UserCommand::COMMAND_NOT_SET:
        break;
    }

    throw std::runtime_error("Protocol Buffers command payload is empty");
}

pb::CommandEnvelope BuildEnvelope(const CommandBatch& batch)
{
    pb::CommandEnvelope envelope;
    envelope.set_sequence(batch.sequence);
    envelope.set_mapping_hash(batch.mappingHash);

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
        if (envelope.commands_size() == 0)
        {
            throw std::runtime_error("Protocol Buffers command payload is empty");
        }

        ValidateContainerSize(
            static_cast<std::size_t>(envelope.commands_size()),
            kMaxCommandsPerEnvelope,
            "Protocol Buffers CommandEnvelope.commands");
        batch.commands.reserve(envelope.commands_size());

        for (const pb::UserCommand& command : envelope.commands())
        {
            batch.commands.push_back(FromProtoUserCommand(command));
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
} // namespace mfd
