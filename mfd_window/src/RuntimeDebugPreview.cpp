/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the internal runtime debug preview scene.
 */

#include "mfd/window/debug/RuntimeDebugPreview.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"

namespace mfd::window::debug
{
namespace
{
bool SameVector(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool SameColor(const ColorRgba& lhs, const ColorRgba& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool SameTransform(const Transform2D& lhs, const Transform2D& rhs) noexcept
{
    return SameVector(lhs.position, rhs.position) &&
           lhs.rotationDegrees == rhs.rotationDegrees &&
           SameVector(lhs.scale, rhs.scale);
}

const PageDefinition* FindPage(const MfdDocument& document, std::string_view pageName)
{
    return FindPageDefinition(document, pageName);
}

const TransportMapPageEntry* FindTransportPageEntry(const GeneratedTransportMap& map, const TransportId pageId) noexcept
{
    const auto iterator = std::find_if(
        map.pages.begin(),
        map.pages.end(),
        [pageId](const TransportMapPageEntry& page)
        {
            return page.id == pageId;
        });
    return iterator == map.pages.end() ? nullptr : &(*iterator);
}

std::string ResolveTransportPageName(const std::optional<GeneratedTransportMap>& transportMap,
                                     const std::string_view pageName,
                                     const TransportId pageId)
{
    if (!pageName.empty() || !transportMap.has_value() || pageId == 0)
    {
        return std::string(pageName);
    }

    const TransportMapPageEntry* pageEntry = FindTransportPageEntry(*transportMap, pageId);
    return pageEntry == nullptr ? std::string(pageName) : pageEntry->name;
}

const ReticleGroup* FindAuthoredReticle(const PageDefinition& page, std::string_view reticleId, const ReticleKind kind)
{
    if (kind == ReticleKind::Strobe)
    {
        const auto iterator = std::find_if(
            page.strobes.begin(),
            page.strobes.end(),
            [reticleId](const PageStrobeDefinition& strobe)
            {
                return strobe.reticle.id == reticleId;
            });
        return iterator == page.strobes.end() ? nullptr : &iterator->reticle;
    }

    const auto iterator = std::find_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [reticleId](const ReticleGroup& reticle)
        {
            return reticle.id == reticleId;
        });
    return iterator == page.staticReticles.end() ? nullptr : &(*iterator);
}

ReticleKind ClassifyReticle(const PageDefinition& page, std::string_view reticleId)
{
    if (std::any_of(
            page.strobes.begin(),
            page.strobes.end(),
            [reticleId](const PageStrobeDefinition& strobe)
            {
                return strobe.reticle.id == reticleId;
            }))
    {
        return ReticleKind::Strobe;
    }

    const auto iterator = std::find_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [reticleId](const ReticleGroup& reticle)
        {
            return reticle.id == reticleId;
        });
    return iterator == page.staticReticles.end() ? ReticleKind::Dynamic : ReticleKind::Static;
}

const ReticleGroup* FindReticle(const SceneRegistry& scene, const ReticleKey& key)
{
    const auto reticles = scene.CollectPageReticlePointers(key.pageName);
    const auto iterator = std::find_if(
        reticles.begin(),
        reticles.end(),
        [&key](const ReticleGroup* reticle)
        {
            return reticle != nullptr && reticle->id == key.reticleId;
        });
    return iterator == reticles.end() ? nullptr : *iterator;
}

PrimitivePatch BuildPrimitivePatch(const Primitive& current, const Primitive* base)
{
    PrimitivePatch patch;
    const Primitive defaultBase {};
    const Primitive& reference = base == nullptr ? defaultBase : *base;

    patch.visible = current.style.visible;
    patch.position = current.transform.position;
    patch.rotationDegrees = current.transform.rotationDegrees;
    patch.scale = current.transform.scale;
    patch.color = current.style.color;
    patch.thickness = current.style.thickness;

    if (const auto* textGeometry = std::get_if<TextGeometry>(&current.geometry))
    {
        patch.text = textGeometry->text;
        patch.letterSpacing = textGeometry->letterSpacing;
    }
    else if (const auto* timeGeometry = std::get_if<TimeGeometry>(&current.geometry))
    {
        patch.letterSpacing = timeGeometry->letterSpacing;
    }
    else if (const auto* lineGeometry = std::get_if<LineGeometry>(&current.geometry))
    {
        patch.lineStart = lineGeometry->start;
        patch.lineEnd = lineGeometry->end;
    }
    else if (const auto* circleGeometry = std::get_if<CircleGeometry>(&current.geometry))
    {
        patch.radius = circleGeometry->radius;
    }
    else if (const auto* ringGeometry = std::get_if<RingGeometry>(&current.geometry))
    {
        patch.innerRadius = ringGeometry->innerRadius;
        patch.outerRadius = ringGeometry->outerRadius;
    }
    else if (const auto* rectangleGeometry = std::get_if<RectangleGeometry>(&current.geometry))
    {
        patch.width = rectangleGeometry->width;
        patch.height = rectangleGeometry->height;
    }
    else if (const auto* ellipseGeometry = std::get_if<EllipseGeometry>(&current.geometry))
    {
        patch.width = ellipseGeometry->width;
        patch.height = ellipseGeometry->height;
    }
    else if (const auto* squareGeometry = std::get_if<SquareGeometry>(&current.geometry))
    {
        patch.width = squareGeometry->width;
        patch.height = squareGeometry->height;
    }
    else if (const auto* diamondGeometry = std::get_if<DiamondGeometry>(&current.geometry))
    {
        patch.width = diamondGeometry->width;
        patch.height = diamondGeometry->height;
    }

    if (base == nullptr)
    {
        return patch;
    }

    if (current.style.visible == reference.style.visible)
    {
        patch.visible.reset();
    }
    if (SameVector(current.transform.position, reference.transform.position))
    {
        patch.position.reset();
    }
    if (current.transform.rotationDegrees == reference.transform.rotationDegrees)
    {
        patch.rotationDegrees.reset();
    }
    if (SameVector(current.transform.scale, reference.transform.scale))
    {
        patch.scale.reset();
    }
    if (SameColor(current.style.color, reference.style.color))
    {
        patch.color.reset();
    }
    if (current.style.thickness == reference.style.thickness)
    {
        patch.thickness.reset();
    }

    if (const auto* textGeometry = std::get_if<TextGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<TextGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && textGeometry->text == baseGeometry->text)
        {
            patch.text.reset();
        }
        if (baseGeometry != nullptr && textGeometry->letterSpacing == baseGeometry->letterSpacing)
        {
            patch.letterSpacing.reset();
        }
    }
    else if (const auto* timeGeometry = std::get_if<TimeGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<TimeGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && timeGeometry->letterSpacing == baseGeometry->letterSpacing)
        {
            patch.letterSpacing.reset();
        }
    }
    else if (const auto* lineGeometry = std::get_if<LineGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<LineGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && SameVector(lineGeometry->start, baseGeometry->start))
        {
            patch.lineStart.reset();
        }
        if (baseGeometry != nullptr && SameVector(lineGeometry->end, baseGeometry->end))
        {
            patch.lineEnd.reset();
        }
    }
    else if (const auto* circleGeometry = std::get_if<CircleGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<CircleGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && circleGeometry->radius == baseGeometry->radius)
        {
            patch.radius.reset();
        }
    }
    else if (const auto* ringGeometry = std::get_if<RingGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<RingGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && ringGeometry->innerRadius == baseGeometry->innerRadius)
        {
            patch.innerRadius.reset();
        }
        if (baseGeometry != nullptr && ringGeometry->outerRadius == baseGeometry->outerRadius)
        {
            patch.outerRadius.reset();
        }
    }
    else if (const auto* rectangleGeometry = std::get_if<RectangleGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<RectangleGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && rectangleGeometry->width == baseGeometry->width)
        {
            patch.width.reset();
        }
        if (baseGeometry != nullptr && rectangleGeometry->height == baseGeometry->height)
        {
            patch.height.reset();
        }
    }
    else if (const auto* ellipseGeometry = std::get_if<EllipseGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<EllipseGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && ellipseGeometry->width == baseGeometry->width)
        {
            patch.width.reset();
        }
        if (baseGeometry != nullptr && ellipseGeometry->height == baseGeometry->height)
        {
            patch.height.reset();
        }
    }
    else if (const auto* squareGeometry = std::get_if<SquareGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<SquareGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && squareGeometry->width == baseGeometry->width)
        {
            patch.width.reset();
        }
        if (baseGeometry != nullptr && squareGeometry->height == baseGeometry->height)
        {
            patch.height.reset();
        }
    }
    else if (const auto* diamondGeometry = std::get_if<DiamondGeometry>(&current.geometry))
    {
        const auto* baseGeometry = std::get_if<DiamondGeometry>(&reference.geometry);
        if (baseGeometry != nullptr && diamondGeometry->width == baseGeometry->width)
        {
            patch.width.reset();
        }
        if (baseGeometry != nullptr && diamondGeometry->height == baseGeometry->height)
        {
            patch.height.reset();
        }
    }

    return patch;
}

