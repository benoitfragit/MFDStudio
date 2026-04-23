/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for SceneRegistry.
 */

#include "mfd/runtime/SceneRegistry.h"

#include "mfd/control/CommandTypes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace mfd
{
namespace
{
constexpr std::size_t kStrobeDrawOrder = std::numeric_limits<std::size_t>::max() - 1U;
using BlinkClock = std::chrono::steady_clock;

std::string NormalizeReticleId(const std::string_view value)
{
    return NormalizePageName(value);
}

float DistanceSquared(const Vec2& lhs, const Vec2& rhs) noexcept
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

bool IsInsideCaptureArea(const Vec2& strobePosition,
                         const Vec2& candidatePosition,
                         const StrobeCaptureConfig& capture) noexcept
{
    const Vec2 offset = candidatePosition - strobePosition;

    switch (capture.shape)
    {
    case StrobeCaptureShape::Circle:
        return DistanceSquared(strobePosition, candidatePosition) <= capture.radius * capture.radius;

    case StrobeCaptureShape::Rectangle:
        return std::abs(offset.x) <= capture.size.x * 0.5f &&
               std::abs(offset.y) <= capture.size.y * 0.5f;
    }

    return false;
}

bool IsEmptyPatch(const ReticlePatch& patch) noexcept
{
    return !patch.visible.has_value() &&
           !patch.blinkEnabled.has_value() &&
           !patch.blinkType.has_value() &&
           !patch.blinkTypeId.has_value() &&
           !patch.position.has_value() &&
           !patch.rotationDegrees.has_value() &&
           !patch.color.has_value() &&
           !patch.thickness.has_value() &&
           !patch.text.has_value() &&
           patch.texts.empty() &&
           patch.textsById.empty() &&
           !patch.letterSpacing.has_value() &&
           patch.letterSpacings.empty() &&
           patch.letterSpacingsById.empty() &&
           patch.primitivePatches.empty() &&
           patch.primitivePatchesById.empty();
}

template <typename Geometry>
bool ApplyBoxPrimitivePatch(Geometry& geometry, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.width.has_value())
    {
        geometry.width = *patch.width;
        applied = true;
    }

    if (patch.height.has_value())
    {
        geometry.height = *patch.height;
        applied = true;
    }

    if (patch.size.has_value())
    {
        geometry.width = patch.size->x;
        geometry.height = patch.size->y;
        applied = true;
    }

    return applied;
}

bool ApplyPatchToPrimitive(Primitive& primitive, const PrimitivePatch& patch)
{
    bool applied = false;

    if (patch.visible.has_value())
    {
        primitive.style.visible = *patch.visible;
        applied = true;
    }

    if (patch.position.has_value())
    {
        primitive.transform.position = *patch.position;
        applied = true;
    }

    if (patch.rotationDegrees.has_value())
    {
        primitive.transform.rotationDegrees = *patch.rotationDegrees;
        applied = true;
    }

    if (patch.scale.has_value())
    {
        primitive.transform.scale = *patch.scale;
        applied = true;
    }

    if (patch.color.has_value())
    {
        primitive.style.color = *patch.color;
        applied = true;
    }

    if (patch.thickness.has_value())
    {
        primitive.style.thickness = *patch.thickness;
        applied = true;
    }

    if (patch.text.has_value())
    {
        if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            geometry->text = *patch.text;
            applied = true;
        }
    }

    if (patch.letterSpacing.has_value())
    {
        if (TextGeometry* textGeometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            textGeometry->letterSpacing = *patch.letterSpacing;
            applied = true;
        }
        else if (TimeGeometry* timeGeometry = std::get_if<TimeGeometry>(&primitive.geometry))
        {
            timeGeometry->letterSpacing = *patch.letterSpacing;
            applied = true;
        }
    }

    if (patch.lineStart.has_value())
    {
        if (LineGeometry* geometry = std::get_if<LineGeometry>(&primitive.geometry))
        {
            geometry->start = *patch.lineStart;
            applied = true;
        }
    }

    if (patch.lineEnd.has_value())
    {
        if (LineGeometry* geometry = std::get_if<LineGeometry>(&primitive.geometry))
        {
            geometry->end = *patch.lineEnd;
            applied = true;
        }
    }

    if (patch.radius.has_value())
    {
        if (CircleGeometry* geometry = std::get_if<CircleGeometry>(&primitive.geometry))
        {
            geometry->radius = *patch.radius;
            applied = true;
        }
    }

    if (patch.innerRadius.has_value() || patch.outerRadius.has_value())
    {
        if (RingGeometry* geometry = std::get_if<RingGeometry>(&primitive.geometry))
        {
            if (patch.innerRadius.has_value())
            {
                geometry->innerRadius = *patch.innerRadius;
                applied = true;
            }

            if (patch.outerRadius.has_value())
            {
                geometry->outerRadius = *patch.outerRadius;
                applied = true;
            }
        }
    }

    if (RectangleGeometry* rectangleGeometry = std::get_if<RectangleGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*rectangleGeometry, patch) || applied;
    }
    else if (EllipseGeometry* ellipseGeometry = std::get_if<EllipseGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*ellipseGeometry, patch) || applied;
    }
    else if (SquareGeometry* squareGeometry = std::get_if<SquareGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*squareGeometry, patch) || applied;
    }
    else if (DiamondGeometry* diamondGeometry = std::get_if<DiamondGeometry>(&primitive.geometry))
    {
        applied = ApplyBoxPrimitivePatch(*diamondGeometry, patch) || applied;
    }

    return applied;
}

bool IsEmptyPatch(const WindowDisplayPatch& patch) noexcept
{
    return !patch.invertColors.has_value() &&
           !patch.brightness.has_value() &&
           !patch.disabled.has_value();
}

