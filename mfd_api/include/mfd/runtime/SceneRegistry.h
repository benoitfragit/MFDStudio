/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief EnTT-based runtime scene storing pages, reticles, strobes and dynamic data.
 */

#include <entt/entt.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Types.h"
#include "mfd/runtime/GeneratedTransportMap.h"

namespace mfd
{
struct ReticlePatch;
struct WindowDisplayPatch;

/**
 * @brief Lightweight description of one page exposed by the runtime scene.
 */
struct PageSummary
{
    /** @brief User-facing page name. */
    std::string name;
    /** @brief Optional page title used by tools. */
    std::string title;
    /** @brief Visibility and styling state of the page title chrome. */
    PageTitleDisplayDefinition titleDisplay {};
    /** @brief Background color currently associated with the page. */
    ColorRgba backgroundColor {6, 14, 20, 255};
    /** @brief Indicates whether the page exposes a strobe. */
    bool hasStrobe = false;
    /** @brief Indicates whether the page is currently active. */
    bool active = false;
};

/**
 * @brief Runtime state of a page strobe.
 */
struct StrobeSummary
{
    /** @brief Owning page name. */
    std::string pageName;
    /** @brief Generated transport id of the owning page when available. */
    TransportId pageId = 0;
    /** @brief User-facing strobe catalog name currently active on the page. */
    std::string strobeName;
    /** @brief Generated transport id of the active strobe when available. */
    TransportId strobeTransportId = 0;
    /** @brief Public reticle id of the strobe cursor. */
    std::string reticleId;
    /** @brief Current strobe position in logical coordinates. */
    Vec2 position {};
    /** @brief Capture rules applied by the strobe. */
    StrobeCaptureConfig capture;
    /** @brief Magnetization rules applied by the strobe. */
    StrobeMagnetConfig magnet;
    /** @brief Current strobe visibility state. */
    bool visible = true;
};

/**
 * @brief Runtime magnetization state of a strobe.
 */
struct StrobeMagnetSummary
{
    /** @brief Indicates whether magnetization is enabled on this strobe. */
    bool enabled = false;
    /** @brief Attraction radius in logical coordinates. */
    float radius = 0.0f;
    /** @brief Blend factor applied when the strobe is attracted to a target. */
    float strength = 1.0f;
    /** @brief Indicates whether a dynamic reticle is currently magnetized. */
    bool magnetized = false;
    /** @brief Runtime identifier of the magnetized dynamic reticle when available. */
    RuntimeDynamicId runtimeReticleId = 0;
    /** @brief Public id of the magnetized dynamic reticle. */
    std::string reticleId;
    /** @brief Position of the magnetized target. */
    Vec2 targetPosition {};
    /** @brief Distance between the strobe and the magnetized target. */
    float distance = 0.0f;
};

/**
 * @brief Result returned when a strobe captures a dynamic reticle.
 */
struct StrobeCaptureResult
{
    /** @brief Page on which the capture happened. */
    std::string pageName;
    /** @brief Generated transport id of the page on which the capture happened. */
    TransportId pageId = 0;
    /** @brief Public id of the strobe that captured the target. */
    std::string strobeId;
    /** @brief Runtime identifier of the captured dynamic reticle. */
    RuntimeDynamicId runtimeReticleId = 0;
    /** @brief Public id of the captured dynamic reticle. */
    std::string reticleId;
    /** @brief Generated transport id of the authored template that created the captured reticle. */
    TransportId sourceTemplateTransportId = 0;
    /** @brief Template id originally used to create the captured reticle. */
    std::string sourceTemplateId;
    /** @brief Human-readable label exposed by the captured reticle. */
    std::string label;
    /** @brief Category exposed by the captured reticle. */
    std::string category;
    /** @brief Captured reticle position in logical coordinates. */
    Vec2 position {};
    /** @brief Distance between strobe center and captured reticle. */
    float distance = 0.0f;
    /** @brief Arbitrary metadata copied from the captured reticle. */
    ReticleMetadata metadata;
};

/**
 * @brief Combined strobe magnet and capture state resolved in a single dynamic-reticle pass.
 *
 * The runtime feedback path needs both the magnetization summary and the capture result every
 * frame the strobe is published. Computing them together avoids scanning the dynamic reticle set
 * twice per frame.
 */
struct StrobeScanResult
{
    /** @brief Magnetization summary, present whenever the active page owns a strobe. */
    std::optional<StrobeMagnetSummary> magnet;
    /** @brief Capture result, present only when the strobe captured a dynamic reticle. */
    std::optional<StrobeCaptureResult> capture;
};

/**
 * @brief Whole-window display properties applied after page rendering.
 */
struct WindowDisplayState
{
    /** @brief Inverts every rendered color when enabled. */
    bool invertColors = false;
    /** @brief Global brightness factor in the `[0, 1]` range. */
    float brightness = 1.0f;
    /** @brief Replaces the full rendered output with a black screen when enabled. */
    bool disabled = false;
};

/**
 * @brief Lightweight render view over one reticle with blink-resolved visibility.
 */
struct ReticleRenderView
{
    /** @brief Reticle payload owned by the scene registry. */
    const ReticleGroup* group = nullptr;
    /** @brief Visibility resolved for the current frame, including blink state. */
    bool visible = false;
    /** @brief Inter-layer render order of the reticle's layer; lower values draw earlier. */
    std::size_t layerOrder = 0;
};

/**
 * @brief Runtime scene built on top of EnTT for pages, reticles and strobes.
 *
 * @note `SceneRegistry` is the central state container used by the renderer,
 * the command processor and the editor/runtime helpers.
 */
class MFD_API SceneRegistry
{
public:
    SceneRegistry();
    ~SceneRegistry();
    SceneRegistry(const SceneRegistry&) = delete;
    SceneRegistry& operator=(const SceneRegistry&) = delete;
    SceneRegistry(SceneRegistry&&) noexcept;
    SceneRegistry& operator=(SceneRegistry&&) noexcept;

