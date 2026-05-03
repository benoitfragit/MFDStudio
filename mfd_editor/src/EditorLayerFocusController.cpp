/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorLayerFocusController.h"

/**
 * @file
 * @brief Implementation of the page-preview layer-focus controller.
 */

#include <algorithm>

namespace editor
{
namespace
{
std::size_t CountLayerReticles(const mfd::PageDefinition& page, const std::string_view layerId)
{
    return static_cast<std::size_t>(std::count_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [layerId](const mfd::ReticleGroup& reticle)
        {
            return reticle.editor.layerId == layerId;
        }));
}

const mfd::EditorLayerDefinition* FindLayerById(const mfd::PageDefinition& page, const std::string_view layerId)
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
} // namespace

void LayerFocusController::SanitizeFocusState(const mfd::PageDefinition& page, LayerFocusState& state) const
{
    if (!state.focusedLayerId.empty() && FindLayerById(page, state.focusedLayerId) == nullptr)
    {
        state.focusedLayerId.clear();
    }
}

LayerFocusStripModel LayerFocusController::BuildStripModel(const mfd::PageDefinition& page,
                                                           const LayerFocusState& state) const
{
    LayerFocusState sanitized = state;
    SanitizeFocusState(page, sanitized);

    LayerFocusStripModel model;
    model.focusActive = !sanitized.focusedLayerId.empty();
    model.entries.reserve(page.editor.layers.size() + 1U);
    model.entries.push_back(
        LayerFocusStripEntry {{}, "Full View", true, !model.focusActive, true, page.staticReticles.size()});

    for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
    {
        model.entries.push_back(
            LayerFocusStripEntry {layer.id,
                                  layer.id + (layer.visible ? "" : " (hidden)"),
                                  false,
                                  sanitized.focusedLayerId == layer.id,
                                  layer.visible,
                                  CountLayerReticles(page, layer.id)});
    }

    return model;
}

bool LayerFocusController::IsFocusActive(const mfd::PageDefinition& page, const LayerFocusState& state) const
{
    return !state.focusedLayerId.empty() && FindLayerById(page, state.focusedLayerId) != nullptr;
}

bool LayerFocusController::IsReticleSelectable(const mfd::PageDefinition& page,
                                               const mfd::ReticleGroup& reticle,
                                               const LayerFocusState& state) const
{
    if (!IsFocusActive(page, state))
    {
        return true;
    }

    return !reticle.editor.layerId.empty() && reticle.editor.layerId == state.focusedLayerId;
}

bool LayerFocusController::ShouldReticleBeDimmed(const mfd::PageDefinition& page,
                                                 const mfd::ReticleGroup& reticle,
                                                 const LayerFocusState& state) const
{
    return IsFocusActive(page, state) && !IsReticleSelectable(page, reticle, state);
}

std::vector<int> LayerFocusController::FilterSelectableReticleIndices(const mfd::PageDefinition& page,
                                                                      const std::vector<int>& candidateIndices,
                                                                      const LayerFocusState& state) const
{
    std::vector<int> filtered;
    filtered.reserve(candidateIndices.size());

    for (const int reticleIndex : candidateIndices)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()))
        {
            continue;
        }

        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (IsReticleSelectable(page, reticle, state))
        {
            filtered.push_back(reticleIndex);
        }
    }

    return filtered;
}

LayerFocusState LayerFocusController::MakeFocusedState(const mfd::PageDefinition& page,
                                                       const std::string_view layerId) const
{
    LayerFocusState state;
    if (const mfd::EditorLayerDefinition* layer = FindLayerById(page, layerId); layer != nullptr)
    {
        state.focusedLayerId = layer->id;
    }
    return state;
}
} // namespace editor