float* FindTextLikeLetterSpacing(Primitive& primitive) noexcept
{
    if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    if (TimeGeometry* geometry = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    return nullptr;
}

bool ApplyPatchToReticleGroup(ReticleGroup& reticle, const ReticlePatch& patch)
{
    bool applied = false;

    if (patch.visible.has_value())
    {
        reticle.visible = *patch.visible;
        applied = true;
    }

    if (patch.position.has_value())
    {
        reticle.transform.position = *patch.position;
        applied = true;
    }

    if (patch.rotationDegrees.has_value())
    {
        reticle.transform.rotationDegrees = *patch.rotationDegrees;
        applied = true;
    }

    if (patch.color.has_value())
    {
        reticle.overrides.color = *patch.color;
        applied = true;
    }

    if (patch.thickness.has_value())
    {
        reticle.overrides.thickness = *patch.thickness;
        applied = true;
    }

    if (patch.text.has_value())
    {
        for (auto& primitive : reticle.primitives)
        {
            if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->text = *patch.text;
                applied = true;
                break;
            }
        }
    }

    for (const auto& [primitiveId, text] : patch.texts)
    {
        applied = SetTextPrimitive(reticle, primitiveId, text) || applied;
    }

    if (patch.letterSpacing.has_value())
    {
        for (auto& primitive : reticle.primitives)
        {
            if (float* letterSpacing = FindTextLikeLetterSpacing(primitive))
            {
                *letterSpacing = *patch.letterSpacing;
                applied = true;
                break;
            }
        }
    }

    for (const auto& [primitiveId, letterSpacing] : patch.letterSpacings)
    {
        applied = SetTextPrimitiveLetterSpacing(reticle, primitiveId, letterSpacing) || applied;
    }

    for (const auto& [primitiveId, primitivePatch] : patch.primitivePatches)
    {
        Primitive* primitive = FindPrimitive(reticle, primitiveId);
        if (primitive != nullptr)
        {
            applied = ApplyPatchToPrimitive(*primitive, primitivePatch) || applied;
        }
    }

    return applied;
}

struct BlinkPatchResult
{
    enum class Status
    {
        Unchanged,
        Applied,
        Invalid
    };

    Status status = Status::Unchanged;
    ReticleBlinkState blink {};
};
} // namespace

struct SceneRegistry::PageComponent
{
    std::string name;
    std::string normalizedName;
    std::string title;
    ColorRgba backgroundColor {6, 14, 20, 255};
    PageViewState view {};
    std::unordered_map<std::string, std::uint32_t, TransparentStringHash, TransparentStringEqual> blinkDurationsByType {};
    std::string defaultBlinkTypeName;
    std::string normalizedDefaultBlinkTypeName;
    std::uint32_t defaultBlinkDurationMs = 0;
    BlinkClock::time_point blinkEpoch = BlinkClock::now();
    std::vector<entt::entity> orderedReticleEntities;
    bool hasStrobe = false;
    bool active = false;
};

struct SceneRegistry::PageMembership
{
    std::string pageName;
};

struct SceneRegistry::ReticleComponent
{
    ReticleGroup group;
    std::size_t drawOrder = 0;
};

struct SceneRegistry::DynamicTag
{
};

struct SceneRegistry::StaticTag
{
};

struct SceneRegistry::StrobeTag
{
};

struct SceneRegistry::StrobeBehaviorComponent
{
    StrobeCaptureConfig capture;
    StrobeMagnetConfig magnet;
    std::string lockedReticleId;
};

namespace
{
template <typename PageLike>
bool TryResolveBlinkSelection(const PageLike& page,
                              const ReticleBlinkState& candidate,
                              std::uint32_t& durationMs) noexcept
{
    if (!candidate.typeName.empty())
    {
        const std::string normalizedTypeName =
            candidate.normalizedTypeName.empty() ? NormalizePageName(candidate.typeName) : candidate.normalizedTypeName;
        const auto iterator = page.blinkDurationsByType.find(normalizedTypeName);
        if (iterator == page.blinkDurationsByType.end())
        {
            return false;
        }

        durationMs = iterator->second;
        return true;
    }

    if (!candidate.enabled)
    {
        durationMs = 0;
        return true;
    }

    if (page.defaultBlinkDurationMs == 0)
    {
        return false;
    }

    durationMs = page.defaultBlinkDurationMs;
    return true;
}

template <typename PageLike>
BlinkPatchResult EvaluateBlinkPatch(const PageLike& page,
                                    const ReticleGroup& reticle,
                                    const ReticlePatch& patch) noexcept
{
    if (!patch.blinkEnabled.has_value() && !patch.blinkType.has_value())
    {
        return {};
    }

    BlinkPatchResult result;
    result.status = BlinkPatchResult::Status::Applied;
    result.blink = reticle.blink;

    if (patch.blinkType.has_value())
    {
        result.blink.typeName = *patch.blinkType;
        result.blink.normalizedTypeName = NormalizePageName(result.blink.typeName);
        if (!patch.blinkType->empty())
        {
            result.blink.enabled = true;
        }
    }

    if (patch.blinkEnabled.has_value())
    {
        result.blink.enabled = *patch.blinkEnabled;
    }

    std::uint32_t durationMs = result.blink.durationMs;
    if (!TryResolveBlinkSelection(page, result.blink, durationMs))
    {
        result.status = BlinkPatchResult::Status::Invalid;
        return result;
    }

    result.blink.durationMs = durationMs;
    return result;
}

template <typename PageLike>
bool ResolveBlinkForReticle(const PageLike& page, ReticleGroup& reticle) noexcept
{
    std::uint32_t durationMs = reticle.blink.durationMs;
    if (!TryResolveBlinkSelection(page, reticle.blink, durationMs))
    {
        reticle.blink = {};
        return false;
    }

    reticle.blink.durationMs = durationMs;
    return true;
}

bool IsBlinkVisible(const std::uint32_t durationMs,
                    const BlinkClock::time_point blinkEpoch,
                    const BlinkClock::time_point now) noexcept
{
    if (durationMs == 0)
    {
        return true;
    }

    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - blinkEpoch).count();
    const std::uint64_t periodMs = static_cast<std::uint64_t>(durationMs);
    const std::uint64_t phaseMs =
        static_cast<std::uint64_t>(elapsedMs < 0 ? 0 : elapsedMs) % periodMs;
    const std::uint64_t visibleWindowMs = std::max<std::uint64_t>(1U, (periodMs + 1U) / 2U);
    return phaseMs < visibleWindowMs;
}

template <typename PageLike>
bool IsReticleVisibleNow(const PageLike& page,
                         const ReticleGroup& reticle,
                         const BlinkClock::time_point now) noexcept
{
    return reticle.visible &&
           (!reticle.blink.enabled || IsBlinkVisible(reticle.blink.durationMs, page.blinkEpoch, now));
}
} // namespace

SceneRegistry::SceneRegistry(MfdDocument document)
{
    LoadDocument(std::move(document));
}

SceneRegistry::SceneRegistry(MfdDocument document, std::optional<GeneratedTransportMap> transportMap)
{
    LoadDocument(std::move(document), std::move(transportMap));
}

void SceneRegistry::LoadDocument(MfdDocument document)
{
    LoadDocument(std::move(document), std::nullopt);
}