    /**
     * @brief Creates a scene and immediately loads a document into it.
     * @param document Document to load.
     */
    explicit SceneRegistry(MfdDocument document);
    /**
     * @brief Creates a scene and immediately loads a document and its optional generated transport map.
     * @param document Document to load.
     * @param transportMap Optional generated transport map resolved for the same window.
     */
    SceneRegistry(MfdDocument document, std::optional<GeneratedTransportMap> transportMap);

    /**
     * @brief Replaces the current scene content with a new document.
     * @param document Document to load.
     */
    void LoadDocument(MfdDocument document);
    /**
     * @brief Replaces the current scene content with a new document and optional generated transport map.
     * @param document Document to load.
     * @param transportMap Optional generated transport map resolved for the same window.
     */
    void LoadDocument(MfdDocument document, std::optional<GeneratedTransportMap> transportMap);

    /** @brief Returns the document currently backing the scene. */
    const MfdDocument& Document() const noexcept;
    /** @brief Returns the generated transport map currently attached to the scene, when available. */
    const std::optional<GeneratedTransportMap>& TransportMap() const noexcept;
    /** @brief Indicates whether the scene currently owns a generated transport map. */
    bool HasTransportMap() const noexcept;
    /** @brief Returns the loaded reticle template library. */
    const ReticleLibrary& Library() const noexcept;

    /** @brief Returns a summary for every page in the scene. */
    std::vector<PageSummary> Pages() const;
    /** @brief Returns `true` if a page exists. */
    bool HasPage(std::string_view pageName) const noexcept;
    /** @brief Returns `true` if a page owns a strobe. */
    bool HasStrobe(std::string_view pageName) const noexcept;
    /** @brief Returns `true` if the active page owns a strobe. */
    bool ActivePageHasStrobe() const noexcept;

    /** @brief Activates a page by name. */
    void SetActivePage(std::string_view pageName) noexcept;
    /** @brief Returns the display name of the active page. */
    std::string ActivePageName() const;
    /** @brief Returns the active page summary. */
    std::optional<PageSummary> ActivePageSummary() const;
    /** @brief Returns the view of a page when it exists. */
    std::optional<PageViewState> ViewForPage(std::string_view pageName) const noexcept;
    /** @brief Returns the current active page view. */
    PageViewState ActivePageView() const noexcept;
    /** @brief Returns the live strobe state of a page when that page is currently active. */
    std::optional<StrobeSummary> StrobeForPage(std::string_view pageName) const;
    /** @brief Returns the strobe state of the active page when it exists. */
    std::optional<StrobeSummary> ActiveStrobeSummary() const;
    /** @brief Returns the current strobe magnetization state of a page when that page is currently active. */
    std::optional<StrobeMagnetSummary> StrobeMagnetForPage(std::string_view pageName) const;
    /** @brief Returns the current strobe magnetization state of the active page when it exists. */
    std::optional<StrobeMagnetSummary> ActiveStrobeMagnetSummary() const;
    /** @brief Returns the background color of the active page. */
    ColorRgba ActiveBackgroundColor() const noexcept;
    /** @brief Returns the whole-window display state currently applied by the runtime. */
    WindowDisplayState WindowDisplay() const noexcept;

