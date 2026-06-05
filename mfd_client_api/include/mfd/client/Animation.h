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

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/PageName.h"
#include "mfd/model/PageDefinition.h"

namespace mfd::client
{
using LineStyle = mfd::LineStyle;

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
 * @brief One selectable authored strobe entry exposed by generated page APIs.
 *
 * @note This type stays page-scoped. Client code selects one of the generated
 * entries through `StrobeHandle`, without addressing transport labels directly.
 */
class MFD_CLIENT_API StrobeType
{
public:
    /**
     * @brief Builds one authored strobe catalog entry.
     * @param name User-facing strobe name emitted by the generator.
     * @param info Authored capture and magnet configuration of the strobe.
     * @param transportId Stable generated transport id of the strobe entry.
     * @param defaultActive Indicates whether this strobe is the authored default on its page.
     */
    explicit StrobeType(std::string_view name,
                        StrobeInfo info = {},
                        mfd::TransportId transportId = 0,
                        bool defaultActive = false);

    /**
     * @brief Returns the user-facing name of the strobe entry.
     * @return Stable name emitted by the generator.
     */
    const std::string& Name() const noexcept;

    /**
     * @brief Returns the generated transport id of the strobe entry.
     * @return Stable generated id, or `0` when the entry has no generated id.
     */
    mfd::TransportId GeneratedId() const noexcept;

    /**
     * @brief Returns the authored capture and magnet configuration of the strobe.
     * @return Immutable authored strobe metadata.
     */
    const StrobeInfo& Info() const noexcept;

    /**
     * @brief Returns whether the authored page selects this strobe by default.
     * @return `true` when this entry is the authored startup strobe.
     */
    bool IsDefaultActive() const noexcept;

private:
    std::string name_;
    StrobeInfo info_ {};
    mfd::TransportId transportId_ = 0;
    bool defaultActive_ = false;
};

/**
 * @brief Client-side runtime feedback tracker used by generated page and dynamic APIs.
 *
 * @note This tracker stores only the authoritative runtime state needed by the
 * generated API convenience queries such as `Page.IsActive()` and
 * `DynamicReticle.IsStrobeCaptured()`.
 */
class MFD_CLIENT_API RuntimeFeedbackState
{
public:
    /**
     * @brief Applies one decoded strobe feedback payload.
     * @param feedback Runtime feedback emitted by the window.
     * @return `true` when the internal state changed.
     */
    bool Apply(const mfd::StrobeStatusFeedback& feedback);

    /**
     * @brief Applies one decoded active-page feedback payload.
     * @param feedback Runtime feedback emitted by the window.
     * @return `true` when the internal state changed.
     */
    bool Apply(const mfd::ActivePageFeedback& feedback);

    /**
     * @brief Decodes and applies one binary runtime feedback payload.
     * @param payload Raw Protocol Buffers payload received from the window.
     * @param error Optional output string receiving the parsing error.
     * @return `true` when the payload was recognized and changed the state.
     */
    bool ApplyPayload(std::string_view payload, std::string* error = nullptr);

    /**
     * @brief Drains runtime feedback packets from a channel and applies them.
     * @param channel Receiver channel providing raw feedback packets.
     * @param maxMessages Maximum number of packets consumed during this call.
     * @param error Optional output string receiving the last decoding error.
     * @return Number of packets that changed the tracked runtime state.
     */
    std::size_t Poll(mfd::IExchangeChannel& channel,
                     std::size_t maxMessages = 64,
                     std::string* error = nullptr);

    /**
     * @brief Clears all tracked runtime feedback state.
     */
    void Reset() noexcept;

    /**
     * @brief Returns whether any active-page feedback has been received.
     * @return `true` once the runtime reported an authoritative active page.
     */
    bool HasActivePage() const noexcept;

    /**
     * @brief Returns the last authoritative active page name.
     * @return Empty string when no active-page feedback has been received yet.
     */
    const std::string& ActivePageName() const noexcept;

    /**
     * @brief Returns whether one authored page is currently active at render time.
     * @param pageName Authored page name to query.
     * @return `true` only when the runtime reported that page as active.
     */
    bool IsPageActive(std::string_view pageName) const noexcept;