void SceneRegistry::LoadDocument(MfdDocument document, std::optional<GeneratedTransportMap> transportMap)
{
    document_ = std::move(document);
    transportMap_ = std::move(transportMap);
    registry_.clear();
    pageEntities_.clear();
    strobeEntities_.clear();
    reticleEntities_.clear();
    dynamicTemplateVisibility_.clear();
    transportPageNames_.clear();
    transportReticles_.clear();
    transportTemplates_.clear();
    transportPrimitives_.clear();
    transportBlinks_.clear();
    nextDynamicOrder_ = 10000;
    activePage_.clear();
    windowDisplay_ = {};

    for (const auto& page : document_.pages)
    {
        const entt::entity pageEntity = registry_.create();
        PageComponent pageComponent;
        pageComponent.name = page.name;
        pageComponent.normalizedName = page.normalizedName;
        pageComponent.title = page.title;
        pageComponent.backgroundColor = page.backgroundColor;
        pageComponent.view = page.view;
        pageComponent.defaultBlinkTypeName = page.defaultBlinkTypeName;
        pageComponent.normalizedDefaultBlinkTypeName = page.normalizedDefaultBlinkTypeName;
        pageComponent.hasStrobe = page.strobe.has_value();
        pageComponent.blinkEpoch = BlinkClock::now();

        for (const auto& blinkType : page.blinkTypes)
        {
            pageComponent.blinkDurationsByType.emplace(blinkType.normalizedName, blinkType.durationMs);
        }

        if (!pageComponent.normalizedDefaultBlinkTypeName.empty())
        {
            const auto iterator = pageComponent.blinkDurationsByType.find(pageComponent.normalizedDefaultBlinkTypeName);
            if (iterator != pageComponent.blinkDurationsByType.end())
            {
                pageComponent.defaultBlinkDurationMs = iterator->second;
            }
        }
        else if (!page.blinkTypes.empty())
        {
            pageComponent.defaultBlinkTypeName = page.blinkTypes.front().name;
            pageComponent.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
            pageComponent.defaultBlinkDurationMs = page.blinkTypes.front().durationMs;
        }

        registry_.emplace<PageComponent>(pageEntity, std::move(pageComponent));
        pageEntities_.emplace(page.normalizedName, pageEntity);

        for (std::size_t index = 0; index < page.staticReticles.size(); ++index)
        {
            const entt::entity reticleEntity = registry_.create();
            registry_.emplace<ReticleComponent>(reticleEntity, ReticleComponent {page.staticReticles[index], index});
            registry_.emplace<PageMembership>(reticleEntity, PageMembership {page.normalizedName});
            registry_.emplace<StaticTag>(reticleEntity);
            IndexReticle(page.normalizedName, page.staticReticles[index], reticleEntity);
            InsertReticleIntoPageDrawList(page.normalizedName, reticleEntity);
        }

        if (page.strobe.has_value())
        {
            const entt::entity strobeEntity = registry_.create();
            registry_.emplace<ReticleComponent>(strobeEntity, ReticleComponent {page.strobe->reticle, kStrobeDrawOrder});
            registry_.emplace<PageMembership>(strobeEntity, PageMembership {page.normalizedName});
            registry_.emplace<StrobeBehaviorComponent>(strobeEntity,
                                                       StrobeBehaviorComponent {page.strobe->capture, page.strobe->magnet});
            registry_.emplace<StrobeTag>(strobeEntity);
            strobeEntities_.emplace(page.normalizedName, strobeEntity);
            IndexReticle(page.normalizedName, page.strobe->reticle, strobeEntity);
            InsertReticleIntoPageDrawList(page.normalizedName, strobeEntity);
        }
    }

    if (!document_.pages.empty())
    {
        const auto defaultPageIterator = std::find_if(
            document_.pages.begin(),
            document_.pages.end(),
            [](const PageDefinition& page)
            {
                return page.defaultPage;
            });

        activePage_ =
            defaultPageIterator != document_.pages.end()
                ? defaultPageIterator->normalizedName
                : document_.pages.front().normalizedName;
        SetActiveFlag(activePage_, true);
    }

    RebuildTransportIndexes();
}

const MfdDocument& SceneRegistry::Document() const noexcept
{
    return document_;
}

const std::optional<GeneratedTransportMap>& SceneRegistry::TransportMap() const noexcept
{
    return transportMap_;
}

bool SceneRegistry::HasTransportMap() const noexcept
{
    return transportMap_.has_value();
}

void SceneRegistry::RebuildTransportIndexes()
{
    transportPageNames_.clear();
    transportReticles_.clear();
    transportTemplates_.clear();
    transportPrimitives_.clear();
    transportBlinks_.clear();

    if (!transportMap_.has_value())
    {
        return;
    }

    for (const auto& page : transportMap_->pages)
    {
        transportPageNames_.emplace(page.id, page.name);
    }

    for (const auto& reticle : transportMap_->reticles)
    {
        transportReticles_.emplace(
            reticle.id,
            TransportReticleLookup {reticle.pageId, reticle.pageId == 0 ? std::string {} : transportPageNames_[reticle.pageId], reticle.reticleId});
    }

    for (const auto& templ : transportMap_->templates)
    {
        transportTemplates_.emplace(templ.id, templ.templateId);
    }

    for (const auto& primitive : transportMap_->primitives)
    {
        transportPrimitives_.emplace(
            primitive.id,
            TransportPrimitiveLookup {primitive.ownerKind, primitive.ownerId, primitive.primitiveId});
    }

    for (const auto& blink : transportMap_->blinkTypes)
    {
        transportBlinks_.emplace(blink.id, TransportBlinkLookup {blink.pageId, blink.blinkType});
    }
}

bool SceneRegistry::HasMatchingTransportMap(const std::string_view mappingHash) const noexcept
{
    return !mappingHash.empty() &&
           transportMap_.has_value() &&
           transportMap_->mappingHash == mappingHash;
}

const std::string* SceneRegistry::ResolvePageName(const TransportId pageId) const noexcept
{
    const auto iterator = transportPageNames_.find(pageId);
    return iterator == transportPageNames_.end() ? nullptr : &iterator->second;
}

const SceneRegistry::TransportReticleLookup* SceneRegistry::ResolveStaticReticle(const TransportId reticleId) const noexcept
{
    const auto iterator = transportReticles_.find(reticleId);
    return iterator == transportReticles_.end() ? nullptr : &iterator->second;
}

const std::string* SceneRegistry::ResolveTemplateId(const TransportId templateId) const noexcept
{
    const auto iterator = transportTemplates_.find(templateId);
    return iterator == transportTemplates_.end() ? nullptr : &iterator->second;
}

const std::string* SceneRegistry::ResolveBlinkType(const TransportId pageId, const TransportId blinkTypeId) const noexcept
{
    const auto iterator = transportBlinks_.find(blinkTypeId);
    if (iterator == transportBlinks_.end() || iterator->second.pageId != pageId)
    {
        return nullptr;
    }

    return &iterator->second.blinkType;
}