    /** @brief Collects all reticles rendered on a given page with blink-resolved visibility. */
    std::vector<ReticleGroup> CollectPageReticles(std::string_view pageName) const;
    /** @brief Collects all reticles rendered on the active page with blink-resolved visibility. */
    std::vector<ReticleGroup> CollectActiveReticles() const;
    /** @brief Collects non-owning render views on a given page with blink-resolved visibility. */
    std::vector<ReticleRenderView> CollectPageReticleViews(std::string_view pageName) const;
    /**
     * @brief Fills one caller-owned buffer with non-owning render views for a given page.
     * @param pageName Page to inspect.
     * @param destination Buffer cleared then filled with blink-resolved render views.
     */
    void CollectPageReticleViews(std::string_view pageName, std::vector<ReticleRenderView>& destination) const;
    /** @brief Collects non-owning render views on the active page with blink-resolved visibility. */
    std::vector<ReticleRenderView> CollectActiveReticleViews() const;
    /**
     * @brief Fills one caller-owned buffer with non-owning render views for the active page.
     * @param destination Buffer cleared then filled with blink-resolved render views.
     */
    void CollectActiveReticleViews(std::vector<ReticleRenderView>& destination) const;
    /** @brief Collects pointers to all reticles on a given page without copying them. */
    std::vector<const ReticleGroup*> CollectPageReticlePointers(std::string_view pageName) const;
    /** @brief Collects pointers to all reticles on the active page without copying them. */
    std::vector<const ReticleGroup*> CollectActiveReticlePointers() const;

    /** @brief Sets the full page view in one call. */
    bool SetPageView(std::string_view pageName, const PageViewState& view) noexcept;
    /** @brief Sets the page view center. */
    bool SetPageViewCenter(std::string_view pageName, Vec2 center) noexcept;
    /** @brief Sets the page zoom. */
    bool SetPageZoom(std::string_view pageName, float zoom) noexcept;
    /** @brief Enables or disables whole-window color inversion. */
    bool SetWindowColorInverted(bool invertColors) noexcept;
    /** @brief Sets the whole-window brightness factor in the `[0, 1]` range. */
    bool SetWindowBrightness(float brightness) noexcept;
    /** @brief Enables or disables whole-window blackout. */
    bool SetWindowDisabled(bool disabled) noexcept;
    /** @brief Applies a whole-window display patch in one call. */
    bool ApplyWindowDisplayPatch(const WindowDisplayPatch& patch) noexcept;

    /** @brief Sets a reticle visibility flag. */
    bool SetReticleVisible(std::string_view pageName, std::string_view reticleId, bool visible) noexcept;
    /** @brief Enables or disables reticle blinking. */
    bool SetReticleBlinkEnabled(std::string_view pageName, std::string_view reticleId, bool enabled) noexcept;
    /** @brief Sets the page blink type used by a reticle and enables blinking. */
    bool SetReticleBlinkType(std::string_view pageName,
                             std::string_view reticleId,
                             std::string_view blinkType) noexcept;
    /** @brief Clears the reticle-specific blink type and falls back to the page default when blinking is enabled. */
    bool ClearReticleBlinkType(std::string_view pageName, std::string_view reticleId) noexcept;
    /** @brief Sets a reticle position. */
    bool SetReticlePosition(std::string_view pageName, std::string_view reticleId, Vec2 position) noexcept;
    /** @brief Sets a reticle rotation in degrees. */
    bool SetReticleRotation(std::string_view pageName, std::string_view reticleId, float rotationDegrees) noexcept;
    /** @brief Sets a reticle stroke color override. */
    bool SetReticleColor(std::string_view pageName, std::string_view reticleId, ColorRgba color) noexcept;
    /** @brief Sets a reticle stroke thickness override. */
    bool SetReticleThickness(std::string_view pageName, std::string_view reticleId, float thickness) noexcept;
    /** @brief Sets the first text primitive value found in a reticle. */
    bool SetReticleText(std::string_view pageName, std::string_view reticleId, std::string value) noexcept;
    /** @brief Sets the text of a named text primitive inside a reticle. */
    bool SetReticleText(std::string_view pageName,
                        std::string_view reticleId,
                        std::string_view primitiveId,
                        std::string value) noexcept;
    /** @brief Sets the first text primitive letter spacing found in a reticle. */
    bool SetReticleLetterSpacing(std::string_view pageName,
                                 std::string_view reticleId,
                                 float letterSpacing) noexcept;
    /** @brief Sets the letter spacing of a named text primitive inside a reticle. */
    bool SetReticleLetterSpacing(std::string_view pageName,
                                 std::string_view reticleId,
                                 std::string_view primitiveId,
                                 float letterSpacing) noexcept;
    /** @brief Applies a multi-field patch to a reticle in one lookup. */
    bool ApplyReticlePatch(std::string_view pageName, std::string_view reticleId, const ReticlePatch& patch) noexcept;
    /** @brief Returns `true` when a dynamic reticle exists on a page. */
    bool HasDynamicReticle(std::string_view pageName, std::string_view reticleId) const noexcept;
    /** @brief Returns `true` when one dynamic reticle is backed by one specific template on a page. */
    bool DynamicReticleUsesTemplate(std::string_view pageName,
                                    std::string_view reticleId,
                                    std::string_view templateId) const noexcept;
    /** @brief Applies a patch to an existing dynamic reticle in one lookup. */
    bool ApplyDynamicReticlePatch(std::string_view pageName,
                                  std::string_view reticleId,
                                  const ReticlePatch& patch) noexcept;
    /** @brief Sets visibility for all dynamic reticles matching one page/template set. */
    bool SetDynamicReticleSetVisible(std::string_view pageName,
                                     std::string_view templateId,
                                     bool visible) noexcept;
    /** @brief Enables or disables strobe-magnet eligibility for all dynamic reticles matching one page/template set.
     *  @note Dynamic sets are non-magnetized by default until explicitly enabled.
     */
    bool SetDynamicReticleSetStrobeMagnetEnabled(std::string_view pageName,
                                                 std::string_view templateId,
                                                 bool enabled) noexcept;

