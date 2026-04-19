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

#include <optional>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Types.h"

namespace mfd
{
struct ReticlePatch;
struct WindowDisplayPatch;

/**
 * @brief Structured result returned by validated runtime mutations.
 */
struct Result
{
    /** @brief Indicates whether the operation succeeded. */
    bool ok = true;
    /** @brief Human-readable error message when `ok == false`. */
    std::string error {};

    /** @brief Creates a successful result. */
    static Result Ok() { return {}; }
    /** @brief Creates a failed result with an explanatory message. */
    static Result Error(std::string message) { return Result {false, std::move(message)}; }
};

/**
 * @brief Strongly-typed reticle lookup key.
 */
struct ReticleKey
{
    /** @brief Normalized page name. */
    std::string page;
    /** @brief Normalized reticle identifier. */
    std::string id;

    /** @brief Equality operator used by hashed containers. */
    bool operator==(const ReticleKey& other) const noexcept
    {
        return page == other.page && id == other.id;
    }
};

/**
 * @brief Lightweight description of one page exposed by the runtime scene.
 */
struct PageSummary
{
    /** @brief User-facing page name. */
    std::string name;
    /** @brief Optional page title used by tools. */
    std::string title;
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
    /** @brief Public id of the strobe that captured the target. */
    std::string strobeId;
    /** @brief Public id of the captured dynamic reticle. */
    std::string reticleId;
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
    SceneRegistry() = default;

    /**
     * @brief Creates a scene and immediately loads a document into it.
     * @param document Document to load.
     */
    explicit SceneRegistry(MfdDocument document);

    /**
     * @brief Replaces the current scene content with a new document.
     * @param document Document to load.
     */
    void LoadDocument(MfdDocument document);

    /** @brief Returns the document currently backing the scene. */
    const MfdDocument& Document() const noexcept;
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
    /** @brief Returns the strobe state of a page when it exists. */
    std::optional<StrobeSummary> StrobeForPage(std::string_view pageName) const;
    /** @brief Returns the strobe state of the active page when it exists. */
    std::optional<StrobeSummary> ActiveStrobeSummary() const;
    /** @brief Returns the current strobe magnetization state of a page when it exists. */
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
    /** @brief Collects non-owning render views on the active page with blink-resolved visibility. */
    std::vector<ReticleRenderView> CollectActiveReticleViews() const;
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
    /** @brief Applies a patch to an existing dynamic reticle in one lookup. */
    bool ApplyDynamicReticlePatch(std::string_view pageName,
                                  std::string_view reticleId,
                                  const ReticlePatch& patch) noexcept;
    /** @brief Sets visibility for all dynamic reticles matching one page/template set. */
    bool SetDynamicReticleSetVisible(std::string_view pageName,
                                     std::string_view templateId,
                                     bool visible) noexcept;

    /** @brief Enables or disables a page strobe. */
    bool SetStrobeActive(std::string_view pageName, bool active) noexcept;
    /** @brief Sets the strobe position. */
    bool SetStrobePosition(std::string_view pageName, Vec2 position) noexcept;
    /** @brief Offsets the strobe position by a delta. */
    bool OffsetStrobe(std::string_view pageName, Vec2 delta) noexcept;
    /** @brief Attempts to capture a dynamic reticle with a page strobe. */
    std::optional<StrobeCaptureResult> CaptureWithStrobe(std::string_view pageName) const;
    /** @brief Attempts to capture a dynamic reticle with the active page strobe. */
    std::optional<StrobeCaptureResult> CaptureActivePageStrobe() const;

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

    /** @brief Returns the underlying EnTT registry. */
    entt::registry& Raw() noexcept;
    /** @brief Returns the underlying EnTT registry. */
    const entt::registry& Raw() const noexcept;
    /** @brief Returns the last structured runtime error emitted by a mutation command. */
    const std::string& LastError() const noexcept;

private:
    friend class CommandProcessor;

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

