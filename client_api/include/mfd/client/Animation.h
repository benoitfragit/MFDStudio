/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief High-level client-side helpers used to animate authored pages.
 */

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"

namespace mfd::client
{
class MFD_CLIENT_API BlinkType
{
public:
    explicit BlinkType(std::string_view name);

    const std::string& Name() const noexcept;

private:
    std::string name_;
};

class MFD_CLIENT_API ReticleBlink
{
public:
    ReticleBlink(mfd::ReticlePatch& patch, bool* dirty) noexcept;
    ReticleBlink(const ReticleBlink&) = delete;
    ReticleBlink& operator=(const ReticleBlink&) = delete;
    ReticleBlink(ReticleBlink&&) = delete;
    ReticleBlink& operator=(ReticleBlink&&) = delete;

    ReticleBlink& operator=(const BlinkType& blinkType);
    ReticleBlink& operator=(std::nullptr_t);

    void SetEnabled(bool enabled);
    void Set(bool enabled, const BlinkType& blinkType);
    void Use(const BlinkType& blinkType);
    void Disable();
    void ClearType();

private:
    void MarkDirty() noexcept;

    mfd::ReticlePatch* patch_ = nullptr;
    bool* dirty_ = nullptr;
};

/**
 * @brief Static strobe metadata exposed by the generated client API.
 */
struct StrobeInfo
{
    /** @brief Indicates whether the page actually exposes a strobe. */
    bool valid = false;
    /** @brief Authored capture configuration of the page strobe. */
    mfd::StrobeCaptureConfig capture {};
    /** @brief Authored magnetization configuration of the page strobe. */
    mfd::StrobeMagnetConfig magnet {};
};

/**
 * @brief Shared client-side handle mutating one named primitive inside a reticle patch.
 */
class MFD_CLIENT_API PrimitiveHandle
{
public:
    PrimitiveHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    const std::string& Id() const noexcept;

    void SetVisible(bool visible);
    void SetPosition(mfd::Vec2 position);
    void SetRotationDegrees(float rotationDegrees);
    void SetScale(mfd::Vec2 scale);
    void SetColor(mfd::ColorRgba color);
    void SetThickness(float thickness);

protected:
    mfd::PrimitivePatch& Patch() noexcept;
    void MarkDirty() noexcept;

private:
    mfd::ReticlePatch* patch_ = nullptr;
    bool* dirty_ = nullptr;
    std::string primitiveId_;
};

/**
 * @brief Primitive handle specialized for authored text primitives.
 */
class MFD_CLIENT_API TextHandle : public PrimitiveHandle
{
public:
    TextHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetText(std::string value);
    void SetLetterSpacing(float letterSpacing);
};

/**
 * @brief Primitive handle specialized for authored time primitives.
 */
class MFD_CLIENT_API TimeHandle : public PrimitiveHandle
{
public:
    TimeHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetLetterSpacing(float letterSpacing);
};

/**
 * @brief Primitive handle specialized for authored line primitives.
 */
class MFD_CLIENT_API LineHandle : public PrimitiveHandle
{
public:
    LineHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetStart(mfd::Vec2 start);
    void SetEnd(mfd::Vec2 end);
};

/**
 * @brief Primitive handle specialized for authored circle primitives.
 */
class MFD_CLIENT_API CircleHandle : public PrimitiveHandle
{
public:
    CircleHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetRadius(float radius);
};

/**
 * @brief Primitive handle specialized for authored ring primitives.
 */
class MFD_CLIENT_API RingHandle : public PrimitiveHandle
{
public:
    RingHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetInnerRadius(float radius);
    void SetOuterRadius(float radius);
};

/**
 * @brief Primitive handle specialized for authored rectangle primitives.
 */
class MFD_CLIENT_API RectangleHandle : public PrimitiveHandle
{
public:
    RectangleHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

/**
 * @brief Primitive handle specialized for authored ellipse primitives.
 */
class MFD_CLIENT_API EllipseHandle : public PrimitiveHandle
{
public:
    EllipseHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

/**
 * @brief Primitive handle specialized for authored square primitives.
 */
class MFD_CLIENT_API SquareHandle : public PrimitiveHandle
{
public:
    SquareHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

/**
 * @brief Primitive handle specialized for authored diamond primitives.
 */
class MFD_CLIENT_API DiamondHandle : public PrimitiveHandle
{
public:
    DiamondHandle(mfd::ReticlePatch& patch, bool* dirty, std::string_view primitiveId);

    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

class MFD_CLIENT_API Reticle
{
public:
    Reticle(std::string_view pageName, std::string_view reticleId);
    Reticle(const Reticle&) = delete;
    Reticle& operator=(const Reticle&) = delete;
    Reticle(Reticle&&) = delete;
    Reticle& operator=(Reticle&&) = delete;

    void Reset() noexcept;