const std::string* SceneRegistry::ResolvePrimitiveIdForReticle(const TransportId reticleId,
                                                               const TransportId primitiveId) const noexcept
{
    const auto iterator = transportPrimitives_.find(primitiveId);
    if (iterator == transportPrimitives_.end() ||
        iterator->second.ownerKind != TransportPrimitiveOwnerKind::Reticle ||
        iterator->second.ownerId != reticleId)
    {
        return nullptr;
    }

    return &iterator->second.primitiveId;
}

const std::string* SceneRegistry::ResolvePrimitiveIdForTemplate(const TransportId templateId,
                                                                const TransportId primitiveId) const noexcept
{
    const auto iterator = transportPrimitives_.find(primitiveId);
    if (iterator == transportPrimitives_.end() ||
        iterator->second.ownerKind != TransportPrimitiveOwnerKind::Template ||
        iterator->second.ownerId != templateId)
    {
        return nullptr;
    }

    return &iterator->second.primitiveId;
}

const ReticleLibrary& SceneRegistry::Library() const noexcept
{
    return document_.reticleLibrary;
}

std::vector<PageSummary> SceneRegistry::Pages() const
{
    std::vector<PageSummary> pages;
    pages.reserve(document_.pages.size());

    for (const auto& page : document_.pages)
    {
        pages.push_back(PageSummary {
            page.name,
            page.title,
            page.backgroundColor,
            page.strobe.has_value(),
            page.normalizedName == activePage_});
    }

    return pages;
}

bool SceneRegistry::HasPage(const std::string_view pageName) const noexcept
{
    return HasNormalizedPage(NormalizePageName(pageName));
}

bool SceneRegistry::HasStrobe(const std::string_view pageName) const noexcept
{
    return HasNormalizedStrobe(NormalizePageName(pageName));
}

bool SceneRegistry::ActivePageHasStrobe() const noexcept
{
    return HasNormalizedStrobe(activePage_);
}

bool SceneRegistry::HasNormalizedPage(const std::string_view pageName) const noexcept
{
    return pageEntities_.find(std::string(pageName)) != pageEntities_.end();
}

bool SceneRegistry::HasNormalizedStrobe(const std::string_view pageName) const noexcept
{
    return strobeEntities_.find(std::string(pageName)) != strobeEntities_.end();
}

void SceneRegistry::SetActivePage(const std::string_view pageName) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (!HasNormalizedPage(normalizedPageName))
    {
        return;
    }

    SetActiveFlag(activePage_, false);
    activePage_ = normalizedPageName;
    SetActiveFlag(activePage_, true);
}

std::string SceneRegistry::ActivePageName() const
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? std::string {} : page->name;
}

std::optional<PageSummary> SceneRegistry::ActivePageSummary() const
{
    const PageComponent* page = FindPage(activePage_);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return PageSummary {
        page->name,
        page->title,
        page->backgroundColor,
        page->hasStrobe,
        true};
}

std::optional<PageViewState> SceneRegistry::ViewForPage(const std::string_view pageName) const noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return std::nullopt;
    }

    return page->view;
}

PageViewState SceneRegistry::ActivePageView() const noexcept
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? PageViewState {} : page->view;
}

std::optional<StrobeSummary> SceneRegistry::StrobeForPage(const std::string_view pageName) const
{
    return StrobeForPageKey(NormalizePageName(pageName));
}

std::optional<StrobeSummary> SceneRegistry::ActiveStrobeSummary() const
{
    return StrobeForPageKey(activePage_);
}

std::optional<StrobeSummary> SceneRegistry::StrobeForPageKey(const std::string_view pageName) const
{
    const auto iterator = strobeEntities_.find(std::string(pageName));
    if (iterator == strobeEntities_.end())
    {
        return std::nullopt;
    }

    const auto* reticle = registry_.try_get<ReticleComponent>(iterator->second);
    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(iterator->second);
    const PageComponent* page = FindPage(pageName);
    if (reticle == nullptr || behavior == nullptr || page == nullptr)
    {
        return std::nullopt;
    }

    const BlinkClock::time_point now = BlinkClock::now();
    Vec2 strobePosition = reticle->group.transform.position;
    if (const ReticleComponent* lockedTarget =
            FindLockedStrobeMagnetTarget(pageName, behavior->lockedReticleId);
        lockedTarget != nullptr)
    {
        strobePosition = lockedTarget->group.transform.position;
    }

    return StrobeSummary {
        page->name,
        reticle->group.id,
        strobePosition,
        behavior->capture,
        behavior->magnet,
        IsReticleVisibleNow(*page, reticle->group, now)};
}

std::optional<StrobeMagnetSummary> SceneRegistry::StrobeMagnetForPage(const std::string_view pageName) const
{
    return StrobeMagnetForPageKey(NormalizePageName(pageName));
}

std::optional<StrobeMagnetSummary> SceneRegistry::ActiveStrobeMagnetSummary() const
{
    return StrobeMagnetForPageKey(activePage_);
}

std::optional<StrobeMagnetSummary> SceneRegistry::StrobeMagnetForPageKey(const std::string_view pageName) const
{
    const auto iterator = strobeEntities_.find(std::string(pageName));
    if (iterator == strobeEntities_.end())
    {
        return std::nullopt;
    }

    const auto* reticle = registry_.try_get<ReticleComponent>(iterator->second);
    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(iterator->second);
    const PageComponent* page = FindPage(pageName);
    if (reticle == nullptr || behavior == nullptr || page == nullptr)
    {
        return std::nullopt;
    }

    StrobeMagnetSummary summary;
    summary.enabled = behavior->magnet.enabled;
    summary.radius = behavior->magnet.radius;
    summary.strength = behavior->magnet.strength;

    const BlinkClock::time_point now = BlinkClock::now();

    if (!IsReticleVisibleNow(*page, reticle->group, now) ||
        !behavior->magnet.enabled || behavior->magnet.radius <= 0.0f)
    {
        return summary;
    }

    if (const ReticleComponent* lockedTarget =
            FindLockedStrobeMagnetTarget(pageName, behavior->lockedReticleId);
        lockedTarget != nullptr)
    {
        summary.magnetized = true;
        summary.reticleId = lockedTarget->group.id;
        summary.targetPosition = lockedTarget->group.transform.position;
        summary.distance = 0.0f;
        return summary;
    }

    if (const ReticleComponent* nearestTarget =
            FindNearestStrobeMagnetTarget(pageName, reticle->group.transform.position, behavior->magnet.radius);
        nearestTarget != nullptr)
    {
        summary.magnetized = true;
        summary.reticleId = nearestTarget->group.id;
        summary.targetPosition = nearestTarget->group.transform.position;
        summary.distance = std::sqrt(
            DistanceSquared(reticle->group.transform.position, nearestTarget->group.transform.position));
    }

    return summary;
}