    /** @brief Enables or disables a page strobe. */
    bool SetStrobeActive(std::string_view pageName, bool active) noexcept;
    /** @brief Selects the active strobe variant on a page. */
    bool SelectStrobe(std::string_view pageName, std::string_view strobeName) noexcept;
    /** @brief Sets the strobe position. */
    bool SetStrobePosition(std::string_view pageName, Vec2 position) noexcept;
    /** @brief Offsets the strobe position by a delta. */
    bool OffsetStrobe(std::string_view pageName, Vec2 delta) noexcept;
    /**
     * @brief Applies an optional strobe selection, activation and position change atomically.
     *
     * Every precondition is validated before any mutation, so the call either applies all the
     * requested changes or leaves the scene untouched. This removes the need for a whole-scene
     * snapshot and rollback on the hot strobe-movement path.
     *
     * @param pageName Target page; must currently own a strobe.
     * @param strobeName Optional strobe variant to select first; empty keeps the active variant.
     * @param active Optional new visibility for the resulting active strobe.
     * @param position Optional new position; must be finite.
     * @return `true` when every requested change applied, `false` when nothing was changed.
     */
    bool ApplyStrobeUpdate(std::string_view pageName,
                           std::string_view strobeName,
                           std::optional<bool> active,
                           std::optional<Vec2> position) noexcept;
    /** @brief Attempts to capture a dynamic reticle with a page strobe when that page is currently active. */
    std::optional<StrobeCaptureResult> CaptureWithStrobe(std::string_view pageName) const;
    /** @brief Attempts to capture a dynamic reticle with the active page strobe. */
    std::optional<StrobeCaptureResult> CaptureActivePageStrobe() const;
    /** @brief Resolves the strobe magnet and capture state of a page in a single dynamic-reticle pass. */
    StrobeScanResult ScanStrobeForPage(std::string_view pageName) const;

    /** @brief Creates or updates a dynamic reticle on a page. */
    void UpsertDynamicReticle(std::string_view pageName, ReticleGroup reticle);
    /** @brief Removes one dynamic reticle from a page. */
    bool RemoveDynamicReticle(std::string_view pageName, std::string_view reticleId);
    /** @brief Removes every dynamic reticle from one page. */
    void ClearDynamicReticles(std::string_view pageName);
    /** @brief Removes every dynamic reticle from every page. */
    void ClearAllDynamicReticles();
    /**
     * @brief Restores the runtime scene to the exact initial state of the loaded document.
     *
     * @note This operation restores the default active page, page views,
     * strobe state, whole-window display state and clears all dynamic reticles.
     */
    void ResetToInitialState();

private:
    /**
     * @brief CommandProcessor is the command-application half of the runtime scene module.
     *
     * @note The processor relies on private identifier resolution
     * (`Resolve*`), runtime-id bookkeeping (`SetDynamicReticleRuntimeIdentifiers`),
     * layer binding lookup and snapshot capture/restore. These operations are
     * deliberately not public: exposing them would let callers bypass command
     * validation, sequencing and transactional rollback.
     */
    friend class CommandProcessor;