    /** @brief Returns `true` when a normalized page key exists in the scene indexes. */
    bool HasNormalizedPage(std::string_view pageName) const noexcept;
    /** @brief Returns `true` when a normalized page key owns a strobe in the scene indexes. */
    bool HasNormalizedStrobe(std::string_view pageName) const noexcept;
    /** @brief Returns a strobe summary from a normalized page key. */
    std::optional<StrobeSummary> StrobeForPageKey(std::string_view pageName) const;
    /** @brief Returns a strobe-magnet summary from a normalized page key. */
    std::optional<StrobeMagnetSummary> StrobeMagnetForPageKey(std::string_view pageName) const;
    /** @brief Attempts one strobe capture using a normalized page key. */
    std::optional<StrobeCaptureResult> CaptureWithStrobeKey(std::string_view pageName) const;
    /** @brief Collects copied reticles for one normalized page key. */
    std::vector<ReticleGroup> CollectPageReticlesByKey(std::string_view pageName) const;
    /** @brief Collects non-owning render views for one normalized page key. */
    std::vector<ReticleRenderView> CollectPageReticleViewsByKey(std::string_view pageName) const;
    /** @brief Collects reticle pointers for one normalized page key without copying. */
    std::vector<const ReticleGroup*> CollectPageReticlePointersByKey(std::string_view pageName) const;
    /** @brief Builds a strongly typed reticle lookup key used by the reticle entity index. */
    ReticleKey MakeReticleKey(std::string_view normalizedPageName, std::string_view reticleId) const;
    /** @brief Finds the entity of one static or dynamic reticle. */
    entt::entity FindReticleEntity(std::string_view normalizedPageName, std::string_view reticleId) const noexcept;
    /** @brief Returns mutable access to one reticle component. */
    ReticleComponent* FindReticle(std::string_view normalizedPageName, std::string_view reticleId) noexcept;
    /** @brief Returns read-only access to one reticle component. */
    const ReticleComponent* FindReticle(std::string_view normalizedPageName, std::string_view reticleId) const noexcept;
    /** @brief Returns mutable access to one page component. */
    PageComponent* FindPage(std::string_view normalizedPageName) noexcept;
    /** @brief Returns read-only access to one page component. */
    const PageComponent* FindPage(std::string_view normalizedPageName) const noexcept;
    /** @brief Registers one reticle entity in the fast lookup map. */
    void IndexReticle(std::string_view normalizedPageName, const ReticleGroup& reticle, entt::entity entity);
    /** @brief Removes one reticle entry from the fast lookup map. */
    void RemoveReticleIndex(std::string_view normalizedPageName, std::string_view reticleId);
    /** @brief Strong key identifying one dynamic template visibility override. */
    struct DynamicTemplateKey
    {
        /** @brief Normalized page key. */
        std::string page;
        /** @brief Normalized template id key. */
        std::string templateId;
        /** @brief Equality operator used by hashed containers. */
        bool operator==(const DynamicTemplateKey& other) const noexcept
        {
            return page == other.page && templateId == other.templateId;
        }
    };
    /** @brief Builds the typed lookup key used by dynamic template visibility overrides. */
    DynamicTemplateKey MakeDynamicTemplateKey(std::string_view normalizedPageName, std::string_view templateId) const;
    /** @brief Returns whether one dynamic template set is currently visible on one page. */
    bool IsDynamicTemplateVisible(std::string_view normalizedPageName, std::string_view templateId) const noexcept;
    /** @brief Inserts one reticle entity into the ordered draw list of a page. */
    void InsertReticleIntoPageDrawList(std::string_view normalizedPageName, entt::entity entity);
    /** @brief Removes one reticle entity from the ordered draw list of a page. */
    void RemoveReticleFromPageDrawList(std::string_view normalizedPageName, entt::entity entity);
    /** @brief Synchronizes the active flag of a page entity with the current active-page selection. */
    void SetActiveFlag(std::string_view pageName, bool active);

    /** @brief Authored document currently backing the runtime scene. */
    MfdDocument document_ {};
    /** @brief EnTT registry owning all runtime entities and components. */
    entt::registry registry_ {};
    /** @brief Fast lookup from normalized page name to page entity. */
    std::unordered_map<std::string, entt::entity, TransparentStringHash, TransparentStringEqual> pageEntities_ {};
    /** @brief Fast lookup from normalized page name to strobe entity. */
    std::unordered_map<std::string, entt::entity, TransparentStringHash, TransparentStringEqual> strobeEntities_ {};
    /** @brief Hash helper for `ReticleKey`. */
    struct ReticleKeyHash
    {
        /** @brief Hashes one `ReticleKey`. */
        std::size_t operator()(const ReticleKey& key) const noexcept
        {
            const std::size_t pageHash = std::hash<std::string> {}(key.page);
            const std::size_t idHash = std::hash<std::string> {}(key.id);
            return pageHash ^ (idHash << 1U);
        }
    };
    /** @brief Hash helper for `DynamicTemplateKey`. */
    struct DynamicTemplateKeyHash
    {
        /** @brief Hashes one `DynamicTemplateKey`. */
        std::size_t operator()(const DynamicTemplateKey& key) const noexcept
        {
            const std::size_t pageHash = std::hash<std::string> {}(key.page);
            const std::size_t templateHash = std::hash<std::string> {}(key.templateId);
            return pageHash ^ (templateHash << 1U);
        }
    };
    /** @brief Fast lookup from page-plus-reticle typed key to reticle entity. */
    std::unordered_map<ReticleKey, entt::entity, ReticleKeyHash> reticleEntities_ {};
    /** @brief Optional per-page visibility overrides indexed by typed dynamic-template key. */
    std::unordered_map<DynamicTemplateKey, bool, DynamicTemplateKeyHash> dynamicTemplateVisibility_ {};
    /** @brief Monotonic ordering counter used to place dynamic reticles after authored content. */
    std::size_t nextDynamicOrder_ = 10000;
    /** @brief Normalized name of the currently active page. */
    std::string activePage_ {};
    /** @brief Whole-window display post-process state. */
    WindowDisplayState windowDisplay_ {};
    /** @brief Most recent structured error produced by a mutating command. */
    std::string lastError_ {};
};
} // namespace mfd
