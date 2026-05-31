/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared authoring rules used by editor panels, page preview, and reticle workflows.
 */

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "mfd/model/PageDefinition.h"

namespace editor::app
{
/**
 * @brief One selectable clipping primitive exposed in combo boxes and context menus.
 */
struct ClipPrimitiveOption
{
    std::string primitiveId;
    std::string label;
};

/**
 * @brief Returns the user-facing label used for one primitive type in the editor UI.
 * @param type Primitive type to describe.
 * @return Human-readable type label.
 */
inline std::string PrimitiveTypeLabel(const mfd::PrimitiveType type)
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
        return "Text";
    case mfd::PrimitiveType::Time:
        return "Time";
    case mfd::PrimitiveType::Line:
        return "Line";
    case mfd::PrimitiveType::Circle:
        return "Circle";
    case mfd::PrimitiveType::Ring:
        return "Ring";
    case mfd::PrimitiveType::Rectangle:
        return "Rectangle";
    case mfd::PrimitiveType::Ellipse:
        return "Ellipse";
    case mfd::PrimitiveType::Square:
        return "Square";
    case mfd::PrimitiveType::Diamond:
        return "Diamond";
    case mfd::PrimitiveType::Triangle:
        return "Triangle";
    case mfd::PrimitiveType::Polyline:
        return "Polyline";
    case mfd::PrimitiveType::Bezier:
        return "Bezier";
    case mfd::PrimitiveType::Arc:
        return "Arc";
    case mfd::PrimitiveType::Image:
        return "Image";
    }

    return "Primitive";
}

/**
 * @brief Returns the UI label used for one page-title decoration mode.
 * @param decoration Decoration mode to describe.
 * @return Stable UI label.
 */
inline const char* PageTitleDecorationLabel(const mfd::PageTitleDecoration decoration) noexcept
{
    switch (decoration)
    {
    case mfd::PageTitleDecoration::None:
        return "None";
    case mfd::PageTitleDecoration::Frame:
        return "Frame";
    case mfd::PageTitleDecoration::Underline:
    default:
        return "Underline";
    }
}

/**
 * @brief Returns the UI label used for one line style.
 * @param lineStyle Line style to describe.
 * @return Stable UI label.
 */
inline const char* LineStyleLabel(const mfd::LineStyle lineStyle) noexcept
{
    switch (lineStyle)
    {
    case mfd::LineStyle::Dotted:
        return "Dotted";
    case mfd::LineStyle::Dashed:
        return "Dashed";
    case mfd::LineStyle::Solid:
    default:
        return "Solid";
    }
}

/**
 * @brief Returns whether the editor should expose the line-style combo for one primitive type.
 * @param type Primitive type to inspect.
 * @return `true` when the primitive renders an outline style.
 */
inline bool SupportsPrimitiveLineStyle(const mfd::PrimitiveType type) noexcept
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
    case mfd::PrimitiveType::Time:
    case mfd::PrimitiveType::Image:
        return false;
    default:
        return true;
    }
}

/**
 * @brief Returns the UI label used for one clipping mode.
 * @param mode Clipping mode to describe.
 * @return Stable UI label.
 */
inline const char* ReticleClipModeLabel(const mfd::ReticleClipMode mode) noexcept
{
    switch (mode)
    {
    case mfd::ReticleClipMode::Inner:
        return "Inner clipping";
    case mfd::ReticleClipMode::Outer:
        return "Outer clipping";
    case mfd::ReticleClipMode::None:
    default:
        return "Disabled";
    }
}

/**
 * @brief Returns the active page-strobe index selected by the authored page metadata.
 * @param page Authored page to inspect.
 * @return Active strobe index, or `-1` when no strobe is active.
 */
inline int FindActivePageStrobeIndex(const mfd::PageDefinition& page) noexcept
{
    if (const mfd::PageStrobeDefinition* active = mfd::FindActivePageStrobeDefinition(page); active != nullptr)
    {
        return static_cast<int>(active - page.strobes.data());
    }

    return -1;
}