ReticlePatch BuildReticlePatch(const ReticleGroup& current, const ReticleGroup& base)
{
    ReticlePatch patch;
    patch.visible = current.visible;
    patch.blinkEnabled = current.blink.enabled;
    patch.blinkType = current.blink.typeName;
    patch.position = current.transform.position;
    patch.rotationDegrees = current.transform.rotationDegrees;

    if (current.overrides.color.has_value())
    {
        patch.color = *current.overrides.color;
    }
    if (current.overrides.thickness.has_value())
    {
        patch.thickness = *current.overrides.thickness;
    }

    for (const Primitive& primitive : current.primitives)
    {
        const Primitive* basePrimitive = FindPrimitive(base, primitive.id);
        PrimitivePatch primitivePatch = BuildPrimitivePatch(primitive, basePrimitive);
        if (primitivePatch.visible.has_value() ||
            primitivePatch.position.has_value() ||
            primitivePatch.rotationDegrees.has_value() ||
            primitivePatch.scale.has_value() ||
            primitivePatch.color.has_value() ||
            primitivePatch.thickness.has_value() ||
            primitivePatch.text.has_value() ||
            primitivePatch.letterSpacing.has_value() ||
            primitivePatch.lineStart.has_value() ||
            primitivePatch.lineEnd.has_value() ||
            primitivePatch.radius.has_value() ||
            primitivePatch.innerRadius.has_value() ||
            primitivePatch.outerRadius.has_value() ||
            primitivePatch.width.has_value() ||
            primitivePatch.height.has_value() ||
            primitivePatch.size.has_value())
        {
            patch.primitivePatches.emplace(primitive.id, std::move(primitivePatch));
        }
    }

    return patch;
}