ColorRgba SceneRegistry::ActiveBackgroundColor() const noexcept
{
    const PageComponent* page = FindPage(activePage_);
    return page == nullptr ? ColorRgba {6, 14, 20, 255} : page->backgroundColor;
}

WindowDisplayState SceneRegistry::WindowDisplay() const noexcept
{
    return windowDisplay_;
}

std::vector<ReticleGroup> SceneRegistry::CollectPageReticles(const std::string_view pageName) const
{
    return CollectPageReticlesByKey(NormalizePageName(pageName));
}

std::vector<ReticleGroup> SceneRegistry::CollectPageReticlesByKey(const std::string_view pageName) const
{
    const std::vector<ReticleRenderView> reticleViews = CollectPageReticleViewsByKey(pageName);
    std::vector<ReticleGroup> result;
    result.reserve(reticleViews.size());

    for (const ReticleRenderView& reticleView : reticleViews)
    {
        if (reticleView.group != nullptr)
        {
            ReticleGroup copy = *reticleView.group;
            copy.visible = reticleView.visible;
            result.push_back(std::move(copy));
        }
    }

    return result;
}

std::vector<ReticleRenderView> SceneRegistry::CollectPageReticleViews(const std::string_view pageName) const
{
    return CollectPageReticleViewsByKey(NormalizePageName(pageName));
}

std::vector<ReticleRenderView> SceneRegistry::CollectPageReticleViewsByKey(const std::string_view pageName) const
{
    const PageComponent* page = FindPage(pageName);
    if (page == nullptr)
    {
        return {};
    }

    std::vector<ReticleRenderView> result;
    result.reserve(page->orderedReticleEntities.size());
    const BlinkClock::time_point now = BlinkClock::now();

    for (const entt::entity entity : page->orderedReticleEntities)
    {
        const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
        if (reticle == nullptr)
        {
            continue;
        }

        bool visible = IsReticleVisibleNow(*page, reticle->group, now);
        if (visible && registry_.all_of<DynamicTag>(entity))
        {
            visible = IsDynamicTemplateVisible(pageName, reticle->group.sourceTemplateId);
        }

        result.push_back(ReticleRenderView {
            &reticle->group,
            visible});
    }

    return result;
}

std::vector<const ReticleGroup*> SceneRegistry::CollectPageReticlePointers(const std::string_view pageName) const
{
    return CollectPageReticlePointersByKey(NormalizePageName(pageName));
}

std::vector<const ReticleGroup*> SceneRegistry::CollectPageReticlePointersByKey(const std::string_view pageName) const
{
    const PageComponent* page = FindPage(pageName);
    if (page == nullptr)
    {
        return {};
    }

    std::vector<const ReticleGroup*> result;
    result.reserve(page->orderedReticleEntities.size());

    for (const entt::entity entity : page->orderedReticleEntities)
    {
        if (const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity); reticle != nullptr)
        {
            result.push_back(&reticle->group);
        }
    }

    return result;
}

std::vector<ReticleGroup> SceneRegistry::CollectActiveReticles() const
{
    return CollectPageReticlesByKey(activePage_);
}

std::vector<ReticleRenderView> SceneRegistry::CollectActiveReticleViews() const
{
    return CollectPageReticleViewsByKey(activePage_);
}

std::vector<const ReticleGroup*> SceneRegistry::CollectActiveReticlePointers() const
{
    return CollectPageReticlePointersByKey(activePage_);
}

bool SceneRegistry::SetPageView(const std::string_view pageName, const PageViewState& view) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.center = view.center;
    page->view.zoom = SanitizeZoom(view.zoom);
    return true;
}

bool SceneRegistry::SetPageViewCenter(const std::string_view pageName, const Vec2 center) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.center = center;
    return true;
}

bool SceneRegistry::SetPageZoom(const std::string_view pageName, const float zoom) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return false;
    }

    page->view.zoom = SanitizeZoom(zoom);
    return true;
}

bool SceneRegistry::SetWindowColorInverted(const bool invertColors) noexcept
{
    windowDisplay_.invertColors = invertColors;
    return true;
}

bool SceneRegistry::SetWindowBrightness(const float brightness) noexcept
{
    if (!std::isfinite(brightness))
    {
        return false;
    }

    windowDisplay_.brightness = std::clamp(brightness, 0.0f, 1.0f);
    return true;
}

bool SceneRegistry::SetWindowDisabled(const bool disabled) noexcept
{
    windowDisplay_.disabled = disabled;
    return true;
}

bool SceneRegistry::ApplyWindowDisplayPatch(const WindowDisplayPatch& patch) noexcept
{
    if (patch.brightness.has_value() && !std::isfinite(*patch.brightness))
    {
        return false;
    }

    bool applied = false;

    if (patch.invertColors.has_value())
    {
        windowDisplay_.invertColors = *patch.invertColors;
        applied = true;
    }

    if (patch.disabled.has_value())
    {
        windowDisplay_.disabled = *patch.disabled;
        applied = true;
    }

    if (patch.brightness.has_value())
    {
        windowDisplay_.brightness = std::clamp(*patch.brightness, 0.0f, 1.0f);
        applied = true;
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::SetReticleVisible(const std::string_view pageName,
                                      const std::string_view reticleId,
                                      const bool visible) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.visible = visible;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleBlinkEnabled(const std::string_view pageName,
                                           const std::string_view reticleId,
                                           const bool enabled) noexcept
{
    ReticlePatch patch;
    patch.blinkEnabled = enabled;
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::SetReticleBlinkType(const std::string_view pageName,
                                        const std::string_view reticleId,
                                        const std::string_view blinkType) noexcept
{
    ReticlePatch patch;
    patch.blinkType = std::string(blinkType);
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::ClearReticleBlinkType(const std::string_view pageName,
                                          const std::string_view reticleId) noexcept
{
    ReticlePatch patch;
    patch.blinkType = std::string {};
    return ApplyReticlePatch(pageName, reticleId, patch);
}

bool SceneRegistry::SetReticlePosition(const std::string_view pageName,
                                       const std::string_view reticleId,
                                       const Vec2 position) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.transform.position = position;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleRotation(const std::string_view pageName,
                                       const std::string_view reticleId,
                                       const float rotationDegrees) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.transform.rotationDegrees = rotationDegrees;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleColor(const std::string_view pageName,
                                    const std::string_view reticleId,
                                    const ColorRgba color) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.overrides.color = color;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleThickness(const std::string_view pageName,
                                        const std::string_view reticleId,
                                        const float thickness) noexcept
{
    if (!std::isfinite(thickness) || thickness <= 0.0f)
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    if (ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId))
    {
        reticle->group.overrides.thickness = thickness;
        return true;
    }

    return false;
}

bool SceneRegistry::SetReticleText(const std::string_view pageName,
                                   const std::string_view reticleId,
                                   std::string value) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    if (reticle == nullptr)
    {
        return false;
    }

    for (auto& primitive : reticle->group.primitives)
    {
        if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            geometry->text = std::move(value);
            return true;
        }
    }

    return false;
}

bool SceneRegistry::SetReticleText(const std::string_view pageName,
                                   const std::string_view reticleId,
                                   const std::string_view primitiveId,
                                   std::string value) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    return reticle != nullptr && SetTextPrimitive(reticle->group, primitiveId, std::move(value));
}

