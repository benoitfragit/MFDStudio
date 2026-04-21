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
#include <optional>
#include <utility>

namespace mfd::client
{
namespace
{
bool Equal(const mfd::Vec2& lhs, const mfd::Vec2& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool Equal(const mfd::ColorRgba& lhs, const mfd::ColorRgba& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
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

bool Equal(const mfd::PrimitivePatch& lhs, const mfd::PrimitivePatch& rhs)
{
    return EqualOptional(lhs.visible, rhs.visible) &&
           EqualOptional(lhs.position, rhs.position) &&
           EqualOptional(lhs.rotationDegrees, rhs.rotationDegrees) &&
           EqualOptional(lhs.scale, rhs.scale) &&
           EqualOptional(lhs.color, rhs.color) &&
           EqualOptional(lhs.thickness, rhs.thickness) &&
           EqualOptional(lhs.text, rhs.text) &&
           EqualOptional(lhs.letterSpacing, rhs.letterSpacing) &&
           EqualOptional(lhs.lineStart, rhs.lineStart) &&
           EqualOptional(lhs.lineEnd, rhs.lineEnd) &&
           EqualOptional(lhs.radius, rhs.radius) &&
           EqualOptional(lhs.innerRadius, rhs.innerRadius) &&
           EqualOptional(lhs.outerRadius, rhs.outerRadius) &&
           EqualOptional(lhs.width, rhs.width) &&
           EqualOptional(lhs.height, rhs.height) &&
           EqualOptional(lhs.size, rhs.size);
}

bool Equal(const mfd::ReticlePatch& lhs, const mfd::ReticlePatch& rhs)
{
    return EqualOptional(lhs.visible, rhs.visible) &&
           EqualOptional(lhs.blinkEnabled, rhs.blinkEnabled) &&
           EqualOptional(lhs.blinkType, rhs.blinkType) &&
           EqualOptional(lhs.position, rhs.position) &&
           EqualOptional(lhs.rotationDegrees, rhs.rotationDegrees) &&
           EqualOptional(lhs.color, rhs.color) &&
           EqualOptional(lhs.thickness, rhs.thickness) &&
           EqualOptional(lhs.text, rhs.text) &&
           lhs.texts == rhs.texts &&
           EqualOptional(lhs.letterSpacing, rhs.letterSpacing) &&
           lhs.letterSpacings == rhs.letterSpacings &&
           [&lhs, &rhs]() -> bool
           {
               if (lhs.primitivePatches.size() != rhs.primitivePatches.size())
               {
                   return false;
               }

               for (const auto& [primitiveId, primitivePatch] : lhs.primitivePatches)
               {
                   const auto rightIt = rhs.primitivePatches.find(primitiveId);
                   if (rightIt == rhs.primitivePatches.end())
                   {
                       return false;
                   }

                   if (!Equal(primitivePatch, rightIt->second))
                   {
                       return false;
                   }
               }

               return true;
           }();
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

mfd::PrimitivePatch BuildDeltaPrimitivePatch(const mfd::PrimitivePatch& desired,
                                            const mfd::PrimitivePatch& previous)
{
    mfd::PrimitivePatch delta;
    CopyChangedOptionalField(desired.visible, previous.visible, delta.visible);
    CopyChangedOptionalField(desired.position, previous.position, delta.position);
    CopyChangedOptionalField(desired.rotationDegrees, previous.rotationDegrees, delta.rotationDegrees);
    CopyChangedOptionalField(desired.scale, previous.scale, delta.scale);
    CopyChangedOptionalField(desired.color, previous.color, delta.color);
    CopyChangedOptionalField(desired.thickness, previous.thickness, delta.thickness);
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
    CopyChangedOptionalField(desired.position, previous.position, delta.position);
    CopyChangedOptionalField(desired.rotationDegrees, previous.rotationDegrees, delta.rotationDegrees);
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

BlinkType::BlinkType(const std::string_view name) :
    name_(name)
{
}

const std::string& BlinkType::Name() const noexcept
{
    return name_;
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
    }
    else
    {
        PatchSetBlink(*patch_, false, {});
    }

    MarkDirty();
}

void ReticleBlink::Use(const BlinkType& blinkType)
{
    PatchSetBlinkType(*patch_, blinkType.Name());
    MarkDirty();
}

void ReticleBlink::Disable()
{
    PatchSetBlink(*patch_, false, {});
    MarkDirty();
}

void ReticleBlink::ClearType()
{
    PatchClearBlinkType(*patch_);
    MarkDirty();
}

void ReticleBlink::MarkDirty() noexcept
{
    if (dirty_ != nullptr)
    {
        *dirty_ = true;
    }
}

PrimitiveHandle::PrimitiveHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    patch_(&patch),
    dirty_(dirty),
    primitiveId_(primitiveId)
{
}

const std::string& PrimitiveHandle::Id() const noexcept
{
    return primitiveId_;
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

void PrimitiveHandle::SetThickness(const float thickness)
{
    Patch().thickness = thickness;
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

TextHandle::TextHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
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

TimeHandle::TimeHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
}

void TimeHandle::SetLetterSpacing(const float letterSpacing)
{
    Patch().letterSpacing = letterSpacing;
    MarkDirty();
}

LineHandle::LineHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
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

CircleHandle::CircleHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
}

void CircleHandle::SetRadius(const float radius)
{
    Patch().radius = radius;
    MarkDirty();
}

RingHandle::RingHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
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

RectangleHandle::RectangleHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
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

EllipseHandle::EllipseHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
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

SquareHandle::SquareHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
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

DiamondHandle::DiamondHandle(mfd::ReticlePatch& patch, bool* dirty, const std::string_view primitiveId) :
    PrimitiveHandle(patch, dirty, primitiveId)
{
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

Reticle::Reticle(const std::string_view pageName, const std::string_view reticleId) :
    pageName_(pageName),
    reticleId_(reticleId),
    Blink(desiredPatch_, &dirty_)
{
}

void Reticle::Reset() noexcept
{
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
        return false;
    }

    const mfd::ReticlePatch deltaPatch = BuildDeltaPatch(desiredPatch_, lastSentPatch_);
    commands.emplace_back(mfd::UpdateReticleCommand {
        mfd::ReticleHandle {pageName_, reticleId_},
        deltaPatch});
    lastSentPatch_ = desiredPatch_;
    return true;
}

const mfd::ReticlePatch& Reticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

mfd::ReticlePatch& Reticle::MutableDesiredPatch() noexcept
{
    return desiredPatch_;
}

bool* Reticle::DirtyFlag() noexcept
{
    return &dirty_;
}

TextReticle::TextReticle(const std::string_view pageName,
                         const std::string_view reticleId,
                         const std::string_view primitiveId) :
    Reticle(pageName, reticleId),
    primitiveId_(primitiveId)
{
}

void TextReticle::SetValue(std::string value)
{
    SetText(primitiveId_, std::move(value));
}

StrobeHandle::StrobeHandle(const std::string_view pageName, StrobeInfo info) :
    pageName_(pageName),
    info_(std::move(info))
{
}

void StrobeHandle::Reset() noexcept
{
    dirty_ = false;
}

bool StrobeHandle::IsValid() const noexcept
{
    return info_.valid;
}

std::string_view StrobeHandle::PageName() const noexcept
{
    return pageName_;
}

const StrobeInfo& StrobeHandle::Info() const noexcept
{
    return info_;
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

    if (!EqualOptional(desiredActive_, lastSentActive_))
    {
        command.active = desiredActive_;
        lastSentActive_ = desiredActive_;
        emitted = true;
    }

    if (!EqualOptional(desiredPosition_, lastSentPosition_))
    {
        command.position = desiredPosition_;
        lastSentPosition_ = desiredPosition_;
        emitted = true;
    }

    if (!emitted)
    {
        return false;
    }

    commands.emplace_back(std::move(command));
    return true;
}

DynamicReticle::DynamicReticle(const std::string_view reticleId) :
    reticleId_(reticleId),
    Blink(desiredPatch_, &dirty_)
{
}

void DynamicReticle::Reset() noexcept
{
    seenThisCycle_ = false;
}

const std::string& DynamicReticle::Id() const noexcept
{
    return reticleId_;
}

void DynamicReticle::SetVisible(const bool visible)
{
    PatchSetVisible(desiredPatch_, visible);
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
}

void DynamicReticle::SetRotationDegrees(const float rotationDegrees)
{
    PatchSetRotationDegrees(desiredPatch_, rotationDegrees);
}

void DynamicReticle::SetColor(const mfd::ColorRgba color)
{
    PatchSetColor(desiredPatch_, color);
}

void DynamicReticle::SetThickness(const float thickness)
{
    PatchSetThickness(desiredPatch_, thickness);
}

void DynamicReticle::SetText(std::string value)
{
    PatchSetText(desiredPatch_, std::move(value));
}

void DynamicReticle::SetText(const std::string_view primitiveId, std::string value)
{
    PatchSetText(desiredPatch_, primitiveId, std::move(value));
}

void DynamicReticle::SetLetterSpacing(const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, letterSpacing);
}

void DynamicReticle::SetLetterSpacing(const std::string_view primitiveId, const float letterSpacing)
{
    PatchSetLetterSpacing(desiredPatch_, primitiveId, letterSpacing);
}

const mfd::ReticlePatch& DynamicReticle::DesiredPatch() const noexcept
{
    return desiredPatch_;
}

DynamicReticleSet::DynamicReticleSet(const std::string_view pageName, const std::string_view templateId) :
    pageName_(pageName),
    templateId_(templateId)
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

DynamicReticle& DynamicReticleSet::Upsert(const std::string_view reticleId)
{
    if (DynamicReticle* existing = Find(reticleId); existing != nullptr)
    {
        existing->seenThisCycle_ = true;
        return *existing;
    }

    reticles_.push_back(std::make_unique<DynamicReticle>(reticleId));
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
        commands.emplace_back(mfd::SetDynamicReticleSetVisibilityCommand {
            pageName_,
            templateId_,
            desiredVisible_});
        lastSentVisible_ = desiredVisible_;
        ++count;
    }

    for (const auto& reticle : reticles_)
    {
        if (reticle->seenThisCycle_)
        {
            if (!reticle->published_)
            {
                updates.push_back(mfd::DynamicReticleState {reticle->reticleId_, reticle->desiredPatch_});
                reticle->lastSentPatch_ = reticle->desiredPatch_;
                ++count;
            }
            else if (!Equal(reticle->desiredPatch_, reticle->lastSentPatch_))
            {
                updates.push_back(mfd::DynamicReticleState {
                    reticle->reticleId_,
                    BuildDeltaPatch(reticle->desiredPatch_, reticle->lastSentPatch_)});
                reticle->lastSentPatch_ = reticle->desiredPatch_;
                ++count;
            }

            reticle->published_ = true;
            continue;
        }

        if (!reticle->published_)
        {
            continue;
        }

        commands.emplace_back(mfd::RemoveDynamicReticleCommand {
            mfd::ReticleHandle {pageName_, reticle->reticleId_}});
        reticle->published_ = false;
        ++count;
    }

    if (!updates.empty())
    {
        commands.emplace_back(mfd::UpsertDynamicReticlesCommand {
            pageName_,
            templateId_,
            std::move(updates)});
    }

    visibilityDirty_ = false;
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
            mfd::ReticleHandle {pageName_, reticle->reticleId_}});
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

void WindowDisplay::Reset() noexcept
{
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
        return false;
    }

    commands.emplace_back(mfd::UpdateWindowDisplayCommand {BuildDeltaPatch(desiredPatch_, lastSentPatch_)});
    lastSentPatch_ = desiredPatch_;
    return true;
}
} // namespace mfd::client