/**
 * @brief Marks one page strobe as the authored default selection.
 * @param page Page to mutate.
 * @param strobe Strobe definition that becomes active.
 */
inline void SetActivePageStrobe(mfd::PageDefinition& page, const mfd::PageStrobeDefinition& strobe)
{
    page.activeStrobeName = strobe.name;
    page.normalizedActiveStrobeName = strobe.normalizedName;
}

/**
 * @brief Suggests the next valid name for one new page strobe.
 * @param page Page that owns the authored strobes.
 * @return Unique strobe name suitable for the add-strobe draft.
 */
inline std::string SuggestPageStrobeDraftName(const mfd::PageDefinition& page)
{
    if (page.strobes.empty() && mfd::FindPageStrobeDefinition(page, "Default") == nullptr)
    {
        return "Default";
    }

    int suffix = 1;
    while (true)
    {
        const std::string candidate = "Strobe" + std::to_string(suffix);
        if (mfd::FindPageStrobeDefinition(page, candidate) == nullptr)
        {
            return candidate;
        }

        ++suffix;
    }
}

/**
 * @brief Builds the list of clip-capable primitives shown by clipping pickers.
 * @param reticle Reticle to inspect.
 * @return Unique primitive ids paired with their display labels.
 */
inline std::vector<ClipPrimitiveOption> CollectClipPrimitiveOptions(const mfd::ReticleGroup& reticle)
{
    std::vector<ClipPrimitiveOption> options;
    std::unordered_set<std::string> seenPrimitiveIds;

    for (int index = 0; index < static_cast<int>(reticle.primitives.size()); ++index)
    {
        const auto& primitive = reticle.primitives[static_cast<std::size_t>(index)];
        if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive))
        {
            continue;
        }

        if (!seenPrimitiveIds.insert(primitive.id).second)
        {
            continue;
        }

        options.push_back(ClipPrimitiveOption {
            primitive.id,
            primitive.id + " (" + PrimitiveTypeLabel(primitive.type) + ")"});
    }

    return options;
}

/**
 * @brief Returns whether one page already owns a strobe with the exact provided display name.
 * @param page Page to inspect.
 * @param name Display name to match.
 * @return `true` when one strobe already uses the exact same display name.
 */
inline bool PageStrobeNameExistsExact(const mfd::PageDefinition& page, const std::string_view name)
{
    return std::any_of(page.strobes.begin(),
                       page.strobes.end(),
                       [name](const mfd::PageStrobeDefinition& strobe)
                       {
                           return strobe.name == name;
                       });
}

/**
 * @brief Returns the tree/inspector label used for one page strobe.
 * @param page Page that owns the strobe.
 * @param strobe Authored strobe to label.
 * @param strobeIndex Stable position of the strobe in the page list.
 * @return Display label including the active marker when relevant.
 */
inline std::string PageStrobeDisplayLabel(const mfd::PageDefinition& page,
                                          const mfd::PageStrobeDefinition& strobe,
                                          const std::size_t strobeIndex)
{
    const std::string baseName = strobe.name.empty() ? "strobe_" + std::to_string(strobeIndex + 1U) : strobe.name;
    if (page.normalizedActiveStrobeName == strobe.normalizedName ||
        (page.normalizedActiveStrobeName.empty() && strobeIndex == 0U))
    {
        return baseName + " (active)";
    }

    return baseName;
}

/** @brief Sentinel returned when no blink type matches the queried normalized name. */
inline constexpr std::size_t kInvalidBlinkTypeIndex = std::numeric_limits<std::size_t>::max();

/**
 * @brief Resolves one blink type by name after page-name normalization.
 * @param page Page that owns the blink types.
 * @param blinkTypeName Name or normalized name to resolve.
 * @return Matching blink type index, or `kInvalidBlinkTypeIndex`.
 */