bool SceneRegistry::SetReticleLetterSpacing(const std::string_view pageName,
                                            const std::string_view reticleId,
                                            const float letterSpacing) noexcept
{
    if (!std::isfinite(letterSpacing))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    if (reticle == nullptr)
    {
        return false;
    }

    for (auto& primitive : reticle->group.primitives)
    {
        if (float* primitiveLetterSpacing = FindTextLikeLetterSpacing(primitive))
        {
            *primitiveLetterSpacing = letterSpacing;
            return true;
        }
    }

    return false;
}

bool SceneRegistry::SetReticleLetterSpacing(const std::string_view pageName,
                                            const std::string_view reticleId,
                                            const std::string_view primitiveId,
                                            const float letterSpacing) noexcept
{
    if (!std::isfinite(letterSpacing))
    {
        return false;
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    return reticle != nullptr && SetTextPrimitiveLetterSpacing(reticle->group, primitiveId, letterSpacing);
}

bool SceneRegistry::ApplyReticlePatch(const std::string_view pageName,
                                      const std::string_view reticleId,
                                      const ReticlePatch& patch) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    ReticleComponent* reticle = FindReticle(normalizedPageName, reticleId);
    if (page == nullptr || reticle == nullptr)
    {
        return false;
    }

    const BlinkPatchResult blinkResult = EvaluateBlinkPatch(*page, reticle->group, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Invalid)
    {
        return false;
    }

    bool applied = ApplyPatchToReticleGroup(reticle->group, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Applied)
    {
        reticle->group.blink = blinkResult.blink;
        applied = true;
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::HasDynamicReticle(const std::string_view pageName, const std::string_view reticleId) const noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity != entt::null && registry_.all_of<DynamicTag>(entity);
}

bool SceneRegistry::ApplyDynamicReticlePatch(const std::string_view pageName,
                                             const std::string_view reticleId,
                                             const ReticlePatch& patch) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    PageComponent* page = FindPage(normalizedPageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (page == nullptr || entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return false;
    }

    ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
    if (reticle == nullptr)
    {
        return false;
    }

    const BlinkPatchResult blinkResult = EvaluateBlinkPatch(*page, reticle->group, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Invalid)
    {
        return false;
    }

    bool applied = ApplyPatchToReticleGroup(reticle->group, patch);
    if (blinkResult.status == BlinkPatchResult::Status::Applied)
    {
        reticle->group.blink = blinkResult.blink;
        applied = true;
    }

    if (applied)
    {
        RefreshStickyStrobePosition(normalizedPageName);
    }

    return applied || IsEmptyPatch(patch);
}

bool SceneRegistry::SetDynamicReticleSetVisible(const std::string_view pageName,
                                                const std::string_view templateId,
                                                const bool visible) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const std::string normalizedTemplateId = NormalizePageName(templateId);
    if (!HasNormalizedPage(normalizedPageName) || normalizedTemplateId.empty())
    {
        return false;
    }

    const std::string key = MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId);
    if (visible)
    {
        dynamicTemplateVisibility_.erase(key);
    }
    else
    {
        dynamicTemplateVisibility_[key] = false;
    }

    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindLockedStrobeMagnetTarget(
    const std::string_view normalizedPageName,
    const std::string_view reticleId) const noexcept
{
    if (reticleId.empty())
    {
        return nullptr;
    }

    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return nullptr;
    }

    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return nullptr;
    }

    const auto* membership = registry_.try_get<PageMembership>(entity);
    const auto* reticle = registry_.try_get<ReticleComponent>(entity);
    if (membership == nullptr || reticle == nullptr || membership->pageName != normalizedPageName)
    {
        return nullptr;
    }

    const BlinkClock::time_point now = BlinkClock::now();
    if (!IsReticleVisibleNow(*page, reticle->group, now) ||
        !IsDynamicTemplateVisible(normalizedPageName, reticle->group.sourceTemplateId))
    {
        return nullptr;
    }

    return reticle;
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindNearestStrobeMagnetTarget(
    const std::string_view normalizedPageName,
    const Vec2 position,
    const float radius) const noexcept
{
    const PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr || radius <= 0.0f)
    {
        return nullptr;
    }

    const float radiusSquared = radius * radius;
    float bestDistanceSquared = radiusSquared;
    const ReticleComponent* bestTarget = nullptr;
    const BlinkClock::time_point now = BlinkClock::now();

    auto dynamicView = registry_.view<ReticleComponent, PageMembership, DynamicTag>();
    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName != normalizedPageName)
        {
            continue;
        }

        const auto& dynamicReticle = dynamicView.get<ReticleComponent>(entity);
        if (!IsReticleVisibleNow(*page, dynamicReticle.group, now) ||
            !IsDynamicTemplateVisible(normalizedPageName, dynamicReticle.group.sourceTemplateId))
        {
            continue;
        }

        const float distanceSquared = DistanceSquared(position, dynamicReticle.group.transform.position);
        if (!std::isfinite(distanceSquared) ||
            distanceSquared > radiusSquared ||
            distanceSquared >= bestDistanceSquared)
        {
            continue;
        }

        bestDistanceSquared = distanceSquared;
        bestTarget = &dynamicReticle;
    }

    return bestTarget;
}

void SceneRegistry::RefreshStickyStrobePosition(const std::string_view normalizedPageName) noexcept
{
    const auto iterator = strobeEntities_.find(std::string(normalizedPageName));
    if (iterator == strobeEntities_.end())
    {
        return;
    }

    auto* strobe = registry_.try_get<ReticleComponent>(iterator->second);
    auto* behavior = registry_.try_get<StrobeBehaviorComponent>(iterator->second);
    if (strobe == nullptr || behavior == nullptr || behavior->lockedReticleId.empty())
    {
        return;
    }

    const ReticleComponent* target = FindLockedStrobeMagnetTarget(normalizedPageName, behavior->lockedReticleId);
    if (target == nullptr)
    {
        behavior->lockedReticleId.clear();
        return;
    }

    strobe->group.transform.position = target->group.transform.position;
}