    /**
     * @brief Complete runtime snapshot used to roll back a partially applied command batch.
     */
    struct RuntimeSnapshot
    {
        struct PageState
        {
            std::string normalizedPageName;
            std::string activeStrobeName;
            PageViewState view {};
            std::chrono::steady_clock::time_point blinkEpoch {};
        };

        struct ReticleState
        {
            std::string normalizedPageName;
            ReticleGroup group;
        };

        struct DynamicReticleState
        {
            std::string normalizedPageName;
            ReticleGroup group;
            RuntimeDynamicId runtimeReticleId = 0;
            TransportId sourceTemplateTransportId = 0;
            std::uint64_t dynamicCreationSequence = 0;
        };

        struct StrobeState
        {
            std::string normalizedPageName;
            std::string normalizedStrobeName;
            ReticleGroup group;
            StrobeCaptureConfig capture;
            StrobeMagnetConfig magnet;
            std::vector<Primitive> authoredPrimitives;
            ReticleStyleOverride authoredOverrides;
            ReticleClipState authoredClipping {};
            bool visualShapeApplied = false;
            std::string lockedReticleId;
        };

        struct DynamicTemplateVisibilityState
        {
            std::string normalizedPageName;
            std::string normalizedTemplateId;
            bool visible = true;
        };

        struct DynamicTemplateStrobeMagnetState
        {
            std::string normalizedPageName;
            std::string normalizedTemplateId;
            bool enabled = false;
        };

        std::vector<PageState> pages;
        std::vector<ReticleState> staticReticles;
        std::vector<DynamicReticleState> dynamicReticles;
        std::vector<StrobeState> strobes;
        std::vector<DynamicTemplateVisibilityState> dynamicTemplateVisibility;
        std::vector<DynamicTemplateStrobeMagnetState> dynamicTemplateStrobeMagnet;
        /** @brief Normalized page keys covered by a page-scoped snapshot (`scoped == true`). */
        std::vector<std::string> scopedPageKeys;
        std::uint64_t nextDynamicCreationSequence = 1;
        std::string activePage;
        WindowDisplayState windowDisplay {};
        /** @brief `true` when the snapshot only covers `scopedPageKeys` instead of the whole scene. */
        bool scoped = false;
    };

    /** @brief Captures the full mutable runtime state needed to roll back one failed batch. */
    RuntimeSnapshot CaptureRuntimeSnapshot() const;
    /**
     * @brief Captures the mutable state of a bounded page set plus scene-wide state.
     * @param normalizedPageKeys Normalized page keys mutated by the batch to protect.
     * @note Cost is proportional to the touched pages instead of the whole scene, so
     * transactional batches no longer pay a full-scene copy on the nominal path.
     */
    RuntimeSnapshot CaptureRuntimeSnapshotForPages(std::vector<std::string> normalizedPageKeys) const;
    /** @brief Restores one previously captured mutable runtime state. */
    void RestoreRuntimeSnapshot(const RuntimeSnapshot& snapshot);
    /** @brief Sorts snapshot rows so restoration order stays deterministic. */
    static void SortRuntimeSnapshot(RuntimeSnapshot& snapshot);
    /** @brief Re-applies one captured snapshot onto the current scene state. */
    void ApplySnapshotState(const RuntimeSnapshot& snapshot);
    /** @brief Restores a page-scoped snapshot without reloading the whole document. */
    void RestoreScopedRuntimeSnapshot(const RuntimeSnapshot& snapshot);
    /** @brief Removes every dynamic template visibility/magnet override of one page. */
    void EraseDynamicTemplateOverridesForPage(std::string_view normalizedPageName);

    /** @brief Per-page runtime component stored in the EnTT registry. */
    struct PageComponent;
    /** @brief Link between one reticle entity and its owning page. */
    struct PageMembership;
    /** @brief Runtime reticle payload stored in the EnTT registry. */
    struct ReticleComponent;
    /** @brief Marker component identifying runtime-created reticles. */
    struct DynamicTag;
    /** @brief Marker component identifying authored static reticles. */
    struct StaticTag;
    /** @brief Marker component identifying one page strobe entity. */
    struct StrobeTag;
    /** @brief Runtime behavior configuration attached to a strobe entity. */
    struct StrobeBehaviorComponent;
    /** @brief Transport lookup row for one generated static reticle. */
    struct TransportReticleLookup
    {
        TransportId pageId = 0;
        std::string pageName;
        std::string reticleId;
    };
    /** @brief Transport lookup row for one generated primitive. */
    struct TransportPrimitiveLookup
    {
        TransportPrimitiveOwnerKind ownerKind = TransportPrimitiveOwnerKind::Reticle;
        TransportId ownerId = 0;
        std::string primitiveId;
    };
    /** @brief Transport lookup row for one generated blink type. */
    struct TransportBlinkLookup
    {
        TransportId pageId = 0;
        std::string blinkType;
    };
    /** @brief Transport lookup row for one generated strobe catalog entry. */
    struct TransportStrobeLookup
    {
        TransportId pageId = 0;
        std::string strobeName;
    };

