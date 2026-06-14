/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for Animation.
 */

#include "mfd/client/Animation.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace mfd::client
{
namespace
{
constexpr mfd::RuntimeDynamicId kGeneratedDynamicRuntimeIdBit = mfd::RuntimeDynamicId {1} << 63U;

std::string NormalizeFeedbackKey(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

std::string NormalizeStrobeName(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

bool Equal(const mfd::PrimitivePatch& lhs, const mfd::PrimitivePatch& rhs);

bool PrimitivePatchMapsEqual(const std::unordered_map<std::string, mfd::PrimitivePatch>& lhs,
                             const std::unordered_map<std::string, mfd::PrimitivePatch>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (const auto& primitivePatchEntry : lhs)
    {
        const auto rightIt = rhs.find(primitivePatchEntry.first);
        if (rightIt == rhs.end())
        {
            return false;
        }

        if (!Equal(primitivePatchEntry.second, rightIt->second))
        {
            return false;
        }
    }

    return true;
}

bool SameStrobeSelection(const mfd::TransportId lhsId,
                         const std::string_view lhsNameNormalized,
                         const mfd::TransportId rhsId,
                         const std::string_view rhsNameNormalized) noexcept
{
    if (lhsId != 0 || rhsId != 0)
    {
        return lhsId == rhsId;
    }

    return lhsNameNormalized == rhsNameNormalized;
}

// Accept-or-equal comparison for runtime feedback ordering. The `>=` is intentional: re-applying the
// same sequence stays idempotent (callers compute "changed" separately), so a duplicated feedback is not
// rejected. Wraparound of the 32-bit sequence is deliberately not handled: sequences are monotonic within
// a session and cannot reach UINT32_MAX in practice. If the protocol ever needs to survive a wraparound,
// replace this with an explicit modulo comparison and add the matching tests.
bool SequenceIsNewer(const std::uint32_t sequence, const std::uint32_t previous) noexcept
{
    return sequence >= previous;
}

template <typename T>
bool Equal(const T& lhs, const T& rhs)
{
    return lhs == rhs;
}

bool Equal(const mfd::Vec2& lhs, const mfd::Vec2& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool Equal(const mfd::ColorRgba& lhs, const mfd::ColorRgba& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool Equal(const std::vector<mfd::Vec2>& lhs, const std::vector<mfd::Vec2>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (!Equal(lhs[index], rhs[index]))
        {
            return false;
        }
    }

    return true;
}

template <typename T>
bool EqualOptional(const std::optional<T>& lhs, const std::optional<T>& rhs)
{
    return lhs == rhs;
}

template <>
bool EqualOptional(const std::optional<mfd::Vec2>& lhs, const std::optional<mfd::Vec2>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || Equal(*lhs, *rhs);
}

template <>
bool EqualOptional(const std::optional<mfd::ColorRgba>& lhs, const std::optional<mfd::ColorRgba>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || Equal(*lhs, *rhs);
}

template <>
bool EqualOptional(const std::optional<std::vector<mfd::Vec2>>& lhs,
                   const std::optional<std::vector<mfd::Vec2>>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || Equal(*lhs, *rhs);
}

bool Equal(const mfd::PrimitivePatch& lhs, const mfd::PrimitivePatch& rhs)
{
    return EqualOptional(lhs.visible, rhs.visible) &&
           EqualOptional(lhs.position, rhs.position) &&
           EqualOptional(lhs.rotationDegrees, rhs.rotationDegrees) &&
           EqualOptional(lhs.scale, rhs.scale) &&
           EqualOptional(lhs.color, rhs.color) &&
           EqualOptional(lhs.fillColor, rhs.fillColor) &&
           EqualOptional(lhs.filled, rhs.filled) &&
           EqualOptional(lhs.thickness, rhs.thickness) &&
           EqualOptional(lhs.lineStyle, rhs.lineStyle) &&
           EqualOptional(lhs.text, rhs.text) &&
           EqualOptional(lhs.letterSpacing, rhs.letterSpacing) &&
           EqualOptional(lhs.lineStart, rhs.lineStart) &&
           EqualOptional(lhs.lineEnd, rhs.lineEnd) &&
           EqualOptional(lhs.radius, rhs.radius) &&
           EqualOptional(lhs.innerRadius, rhs.innerRadius) &&
           EqualOptional(lhs.outerRadius, rhs.outerRadius) &&
           EqualOptional(lhs.width, rhs.width) &&
           EqualOptional(lhs.height, rhs.height) &&
           EqualOptional(lhs.size, rhs.size) &&
           EqualOptional(lhs.points, rhs.points) &&
           EqualOptional(lhs.closed, rhs.closed) &&
           EqualOptional(lhs.segments, rhs.segments) &&
           EqualOptional(lhs.startAngleDegrees, rhs.startAngleDegrees) &&
           EqualOptional(lhs.endAngleDegrees, rhs.endAngleDegrees) &&
           EqualOptional(lhs.timeValue, rhs.timeValue) &&
           lhs.clearTimeValue == rhs.clearTimeValue &&
           EqualOptional(lhs.timeUtc, rhs.timeUtc) &&
           EqualOptional(lhs.timeFields, rhs.timeFields);
}

bool Equal(const mfd::ReticlePatch& lhs, const mfd::ReticlePatch& rhs)
{
    return EqualOptional(lhs.visible, rhs.visible) &&
           EqualOptional(lhs.blinkEnabled, rhs.blinkEnabled) &&
           EqualOptional(lhs.blinkType, rhs.blinkType) &&
           EqualOptional(lhs.blinkTypeId, rhs.blinkTypeId) &&
           EqualOptional(lhs.position, rhs.position) &&
           EqualOptional(lhs.rotationDegrees, rhs.rotationDegrees) &&
           EqualOptional(lhs.scale, rhs.scale) &&
           EqualOptional(lhs.color, rhs.color) &&
           EqualOptional(lhs.thickness, rhs.thickness) &&
           EqualOptional(lhs.text, rhs.text) &&
           lhs.texts == rhs.texts &&
           EqualOptional(lhs.letterSpacing, rhs.letterSpacing) &&
           lhs.letterSpacings == rhs.letterSpacings &&
           PrimitivePatchMapsEqual(lhs.primitivePatches, rhs.primitivePatches);
}

bool Equal(const mfd::WindowDisplayPatch& lhs, const mfd::WindowDisplayPatch& rhs)
{
    return EqualOptional(lhs.invertColors, rhs.invertColors) &&
           EqualOptional(lhs.brightness, rhs.brightness) &&
           EqualOptional(lhs.disabled, rhs.disabled);
}

template <typename T>
void CopyChangedOptionalField(const std::optional<T>& desired,
                              const std::optional<T>& previous,
                              std::optional<T>& destination)
{
    if (!EqualOptional(desired, previous))
    {
        destination = desired;
    }
}

template <typename T>
void CopyChangedMapFields(const std::unordered_map<std::string, T>& desired,
                          const std::unordered_map<std::string, T>& previous,
                          std::unordered_map<std::string, T>& destination)
{
    for (const auto& [key, value] : desired)
    {
        const auto previousIt = previous.find(key);
        if (previousIt == previous.end() || previousIt->second != value)
        {
            destination.emplace(key, value);
        }
    }
}

template <typename T>
void MoveGeneratedPrimitiveEntries(std::unordered_map<std::string, T>& namedValues,
                                   const std::unordered_map<std::string, mfd::TransportId>& primitiveTransportIds,
                                   std::unordered_map<mfd::TransportId, T>& generatedValues)
{
    std::vector<std::string> consumedKeys;
    consumedKeys.reserve(namedValues.size());

    for (const auto& [primitiveId, value] : namedValues)
    {
        const auto iterator = primitiveTransportIds.find(primitiveId);
        if (iterator == primitiveTransportIds.end())
        {
            continue;
        }

        generatedValues.insert_or_assign(iterator->second, value);
        consumedKeys.push_back(primitiveId);
    }

    for (const std::string& primitiveId : consumedKeys)
    {
        namedValues.erase(primitiveId);
    }
}

mfd::PrimitivePatch BuildDeltaPrimitivePatch(const mfd::PrimitivePatch& desired,
                                            const mfd::PrimitivePatch& previous)
{
    mfd::PrimitivePatch delta;
    CopyChangedOptionalField(desired.visible, previous.visible, delta.visible);
    CopyChangedOptionalField(desired.position, previous.position, delta.position);
    CopyChangedOptionalField(desired.rotationDegrees, previous.rotationDegrees, delta.rotationDegrees);
    CopyChangedOptionalField(desired.scale, previous.scale, delta.scale);
    CopyChangedOptionalField(desired.color, previous.color, delta.color);
    CopyChangedOptionalField(desired.fillColor, previous.fillColor, delta.fillColor);
    CopyChangedOptionalField(desired.filled, previous.filled, delta.filled);
    CopyChangedOptionalField(desired.thickness, previous.thickness, delta.thickness);
    CopyChangedOptionalField(desired.lineStyle, previous.lineStyle, delta.lineStyle);
    CopyChangedOptionalField(desired.text, previous.text, delta.text);
    CopyChangedOptionalField(desired.letterSpacing, previous.letterSpacing, delta.letterSpacing);
    CopyChangedOptionalField(desired.lineStart, previous.lineStart, delta.lineStart);
    CopyChangedOptionalField(desired.lineEnd, previous.lineEnd, delta.lineEnd);
    CopyChangedOptionalField(desired.radius, previous.radius, delta.radius);
    CopyChangedOptionalField(desired.innerRadius, previous.innerRadius, delta.innerRadius);
    CopyChangedOptionalField(desired.outerRadius, previous.outerRadius, delta.outerRadius);
    CopyChangedOptionalField(desired.width, previous.width, delta.width);
    CopyChangedOptionalField(desired.height, previous.height, delta.height);
    CopyChangedOptionalField(desired.size, previous.size, delta.size);
    CopyChangedOptionalField(desired.points, previous.points, delta.points);
    CopyChangedOptionalField(desired.closed, previous.closed, delta.closed);
    CopyChangedOptionalField(desired.segments, previous.segments, delta.segments);
    CopyChangedOptionalField(desired.startAngleDegrees, previous.startAngleDegrees, delta.startAngleDegrees);
    CopyChangedOptionalField(desired.endAngleDegrees, previous.endAngleDegrees, delta.endAngleDegrees);
    CopyChangedOptionalField(desired.timeValue, previous.timeValue, delta.timeValue);
    if (desired.clearTimeValue && !previous.clearTimeValue)
    {
        delta.clearTimeValue = true;
    }
    CopyChangedOptionalField(desired.timeUtc, previous.timeUtc, delta.timeUtc);
    CopyChangedOptionalField(desired.timeFields, previous.timeFields, delta.timeFields);
    return delta;
}

void CopyChangedPrimitiveMapFields(const std::unordered_map<std::string, mfd::PrimitivePatch>& desired,
                                   const std::unordered_map<std::string, mfd::PrimitivePatch>& previous,
                                   std::unordered_map<std::string, mfd::PrimitivePatch>& destination)
{
    for (const auto& [key, value] : desired)
    {
        const auto previousIt = previous.find(key);
        if (previousIt == previous.end())
        {
            destination.emplace(key, value);
            continue;
        }

        const mfd::PrimitivePatch delta = BuildDeltaPrimitivePatch(value, previousIt->second);
        const mfd::PrimitivePatch empty {};
        if (!Equal(delta, empty))
        {
            destination.emplace(key, delta);
        }
    }
}

mfd::ReticlePatch BuildDeltaPatch(const mfd::ReticlePatch& desired, const mfd::ReticlePatch& previous)
{
    mfd::ReticlePatch delta;
    CopyChangedOptionalField(desired.visible, previous.visible, delta.visible);
    CopyChangedOptionalField(desired.blinkEnabled, previous.blinkEnabled, delta.blinkEnabled);
    CopyChangedOptionalField(desired.blinkType, previous.blinkType, delta.blinkType);
    CopyChangedOptionalField(desired.blinkTypeId, previous.blinkTypeId, delta.blinkTypeId);
    CopyChangedOptionalField(desired.position, previous.position, delta.position);
    CopyChangedOptionalField(desired.rotationDegrees, previous.rotationDegrees, delta.rotationDegrees);
    CopyChangedOptionalField(desired.scale, previous.scale, delta.scale);
    CopyChangedOptionalField(desired.color, previous.color, delta.color);
    CopyChangedOptionalField(desired.thickness, previous.thickness, delta.thickness);
    CopyChangedOptionalField(desired.text, previous.text, delta.text);
    CopyChangedMapFields(desired.texts, previous.texts, delta.texts);
    CopyChangedOptionalField(desired.letterSpacing, previous.letterSpacing, delta.letterSpacing);
    CopyChangedMapFields(desired.letterSpacings, previous.letterSpacings, delta.letterSpacings);
    CopyChangedPrimitiveMapFields(desired.primitivePatches, previous.primitivePatches, delta.primitivePatches);
    return delta;
}

mfd::WindowDisplayPatch BuildDeltaPatch(const mfd::WindowDisplayPatch& desired,
                                        const mfd::WindowDisplayPatch& previous)
{
    mfd::WindowDisplayPatch delta;
    CopyChangedOptionalField(desired.invertColors, previous.invertColors, delta.invertColors);
    CopyChangedOptionalField(desired.brightness, previous.brightness, delta.brightness);
    CopyChangedOptionalField(desired.disabled, previous.disabled, delta.disabled);
    return delta;
}

void PatchSetVisible(mfd::ReticlePatch& patch, const bool visible)
{
    patch.visible = visible;
}

void PatchSetBlinkEnabled(mfd::ReticlePatch& patch, const bool enabled)
{
    patch.blinkEnabled = enabled;
}

void PatchSetBlink(mfd::ReticlePatch& patch, const bool enabled, const std::string_view blinkType)
{
    patch.blinkEnabled = enabled;
    patch.blinkType = std::string(blinkType);
}

void PatchSetBlinkType(mfd::ReticlePatch& patch, const std::string_view blinkType)
{
    patch.blinkEnabled = true;
    patch.blinkType = std::string(blinkType);
}

void PatchClearBlinkType(mfd::ReticlePatch& patch)
{
    patch.blinkType = std::string {};
}

void PatchSetPosition(mfd::ReticlePatch& patch, const mfd::Vec2 position)
{
    patch.position = position;
}

void PatchSetRotationDegrees(mfd::ReticlePatch& patch, const float rotationDegrees)
{
    patch.rotationDegrees = rotationDegrees;
}

void PatchSetScale(mfd::ReticlePatch& patch, const mfd::Vec2 scale)
{
    patch.scale = scale;
}

void PatchSetColor(mfd::ReticlePatch& patch, const mfd::ColorRgba color)
{
    patch.color = color;
}

void PatchSetThickness(mfd::ReticlePatch& patch, const float thickness)
{
    patch.thickness = thickness;
}

void PatchSetText(mfd::ReticlePatch& patch, std::string value)
{
    patch.text = std::move(value);
}

void PatchSetText(mfd::ReticlePatch& patch, const std::string_view primitiveId, std::string value)
{
    patch.texts[std::string(primitiveId)] = std::move(value);
}

void PatchSetLetterSpacing(mfd::ReticlePatch& patch, const float letterSpacing)
{
    patch.letterSpacing = letterSpacing;
}

void PatchSetLetterSpacing(mfd::ReticlePatch& patch,
                           const std::string_view primitiveId,
                           const float letterSpacing)
{
    patch.letterSpacings[std::string(primitiveId)] = letterSpacing;
}

mfd::PrimitivePatch& PatchPrimitive(mfd::ReticlePatch& patch, const std::string_view primitiveId)
{
    return patch.primitivePatches[std::string(primitiveId)];
}
} // namespace

BlinkType::BlinkType(const std::string_view name, const mfd::TransportId transportId) :
    name_(name),
    transportId_(transportId)
{
}

const std::string& BlinkType::Name() const noexcept
{
    return name_;
}

mfd::TransportId BlinkType::GeneratedId() const noexcept
{
    return transportId_;
}

StrobeType::StrobeType(const std::string_view name,
                       StrobeInfo info,
                       const mfd::TransportId transportId,
                       const bool defaultActive) :
    name_(name),
    info_(std::move(info)),
    transportId_(transportId),
    defaultActive_(defaultActive)
{
}

const std::string& StrobeType::Name() const noexcept
{
    return name_;
}

mfd::TransportId StrobeType::GeneratedId() const noexcept
{
    return transportId_;
}

const StrobeInfo& StrobeType::Info() const noexcept
{
    return info_;
}

bool StrobeType::IsDefaultActive() const noexcept
{
    return defaultActive_;
}

bool RuntimeFeedbackState::Apply(const mfd::StrobeStatusFeedback& feedback)
{
    const std::string normalizedPageName = NormalizeFeedbackKey(feedback.pageName);
    const auto applyCaptureState = [&feedback, &normalizedPageName](PageCaptureState& state)
    {
        if (state.hasSequence && !SequenceIsNewer(feedback.sequence, state.lastStrobeSequence))
        {
            return false;
        }

        const bool nextCaptured = feedback.captureResult.has_value();
        const mfd::RuntimeDynamicId nextRuntimeReticleId =
            nextCaptured ? feedback.captureResult->runtimeReticleId : mfd::RuntimeDynamicId {0};
        const bool changed = !state.hasSequence ||
                             state.captured != nextCaptured ||
                             state.capturedRuntimeReticleId != nextRuntimeReticleId ||
                             state.pageNameNormalized != normalizedPageName;

        state.lastStrobeSequence = feedback.sequence;
        state.hasSequence = true;
        state.captured = nextCaptured;
        state.capturedRuntimeReticleId = nextRuntimeReticleId;
        state.pageNameNormalized = normalizedPageName;
        return changed;
    };

    if (feedback.pageId == 0)
    {
        return false;
    }

    PageCaptureState& state = pageCaptureById_[feedback.pageId];
    return applyCaptureState(state);
}

bool RuntimeFeedbackState::Apply(const mfd::ActivePageFeedback& feedback)
{
    if (hasActivePage_ && !SequenceIsNewer(feedback.sequence, lastActivePageSequence_))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizeFeedbackKey(feedback.pageName);
    const bool changed = !hasActivePage_ ||
                         activePageName_ != feedback.pageName ||
                         activePageNameNormalized_ != normalizedPageName;

    activePageName_ = feedback.pageName;
    activePageNameNormalized_ = normalizedPageName;
    lastActivePageSequence_ = feedback.sequence;
    hasActivePage_ = true;

    if (changed)
    {
        for (auto& pageCaptureEntry : pageCaptureById_)
        {
            auto& state = pageCaptureEntry.second;
            if (state.lastStrobeSequence < feedback.sequence)
            {
                state.captured = false;
                state.capturedRuntimeReticleId = 0;
            }
        }
    }

    return changed;
}

bool RuntimeFeedbackState::ApplyPayload(const std::string_view payload, std::string* error)
{
    std::string localError;
    std::string* errorTarget = error == nullptr ? &localError : error;
    errorTarget->clear();

    const auto decoded = mfd::DeserializeFeedbackPayload(payload, errorTarget);
    if (!decoded.has_value())
    {
        return false;
    }

    return std::visit(
        [this](const auto& value)
        {
            return Apply(value);
        },
        *decoded);
}

std::size_t RuntimeFeedbackState::Poll(mfd::IExchangeChannel& channel,
                                       const std::size_t maxMessages,
                                       std::string* error)
{
    if (maxMessages == 0)
    {
        if (error != nullptr)
        {
            error->clear();
        }

        return 0;
    }

    std::size_t appliedCount = 0;
    std::string lastError;

    for (std::size_t index = 0; index < maxMessages; ++index)
    {
        const auto payload = channel.TryReceive();
        if (!payload.has_value())
        {
            break;
        }

        const auto* raw = reinterpret_cast<const char*>(payload->data());
        std::string decodeError;
        if (ApplyPayload(std::string_view(raw, payload->size()), &decodeError))
        {
            ++appliedCount;
            continue;
        }

        if (!decodeError.empty())
        {
            lastError = std::move(decodeError);
        }
    }

    if (error != nullptr)
    {
        *error = std::move(lastError);
    }

    return appliedCount;
}

void RuntimeFeedbackState::Reset() noexcept
{
    pageCaptureById_.clear();
    activePageName_.clear();
    activePageNameNormalized_.clear();
    lastActivePageSequence_ = 0;
    hasActivePage_ = false;
}

bool RuntimeFeedbackState::HasActivePage() const noexcept
{
    return hasActivePage_;
}

const std::string& RuntimeFeedbackState::ActivePageName() const noexcept
{
    return activePageName_;
}

bool RuntimeFeedbackState::IsPageActive(const std::string_view pageName) const noexcept
{
    return hasActivePage_ && activePageNameNormalized_ == NormalizeFeedbackKey(pageName);
}

bool RuntimeFeedbackState::IsDynamicReticleCaptured(const mfd::TransportId pageId,
                                                    const mfd::RuntimeDynamicId runtimeReticleId) const noexcept
{
    if (pageId == 0 || runtimeReticleId == 0 || !hasActivePage_)
    {
        return false;
    }

    const auto pageIt = pageCaptureById_.find(pageId);
    if (pageIt == pageCaptureById_.end())
    {
        return false;
    }

    const PageCaptureState& state = pageIt->second;
    return state.captured &&
           state.capturedRuntimeReticleId == runtimeReticleId &&
           state.pageNameNormalized == activePageNameNormalized_;
}

ReticleBlink::ReticleBlink(mfd::ReticlePatch& patch, bool* dirty) noexcept :
    patch_(&patch),
    dirty_(dirty)
{
}

ReticleBlink& ReticleBlink::operator=(const BlinkType& blinkType)
{
    Use(blinkType);
    return *this;
}

ReticleBlink& ReticleBlink::operator=(std::nullptr_t)
{
    Disable();
    return *this;
}

void ReticleBlink::SetEnabled(const bool enabled)
{
    PatchSetBlinkEnabled(*patch_, enabled);
    MarkDirty();
}

void ReticleBlink::Set(const bool enabled, const BlinkType& blinkType)
{
    if (enabled)
    {
        PatchSetBlink(*patch_, true, blinkType.Name());
        patch_->blinkTypeId = blinkType.GeneratedId() == 0
                                  ? std::optional<mfd::TransportId> {}
                                  : std::optional<mfd::TransportId> {blinkType.GeneratedId()};
    }
    else
    {
        PatchSetBlink(*patch_, false, {});
        patch_->blinkTypeId =
            (patch_->blinkTypeId.has_value() || blinkType.GeneratedId() != 0)
                ? std::optional<mfd::TransportId> {mfd::TransportId {0}}
                : std::optional<mfd::TransportId> {};
    }

    MarkDirty();
}

void ReticleBlink::Use(const BlinkType& blinkType)
{
    PatchSetBlinkType(*patch_, blinkType.Name());
    patch_->blinkTypeId = blinkType.GeneratedId() == 0
                              ? std::optional<mfd::TransportId> {}
                              : std::optional<mfd::TransportId> {blinkType.GeneratedId()};
    MarkDirty();
}

void ReticleBlink::Disable()
{
    PatchSetBlink(*patch_, false, {});
    patch_->blinkTypeId = patch_->blinkTypeId.has_value()
                              ? std::optional<mfd::TransportId> {mfd::TransportId {0}}
                              : std::optional<mfd::TransportId> {};
    MarkDirty();
}

void ReticleBlink::ClearType()
{
    PatchClearBlinkType(*patch_);
    patch_->blinkTypeId = patch_->blinkTypeId.has_value()
                              ? std::optional<mfd::TransportId> {mfd::TransportId {0}}
                              : std::optional<mfd::TransportId> {};
    MarkDirty();
}

void ReticleBlink::MarkDirty() noexcept
{
    if (dirty_ != nullptr)
    {
        *dirty_ = true;
    }
}

PrimitiveHandle::PrimitiveHandle(mfd::ReticlePatch& patch,
                                 bool* dirty,
                                 const std::string_view primitiveId,
                                 const mfd::TransportId transportId,
                                 std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    patch_(&patch),
    dirty_(dirty),
    primitiveId_(primitiveId),
    transportId_(transportId)
{
    if (primitiveTransportIds != nullptr && transportId_ != 0)
    {
        primitiveTransportIds->insert_or_assign(primitiveId_, transportId_);
    }
}

const std::string& PrimitiveHandle::Id() const noexcept
{
    return primitiveId_;
}

mfd::TransportId PrimitiveHandle::GeneratedId() const noexcept
{
    return transportId_;
}

void PrimitiveHandle::SetVisible(const bool visible)
{
    Patch().visible = visible;
    MarkDirty();
}

void PrimitiveHandle::SetPosition(const mfd::Vec2 position)
{
    Patch().position = position;
    MarkDirty();
}

void PrimitiveHandle::SetRotationDegrees(const float rotationDegrees)
{
    Patch().rotationDegrees = rotationDegrees;
    MarkDirty();
}

void PrimitiveHandle::SetScale(const mfd::Vec2 scale)
{
    Patch().scale = scale;
    MarkDirty();
}

void PrimitiveHandle::SetColor(const mfd::ColorRgba color)
{
    Patch().color = color;
    MarkDirty();
}

void PrimitiveHandle::SetFillColorInternal(const mfd::ColorRgba color)
{
    Patch().fillColor = color;
    MarkDirty();
}

void PrimitiveHandle::SetFilledInternal(const bool filled)
{
    Patch().filled = filled;
    MarkDirty();
}

void PrimitiveHandle::SetThickness(const float thickness)
{
    Patch().thickness = thickness;
    MarkDirty();
}

void PrimitiveHandle::SetLineStyle(const LineStyle lineStyle)
{
    Patch().lineStyle = lineStyle;
    MarkDirty();
}

mfd::PrimitivePatch& PrimitiveHandle::Patch() noexcept
{
    return PatchPrimitive(*patch_, primitiveId_);
}

void PrimitiveHandle::MarkDirty() noexcept
{
    if (dirty_ != nullptr)
    {
        *dirty_ = true;
    }
}

TextHandle::TextHandle(mfd::ReticlePatch& patch,
                       bool* dirty,
                       const std::string_view primitiveId,
                       const mfd::TransportId transportId,
                       std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void TextHandle::SetText(std::string value)
{
    Patch().text = std::move(value);
    MarkDirty();
}

void TextHandle::SetLetterSpacing(const float letterSpacing)
{
    Patch().letterSpacing = letterSpacing;
    MarkDirty();
}

TimeHandle::TimeHandle(mfd::ReticlePatch& patch,
                       bool* dirty,
                       const std::string_view primitiveId,
                       const mfd::TransportId transportId,
                       std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void TimeHandle::SetLetterSpacing(const float letterSpacing)
{
    Patch().letterSpacing = letterSpacing;
    MarkDirty();
}

void TimeHandle::SetTimeValue(const mfd::TimeValue value)
{
    Patch().timeValue = value;
    Patch().clearTimeValue = false;
    MarkDirty();
}

void TimeHandle::ClearTimeValue()
{
    Patch().timeValue.reset();
    Patch().clearTimeValue = true;
    MarkDirty();
}

void TimeHandle::SetUtc(const bool utc)
{
    Patch().timeUtc = utc;
    MarkDirty();
}

void TimeHandle::SetFieldVisibility(const mfd::TimeFieldVisibility fields)
{
    Patch().timeFields = fields;
    MarkDirty();
}

LineHandle::LineHandle(mfd::ReticlePatch& patch,
                       bool* dirty,
                       const std::string_view primitiveId,
                       const mfd::TransportId transportId,
                       std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void LineHandle::SetStart(const mfd::Vec2 start)
{
    Patch().lineStart = start;
    MarkDirty();
}

void LineHandle::SetEnd(const mfd::Vec2 end)
{
    Patch().lineEnd = end;
    MarkDirty();
}

CircleHandle::CircleHandle(mfd::ReticlePatch& patch,
                           bool* dirty,
                           const std::string_view primitiveId,
                           const mfd::TransportId transportId,
                           std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void CircleHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void CircleHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void CircleHandle::SetRadius(const float radius)
{
    Patch().radius = radius;
    MarkDirty();
}

RingHandle::RingHandle(mfd::ReticlePatch& patch,
                       bool* dirty,
                       const std::string_view primitiveId,
                       const mfd::TransportId transportId,
                       std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void RingHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void RingHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void RingHandle::SetInnerRadius(const float radius)
{
    Patch().innerRadius = radius;
    MarkDirty();
}

void RingHandle::SetOuterRadius(const float radius)
{
    Patch().outerRadius = radius;
    MarkDirty();
}

void RingHandle::SetSegments(const int segments)
{
    Patch().segments = segments;
    MarkDirty();
}

RectangleHandle::RectangleHandle(mfd::ReticlePatch& patch,
                                 bool* dirty,
                                 const std::string_view primitiveId,
                                 const mfd::TransportId transportId,
                                 std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void RectangleHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void RectangleHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void RectangleHandle::SetWidth(const float width)
{
    Patch().width = width;
    MarkDirty();
}

void RectangleHandle::SetHeight(const float height)
{
    Patch().height = height;
    MarkDirty();
}

void RectangleHandle::SetSize(const mfd::Vec2 size)
{
    Patch().size = size;
    MarkDirty();
}

EllipseHandle::EllipseHandle(mfd::ReticlePatch& patch,
                             bool* dirty,
                             const std::string_view primitiveId,
                             const mfd::TransportId transportId,
                             std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void EllipseHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void EllipseHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void EllipseHandle::SetWidth(const float width)
{
    Patch().width = width;
    MarkDirty();
}

void EllipseHandle::SetHeight(const float height)
{
    Patch().height = height;
    MarkDirty();
}

void EllipseHandle::SetSize(const mfd::Vec2 size)
{
    Patch().size = size;
    MarkDirty();
}

SquareHandle::SquareHandle(mfd::ReticlePatch& patch,
                           bool* dirty,
                           const std::string_view primitiveId,
                           const mfd::TransportId transportId,
                           std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void SquareHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void SquareHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void SquareHandle::SetWidth(const float width)
{
    Patch().width = width;
    MarkDirty();
}

void SquareHandle::SetHeight(const float height)
{
    Patch().height = height;
    MarkDirty();
}

void SquareHandle::SetSize(const mfd::Vec2 size)
{
    Patch().size = size;
    MarkDirty();
}

DiamondHandle::DiamondHandle(mfd::ReticlePatch& patch,
                             bool* dirty,
                             const std::string_view primitiveId,
                             const mfd::TransportId transportId,
                             std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void DiamondHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void DiamondHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void DiamondHandle::SetWidth(const float width)
{
    Patch().width = width;
    MarkDirty();
}

void DiamondHandle::SetHeight(const float height)
{
    Patch().height = height;
    MarkDirty();
}

void DiamondHandle::SetSize(const mfd::Vec2 size)
{
    Patch().size = size;
    MarkDirty();
}

TriangleHandle::TriangleHandle(mfd::ReticlePatch& patch,
                               bool* dirty,
                               const std::string_view primitiveId,
                               const mfd::TransportId transportId,
                               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void TriangleHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void TriangleHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void TriangleHandle::SetPoints(const std::array<mfd::Vec2, 3>& points)
{
    Patch().points = std::vector<mfd::Vec2>(points.begin(), points.end());
    MarkDirty();
}

PolylineHandle::PolylineHandle(mfd::ReticlePatch& patch,
                               bool* dirty,
                               const std::string_view primitiveId,
                               const mfd::TransportId transportId,
                               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void PolylineHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void PolylineHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void PolylineHandle::SetPoints(std::vector<mfd::Vec2> points)
{
    Patch().points = std::move(points);
    MarkDirty();
}

void PolylineHandle::SetClosed(const bool closed)
{
    Patch().closed = closed;
    MarkDirty();
}

BezierHandle::BezierHandle(mfd::ReticlePatch& patch,
                           bool* dirty,
                           const std::string_view primitiveId,
                           const mfd::TransportId transportId,
                           std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void BezierHandle::SetControlPoints(std::vector<mfd::Vec2> controlPoints)
{
    Patch().points = std::move(controlPoints);
    MarkDirty();
}

void BezierHandle::SetSegments(const int segments)
{
    Patch().segments = segments;
    MarkDirty();
}

ArcHandle::ArcHandle(mfd::ReticlePatch& patch,
                     bool* dirty,
                     const std::string_view primitiveId,
                     const mfd::TransportId transportId,
                     std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

void ArcHandle::SetFillColor(const mfd::ColorRgba color)
{
    SetFillColorInternal(color);
}

void ArcHandle::SetFilled(const bool filled)
{
    SetFilledInternal(filled);
}

void ArcHandle::SetRadius(const float radius)
{
    Patch().radius = radius;
    MarkDirty();
}

void ArcHandle::SetStartAngleDegrees(const float startAngleDegrees)
{
    Patch().startAngleDegrees = startAngleDegrees;
    MarkDirty();
}

void ArcHandle::SetEndAngleDegrees(const float endAngleDegrees)
{
    Patch().endAngleDegrees = endAngleDegrees;
    MarkDirty();
}

void ArcHandle::SetSegments(const int segments)
{
    Patch().segments = segments;
    MarkDirty();
}

ImageHandle::ImageHandle(mfd::ReticlePatch& patch,
                         bool* dirty,
                         const std::string_view primitiveId,
                         const mfd::TransportId transportId,
                         std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds) :
    PrimitiveHandle(patch, dirty, primitiveId, transportId, primitiveTransportIds)
{
}

Reticle::Reticle(const std::string_view pageName,
                 const std::string_view reticleId,
                 const mfd::TransportId pageTransportId,
                 const mfd::TransportId reticleTransportId) :
    pageName_(pageName),
    reticleId_(reticleId),
    pageTransportId_(pageTransportId),
    reticleTransportId_(reticleTransportId),
    Blink(desiredPatch_, &dirty_)
{
}

void Reticle::ClearDirty() noexcept
{
    dirty_ = false;
}

void Reticle::Reset() noexcept
{
    ClearDirty();
}

void Reticle::ResetToAuthored() noexcept
{
    desiredPatch_ = {};
    lastSentPatch_ = {};
    dirty_ = false;
}

void Reticle::SetVisible(const bool visible)
{
    PatchSetVisible(desiredPatch_, visible);
    dirty_ = true;
}

void Reticle::SetBlinkEnabled(const bool enabled)
{
    Blink.SetEnabled(enabled);
}

void Reticle::SetBlink(const bool enabled, const BlinkType& blinkType)
{
    Blink.Set(enabled, blinkType);
}

void Reticle::SetBlinkType(const BlinkType& blinkType)
{
    Blink.Use(blinkType);
}

void Reticle::ClearBlinkType()
{
    Blink.ClearType();
}

void Reticle::SetPosition(const mfd::Vec2 position)
{
    PatchSetPosition(desiredPatch_, position);
    dirty_ = true;
}

void Reticle::SetRotationDegrees(const float rotationDegrees)
{
    PatchSetRotationDegrees(desiredPatch_, rotationDegrees);
    dirty_ = true;
}

void Reticle::SetScale(const mfd::Vec2 scale)
{
    PatchSetScale(desiredPatch_, scale);
    dirty_ = true;
}

void Reticle::SetColor(const mfd::ColorRgba color)
{
    PatchSetColor(desiredPatch_, color);
    dirty_ = true;
}

void Reticle::SetThickness(const float thickness)
{
    PatchSetThickness(desiredPatch_, thickness);
    dirty_ = true;
}

void Reticle::SetText(std::string value)
{
    PatchSetText(desiredPatch_, std::move(value));
    dirty_ = true;
}

void Reticle::SetText(const std::string_view primitiveId, std::string value)
{
    PatchSetText(desiredPatch_, primitiveId, std::move(value));
    dirty_ = true;
}

void Reticle::SetLetterSpacing(const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, letterSpacing);
    dirty_ = true;
}

void Reticle::SetLetterSpacing(const std::string_view primitiveId, const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, primitiveId, letterSpacing);
    dirty_ = true;
}

bool Reticle::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    if (!dirty_)
    {
        return false;
    }

    if (Equal(desiredPatch_, lastSentPatch_))
    {
        dirty_ = false;
        return false;
    }

    mfd::UpdateReticleCommand command;
    command.target = mfd::StaticReticleHandle {pageName_, reticleId_, pageTransportId_, reticleTransportId_};
    command.patch = BuildDeltaPatch(desiredPatch_, lastSentPatch_);
    PopulateGeneratedIdentifiers(command);
    commands.emplace_back(std::move(command));
    lastSentPatch_ = desiredPatch_;
    dirty_ = false;
    return true;
}

const mfd::ReticlePatch& Reticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

mfd::ReticlePatch& Reticle::MutableDesiredPatch() noexcept
{
    dirty_ = true;
    return desiredPatch_;
}

bool* Reticle::DirtyFlag() noexcept
{
    return &dirty_;
}

std::unordered_map<std::string, mfd::TransportId>* Reticle::PrimitiveTransportIds() noexcept
{
    return &primitiveTransportIds_;
}

void Reticle::PopulateGeneratedIdentifiers(mfd::UpdateReticleCommand& command) const
{
    if (command.patch.blinkTypeId.has_value() && pageTransportId_ != 0)
    {
        command.patch.blinkType.reset();
    }

    MoveGeneratedPrimitiveEntries(command.patch.texts, primitiveTransportIds_, command.patch.textsById);
    MoveGeneratedPrimitiveEntries(
        command.patch.letterSpacings,
        primitiveTransportIds_,
        command.patch.letterSpacingsById);
    MoveGeneratedPrimitiveEntries(
        command.patch.primitivePatches,
        primitiveTransportIds_,
        command.patch.primitivePatchesById);
}

TextReticle::TextReticle(const std::string_view pageName,
                         const std::string_view reticleId,
                         const std::string_view primitiveId,
                         const mfd::TransportId pageTransportId,
                         const mfd::TransportId reticleTransportId,
                         const mfd::TransportId primitiveTransportId) :
    Reticle(pageName, reticleId, pageTransportId, reticleTransportId),
    primitiveId_(primitiveId)
{
    if (primitiveTransportId != 0)
    {
        PrimitiveTransportIds()->insert_or_assign(primitiveId_, primitiveTransportId);
    }
}

void TextReticle::SetValue(std::string value)
{
    SetText(primitiveId_, std::move(value));
}

StrobeHandle::StrobeHandle(const std::string_view pageName,
                            StrobeInfo info,
                            const mfd::TransportId pageTransportId) :
    pageName_(pageName),
    pageTransportId_(pageTransportId),
    strobes_ {StrobeType {"Default", std::move(info), 0, true}}
{
    SelectDefaultStrobe();
}

StrobeHandle::StrobeHandle(const std::string_view pageName,
                           const std::initializer_list<StrobeType> strobes,
                           const mfd::TransportId pageTransportId) :
    pageName_(pageName),
    pageTransportId_(pageTransportId),
    strobes_(strobes)
{
    SelectDefaultStrobe();
}

void StrobeHandle::ClearDirty() noexcept
{
    dirty_ = false;
}

void StrobeHandle::Reset() noexcept
{
    ClearDirty();
}

void StrobeHandle::ResetToAuthored() noexcept
{
    SelectDefaultStrobe();
    lastSentStrobeId_ = desiredStrobeId_;
    lastSentStrobeNameNormalized_ = desiredStrobeNameNormalized_;
    desiredActive_.reset();
    lastSentActive_.reset();
    desiredPosition_.reset();
    lastSentPosition_.reset();
    dirty_ = false;
}

bool StrobeHandle::IsValid() const noexcept
{
    const StrobeType* selectedType = ResolveSelectedType();
    return selectedType != nullptr && selectedType->Info().valid;
}

std::string_view StrobeHandle::PageName() const noexcept
{
    return pageName_;
}

const StrobeType* StrobeHandle::SelectedType() const noexcept
{
    return ResolveSelectedType();
}

const StrobeInfo& StrobeHandle::Info() const noexcept
{
    static const StrobeInfo invalidInfo {};
    const StrobeType* selectedType = ResolveSelectedType();
    return selectedType != nullptr ? selectedType->Info() : invalidInfo;
}

StrobeHandle& StrobeHandle::operator=(const StrobeType& strobeType)
{
    Use(strobeType);
    return *this;
}

void StrobeHandle::Use(const StrobeType& strobeType)
{
    if (strobes_.empty())
    {
        return;
    }

    const auto iterator = std::find_if(
        strobes_.begin(),
        strobes_.end(),
        [&strobeType](const StrobeType& candidate)
        {
            return candidate.GeneratedId() == strobeType.GeneratedId() &&
                   candidate.Name() == strobeType.Name();
        });
    if (iterator == strobes_.end())
    {
        return;
    }

    desiredStrobeId_ = iterator->GeneratedId();
    desiredStrobeNameNormalized_ = NormalizeStrobeName(iterator->Name());
    dirty_ = true;
}

bool StrobeHandle::IsSelected(const StrobeType& strobeType) const noexcept
{
    const StrobeType* selectedType = ResolveSelectedType();
    return selectedType != nullptr &&
           selectedType->GeneratedId() == strobeType.GeneratedId() &&
           selectedType->Name() == strobeType.Name();
}

void StrobeHandle::SetActive(const bool active)
{
    if (!IsValid())
    {
        return;
    }

    desiredActive_ = active;
    dirty_ = true;
}

void StrobeHandle::SetPosition(const mfd::Vec2 position)
{
    if (!IsValid())
    {
        return;
    }

    desiredPosition_ = position;
    dirty_ = true;
}

bool StrobeHandle::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    if (!IsValid() || !dirty_)
    {
        return false;
    }

    bool emitted = false;
    mfd::UpdateStrobeCommand command;
    command.page = pageName_;
    command.pageId = pageTransportId_;
    const StrobeType* const selectedType = ResolveSelectedType();
    const bool selectionChanged = !SameStrobeSelection(
        desiredStrobeId_,
        desiredStrobeNameNormalized_,
        lastSentStrobeId_,
        lastSentStrobeNameNormalized_);

    if (selectionChanged)
    {
        if (selectedType != nullptr)
        {
            if (desiredStrobeId_ != 0)
            {
                command.strobeId = desiredStrobeId_;
            }
            else
            {
                command.strobe = selectedType->Name();
            }
        }

        lastSentStrobeId_ = desiredStrobeId_;
        lastSentStrobeNameNormalized_ = desiredStrobeNameNormalized_;
        emitted = true;
    }

    if ((selectionChanged && desiredActive_.has_value()) || !EqualOptional(desiredActive_, lastSentActive_))
    {
        command.active = desiredActive_;
        lastSentActive_ = desiredActive_;
        emitted = true;
    }

    if ((selectionChanged && desiredPosition_.has_value()) || !EqualOptional(desiredPosition_, lastSentPosition_))
    {
        command.position = desiredPosition_;
        lastSentPosition_ = desiredPosition_;
        emitted = true;
    }

    if (!emitted)
    {
        dirty_ = false;
        return false;
    }

    commands.emplace_back(std::move(command));
    dirty_ = false;
    return true;
}

const StrobeType* StrobeHandle::ResolveSelectedType() const noexcept
{
    if (strobes_.empty())
    {
        return nullptr;
    }

    if (desiredStrobeId_ != 0)
    {
        const auto iterator = std::find_if(
            strobes_.begin(),
            strobes_.end(),
            [this](const StrobeType& strobe)
            {
                return strobe.GeneratedId() == desiredStrobeId_;
            });
        if (iterator != strobes_.end())
        {
            return &(*iterator);
        }
    }

    if (!desiredStrobeNameNormalized_.empty())
    {
        const auto iterator = std::find_if(
            strobes_.begin(),
            strobes_.end(),
            [this](const StrobeType& strobe)
            {
                return NormalizeStrobeName(strobe.Name()) == desiredStrobeNameNormalized_;
            });
        if (iterator != strobes_.end())
        {
            return &(*iterator);
        }
    }

    return &strobes_.front();
}

void StrobeHandle::SelectDefaultStrobe() noexcept
{
    if (strobes_.empty())
    {
        desiredStrobeId_ = 0;
        desiredStrobeNameNormalized_.clear();
        return;
    }

    const auto iterator = std::find_if(
        strobes_.begin(),
        strobes_.end(),
        [](const StrobeType& strobe)
        {
            return strobe.IsDefaultActive();
        });
    const StrobeType& selected = iterator != strobes_.end() ? *iterator : strobes_.front();
    desiredStrobeId_ = selected.GeneratedId();
    desiredStrobeNameNormalized_ = NormalizeStrobeName(selected.Name());
}

DynamicReticle::DynamicReticle(const std::string_view reticleId) :
    reticleId_(reticleId),
    Blink(desiredPatch_, &dirty_)
{
}

void DynamicReticle::ClearDirty() noexcept
{
    dirty_ = false;
}

void DynamicReticle::Reset() noexcept
{
    seenThisCycle_ = false;
}

const std::string& DynamicReticle::Id() const noexcept
{
    return reticleId_;
}

bool DynamicReticle::IsAlive() const noexcept
{
    return alive_;
}

bool DynamicReticle::IsStrobeCaptured() const noexcept
{
    return alive_ &&
           feedbackState_ != nullptr &&
           feedbackPageTransportId_ != 0 &&
           runtimeReticleId_ != 0 &&
           feedbackState_->IsDynamicReticleCaptured(feedbackPageTransportId_, runtimeReticleId_);
}

void DynamicReticle::SetVisible(const bool visible)
{
    PatchSetVisible(desiredPatch_, visible);
    dirty_ = true;
}

void DynamicReticle::SetBlinkEnabled(const bool enabled)
{
    Blink.SetEnabled(enabled);
}

void DynamicReticle::SetBlink(const bool enabled, const BlinkType& blinkType)
{
    Blink.Set(enabled, blinkType);
}

void DynamicReticle::SetBlinkType(const BlinkType& blinkType)
{
    Blink.Use(blinkType);
}

void DynamicReticle::ClearBlinkType()
{
    Blink.ClearType();
}

void DynamicReticle::SetPosition(const mfd::Vec2 position)
{
    PatchSetPosition(desiredPatch_, position);
    dirty_ = true;
}

void DynamicReticle::SetRotationDegrees(const float rotationDegrees)
{
    PatchSetRotationDegrees(desiredPatch_, rotationDegrees);
    dirty_ = true;
}

void DynamicReticle::SetScale(const mfd::Vec2 scale)
{
    PatchSetScale(desiredPatch_, scale);
    dirty_ = true;
}

void DynamicReticle::SetColor(const mfd::ColorRgba color)
{
    PatchSetColor(desiredPatch_, color);
    dirty_ = true;
}

void DynamicReticle::SetThickness(const float thickness)
{
    PatchSetThickness(desiredPatch_, thickness);
    dirty_ = true;
}

void DynamicReticle::SetText(std::string value)
{
    PatchSetText(desiredPatch_, std::move(value));
    dirty_ = true;
}

void DynamicReticle::SetText(const std::string_view primitiveId, std::string value)
{
    PatchSetText(desiredPatch_, primitiveId, std::move(value));
    dirty_ = true;
}

void DynamicReticle::SetLetterSpacing(const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, letterSpacing);
    dirty_ = true;
}

void DynamicReticle::SetLetterSpacing(const std::string_view primitiveId, const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, primitiveId, letterSpacing);
    dirty_ = true;
}

mfd::ReticlePatch& DynamicReticle::MutableDesiredPatch() noexcept
{
    dirty_ = true;
    return desiredPatch_;
}

bool* DynamicReticle::DirtyFlag() noexcept
{
    return &dirty_;
}

std::unordered_map<std::string, mfd::TransportId>* DynamicReticle::PrimitiveTransportIds() noexcept
{
    return &primitiveTransportIds_;
}

const mfd::ReticlePatch& DynamicReticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

void DynamicReticle::BindRuntimeFeedback(const mfd::TransportId pageTransportId,
                                         RuntimeFeedbackState* feedbackState) noexcept
{
    feedbackPageTransportId_ = pageTransportId;
    feedbackState_ = feedbackState;
}

void DynamicReticle::ResetToAuthored() noexcept
{
    desiredPatch_ = {};
    lastSentPatch_ = {};
    dirty_ = false;
    seenThisCycle_ = false;
    published_ = false;
}

void DynamicReticle::MarkDead() noexcept
{
    alive_ = false;
    dirty_ = false;
    seenThisCycle_ = false;
}

void DynamicReticle::PopulateGeneratedIdentifiers(mfd::ReticlePatch& patch, const bool useGeneratedBlinkTypeId) const
{
    if (patch.blinkTypeId.has_value() && useGeneratedBlinkTypeId)
    {
        patch.blinkType.reset();
    }

    MoveGeneratedPrimitiveEntries(patch.texts, primitiveTransportIds_, patch.textsById);
    MoveGeneratedPrimitiveEntries(patch.letterSpacings, primitiveTransportIds_, patch.letterSpacingsById);
    MoveGeneratedPrimitiveEntries(patch.primitivePatches, primitiveTransportIds_, patch.primitivePatchesById);
}

GeneratedDynamicReticleSet::GeneratedDynamicReticleSet(const std::string_view pageName,
                                                       const std::string_view templateId,
                                                       const mfd::TransportId pageTransportId,
                                                       const mfd::TransportId templateTransportId,
                                                       RuntimeFeedbackState* feedbackState) :
    pageName_(pageName),
    templateId_(templateId),
    pageTransportId_(pageTransportId),
    templateTransportId_(templateTransportId),
    feedbackState_(feedbackState)
{
}

void GeneratedDynamicReticleSet::ClearDirty() noexcept
{
    for (DynamicEntry& entry : reticles_)
    {
        if (entry.reticle->IsAlive())
        {
            entry.reticle->ClearDirty();
        }
    }

    visibilityDirty_ = false;
    strobeMagnetDirty_ = false;
}

void GeneratedDynamicReticleSet::ResetToAuthored() noexcept
{
    desiredVisible_ = true;
    lastSentVisible_ = true;
    visibilityDirty_ = false;
    desiredStrobeMagnetEnabled_ = false;
    lastSentStrobeMagnetEnabled_ = false;
    strobeMagnetDirty_ = false;

    for (DynamicEntry& entry : reticles_)
    {
        entry.reticle->ResetToAuthored();
        entry.reticle->MarkDead();
        entry.removeRequested = false;
    }
}

void GeneratedDynamicReticleSet::Reset() noexcept
{
    ResetToAuthored();
}

void GeneratedDynamicReticleSet::SetVisible(const bool visible)
{
    desiredVisible_ = visible;
    visibilityDirty_ = true;
}

void GeneratedDynamicReticleSet::SetStrobeMagnetEnabled(const bool enabled)
{
    desiredStrobeMagnetEnabled_ = enabled;
    strobeMagnetDirty_ = true;
}

DynamicReticle& GeneratedDynamicReticleSet::Create()
{
    DynamicEntry entry;
    entry.reticle = CreateReticle(NextReticleId());
    BindReticleRuntimeFeedback(*entry.reticle);
    entry.reticle->runtimeReticleId_ = NextRuntimeReticleId();
    reticles_.push_back(std::move(entry));
    return *reticles_.back().reticle;
}

void GeneratedDynamicReticleSet::Remove(DynamicReticle& reticle)
{
    if (DynamicEntry* entry = FindEntry(reticle); entry != nullptr)
    {
        entry->reticle->MarkDead();
        entry->removeRequested = entry->reticle->published_;
    }
}

std::size_t GeneratedDynamicReticleSet::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    std::vector<mfd::DynamicReticleState> updates;
    updates.reserve(reticles_.size());
    std::size_t count = 0;

    if (visibilityDirty_ && desiredVisible_ != lastSentVisible_)
    {
        mfd::SetDynamicReticleSetVisibilityCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.visible = desiredVisible_;
        commands.emplace_back(std::move(command));
        lastSentVisible_ = desiredVisible_;
        ++count;
    }

    if (strobeMagnetDirty_ && desiredStrobeMagnetEnabled_ != lastSentStrobeMagnetEnabled_)
    {
        mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.enabled = desiredStrobeMagnetEnabled_;
        commands.emplace_back(std::move(command));
        lastSentStrobeMagnetEnabled_ = desiredStrobeMagnetEnabled_;
        ++count;
    }

    for (DynamicEntry& entry : reticles_)
    {
        DynamicReticle& reticle = *entry.reticle;

        if (!reticle.IsAlive())
        {
            if (entry.removeRequested && reticle.published_)
            {
                commands.emplace_back(mfd::RemoveDynamicReticleCommand {
                    mfd::DynamicReticleHandle {
                        pageName_,
                        reticle.reticleId_,
                        pageTransportId_,
                        reticle.runtimeReticleId_}});
                reticle.published_ = false;
                entry.removeRequested = false;
                ++count;
            }
            continue;
        }

        if (!reticle.published_)
        {
            mfd::ReticlePatch patch = reticle.desiredPatch_;
            reticle.PopulateGeneratedIdentifiers(patch, pageTransportId_ != 0);
            updates.push_back(
                mfd::DynamicReticleState {reticle.reticleId_, reticle.runtimeReticleId_, std::move(patch)});
            reticle.lastSentPatch_ = reticle.desiredPatch_;
            reticle.dirty_ = false;
            reticle.published_ = true;
            ++count;
            continue;
        }

        if (reticle.dirty_ && !Equal(reticle.desiredPatch_, reticle.lastSentPatch_))
        {
            mfd::ReticlePatch patch = BuildDeltaPatch(reticle.desiredPatch_, reticle.lastSentPatch_);
            reticle.PopulateGeneratedIdentifiers(patch, pageTransportId_ != 0);
            updates.push_back(
                mfd::DynamicReticleState {reticle.reticleId_, reticle.runtimeReticleId_, std::move(patch)});
            reticle.lastSentPatch_ = reticle.desiredPatch_;
            ++count;
        }

        reticle.dirty_ = false;
    }

    if (!updates.empty())
    {
        mfd::UpsertDynamicReticlesCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.reticles = std::move(updates);
        commands.emplace_back(std::move(command));
    }

    visibilityDirty_ = false;
    strobeMagnetDirty_ = false;

    return count;
}

std::size_t GeneratedDynamicReticleSet::AppendRemovalCommands(std::vector<mfd::UserCommand>& commands)
{
    std::size_t count = 0;

    for (DynamicEntry& entry : reticles_)
    {
        DynamicReticle& reticle = *entry.reticle;
        if (!reticle.IsAlive())
        {
            if (entry.removeRequested && reticle.published_)
            {
                commands.emplace_back(mfd::RemoveDynamicReticleCommand {
                    mfd::DynamicReticleHandle {
                        pageName_,
                        reticle.reticleId_,
                        pageTransportId_,
                        reticle.runtimeReticleId_}});
                ++count;
            }

            reticle.published_ = false;
            entry.removeRequested = false;
            reticle.MarkDead();
            continue;
        }

        if (!reticle.published_)
        {
            reticle.MarkDead();
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::DynamicReticleHandle {
                pageName_,
                reticle.reticleId_,
                pageTransportId_,
                reticle.runtimeReticleId_}});
        reticle.published_ = false;
        reticle.MarkDead();
        entry.removeRequested = false;
        ++count;
    }
    return count;
}

std::string GeneratedDynamicReticleSet::NextReticleId()
{
    return "__generated_dynamic_" + std::to_string(nextReticleSequence_++);
}

mfd::RuntimeDynamicId GeneratedDynamicReticleSet::NextRuntimeReticleId()
{
    if (runtimeIdSessionNonce_ == 0)
    {
        std::random_device device;
        runtimeIdSessionNonce_ = device();
        if (runtimeIdSessionNonce_ == 0)
        {
            runtimeIdSessionNonce_ = 1U;
        }
    }

    const mfd::RuntimeDynamicId sequence = static_cast<mfd::RuntimeDynamicId>(nextReticleSequence_ - 1U);
    return kGeneratedDynamicRuntimeIdBit |
           (static_cast<mfd::RuntimeDynamicId>(runtimeIdSessionNonce_) << 32U) |
           (sequence & 0xFFFFFFFFull);
}

GeneratedDynamicReticleSet::DynamicEntry* GeneratedDynamicReticleSet::FindEntry(const DynamicReticle& reticle) noexcept
{
    const auto iterator = std::find_if(
        reticles_.begin(),
        reticles_.end(),
        [&reticle](const DynamicEntry& entry)
        {
            return entry.reticle.get() == &reticle;
        });
    return iterator == reticles_.end() ? nullptr : &(*iterator);
}

void GeneratedDynamicReticleSet::BindReticleRuntimeFeedback(DynamicReticle& reticle) noexcept
{
    reticle.BindRuntimeFeedback(pageTransportId_, feedbackState_);
}

DynamicReticleSet::DynamicReticleSet(const std::string_view pageName,
                                     const std::string_view templateId,
                                     const mfd::TransportId pageTransportId,
                                     const mfd::TransportId templateTransportId,
                                     RuntimeFeedbackState* feedbackState) :
    pageName_(pageName),
    templateId_(templateId),
    pageTransportId_(pageTransportId),
    templateTransportId_(templateTransportId),
    feedbackState_(feedbackState)
{
}

void DynamicReticleSet::Reset() noexcept
{
    for (const auto& reticle : reticles_)
    {
        reticle->Reset();
    }
}

void DynamicReticleSet::SetVisible(const bool visible)
{
    desiredVisible_ = visible;
    visibilityDirty_ = true;
}

void DynamicReticleSet::SetStrobeMagnetEnabled(const bool enabled)
{
    desiredStrobeMagnetEnabled_ = enabled;
    strobeMagnetDirty_ = true;
}

DynamicReticle& DynamicReticleSet::Upsert(const std::string_view reticleId)
{
    if (DynamicReticle* existing = Find(reticleId); existing != nullptr)
    {
        existing->seenThisCycle_ = true;
        return *existing;
    }

    reticles_.push_back(std::make_unique<DynamicReticle>(reticleId));
    BindReticleRuntimeFeedback(*reticles_.back());
    reticles_.back()->seenThisCycle_ = true;
    return *reticles_.back();
}

std::size_t DynamicReticleSet::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    std::vector<mfd::DynamicReticleState> updates;
    updates.reserve(reticles_.size());
    std::size_t count = 0;

    if (visibilityDirty_ && desiredVisible_ != lastSentVisible_)
    {
        mfd::SetDynamicReticleSetVisibilityCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.visible = desiredVisible_;
        commands.emplace_back(std::move(command));
        lastSentVisible_ = desiredVisible_;
        ++count;
    }

    if (strobeMagnetDirty_ && desiredStrobeMagnetEnabled_ != lastSentStrobeMagnetEnabled_)
    {
        mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.enabled = desiredStrobeMagnetEnabled_;
        commands.emplace_back(std::move(command));
        lastSentStrobeMagnetEnabled_ = desiredStrobeMagnetEnabled_;
        ++count;
    }

    for (const auto& reticle : reticles_)
    {
        if (reticle->seenThisCycle_)
        {
            if (!reticle->published_)
            {
                mfd::ReticlePatch patch = reticle->desiredPatch_;
                reticle->PopulateGeneratedIdentifiers(patch, pageTransportId_ != 0);
                updates.push_back(
                    mfd::DynamicReticleState {reticle->reticleId_, reticle->runtimeReticleId_, std::move(patch)});
                reticle->lastSentPatch_ = reticle->desiredPatch_;
                ++count;
            }
            else if (reticle->dirty_ && !Equal(reticle->desiredPatch_, reticle->lastSentPatch_))
            {
                mfd::ReticlePatch patch = BuildDeltaPatch(reticle->desiredPatch_, reticle->lastSentPatch_);
                reticle->PopulateGeneratedIdentifiers(patch, pageTransportId_ != 0);
                updates.push_back(mfd::DynamicReticleState {
                    reticle->reticleId_,
                    reticle->runtimeReticleId_,
                    std::move(patch)});
                reticle->lastSentPatch_ = reticle->desiredPatch_;
                ++count;
            }

            reticle->dirty_ = false;
            reticle->published_ = true;
            continue;
        }

        if (!reticle->published_)
        {
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::DynamicReticleHandle {
                pageName_,
                reticle->reticleId_,
                pageTransportId_,
                reticle->runtimeReticleId_}});
        reticle->published_ = false;
        ++count;
    }

    if (!updates.empty())
    {
        mfd::UpsertDynamicReticlesCommand command;
        command.page = pageName_;
        command.pageId = pageTransportId_;
        command.templateId = templateId_;
        command.templateTransportId = templateTransportId_;
        command.reticles = std::move(updates);
        commands.emplace_back(std::move(command));
    }

    visibilityDirty_ = false;
    strobeMagnetDirty_ = false;
    return count;
}

std::size_t DynamicReticleSet::AppendRemovalCommands(std::vector<mfd::UserCommand>& commands)
{
    std::size_t count = 0;

    for (const auto& reticle : reticles_)
    {
        if (!reticle->published_)
        {
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::DynamicReticleHandle {
                pageName_,
                reticle->reticleId_,
                pageTransportId_,
                reticle->runtimeReticleId_}});
        reticle->published_ = false;
        ++count;
    }

    return count;
}

DynamicReticle* DynamicReticleSet::Find(const std::string_view reticleId) noexcept
{
    const auto iterator = std::find_if(
        reticles_.begin(),
        reticles_.end(),
        [reticleId](const std::unique_ptr<DynamicReticle>& candidate)
        {
            return candidate->Id() == reticleId;
        });
    return iterator == reticles_.end() ? nullptr : iterator->get();
}

void DynamicReticleSet::BindReticleRuntimeFeedback(DynamicReticle& reticle) noexcept
{
    reticle.BindRuntimeFeedback(pageTransportId_, feedbackState_);
}

void WindowDisplay::ClearDirty() noexcept
{
    dirty_ = false;
}

void WindowDisplay::Reset() noexcept
{
    ClearDirty();
}

void WindowDisplay::ResetToAuthored() noexcept
{
    desiredPatch_ = {};
    desiredPatch_.invertColors = false;
    desiredPatch_.brightness = 1.0f;
    desiredPatch_.disabled = false;
    lastSentPatch_ = desiredPatch_;
    dirty_ = false;
}

void WindowDisplay::SetColorInverted(const bool invertColors)
{
    desiredPatch_.invertColors = invertColors;
    dirty_ = true;
}

void WindowDisplay::SetBrightness(const float brightness)
{
    desiredPatch_.brightness = brightness;
    dirty_ = true;
}

void WindowDisplay::SetDisabled(const bool disabled)
{
    desiredPatch_.disabled = disabled;
    dirty_ = true;
}

bool WindowDisplay::AppendCommands(std::vector<mfd::UserCommand>& commands)
{
    if (!dirty_)
    {
        return false;
    }

    if (Equal(desiredPatch_, lastSentPatch_))
    {
        dirty_ = false;
        return false;
    }

    commands.emplace_back(mfd::UpdateWindowDisplayCommand {BuildDeltaPatch(desiredPatch_, lastSentPatch_)});
    lastSentPatch_ = desiredPatch_;
    dirty_ = false;
    return true;
}
} // namespace mfd::client