bool SceneRegistry::SetStrobeActive(const std::string_view pageName, const bool active) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const auto iterator = strobeEntities_.find(std::string(normalizedPageName));
    if (iterator == strobeEntities_.end())
    {
        return false;
    }

    if (auto* reticle = registry_.try_get<ReticleComponent>(iterator->second))
    {
        reticle->group.visible = active;
        return true;
    }

    return false;
}

bool SceneRegistry::SetStrobePosition(const std::string_view pageName, const Vec2 position) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const auto iterator = strobeEntities_.find(std::string(normalizedPageName));
    if (iterator == strobeEntities_.end())
    {
        return false;
    }

    auto* reticle = registry_.try_get<ReticleComponent>(iterator->second);
    auto* behavior = registry_.try_get<StrobeBehaviorComponent>(iterator->second);
    if (reticle != nullptr)
    {
        if (behavior != nullptr)
        {
            behavior->lockedReticleId.clear();

            if (behavior->magnet.enabled && behavior->magnet.radius > 0.0f)
            {
                if (const ReticleComponent* target =
                        FindNearestStrobeMagnetTarget(normalizedPageName, position, behavior->magnet.radius);
                    target != nullptr)
                {
                    behavior->lockedReticleId = target->group.id;
                    reticle->group.transform.position = target->group.transform.position;
                    return true;
                }
            }
        }

        reticle->group.transform.position = position;
        return true;
    }

    return false;
}

bool SceneRegistry::OffsetStrobe(const std::string_view pageName, const Vec2 delta) noexcept
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const auto iterator = strobeEntities_.find(std::string(normalizedPageName));
    if (iterator == strobeEntities_.end())
    {
        return false;
    }

    if (const auto* reticle = registry_.try_get<ReticleComponent>(iterator->second))
    {
        return SetStrobePosition(pageName, reticle->group.transform.position + delta);
    }

    return false;
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureWithStrobe(const std::string_view pageName) const
{
    return CaptureWithStrobeKey(NormalizePageName(pageName));
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureActivePageStrobe() const
{
    return CaptureWithStrobeKey(activePage_);
}

std::optional<StrobeCaptureResult> SceneRegistry::CaptureWithStrobeKey(const std::string_view pageName) const
{
    const auto strobeIterator = strobeEntities_.find(std::string(pageName));
    if (strobeIterator == strobeEntities_.end())
    {
        return std::nullopt;
    }

    const auto* strobe = registry_.try_get<ReticleComponent>(strobeIterator->second);
    const auto* behavior = registry_.try_get<StrobeBehaviorComponent>(strobeIterator->second);
    const PageComponent* page = FindPage(pageName);
    if (strobe == nullptr || behavior == nullptr || page == nullptr)
    {
        return std::nullopt;
    }

    const BlinkClock::time_point now = BlinkClock::now();
    if (!IsReticleVisibleNow(*page, strobe->group, now))
    {
        return std::nullopt;
    }

    Vec2 strobePosition = strobe->group.transform.position;
    if (const ReticleComponent* lockedTarget =
            FindLockedStrobeMagnetTarget(pageName, behavior->lockedReticleId);
        lockedTarget != nullptr)
    {
        strobePosition = lockedTarget->group.transform.position;
    }

    std::optional<StrobeCaptureResult> bestCapture;
    float bestDistanceSquared = std::numeric_limits<float>::max();

    auto dynamicView = registry_.view<ReticleComponent, PageMembership, DynamicTag>();
    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName != pageName)
        {
            continue;
        }

        const auto& reticle = dynamicView.get<ReticleComponent>(entity).group;
        if (!IsReticleVisibleNow(*page, reticle, now) ||
            !IsDynamicTemplateVisible(pageName, reticle.sourceTemplateId))
        {
            continue;
        }

        const Vec2 candidatePosition = reticle.transform.position;
        if (!IsInsideCaptureArea(strobePosition, candidatePosition, behavior->capture))
        {
            continue;
        }

        const float distanceSquared = DistanceSquared(strobePosition, candidatePosition);
        if (distanceSquared >= bestDistanceSquared)
        {
            continue;
        }

        bestDistanceSquared = distanceSquared;
        bestCapture = StrobeCaptureResult {
            page->name,
            strobe->group.id,
            reticle.id,
            reticle.sourceTemplateId,
            reticle.info.label,
            reticle.info.category,
            candidatePosition,
            std::sqrt(distanceSquared),
            reticle.info.metadata};
    }

    return bestCapture;
}

void SceneRegistry::UpsertDynamicReticle(const std::string_view pageName, ReticleGroup reticle)
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    if (!HasNormalizedPage(normalizedPageName) || NormalizeReticleId(reticle.id).empty())
    {
        return;
    }

    if (const PageComponent* page = FindPage(normalizedPageName); page != nullptr)
    {
        (void)ResolveBlinkForReticle(*page, reticle);
    }

    const entt::entity existingEntity = FindReticleEntity(normalizedPageName, reticle.id);
    if (existingEntity != entt::null)
    {
        if (registry_.all_of<DynamicTag>(existingEntity))
        {
            if (auto* component = registry_.try_get<ReticleComponent>(existingEntity))
            {
                component->group = std::move(reticle);
                RefreshStickyStrobePosition(normalizedPageName);
            }
        }
        return;
    }

    const entt::entity entity = registry_.create();
    registry_.emplace<ReticleComponent>(entity, ReticleComponent {std::move(reticle), nextDynamicOrder_++});
    registry_.emplace<PageMembership>(entity, PageMembership {normalizedPageName});
    registry_.emplace<DynamicTag>(entity);
    IndexReticle(normalizedPageName, registry_.get<ReticleComponent>(entity).group, entity);
    InsertReticleIntoPageDrawList(normalizedPageName, entity);
    RefreshStickyStrobePosition(normalizedPageName);
}

bool SceneRegistry::RemoveDynamicReticle(const std::string_view pageName, const std::string_view reticleId)
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    if (entity == entt::null || !registry_.all_of<DynamicTag>(entity))
    {
        return false;
    }

    RemoveReticleIndex(normalizedPageName, reticleId);
    RemoveReticleFromPageDrawList(normalizedPageName, entity);
    registry_.destroy(entity);
    RefreshStickyStrobePosition(normalizedPageName);
    return true;
}