inline std::size_t FindBlinkTypeIndex(const mfd::PageDefinition& page, const std::string_view blinkTypeName)
{
    const std::string normalizedBlinkTypeName = mfd::NormalizePageName(blinkTypeName);
    if (normalizedBlinkTypeName.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        if (page.blinkTypes[index].normalizedName == normalizedBlinkTypeName)
        {
            return index;
        }
    }

    return kInvalidBlinkTypeIndex;
}

/**
 * @brief Returns the effective default blink type used by the editor.
 * @param page Page that owns the blink types.
 * @return Index of the effective default blink type, or `kInvalidBlinkTypeIndex`.
 */
inline std::size_t EffectiveDefaultBlinkTypeIndex(const mfd::PageDefinition& page)
{
    if (page.blinkTypes.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    if (!page.normalizedDefaultBlinkTypeName.empty())
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            if (page.blinkTypes[index].normalizedName == page.normalizedDefaultBlinkTypeName)
            {
                return index;
            }
        }
    }

    return 0;
}

/**
 * @brief Generates a unique blink-type display name for one page.
 * @param page Page that owns the blink types.
 * @param baseName Preferred base name.
 * @return Unique display name suitable for a new blink type.
 */
inline std::string MakeUniqueBlinkTypeName(const mfd::PageDefinition& page, std::string_view baseName)
{
    std::string candidate = baseName.empty() ? std::string {"blink"} : std::string(baseName);
    int suffix = 2;

    while (FindBlinkTypeIndex(page, candidate) != kInvalidBlinkTypeIndex)
    {
        candidate = std::string(baseName.empty() ? "blink" : baseName) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

namespace detail
{
inline bool BlinkStateMatchesNormalizedName(const mfd::ReticleBlinkState& blink,
                                            const std::string_view normalizedBlinkTypeName)
{
    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    return currentNormalizedName == normalizedBlinkTypeName;
}

inline void RenameBlinkReference(mfd::ReticleBlinkState& blink,
                                 const std::string_view previousNormalizedName,
                                 const std::string& nextName,
                                 const std::string& nextNormalizedName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName != previousNormalizedName)
    {
        return;
    }

    blink.typeName = nextName;
    blink.normalizedTypeName = nextNormalizedName;
}

inline void ClearBlinkReference(mfd::ReticleBlinkState& blink, const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName == removedNormalizedName)
    {
        blink = {};
    }
}
} // namespace detail

/**
 * @brief Counts every page reticle or strobe that references one blink type.
 * @param page Page to inspect.
 * @param normalizedBlinkTypeName Normalized blink type name.
 * @return Number of authored bindings that resolve to the provided blink type.
 */
inline std::size_t CountBlinkReferences(const mfd::PageDefinition& page, const std::string_view normalizedBlinkTypeName)
{
    if (normalizedBlinkTypeName.empty())
    {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& reticle : page.staticReticles)
    {
        if (detail::BlinkStateMatchesNormalizedName(reticle.blink, normalizedBlinkTypeName))
        {
            ++count;
        }
    }

    for (const auto& strobe : page.strobes)
    {
        if (detail::BlinkStateMatchesNormalizedName(strobe.reticle.blink, normalizedBlinkTypeName))
        {
            ++count;
        }
    }

    return count;
}

/**
 * @brief Renames every authored blink reference that points to one blink type.
 * @param page Page to mutate.
 * @param previousNormalizedName Normalized name currently referenced by authored reticles.
 * @param nextName New display name stored in the page metadata.
 */