    /** @brief Copies one page's mutable runtime state into a snapshot under construction. */
    void CapturePageStateInto(RuntimeSnapshot& snapshot, const PageComponent& page) const;
    /** @brief Copies one reticle, strobe or dynamic entity state into a snapshot under construction. */
    void CaptureReticleEntityInto(RuntimeSnapshot& snapshot,
                                  entt::entity entity,
                                  const PageMembership& membership,
                                  const ReticleComponent& reticle) const;

    /** @brief Returns `true` when a normalized page key exists in the scene indexes. */
    bool HasNormalizedPage(std::string_view pageName) const noexcept;
    /** @brief Returns `true` when a normalized page key owns a strobe in the scene indexes. */
    bool HasNormalizedStrobe(std::string_view pageName) const noexcept;
    /** @brief Returns `true` when a normalized page key matches the current active page. */
    bool IsActivePageKey(std::string_view pageName) const noexcept;
    /** @brief Returns a strobe summary from a normalized page key. */
    std::optional<StrobeSummary> StrobeForPageKey(std::string_view pageName) const;
    /** @brief Returns a strobe-magnet summary from a normalized page key. */
    std::optional<StrobeMagnetSummary> StrobeMagnetForPageKey(std::string_view pageName) const;
    /** @brief Attempts one strobe capture using a normalized page key. */
    std::optional<StrobeCaptureResult> CaptureWithStrobeKey(std::string_view pageName) const;
    /** @brief Resolves magnet and capture state for a normalized page key in one dynamic-reticle pass. */
    StrobeScanResult ScanStrobeByKey(std::string_view pageName) const;
    /** @brief Returns the currently locked magnet target of one strobe when it is still valid. */
    const ReticleComponent* FindLockedStrobeMagnetTarget(std::string_view normalizedPageName,
                                                         std::string_view reticleId) const noexcept;
    /** @brief Finds the nearest visible dynamic reticle eligible for strobe magnetization. */
    const ReticleComponent* FindNearestStrobeMagnetTarget(std::string_view normalizedPageName,
                                                          Vec2 position,
                                                          float radius) const noexcept;
    /** @brief Applies or restores the optional visual shape shown while a strobe is magnetized. */
    void ApplyStrobeMagnetVisualShape(ReticleComponent& strobe,
                                      StrobeBehaviorComponent& behavior,
                                      bool magnetized);
    /** @brief Re-applies one sticky strobe lock after dynamic reticle changes. */
    void RefreshStickyStrobePosition(std::string_view normalizedPageName) noexcept;
    /** @brief Collects copied reticles for one normalized page key. */
    std::vector<ReticleGroup> CollectPageReticlesByKey(std::string_view pageName) const;
    /** @brief Collects non-owning render views for one normalized page key. */
    std::vector<ReticleRenderView> CollectPageReticleViewsByKey(std::string_view pageName) const;
    /** @brief Fills one caller-owned render-view buffer for one normalized page key. */
    void CollectPageReticleViewsByKey(std::string_view pageName, std::vector<ReticleRenderView>& destination) const;
    /** @brief Collects reticle pointers for one normalized page key without copying. */
    std::vector<const ReticleGroup*> CollectPageReticlePointersByKey(std::string_view pageName) const;
    /** @brief Builds the composite lookup key used by the strobe entity index. */
    std::string MakeStrobeLookupKey(std::string_view normalizedPageName, std::string_view strobeName) const;
    /** @brief Finds the entity of one named strobe variant on a page. */
    entt::entity FindStrobeEntity(std::string_view normalizedPageName, std::string_view strobeName) const noexcept;
    /** @brief Finds the entity of the active strobe variant on a page. */
    entt::entity FindActiveStrobeEntity(std::string_view normalizedPageName) const noexcept;
    /** @brief Builds the composite lookup key used by the reticle entity index. */
    std::string MakeReticleLookupKey(std::string_view normalizedPageName, std::string_view reticleId) const;
    /** @brief Finds the entity of one static or dynamic reticle. */
    entt::entity FindReticleEntity(std::string_view normalizedPageName, std::string_view reticleId) const noexcept;
    /** @brief Returns mutable access to one reticle component. */
    ReticleComponent* FindReticle(std::string_view normalizedPageName, std::string_view reticleId) noexcept;
    /** @brief Returns read-only access to one reticle component. */
    const ReticleComponent* FindReticle(std::string_view normalizedPageName, std::string_view reticleId) const noexcept;
    /** @brief Stores runtime-only identifiers for one dynamic reticle instance. */
    bool SetDynamicReticleRuntimeIdentifiers(std::string_view normalizedPageName,
                                             std::string_view reticleId,
                                             RuntimeDynamicId runtimeReticleId,
                                             TransportId sourceTemplateTransportId) noexcept;
    /** @brief Returns mutable access to one page component. */
    PageComponent* FindPage(std::string_view normalizedPageName) noexcept;
    /** @brief Returns read-only access to one page component. */
    const PageComponent* FindPage(std::string_view normalizedPageName) const noexcept;
    /** @brief Registers one reticle entity in the fast lookup map. */
    void IndexReticle(std::string_view normalizedPageName, const ReticleGroup& reticle, entt::entity entity);
    /** @brief Removes one reticle entry from the fast lookup map. */
    void RemoveReticleIndex(std::string_view normalizedPageName, std::string_view reticleId);
    /** @brief Builds the composite lookup key used by dynamic template visibility overrides. */
    std::string MakeDynamicTemplateLookupKey(std::string_view normalizedPageName, std::string_view templateId) const;
    /** @brief Returns whether one dynamic template set is currently visible on one page. */
    bool IsDynamicTemplateVisible(std::string_view normalizedPageName, std::string_view templateId) const noexcept;
    /** @brief Returns whether one dynamic template set is currently eligible for strobe magnetization on one page. */
    bool IsDynamicTemplateStrobeMagnetEnabled(std::string_view normalizedPageName,
                                              std::string_view templateId) const noexcept;
    /** @brief Same as HasDynamicReticle, but takes an already-normalized page key to avoid
     *  renormalizing it per reticle on the bulk dynamic command hot path. */
    bool HasDynamicReticleByKey(std::string_view normalizedPageName, std::string_view reticleId) const noexcept;
    /** @brief Same as DynamicReticleUsesTemplate, but takes an already-normalized page key. */
    bool DynamicReticleUsesTemplateByKey(std::string_view normalizedPageName,
                                         std::string_view reticleId,
                                         std::string_view templateId) const noexcept;
    /** @brief Same as ApplyDynamicReticlePatch, but takes an already-normalized page key. */
    bool ApplyDynamicReticlePatchByKey(std::string_view normalizedPageName,
                                       std::string_view reticleId,
                                       const ReticlePatch& patch) noexcept;
    /** @brief Same as UpsertDynamicReticle, but takes an already-normalized page key. */
    void UpsertDynamicReticleByKey(std::string_view normalizedPageName, ReticleGroup reticle);
    /** @brief Same as IsDynamicTemplateVisible, but takes an already-normalized template id to
     *  avoid renormalizing it per reticle on the render/strobe-scan hot path. */
    bool IsDynamicTemplateVisibleByNormalizedId(std::string_view normalizedPageName,
                                                std::string_view normalizedTemplateId) const noexcept;
    /** @brief Same as IsDynamicTemplateStrobeMagnetEnabled, but takes an already-normalized
     *  template id to avoid renormalizing it per reticle on the strobe-scan hot path. */
    bool IsDynamicTemplateStrobeMagnetEnabledByNormalizedId(std::string_view normalizedPageName,
                                                            std::string_view normalizedTemplateId) const noexcept;
    /** @brief Returns the dynamic reticle binding configured for one page/template pair, when it exists. */
    const DynamicReticleLayerBinding* FindDynamicReticleLayerBinding(std::string_view normalizedPageName,
                                                                     std::string_view templateId) const noexcept;
    /** @brief Inserts one reticle entity into the ordered draw list of a page. */
    void InsertReticleIntoPageDrawList(std::string_view normalizedPageName, entt::entity entity);
    /** @brief Removes one reticle entity from the ordered draw list of a page. */
    void RemoveReticleFromPageDrawList(std::string_view normalizedPageName, entt::entity entity);
    /** @brief Synchronizes the active flag of a page entity with the current active-page selection. */
    void SetActiveFlag(std::string_view pageName, bool active);
    /** @brief Rebuilds generated transport lookup indexes from the current companion map. */
    void RebuildTransportIndexes();
    /** @brief Returns the generated transport id of one template name when available. */
    TransportId ResolveTemplateTransportId(std::string_view templateId) const noexcept;
    /** @brief Returns whether the current scene can validate a mapping hash. */
    bool HasMatchingTransportMap(std::string_view mappingHash) const noexcept;
    /** @brief Resolves an authored page name to its generated transport id when available. */
    TransportId ResolvePageTransportId(std::string_view pageName) const noexcept;
    /** @brief Resolves a generated page transport id to an authored page name. */
    const std::string* ResolvePageName(TransportId pageId) const noexcept;
    /** @brief Resolves a generated static reticle transport id. */
    const TransportReticleLookup* ResolveStaticReticle(TransportId reticleId) const noexcept;
    /** @brief Resolves a generated template transport id. */
    const std::string* ResolveTemplateId(TransportId templateId) const noexcept;
    /** @brief Resolves one generated blink type transport id within a page. */
    const std::string* ResolveBlinkType(TransportId pageId, TransportId blinkTypeId) const noexcept;
    /** @brief Resolves one generated strobe transport id within a page. */
    const std::string* ResolveStrobeName(TransportId pageId, TransportId strobeId) const noexcept;
    /** @brief Resolves one generated primitive transport id for a static reticle owner. */
    const std::string* ResolvePrimitiveIdForReticle(TransportId reticleId, TransportId primitiveId) const noexcept;
    /** @brief Resolves one generated primitive transport id for a template owner. */
    const std::string* ResolvePrimitiveIdForTemplate(TransportId templateId, TransportId primitiveId) const noexcept;