void SceneRegistry::ClearDynamicReticles(const std::string_view pageName)
{
    const std::string normalizedPageName = NormalizePageName(pageName);
    std::vector<entt::entity> entitiesToDestroy;
    auto dynamicView = registry_.view<PageMembership, DynamicTag>();

    for (const entt::entity entity : dynamicView)
    {
        const auto& membership = dynamicView.get<PageMembership>(entity);
        if (membership.pageName == normalizedPageName)
        {
            entitiesToDestroy.push_back(entity);
        }
    }

    for (const entt::entity entity : entitiesToDestroy)
    {
        const auto& membership = registry_.get<PageMembership>(entity);
        const auto& reticle = registry_.get<ReticleComponent>(entity).group;
        RemoveReticleIndex(membership.pageName, reticle.id);
        RemoveReticleFromPageDrawList(membership.pageName, entity);
        registry_.destroy(entity);
        RefreshStickyStrobePosition(membership.pageName);
    }
}

void SceneRegistry::ClearAllDynamicReticles()
{
    std::vector<entt::entity> entitiesToDestroy;
    auto dynamicView = registry_.view<DynamicTag>();

    for (const entt::entity entity : dynamicView)
    {
        entitiesToDestroy.push_back(entity);
    }

    for (const entt::entity entity : entitiesToDestroy)
    {
        const auto& membership = registry_.get<PageMembership>(entity);
        const auto& reticle = registry_.get<ReticleComponent>(entity).group;
        RemoveReticleIndex(membership.pageName, reticle.id);
        RemoveReticleFromPageDrawList(membership.pageName, entity);
        registry_.destroy(entity);
        RefreshStickyStrobePosition(membership.pageName);
    }
}

void SceneRegistry::ResetToInitialState()
{
    LoadDocument(document_);
}

entt::registry& SceneRegistry::Raw() noexcept
{
    return registry_;
}

const entt::registry& SceneRegistry::Raw() const noexcept
{
    return registry_;
}

entt::entity SceneRegistry::FindReticleEntity(const std::string_view normalizedPageName,
                                              const std::string_view reticleId) const noexcept
{
    const auto iterator = reticleEntities_.find(MakeReticleLookupKey(normalizedPageName, reticleId));
    return iterator == reticleEntities_.end() ? entt::null : iterator->second;
}

SceneRegistry::ReticleComponent* SceneRegistry::FindReticle(const std::string_view normalizedPageName,
                                                            const std::string_view reticleId) noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
}

const SceneRegistry::ReticleComponent* SceneRegistry::FindReticle(const std::string_view normalizedPageName,
                                                                  const std::string_view reticleId) const noexcept
{
    const entt::entity entity = FindReticleEntity(normalizedPageName, reticleId);
    return entity == entt::null ? nullptr : registry_.try_get<ReticleComponent>(entity);
}

SceneRegistry::PageComponent* SceneRegistry::FindPage(const std::string_view normalizedPageName) noexcept
{
    const auto iterator = pageEntities_.find(std::string(normalizedPageName));
    return iterator == pageEntities_.end() ? nullptr : registry_.try_get<PageComponent>(iterator->second);
}

const SceneRegistry::PageComponent* SceneRegistry::FindPage(const std::string_view normalizedPageName) const noexcept
{
    const auto iterator = pageEntities_.find(std::string(normalizedPageName));
    return iterator == pageEntities_.end() ? nullptr : registry_.try_get<PageComponent>(iterator->second);
}

std::string SceneRegistry::MakeReticleLookupKey(const std::string_view normalizedPageName,
                                                const std::string_view reticleId) const
{
    std::string key;
    key.reserve(normalizedPageName.size() + reticleId.size() + 1U);
    key.append(normalizedPageName);
    key.push_back(':');
    key.append(NormalizeReticleId(reticleId));
    return key;
}

void SceneRegistry::IndexReticle(const std::string_view normalizedPageName,
                                 const ReticleGroup& reticle,
                                 const entt::entity entity)
{
    reticleEntities_.insert_or_assign(MakeReticleLookupKey(normalizedPageName, reticle.id), entity);
}

void SceneRegistry::RemoveReticleIndex(const std::string_view normalizedPageName, const std::string_view reticleId)
{
    reticleEntities_.erase(MakeReticleLookupKey(normalizedPageName, reticleId));
}

std::string SceneRegistry::MakeDynamicTemplateLookupKey(const std::string_view normalizedPageName,
                                                        const std::string_view templateId) const
{
    return std::string(normalizedPageName) + '\x1F' + std::string(templateId);
}

bool SceneRegistry::IsDynamicTemplateVisible(const std::string_view normalizedPageName,
                                             const std::string_view templateId) const noexcept
{
    const std::string normalizedTemplateId = NormalizePageName(templateId);
    if (normalizedTemplateId.empty())
    {
        return true;
    }

    const auto iterator = dynamicTemplateVisibility_.find(
        MakeDynamicTemplateLookupKey(normalizedPageName, normalizedTemplateId));
    if (iterator == dynamicTemplateVisibility_.end())
    {
        return true;
    }

    return iterator->second;
}

void SceneRegistry::InsertReticleIntoPageDrawList(const std::string_view normalizedPageName, const entt::entity entity)
{
    PageComponent* page = FindPage(normalizedPageName);
    const ReticleComponent* reticle = registry_.try_get<ReticleComponent>(entity);
    if (page == nullptr || reticle == nullptr)
    {
        return;
    }

    auto& drawList = page->orderedReticleEntities;
    if (std::find(drawList.begin(), drawList.end(), entity) != drawList.end())
    {
        return;
    }

    const auto insertionPoint = std::lower_bound(
        drawList.begin(),
        drawList.end(),
        reticle->drawOrder,
        [this](const entt::entity candidate, const std::size_t drawOrder)
        {
            const ReticleComponent* candidateReticle = registry_.try_get<ReticleComponent>(candidate);
            return candidateReticle != nullptr && candidateReticle->drawOrder < drawOrder;
        });
    drawList.insert(insertionPoint, entity);
}

void SceneRegistry::RemoveReticleFromPageDrawList(const std::string_view normalizedPageName, const entt::entity entity)
{
    PageComponent* page = FindPage(normalizedPageName);
    if (page == nullptr)
    {
        return;
    }

    auto& drawList = page->orderedReticleEntities;
    drawList.erase(std::remove(drawList.begin(), drawList.end(), entity), drawList.end());
}

void SceneRegistry::SetActiveFlag(const std::string_view pageName, const bool active)
{
    const auto iterator = pageEntities_.find(std::string(pageName));
    if (iterator == pageEntities_.end())
    {
        return;
    }

    if (auto* page = registry_.try_get<PageComponent>(iterator->second))
    {
        page->active = active;
    }
}
} // namespace mfd