inline void RenameBlinkReferences(mfd::PageDefinition& page,
                                  const std::string_view previousNormalizedName,
                                  const std::string& nextName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string nextNormalizedName = mfd::NormalizePageName(nextName);
    if (page.normalizedDefaultBlinkTypeName == previousNormalizedName)
    {
        page.defaultBlinkTypeName = nextName;
        page.normalizedDefaultBlinkTypeName = nextNormalizedName;
    }

    for (auto& reticle : page.staticReticles)
    {
        detail::RenameBlinkReference(reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }

    for (auto& strobe : page.strobes)
    {
        detail::RenameBlinkReference(strobe.reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }
}

/**
 * @brief Clears every authored blink reference that points to one removed blink type.
 * @param page Page to mutate.
 * @param removedNormalizedName Normalized name that is no longer valid.
 */
inline void ClearBlinkReferencesForRemovedType(mfd::PageDefinition& page,
                                               const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    if (page.normalizedDefaultBlinkTypeName == removedNormalizedName)
    {
        page.defaultBlinkTypeName.clear();
        page.normalizedDefaultBlinkTypeName.clear();
    }

    for (auto& reticle : page.staticReticles)
    {
        detail::ClearBlinkReference(reticle.blink, removedNormalizedName);
    }

    for (auto& strobe : page.strobes)
    {
        detail::ClearBlinkReference(strobe.reticle.blink, removedNormalizedName);
    }
}

/**
 * @brief Refreshes one authored blink binding after a blink-type change.
 * @param page Page that owns the blink types.
 * @param blink Blink binding to normalize and clamp.
 */
inline void RefreshBlinkBindingForEditor(const mfd::PageDefinition& page, mfd::ReticleBlinkState& blink)
{
    blink.normalizedTypeName = blink.typeName.empty() ? std::string {} : mfd::NormalizePageName(blink.typeName);

    if (!blink.typeName.empty())
    {
        const std::size_t blinkTypeIndex = FindBlinkTypeIndex(page, blink.normalizedTypeName);
        blink.durationMs = blinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[blinkTypeIndex].durationMs;
        return;
    }

    if (!blink.enabled)
    {
        blink.durationMs = 0;
        return;
    }

    const std::size_t defaultBlinkTypeIndex = EffectiveDefaultBlinkTypeIndex(page);
    blink.durationMs =
        defaultBlinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[defaultBlinkTypeIndex].durationMs;
}

/**
 * @brief Recomputes normalized page-blink metadata and every authored blink binding.
 * @param page Page to normalize in place.
 */
inline void RefreshPageBlinkStateForEditor(mfd::PageDefinition& page)
{
    for (auto& blinkType : page.blinkTypes)
    {
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = std::max<std::uint32_t>(1U, blinkType.durationMs);
    }

    page.normalizedDefaultBlinkTypeName =
        page.defaultBlinkTypeName.empty() ? std::string {} : mfd::NormalizePageName(page.defaultBlinkTypeName);

    for (auto& reticle : page.staticReticles)
    {
        RefreshBlinkBindingForEditor(page, reticle.blink);
    }

    for (auto& strobe : page.strobes)
    {
        RefreshBlinkBindingForEditor(page, strobe.reticle.blink);
    }
}

/**
 * @brief Resolves one editor-only layer by id.
 * @param page Page that owns the editor-layer metadata.
 * @param layerId Exact layer identifier to resolve.
 * @return Matching editor-layer definition, or `nullptr`.
 */
inline const mfd::EditorLayerDefinition* FindEditorLayer(const mfd::PageDefinition& page,
                                                         const std::string_view layerId)
{
    if (layerId.empty())
    {
        return nullptr;
    }

    const auto iterator = std::find_if(page.editor.layers.begin(),
                                       page.editor.layers.end(),
                                       [layerId](const mfd::EditorLayerDefinition& layer)
                                       {
                                           return layer.id == layerId;
                                       });
    return iterator == page.editor.layers.end() ? nullptr : &(*iterator);
}

/**
 * @brief Returns whether one page reticle is visible under the current editor-layer mask.
 * @param page Page that owns the reticle.
 * @param reticle Reticle to inspect.
 * @return `true` when the reticle should be drawn in the editor.
 */
inline bool IsReticleVisibleInEditor(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    if (const mfd::EditorLayerDefinition* layer = FindEditorLayer(page, reticle.layerId); layer != nullptr)
    {
        return layer->visible;
    }

    return true;
}

/**
 * @brief Returns whether one page strobe is visible under the current editor-layer mask.
 * @param page Page that owns the strobe.
 * @param strobe Strobe to inspect.
 * @return `true` when the strobe is active and visible.
 */
inline bool IsPageStrobeVisibleInEditor(const mfd::PageDefinition& page, const mfd::PageStrobeDefinition& strobe)
{
    const mfd::PageStrobeDefinition* activeStrobe = mfd::FindActivePageStrobeDefinition(page);
    return activeStrobe != nullptr && activeStrobe == &strobe && IsReticleVisibleInEditor(page, strobe.reticle);
}

/**
 * @brief Returns whether one strobe index is visible under the current editor-layer mask.
 * @param page Page that owns the strobe list.
 * @param strobeIndex Authored strobe index to inspect.
 * @return `true` when the indexed strobe is active and visible.
 */
inline bool IsPageStrobeIndexVisibleInEditor(const mfd::PageDefinition& page, const int strobeIndex)
{
    if (strobeIndex < 0 || strobeIndex >= static_cast<int>(page.strobes.size()))
    {
        return false;
    }

    return IsPageStrobeVisibleInEditor(page, page.strobes[static_cast<std::size_t>(strobeIndex)]);
}

/**
 * @brief Returns whether one strobe index can currently be selected in the editor.
 * @param page Page that owns the strobe list.
 * @param strobeIndex Authored strobe index to inspect.
 * @return `true` when the index targets one authored strobe.
 */
inline bool IsPageStrobeSelectableInEditor(const mfd::PageDefinition& page, const int strobeIndex)
{
    return strobeIndex >= 0 && strobeIndex < static_cast<int>(page.strobes.size());
}

/**
 * @brief Returns the authored page-layer order used by page reticles and dynamic bindings.
 * @param page Page that owns the runtime layers.
 * @param layerId Layer identifier to resolve.
 * @return Zero-based authored order, or `page.layers.size()` when the layer is unknown.
 */
inline std::size_t PageLayerOrder(const mfd::PageDefinition& page, const std::string_view layerId)
{
    for (std::size_t index = 0; index < page.layers.size(); ++index)
    {
        if (mfd::PageNamesEqual(page.layers[index].id, layerId))
        {
            return index;
        }
    }

    return page.layers.size();
}

/**
 * @brief Returns whether one dynamic reticle template is already bound on the page.
 * @param page Page that owns the dynamic bindings.
 * @param templateId Reticle template identifier to inspect.
 * @return `true` when one binding already references the template.
 */
inline bool PageHasDynamicTemplateBinding(const mfd::PageDefinition& page, const std::string_view templateId)
{
    return mfd::FindDynamicReticleLayerBinding(page, templateId) != nullptr;
}

/**
 * @brief Returns whether one dynamic-reticle order collides on the target runtime layer.
 * @param page Page that owns the dynamic bindings.
 * @param layerId Runtime layer identifier to inspect.
 * @param orderInLayer Target order on that layer.
 * @param excludedIndex Optional binding index to exclude while checking collisions.
 * @return `true` when the authored page already uses the same order on the same layer.
 */
inline bool PageHasDynamicOrderConflict(const mfd::PageDefinition& page,
                                        const std::string_view layerId,
                                        const int orderInLayer,
                                        const int excludedIndex = -1)
{
    for (int index = 0; index < static_cast<int>(page.dynamicReticleBindings.size()); ++index)
    {
        if (index == excludedIndex)
        {
            continue;
        }

        const mfd::DynamicReticleLayerBinding& binding = page.dynamicReticleBindings[static_cast<std::size_t>(index)];
        if (mfd::PageNamesEqual(binding.layerId, layerId) && binding.orderInLayer == orderInLayer)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Returns the next free dynamic-reticle order for one runtime layer.
 * @param page Page that owns the dynamic bindings.
 * @param layerId Runtime layer identifier to inspect.
 * @param excludedIndex Optional binding index to exclude while checking collisions.
 * @return First non-conflicting order value on the target layer.
 */
inline int NextPageDynamicOrderInLayer(const mfd::PageDefinition& page,
                                       const std::string_view layerId,
                                       const int excludedIndex = -1)
{
    int nextOrder = 0;
    while (PageHasDynamicOrderConflict(page, layerId, nextOrder, excludedIndex))
    {
        ++nextOrder;
    }

    return nextOrder;
}

/**
 * @brief Counts how many static page reticles currently target one editor layer.
 * @param page Page that owns the reticles.
 * @param layerId Editor-layer identifier to inspect.
 * @return Number of static reticles assigned to the layer.
 */
inline std::size_t CountEditorLayerAssignments(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [layerId](const mfd::ReticleGroup& reticle)
        {
            return reticle.layerId == layerId;
        }));
}

/**
 * @brief Counts how many dynamic reticle bindings currently target one editor layer.
 * @param page Page that owns the bindings.
 * @param layerId Editor-layer identifier to inspect.
 * @return Number of dynamic bindings assigned to the layer.
 */
inline std::size_t CountDynamicLayerBindings(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(
        page.dynamicReticleBindings.begin(),
        page.dynamicReticleBindings.end(),
        [layerId](const mfd::DynamicReticleLayerBinding& binding)
        {
            return mfd::PageNamesEqual(binding.layerId, layerId);
        }));
}

/**
 * @brief Returns a concise label listing dynamic reticle templates bound to one layer.
 * @param page Page that owns the bindings.
 * @param layerId Editor-layer identifier to inspect.
 * @return Comma-separated template-id summary.
 */
inline std::string SummarizeDynamicLayerBindings(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return {};
    }

    std::string summary;
    for (const mfd::DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
    {
        if (!mfd::PageNamesEqual(binding.layerId, layerId))
        {
            continue;
        }

        if (!summary.empty())
        {
            summary += ", ";
        }
        summary += binding.templateId;
    }

    return summary;
}

/**
 * @brief Renames every authored layer reference after one layer id change.
 * @param page Page to mutate.
 * @param previousLayerId Previous layer identifier.
 * @param nextLayerId New layer identifier.
 */
inline void RenameEditorLayerReferences(mfd::PageDefinition& page,
                                        const std::string_view previousLayerId,
                                        const std::string& nextLayerId)
{
    if (previousLayerId.empty() || previousLayerId == nextLayerId)
    {
        return;
    }

    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId == previousLayerId)
        {
            reticle.layerId = nextLayerId;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId == previousLayerId)
        {
            binding.layerId = nextLayerId;
        }
    }
}

