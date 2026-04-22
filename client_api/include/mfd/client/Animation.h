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
#include <unordered_map>
#include <vector>

#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"

namespace mfd::client
{
class MFD_CLIENT_API BlinkType
{
public:
    explicit BlinkType(std::string_view name, mfd::TransportId transportId = 0);

    const std::string& Name() const noexcept;
    mfd::TransportId GeneratedId() const noexcept;

private:
    std::string name_;
    mfd::TransportId transportId_ = 0;
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
    PrimitiveHandle(mfd::ReticlePatch& patch,
                    bool* dirty,
                    std::string_view primitiveId,
                    mfd::TransportId transportId = 0,
                    std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    const std::string& Id() const noexcept;
    mfd::TransportId GeneratedId() const noexcept;

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
    mfd::TransportId transportId_ = 0;
};

/**
 * @brief Primitive handle specialized for authored text primitives.
 */
class MFD_CLIENT_API TextHandle : public PrimitiveHandle
{
public:
    TextHandle(mfd::ReticlePatch& patch,
               bool* dirty,
               std::string_view primitiveId,
               mfd::TransportId transportId = 0,
               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetText(std::string value);
    void SetLetterSpacing(float letterSpacing);
};

/**
 * @brief Primitive handle specialized for authored time primitives.
 */
class MFD_CLIENT_API TimeHandle : public PrimitiveHandle
{
public:
    TimeHandle(mfd::ReticlePatch& patch,
               bool* dirty,
               std::string_view primitiveId,
               mfd::TransportId transportId = 0,
               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetLetterSpacing(float letterSpacing);
};

/**
 * @brief Primitive handle specialized for authored line primitives.
 */
class MFD_CLIENT_API LineHandle : public PrimitiveHandle
{
public:
    LineHandle(mfd::ReticlePatch& patch,
               bool* dirty,
               std::string_view primitiveId,
               mfd::TransportId transportId = 0,
               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetStart(mfd::Vec2 start);
    void SetEnd(mfd::Vec2 end);
};

/**
 * @brief Primitive handle specialized for authored circle primitives.
 */
class MFD_CLIENT_API CircleHandle : public PrimitiveHandle
{
public:
    CircleHandle(mfd::ReticlePatch& patch,
                 bool* dirty,
                 std::string_view primitiveId,
                 mfd::TransportId transportId = 0,
                 std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetRadius(float radius);
};

/**
 * @brief Primitive handle specialized for authored ring primitives.
 */
class MFD_CLIENT_API RingHandle : public PrimitiveHandle
{
public:
    RingHandle(mfd::ReticlePatch& patch,
               bool* dirty,
               std::string_view primitiveId,
               mfd::TransportId transportId = 0,
               std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetInnerRadius(float radius);
    void SetOuterRadius(float radius);
};

/**
 * @brief Primitive handle specialized for authored rectangle primitives.
 */
class MFD_CLIENT_API RectangleHandle : public PrimitiveHandle
{
public:
    RectangleHandle(mfd::ReticlePatch& patch,
                    bool* dirty,
                    std::string_view primitiveId,
                    mfd::TransportId transportId = 0,
                    std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

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
    EllipseHandle(mfd::ReticlePatch& patch,
                  bool* dirty,
                  std::string_view primitiveId,
                  mfd::TransportId transportId = 0,
                  std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

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
    SquareHandle(mfd::ReticlePatch& patch,
                 bool* dirty,
                 std::string_view primitiveId,
                 mfd::TransportId transportId = 0,
                 std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

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
    DiamondHandle(mfd::ReticlePatch& patch,
                  bool* dirty,
                  std::string_view primitiveId,
                  mfd::TransportId transportId = 0,
                  std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

class MFD_CLIENT_API Reticle
{
public:
    Reticle(std::string_view pageName,
            std::string_view reticleId,
            mfd::TransportId pageTransportId = 0,
            mfd::TransportId reticleTransportId = 0);
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
    std::unordered_map<std::string, mfd::TransportId>* PrimitiveTransportIds() noexcept;

private:
    void PopulateGeneratedIdentifiers(mfd::UpdateReticleCommand& command) const;

    std::string pageName_;
    std::string reticleId_;
    mfd::TransportId pageTransportId_ = 0;
    mfd::TransportId reticleTransportId_ = 0;
    std::unordered_map<std::string, mfd::TransportId> primitiveTransportIds_ {};
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
    StrobeHandle(std::string_view pageName, StrobeInfo info = {}, mfd::TransportId pageTransportId = 0);

    void Reset() noexcept;

    bool IsValid() const noexcept;
    std::string_view PageName() const noexcept;
    const StrobeInfo& Info() const noexcept;

    void SetActive(bool active);
    void SetPosition(mfd::Vec2 position);

    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

private:
    std::string pageName_;
    mfd::TransportId pageTransportId_ = 0;
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
    TextReticle(std::string_view pageName,
                std::string_view reticleId,
                std::string_view primitiveId,
                mfd::TransportId pageTransportId = 0,
                mfd::TransportId reticleTransportId = 0,
                mfd::TransportId primitiveTransportId = 0);

    void SetValue(std::string value);

private:
    std::string primitiveId_;
};

class MFD_CLIENT_API DynamicReticle
{
public:
    explicit DynamicReticle(std::string_view reticleId);
    virtual ~DynamicReticle() = default;
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

protected:
    mfd::ReticlePatch& MutableDesiredPatch() noexcept;
    bool* DirtyFlag() noexcept;
    std::unordered_map<std::string, mfd::TransportId>* PrimitiveTransportIds() noexcept;

private:
    friend class DynamicReticleSet;
    friend class GeneratedDynamicReticleSet;

    const mfd::ReticlePatch& DesiredPatch() const noexcept;
    void PopulateGeneratedIdentifiers(mfd::ReticlePatch& patch, bool useGeneratedBlinkTypeId) const;

    std::string reticleId_;
    mfd::RuntimeDynamicId runtimeReticleId_ = 0;
    std::unordered_map<std::string, mfd::TransportId> primitiveTransportIds_ {};
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;
    bool seenThisCycle_ = false;
    bool published_ = false;

public:
    ReticleBlink Blink;
};

/**
 * @brief Generated high-level dynamic-reticle set hiding internal instance identifiers.
 */
class MFD_CLIENT_API GeneratedDynamicReticleSet
{
public:
    GeneratedDynamicReticleSet(std::string_view pageName,
                               std::string_view templateId,
                               mfd::TransportId pageTransportId = 0,
                               mfd::TransportId templateTransportId = 0);
    virtual ~GeneratedDynamicReticleSet() = default;
    GeneratedDynamicReticleSet(const GeneratedDynamicReticleSet&) = delete;
    GeneratedDynamicReticleSet& operator=(const GeneratedDynamicReticleSet&) = delete;
    GeneratedDynamicReticleSet(GeneratedDynamicReticleSet&&) = delete;
    GeneratedDynamicReticleSet& operator=(GeneratedDynamicReticleSet&&) = delete;

    void Reset() noexcept;
    /** @brief Enables or disables all generated dynamic reticles of this page/template set at runtime. */
    void SetVisible(bool visible);
    DynamicReticle& Create();
    void Remove(DynamicReticle& reticle);
    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);
    std::size_t AppendRemovalCommands(std::vector<mfd::UserCommand>& commands);

protected:
    virtual std::unique_ptr<DynamicReticle> CreateReticle(std::string_view reticleId) = 0;

private:
    struct DynamicEntry
    {
        std::unique_ptr<DynamicReticle> reticle {};
        bool removeRequested = false;
    };

    std::string NextReticleId();
    mfd::RuntimeDynamicId NextRuntimeReticleId();
    DynamicEntry* FindEntry(const DynamicReticle& reticle) noexcept;

    std::string pageName_;
    std::string templateId_;
    mfd::TransportId pageTransportId_ = 0;
    mfd::TransportId templateTransportId_ = 0;
    std::vector<DynamicEntry> reticles_ {};
    bool desiredVisible_ = true;
    bool lastSentVisible_ = true;
    bool visibilityDirty_ = false;
    std::uint32_t runtimeIdSessionNonce_ = 0;
    std::uint64_t nextReticleSequence_ = 1;
};

class MFD_CLIENT_API DynamicReticleSet
{
public:
    DynamicReticleSet(std::string_view pageName,
                      std::string_view templateId,
                      mfd::TransportId pageTransportId = 0,
                      mfd::TransportId templateTransportId = 0);
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
    mfd::TransportId pageTransportId_ = 0;
    mfd::TransportId templateTransportId_ = 0;
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