bool ReticleHandleBypassed(const RuntimeDebugState& state, const std::string_view pageName, const std::string_view reticleId)
{
    return state.ReticleBypassed(ReticleKey {std::string(pageName), std::string(reticleId), ReticleKind::Static}) ||
           state.ReticleBypassed(ReticleKey {std::string(pageName), std::string(reticleId), ReticleKind::Dynamic}) ||
           state.ReticleBypassed(ReticleKey {std::string(pageName), std::string(reticleId), ReticleKind::Strobe});
}

std::optional<CommandBatch> FilterBatchForPreview(const CommandBatch& batch,
                                                  const RuntimeDebugState& state,
                                                  const SceneRegistry& scene)
{
    CommandBatch filtered;
    filtered.sequence = batch.sequence;
    filtered.mappingHash = batch.mappingHash;

    for (const UserCommand& command : batch.commands)
    {
        bool keep = true;
        UserCommand filteredCommand = command;

        std::visit(
            [&keep, &scene, &state](auto& value)
            {
                using Command = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Command, ActivatePageCommand>)
                {
                    keep = !state.PageBypassed();
                }
                else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
                {
                    keep = !ReticleHandleBypassed(state, value.target.page, value.target.reticle);
                }
                else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
                {
                    keep = !state.StrobeBypassed(
                        ResolveTransportPageName(scene.TransportMap(), value.page, value.pageId));
                }
                else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand> ||
                                   std::is_same_v<Command, RemoveDynamicReticleCommand>)
                {
                    keep = !state.ReticleBypassed(
                        ReticleKey {value.target.page, value.target.reticleId, ReticleKind::Dynamic});
                }
                else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
                {
                    value.reticles.erase(
                        std::remove_if(
                            value.reticles.begin(),
                            value.reticles.end(),
                            [&state, &value](const DynamicReticleState& reticle)
                            {
                                return state.ReticleBypassed(
                                    ReticleKey {value.page, reticle.reticleId, ReticleKind::Dynamic});
                            }),
                        value.reticles.end());
                    keep = !value.reticles.empty();
                }
            },
            filteredCommand);

        if (keep)
        {
            filtered.commands.push_back(std::move(filteredCommand));
        }
    }

    if (filtered.commands.empty())
    {
        return std::nullopt;
    }

    return filtered;
}
} // namespace