/**
 * @brief Clears every authored layer reference that targets one removed layer id.
 * @param page Page to mutate.
 * @param removedLayerId Layer identifier being removed.
 * @return Number of authored references that were cleared.
 */
inline std::size_t ClearEditorLayerReferences(mfd::PageDefinition& page, const std::string_view removedLayerId)
{
    if (removedLayerId.empty())
    {
        return 0;
    }

    std::size_t clearedCount = 0;
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.layerId == removedLayerId)
        {
            reticle.layerId.clear();
            ++clearedCount;
        }
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (binding.layerId == removedLayerId)
        {
            binding.layerId.clear();
            ++clearedCount;
        }
    }

    return clearedCount;
}

/**
 * @brief Returns the default editor layer used for new reticle insertions.
 * @param page Page that owns the editor layers.
 * @return First visible layer id, or the first non-empty layer id when all layers are hidden.
 */
inline std::string DefaultEditorLayerId(const mfd::PageDefinition& page)
{
    const auto visibleLayer = std::find_if(page.editor.layers.begin(),
                                           page.editor.layers.end(),
                                           [](const mfd::EditorLayerDefinition& layer)
                                           {
                                               return layer.visible && !layer.id.empty();
                                           });
    if (visibleLayer != page.editor.layers.end())
    {
        return visibleLayer->id;
    }

    const auto firstLayer = std::find_if(page.editor.layers.begin(),
                                         page.editor.layers.end(),
                                         [](const mfd::EditorLayerDefinition& layer)
                                         {
                                             return !layer.id.empty();
                                         });
    return firstLayer == page.editor.layers.end() ? std::string {} : firstLayer->id;
}
} // namespace editor::app
