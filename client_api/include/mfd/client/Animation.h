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

private:
    std::string pageName_;
    std::string reticleId_;
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;

public:
    ReticleBlink Blink;
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