    void SetVisible(bool visible);
    void SetBlinkEnabled(bool enabled);
    void SetBlink(bool enabled, const BlinkType& blinkType);
    void SetBlinkType(const BlinkType& blinkType);
    void ClearBlinkType();
    void SetPosition(mfd::Vec2 position);
    void SetRotationDegrees(float rotationDegrees);
    void SetColor(mfd::ColorRgba color);
    void SetThickness(float thickness);
    void SetText(std::string value);
    void SetText(std::string_view primitiveId, std::string value);
    void SetLetterSpacing(float letterSpacing);
    void SetLetterSpacing(std::string_view primitiveId, float letterSpacing);

    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

protected:
    const mfd::ReticlePatch& DesiredPatch() const noexcept;
    mfd::ReticlePatch& MutableDesiredPatch() noexcept;
    bool* DirtyFlag() noexcept;

private:
    std::string pageName_;
    std::string reticleId_;
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;

public:
    ReticleBlink Blink;
};

/**
 * @brief Page-scoped optional strobe control surface emitted by generated APIs.
 */
class MFD_CLIENT_API StrobeHandle
{
public:
    StrobeHandle(std::string_view pageName, StrobeInfo info = {});

    void Reset() noexcept;

    bool IsValid() const noexcept;
    std::string_view PageName() const noexcept;
    const StrobeInfo& Info() const noexcept;

    void SetActive(bool active);
    void SetPosition(mfd::Vec2 position);

    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

private:
    std::string pageName_;
    StrobeInfo info_ {};
    std::optional<bool> desiredActive_ {};
    std::optional<bool> lastSentActive_ {};
    std::optional<mfd::Vec2> desiredPosition_ {};
    std::optional<mfd::Vec2> lastSentPosition_ {};
    bool dirty_ = false;
};

class MFD_CLIENT_API TextReticle final : public Reticle
{
public:
    TextReticle(std::string_view pageName, std::string_view reticleId, std::string_view primitiveId);

    void SetValue(std::string value);

private:
    std::string primitiveId_;
};

class MFD_CLIENT_API DynamicReticle
{
public:
    explicit DynamicReticle(std::string_view reticleId);
    DynamicReticle(const DynamicReticle&) = delete;
    DynamicReticle& operator=(const DynamicReticle&) = delete;
    DynamicReticle(DynamicReticle&&) = delete;
    DynamicReticle& operator=(DynamicReticle&&) = delete;

    void Reset() noexcept;

    const std::string& Id() const noexcept;

    void SetVisible(bool visible);
    void SetBlinkEnabled(bool enabled);
    void SetBlink(bool enabled, const BlinkType& blinkType);
    void SetBlinkType(const BlinkType& blinkType);
    void ClearBlinkType();
    void SetPosition(mfd::Vec2 position);
    void SetRotationDegrees(float rotationDegrees);
    void SetColor(mfd::ColorRgba color);
    void SetThickness(float thickness);
    void SetText(std::string value);
    void SetText(std::string_view primitiveId, std::string value);
    void SetLetterSpacing(float letterSpacing);
    void SetLetterSpacing(std::string_view primitiveId, float letterSpacing);

private:
    friend class DynamicReticleSet;

    const mfd::ReticlePatch& DesiredPatch() const noexcept;

    std::string reticleId_;
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;
    bool seenThisCycle_ = false;
    bool published_ = false;

public:
    ReticleBlink Blink;
};

class MFD_CLIENT_API DynamicReticleSet
{
public:
    DynamicReticleSet(std::string_view pageName, std::string_view templateId);
    DynamicReticleSet(const DynamicReticleSet&) = delete;
    DynamicReticleSet& operator=(const DynamicReticleSet&) = delete;
    DynamicReticleSet(DynamicReticleSet&&) = delete;
    DynamicReticleSet& operator=(DynamicReticleSet&&) = delete;

    void Reset() noexcept;
    /** @brief Enables or disables all dynamic reticles of this page/template set at runtime. */
    void SetVisible(bool visible);
    DynamicReticle& Upsert(std::string_view reticleId);
    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);
    std::size_t AppendRemovalCommands(std::vector<mfd::UserCommand>& commands);

private:
    DynamicReticle* Find(std::string_view reticleId) noexcept;

    std::string pageName_;
    std::string templateId_;
    std::vector<std::unique_ptr<DynamicReticle>> reticles_ {};
    bool desiredVisible_ = true;
    bool lastSentVisible_ = true;
    bool visibilityDirty_ = false;
};

class MFD_CLIENT_API WindowDisplay
{
public:
    void Reset() noexcept;

    void SetColorInverted(bool invertColors);
    void SetBrightness(float brightness);
    void SetDisabled(bool disabled);

    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

private:
    mfd::WindowDisplayPatch desiredPatch_ {};
    mfd::WindowDisplayPatch lastSentPatch_ {};
    bool dirty_ = false;
};
} // namespace mfd::client