    /**
     * @brief Returns whether one dynamic reticle instance is currently captured by its page strobe.
     * @param pageId Generated transport id of the page owning the reticle.
     * @param runtimeReticleId Runtime-scoped identifier of the dynamic reticle instance.
     * @return `true` only when the latest authoritative feedback for the
     * currently active page reports that exact runtime reticle instance as
     * captured.
     */
    bool IsDynamicReticleCaptured(mfd::TransportId pageId, mfd::RuntimeDynamicId runtimeReticleId) const noexcept;

private:
    struct PageCaptureState
    {
        std::uint32_t lastStrobeSequence = 0;
        bool hasSequence = false;
        bool captured = false;
        mfd::RuntimeDynamicId capturedRuntimeReticleId = 0;
        std::string pageNameNormalized {};
    };

    std::unordered_map<mfd::TransportId, PageCaptureState> pageCaptureById_ {};
    std::string activePageName_ {};
    std::string activePageNameNormalized_ {};
    std::uint32_t lastActivePageSequence_ = 0;
    bool hasActivePage_ = false;
};

/**
 * @brief Shared client-side handle mutating one named primitive inside a reticle patch.
 *
 * @note Fill controls are intentionally exposed only by fill-capable specialized
 * handles such as `RectangleHandle` or `ArcHandle`.
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
    /** @brief Stages one primitive-local non-uniform scale override. */
    void SetScale(mfd::Vec2 scale);
    void SetColor(mfd::ColorRgba color);
    void SetThickness(float thickness);
    void SetLineStyle(LineStyle lineStyle);

protected:
    mfd::PrimitivePatch& Patch() noexcept;
    void MarkDirty() noexcept;
    /** @brief Stages one fill color override for fill-capable primitive handles. */
    void SetFillColorInternal(mfd::ColorRgba color);
    /** @brief Stages one fill toggle override for fill-capable primitive handles. */
    void SetFilledInternal(bool filled);

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

    /** @brief Stages one fill color override for this circle. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this circle is rendered filled. */
    void SetFilled(bool filled);
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

    /** @brief Stages one fill color override for this ring. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this ring is rendered filled. */
    void SetFilled(bool filled);
    void SetInnerRadius(float radius);
    void SetOuterRadius(float radius);
    void SetSegments(int segments);
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

    /** @brief Stages one fill color override for this rectangle. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this rectangle is rendered filled. */
    void SetFilled(bool filled);
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

    /** @brief Stages one fill color override for this ellipse. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this ellipse is rendered filled. */
    void SetFilled(bool filled);
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

    /** @brief Stages one fill color override for this square. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this square is rendered filled. */
    void SetFilled(bool filled);
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

    /** @brief Stages one fill color override for this diamond. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this diamond is rendered filled. */
    void SetFilled(bool filled);
    void SetWidth(float width);
    void SetHeight(float height);
    void SetSize(mfd::Vec2 size);
};

/**
 * @brief Primitive handle specialized for authored triangle primitives.
 */
class MFD_CLIENT_API TriangleHandle : public PrimitiveHandle
{
public:
    TriangleHandle(mfd::ReticlePatch& patch,
                   bool* dirty,
                   std::string_view primitiveId,
                   mfd::TransportId transportId = 0,
                   std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    /** @brief Stages one fill color override for this triangle. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this triangle is rendered filled. */
    void SetFilled(bool filled);
    void SetPoints(const std::array<mfd::Vec2, 3>& points);
};

/**
 * @brief Primitive handle specialized for authored polyline primitives.
 */
class MFD_CLIENT_API PolylineHandle : public PrimitiveHandle
{
public:
    PolylineHandle(mfd::ReticlePatch& patch,
                   bool* dirty,
                   std::string_view primitiveId,
                   mfd::TransportId transportId = 0,
                   std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    /** @brief Stages one fill color override for this polyline when it is closed. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this polyline is rendered filled when it is closed. */
    void SetFilled(bool filled);
    void SetPoints(std::vector<mfd::Vec2> points);
    void SetClosed(bool closed);
};

/**
 * @brief Primitive handle specialized for authored bezier primitives.
 */
class MFD_CLIENT_API BezierHandle : public PrimitiveHandle
{
public:
    BezierHandle(mfd::ReticlePatch& patch,
                 bool* dirty,
                 std::string_view primitiveId,
                 mfd::TransportId transportId = 0,
                 std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    void SetControlPoints(std::vector<mfd::Vec2> controlPoints);
    void SetSegments(int segments);
};

/**
 * @brief Primitive handle specialized for authored arc primitives.
 */
class MFD_CLIENT_API ArcHandle : public PrimitiveHandle
{
public:
    ArcHandle(mfd::ReticlePatch& patch,
              bool* dirty,
              std::string_view primitiveId,
              mfd::TransportId transportId = 0,
              std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);