bool RuntimeDebugPreview::ResetFromLive(const SceneRegistry& liveScene, const RuntimeDebugState& state)
{
    try
    {
        scene_.LoadDocument(liveScene.Document(), liveScene.TransportMap());
        processor_ = std::make_unique<CommandProcessor>(scene_);

        if (!liveScene.ActivePageName().empty())
        {
            scene_.SetActivePage(liveScene.ActivePageName());
            if (const auto activeStrobe = liveScene.ActiveStrobeSummary(); activeStrobe.has_value())
            {
                scene_.SelectStrobe(activeStrobe->pageName, activeStrobe->strobeName);
            }
        }

        for (const PageSummary& page : liveScene.Pages())
        {
            if (const auto view = liveScene.ViewForPage(page.name); view.has_value())
            {
                scene_.SetPageView(page.name, *view);
            }

            const PageDefinition* pageDefinition = FindPage(scene_.Document(), page.name);
            if (pageDefinition == nullptr)
            {
                continue;
            }

            for (const ReticleGroup* currentReticle : liveScene.CollectPageReticlePointers(page.name))
            {
                if (currentReticle == nullptr)
                {
                    continue;
                }

                const ReticleKind kind = ClassifyReticle(*pageDefinition, currentReticle->id);
                if (kind == ReticleKind::Dynamic)
                {
                    scene_.UpsertDynamicReticle(page.name, *currentReticle);
                    continue;
                }

                const ReticleGroup* authored = FindAuthoredReticle(*pageDefinition, currentReticle->id, kind);
                if (authored == nullptr)
                {
                    continue;
                }

                const ReticlePatch patch = BuildReticlePatch(*currentReticle, *authored);
                scene_.ApplyReticlePatch(page.name, currentReticle->id, patch);
            }
        }

        const WindowDisplayState liveDisplay = liveScene.WindowDisplay();
        WindowDisplayPatch windowPatch;
        windowPatch.invertColors = liveDisplay.invertColors;
        windowPatch.brightness = liveDisplay.brightness;
        windowPatch.disabled = liveDisplay.disabled;
        scene_.ApplyWindowDisplayPatch(windowPatch);

        ready_ = true;
        lastError_.clear();
        if (!ApplyDynamicTemplateVisibility(state) || !ApplyStateOverrides(state))
        {
            return false;
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        lastError_ = exception.what();
    }
    catch (...)
    {
        lastError_ = "Unknown exception while rebuilding the runtime debug preview";
    }

    ready_ = false;
    processor_.reset();
    return false;
}

bool RuntimeDebugPreview::ApplyLiveBatches(const std::vector<CommandBatch>& batches, const RuntimeDebugState& state)
{
    if (!ready_ || processor_ == nullptr)
    {
        return false;
    }

    for (const CommandBatch& batch : batches)
    {
        const auto filteredBatch = FilterBatchForPreview(batch, state, scene_);
        if (!filteredBatch.has_value())
        {
            continue;
        }

        if (!processor_->Submit(*filteredBatch))
        {
            lastError_ = processor_->LastError();
            ready_ = false;
            return false;
        }
    }

    return ApplyStateOverrides(state);
}

bool RuntimeDebugPreview::ApplyStateOverrides(const RuntimeDebugState& state)
{
    if (!ready_)
    {
        return false;
    }

    if (!ApplyDynamicTemplateVisibility(state))
    {
        return false;
    }

    for (const StrobeBypassState& bypass : state.BypassedStrobes())
    {
        if (!scene_.SelectStrobe(bypass.pageName, bypass.strobeName))
        {
            lastError_ =
                "Unable to force strobe '" + bypass.strobeName + "' on page '" + bypass.pageName + "' in the debug preview.";
            ready_ = false;
            return false;
        }
    }

    if (state.PageBypassed() && !state.ForcedActivePage().empty())
    {
        scene_.SetActivePage(state.ForcedActivePage());
    }

    for (const ReticleKey& key : state.BypassedReticles())
    {
        const ReticleBypassState* bypass = state.FindReticleBypass(key);
        if (bypass != nullptr && !ApplyReticleDraft(*bypass))
        {
            return false;
        }
    }

    lastError_.clear();
    return true;
}

void RuntimeDebugPreview::Invalidate() noexcept
{
    ready_ = false;
    lastError_.clear();
    processor_.reset();
}

bool RuntimeDebugPreview::Ready() const noexcept
{
    return ready_;
}

const std::string& RuntimeDebugPreview::LastError() const noexcept
{
    return lastError_;
}

const SceneRegistry& RuntimeDebugPreview::Scene() const noexcept
{
    return scene_;
}

bool RuntimeDebugPreview::ApplyReticleDraft(const ReticleBypassState& bypass)
{
    if (!scene_.HasPage(bypass.key.pageName))
    {
        lastError_ = "Unknown debug-preview page '" + bypass.key.pageName + "'.";
        ready_ = false;
        return false;
    }

    if (bypass.key.kind == ReticleKind::Dynamic)
    {
        scene_.UpsertDynamicReticle(bypass.key.pageName, bypass.draft);
        return true;
    }

    const PageDefinition* page = FindPage(scene_.Document(), bypass.key.pageName);
    if (page == nullptr)
    {
        lastError_ = "Unable to locate authored page '" + bypass.key.pageName + "' in the debug preview.";
        ready_ = false;
        return false;
    }

    const ReticleGroup* authored = FindAuthoredReticle(*page, bypass.key.reticleId, bypass.key.kind);
    if (authored == nullptr)
    {
        lastError_ = "Unable to locate authored reticle '" + bypass.key.reticleId + "' in page '" +
                     bypass.key.pageName + "'.";
        ready_ = false;
        return false;
    }

    const ReticlePatch patch = BuildReticlePatch(bypass.draft, *authored);
    if (!scene_.ApplyReticlePatch(bypass.key.pageName, bypass.key.reticleId, patch))
    {
        lastError_ = "Unable to apply one local bypass draft to reticle '" + bypass.key.reticleId + "'.";
        ready_ = false;
        return false;
    }

    return true;
}

bool RuntimeDebugPreview::ApplyDynamicTemplateVisibility(const RuntimeDebugState& state)
{
    for (const auto& [key, visible] : state.DynamicTemplateVisibilityEntries())
    {
        if (!scene_.SetDynamicReticleSetVisible(key.pageName, key.templateId, visible))
        {
            lastError_ =
                "Unable to restore dynamic template visibility for '" + key.templateId + "' on page '" + key.pageName + "'.";
            ready_ = false;
            return false;
        }
    }

    return true;
}
} // namespace mfd::window::debug