    /** @brief Authored document currently backing the runtime scene. */
    MfdDocument document_ {};
    /** @brief Optional generated companion map loaded with the current document. */
    std::optional<GeneratedTransportMap> transportMap_ {};
    /** @brief EnTT registry owning all runtime entities and components. */
    entt::registry registry_ {};
    /** @brief Fast lookup from normalized page name to page entity. */
    std::unordered_map<std::string, entt::entity, TransparentStringHash, TransparentStringEqual> pageEntities_ {};
    /** @brief Fast lookup from page-plus-strobe key to strobe entity. */
    std::unordered_map<std::string, entt::entity, TransparentStringHash, TransparentStringEqual> strobeEntities_ {};
    /** @brief Fast lookup from page-plus-reticle key to reticle entity. */
    std::unordered_map<std::string, entt::entity, TransparentStringHash, TransparentStringEqual> reticleEntities_ {};
    /** @brief Optional per-page visibility overrides indexed by dynamic template id. */
    std::unordered_map<std::string, bool, TransparentStringHash, TransparentStringEqual> dynamicTemplateVisibility_ {};
    /** @brief Optional per-page strobe-magnet eligibility overrides indexed by dynamic template id. */
    std::unordered_map<std::string, bool, TransparentStringHash, TransparentStringEqual>
        dynamicTemplateStrobeMagnetEnabled_ {};
    /** @brief Generated page-name lookup indexed by generated transport id. */
    std::unordered_map<TransportId, std::string> transportPageNames_ {};
    /** @brief Generated static-reticle lookup indexed by generated transport id. */
    std::unordered_map<TransportId, TransportReticleLookup> transportReticles_ {};
    /** @brief Generated template lookup indexed by generated transport id. */
    std::unordered_map<TransportId, std::string> transportTemplates_ {};
    /** @brief Generated template-transport lookup indexed by normalized template id. */
    std::unordered_map<std::string, TransportId, TransparentStringHash, TransparentStringEqual> templateTransportIds_ {};
    /** @brief Generated primitive lookup indexed by generated transport id. */
    std::unordered_map<TransportId, TransportPrimitiveLookup> transportPrimitives_ {};
    /** @brief Generated blink lookup indexed by generated transport id. */
    std::unordered_map<TransportId, TransportBlinkLookup> transportBlinks_ {};
    /** @brief Generated strobe lookup indexed by generated transport id. */
    std::unordered_map<TransportId, TransportStrobeLookup> transportStrobes_ {};
    /** @brief Monotonic creation sequence used to keep dynamic instance ordering stable inside one template binding. */
    std::uint64_t nextDynamicCreationSequence_ = 1;
    /** @brief Normalized name of the currently active page. */
    std::string activePage_ {};
    /** @brief Whole-window display post-process state. */
    WindowDisplayState windowDisplay_ {};
};
} // namespace mfd