    /** @brief Stages one fill color override for this arc sector. */
    void SetFillColor(mfd::ColorRgba color);
    /** @brief Stages whether this arc is rendered as a filled sector. */
    void SetFilled(bool filled);
    void SetRadius(float radius);
    void SetStartAngleDegrees(float startAngleDegrees);
    void SetEndAngleDegrees(float endAngleDegrees);
    void SetSegments(int segments);
};

/**
 * @brief Primitive handle specialized for authored image primitives.
 */
class MFD_CLIENT_API ImageHandle : public PrimitiveHandle
{
public:
    ImageHandle(mfd::ReticlePatch& patch,
                bool* dirty,
                std::string_view primitiveId,
                mfd::TransportId transportId = 0,
                std::unordered_map<std::string, mfd::TransportId>* primitiveTransportIds = nullptr);
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

    /** @brief Clears the local dirty flag without discarding the staged patch. */
    void ClearDirty() noexcept;
    /**
     * @brief Legacy alias preserving the historical local-only reset behavior.
     *
     * @note Prefer `ClearDirty()` for new code, or generated-UI `Initialize()`
     * when the intent is to restore the runtime window to its authored state.
     */
    void Reset() noexcept;
    /**
     * @brief Realigns the local reticle baseline with the authored document state.
     *
     * @note This helper updates only the client-side baseline. Generated UI
     * roots pair it with `ResetWindowCommand` to reset the runtime window.
     */
    void ResetToAuthored() noexcept;

    void SetVisible(bool visible);
    void SetBlinkEnabled(bool enabled);
    void SetBlink(bool enabled, const BlinkType& blinkType);
    void SetBlinkType(const BlinkType& blinkType);
    void ClearBlinkType();
    void SetPosition(mfd::Vec2 position);
    void SetRotationDegrees(float rotationDegrees);
    /** @brief Stages one reticle-level non-uniform scale override. */
    void SetScale(mfd::Vec2 scale);
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
    /**
     * @brief Builds one page-scoped strobe handle for one legacy single-strobe page.
     * @param pageName Owning authored page name.
     * @param info Authored strobe metadata.
     * @param pageTransportId Optional generated transport id of the owning page.
     */
    explicit StrobeHandle(std::string_view pageName,
                          StrobeInfo info = {},
                          mfd::TransportId pageTransportId = 0);
    /**
     * @brief Builds one page-scoped strobe handle over an authored strobe catalog.
     * @param pageName Owning authored page name.
     * @param strobes Named selectable strobes exposed by the generated page.
     * @param pageTransportId Optional generated transport id of the owning page.
     *
     * @note When `strobes` contains at least one default-active entry, that
     * selection becomes the initial authored active strobe. Otherwise the first
     * provided entry becomes the authored active strobe.
     */
    StrobeHandle(std::string_view pageName,
                 std::initializer_list<StrobeType> strobes,
                 mfd::TransportId pageTransportId = 0);

    /** @brief Clears the local dirty flag without discarding staged strobe values. */
    void ClearDirty() noexcept;
    /**
     * @brief Legacy alias preserving the historical local-only reset behavior.
     *
     * @note Prefer `ClearDirty()` for new code, or generated-UI `Initialize()`
     * when the intent is to restore the runtime window to its authored state.
     */
    void Reset() noexcept;
    /**
     * @brief Restores the authored default strobe selection and clears local overrides.
     *
     * @note This helper updates only the client-side baseline.
     */
    void ResetToAuthored() noexcept;

    /**
     * @brief Reports whether this handle can address at least one authored strobe.
     * @return `true` when the page exposes one valid strobe entry.
     */
    bool IsValid() const noexcept;
    /**
     * @brief Returns the owning authored page name.
     * @return Page name used by generated and raw helper paths.
     */
    std::string_view PageName() const noexcept;
    /**
     * @brief Returns the currently selected authored strobe entry.
     * @return Pointer to the current entry, or `nullptr` when the page has no strobe.
     */
    const StrobeType* SelectedType() const noexcept;
    /**
     * @brief Returns the authored metadata of the currently selected strobe.
     * @return Metadata of the selected entry, or an invalid default metadata block.
     */
    const StrobeInfo& Info() const noexcept;
    /**
     * @brief Selects one authored strobe entry on the page.
     * @param strobeType Generated strobe entry emitted by the page API.
     */
    StrobeHandle& operator=(const StrobeType& strobeType);
    /**
     * @brief Selects one authored strobe entry on the page.
     * @param strobeType Generated strobe entry emitted by the page API.
     */
    void Use(const StrobeType& strobeType);
    /**
     * @brief Reports whether one generated strobe entry is currently selected.
     * @param strobeType Generated strobe entry emitted by the page API.
     * @return `true` when the handle currently targets that entry.
     */
    bool IsSelected(const StrobeType& strobeType) const noexcept;

    /**
     * @brief Stages the active flag of the currently selected strobe entry.
     * @param active Desired active state.
     */
    void SetActive(bool active);
    /**
     * @brief Stages the requested logical position of the currently selected strobe entry.
     * @param position Desired page-space strobe position.
     */
    void SetPosition(mfd::Vec2 position);

    /**
     * @brief Appends one `UpdateStrobeCommand` when the staged state changed.
     * @param commands Destination command list.
     * @return `true` when one command was appended.
     *
     * @note The emitted command keeps the page-scoped mutation model but may
     * carry the generated id of the selected authored strobe entry when
     * available.
     */
    bool AppendCommands(std::vector<mfd::UserCommand>& commands);

private:
    const StrobeType* ResolveSelectedType() const noexcept;
    void SelectDefaultStrobe() noexcept;

    std::string pageName_;
    mfd::TransportId pageTransportId_ = 0;
    std::vector<StrobeType> strobes_ {};
    mfd::TransportId desiredStrobeId_ = 0;
    std::string desiredStrobeNameNormalized_ {};
    mfd::TransportId lastSentStrobeId_ = 0;
    std::string lastSentStrobeNameNormalized_ {};
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

    /** @brief Clears the local dirty flag without discarding the staged patch. */
    void ClearDirty() noexcept;
    /**
     * @brief Marks this reticle unseen for the next raw `DynamicReticleSet` cycle.
     *
     * @note Generated APIs do not expose this helper because their reset model
     * is authored-state based rather than presence-cycle based.
     */
    void Reset() noexcept;

    const std::string& Id() const noexcept;
    bool IsAlive() const noexcept;
    bool IsStrobeCaptured() const noexcept;

    void SetVisible(bool visible);
    void SetBlinkEnabled(bool enabled);
    void SetBlink(bool enabled, const BlinkType& blinkType);
    void SetBlinkType(const BlinkType& blinkType);
    void ClearBlinkType();
    void SetPosition(mfd::Vec2 position);
    void SetRotationDegrees(float rotationDegrees);
    /** @brief Stages one dynamic-reticle non-uniform scale override. */
    void SetScale(mfd::Vec2 scale);
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
    void BindRuntimeFeedback(mfd::TransportId pageTransportId, RuntimeFeedbackState* feedbackState) noexcept;
    void ResetToAuthored() noexcept;
    void MarkDead() noexcept;

    std::string reticleId_;
    mfd::TransportId feedbackPageTransportId_ = 0;
    RuntimeFeedbackState* feedbackState_ = nullptr;
    mfd::RuntimeDynamicId runtimeReticleId_ = 0;
    std::unordered_map<std::string, mfd::TransportId> primitiveTransportIds_ {};
    mfd::ReticlePatch desiredPatch_ {};
    mfd::ReticlePatch lastSentPatch_ {};
    bool dirty_ = false;
    bool seenThisCycle_ = false;
    bool published_ = false;
    bool alive_ = true;

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
                               mfd::TransportId templateTransportId = 0,
                               RuntimeFeedbackState* feedbackState = nullptr);
    virtual ~GeneratedDynamicReticleSet() = default;
    GeneratedDynamicReticleSet(const GeneratedDynamicReticleSet&) = delete;
    GeneratedDynamicReticleSet& operator=(const GeneratedDynamicReticleSet&) = delete;
    GeneratedDynamicReticleSet(GeneratedDynamicReticleSet&&) = delete;
    GeneratedDynamicReticleSet& operator=(GeneratedDynamicReticleSet&&) = delete;

    /** @brief Clears local dirty flags without changing current generated-reticle lifetime intent. */
    void ClearDirty() noexcept;
    /**
     * @brief Resets the generated set to the authored baseline and invalidates created handles.
     *
     * @note Published runtime instances are expected to be removed by an
     * enclosing `ResetWindowCommand`.
     */
    void ResetToAuthored() noexcept;
    /**
     * @brief Legacy alias kept for generated callers that previously used `Reset()`.
     *
     * @note This now maps to `ResetToAuthored()`.
     */
    void Reset() noexcept;
    /** @brief Enables or disables all generated dynamic reticles of this page/template set at runtime. */
    void SetVisible(bool visible);
    /** @brief Enables or disables strobe-magnet eligibility for all generated dynamic reticles of this page/template set.
     *  @note Dynamic sets are non-magnetized by default until this opt-in is enabled.
     */
    void SetStrobeMagnetEnabled(bool enabled);
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

    void BindReticleRuntimeFeedback(DynamicReticle& reticle) noexcept;
    std::string NextReticleId();
    mfd::RuntimeDynamicId NextRuntimeReticleId();
    DynamicEntry* FindEntry(const DynamicReticle& reticle) noexcept;

    std::string pageName_;
    std::string templateId_;
    mfd::TransportId pageTransportId_ = 0;
    mfd::TransportId templateTransportId_ = 0;
    RuntimeFeedbackState* feedbackState_ = nullptr;
    std::vector<DynamicEntry> reticles_ {};
    bool desiredVisible_ = true;
    bool lastSentVisible_ = true;
    bool visibilityDirty_ = false;
    bool desiredStrobeMagnetEnabled_ = false;
    bool lastSentStrobeMagnetEnabled_ = false;
    bool strobeMagnetDirty_ = false;
    std::uint32_t runtimeIdSessionNonce_ = 0;
    std::uint64_t nextReticleSequence_ = 1;
};

class MFD_CLIENT_API DynamicReticleSet
{
public:
    DynamicReticleSet(std::string_view pageName,
                      std::string_view templateId,
                      mfd::TransportId pageTransportId = 0,
                      mfd::TransportId templateTransportId = 0,
                      RuntimeFeedbackState* feedbackState = nullptr);
    DynamicReticleSet(const DynamicReticleSet&) = delete;
    DynamicReticleSet& operator=(const DynamicReticleSet&) = delete;
    DynamicReticleSet(DynamicReticleSet&&) = delete;
    DynamicReticleSet& operator=(DynamicReticleSet&&) = delete;

    void Reset() noexcept;
    /** @brief Enables or disables all dynamic reticles of this page/template set at runtime. */
    void SetVisible(bool visible);
    /** @brief Enables or disables strobe-magnet eligibility for all dynamic reticles of this page/template set.
     *  @note Dynamic sets are non-magnetized by default until this opt-in is enabled.
     */
    void SetStrobeMagnetEnabled(bool enabled);
    DynamicReticle& Upsert(std::string_view reticleId);
    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);
    std::size_t AppendRemovalCommands(std::vector<mfd::UserCommand>& commands);

private:
    void BindReticleRuntimeFeedback(DynamicReticle& reticle) noexcept;
    DynamicReticle* Find(std::string_view reticleId) noexcept;

    std::string pageName_;
    std::string templateId_;
    mfd::TransportId pageTransportId_ = 0;
    mfd::TransportId templateTransportId_ = 0;
    RuntimeFeedbackState* feedbackState_ = nullptr;
    std::vector<std::unique_ptr<DynamicReticle>> reticles_ {};
    bool desiredVisible_ = true;
    bool lastSentVisible_ = true;
    bool visibilityDirty_ = false;
    bool desiredStrobeMagnetEnabled_ = false;
    bool lastSentStrobeMagnetEnabled_ = false;
    bool strobeMagnetDirty_ = false;
};

class MFD_CLIENT_API WindowDisplay
{
public:
    /** @brief Clears the local dirty flag without discarding the staged display patch. */
    void ClearDirty() noexcept;
    /**
     * @brief Legacy alias preserving the historical local-only reset behavior.
     *
     * @note Prefer `ClearDirty()` for new code, or generated-UI `Initialize()`
     * when the intent is to restore the runtime window to its authored state.
     */
    void Reset() noexcept;
    /**
     * @brief Restores the local display baseline to the authored window defaults.
     *
     * @note This helper updates only the client-side baseline.
     */
    void ResetToAuthored() noexcept;

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
