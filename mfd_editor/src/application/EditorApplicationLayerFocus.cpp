/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Layer focus state and page-reticle selectability rules extracted from `EditorApplication`.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::app::DefaultEditorLayerId;
}

void EditorApplication::ClearLayerFocus(const bool announceStatus)
{
    if (layoutState_.layerFocusState.focusedLayerId.empty())
    {
        return;
    }

    layoutState_.layerFocusState.focusedLayerId.clear();
    SanitizePageReticleSelectionForCurrentFocus();
    if (announceStatus)
    {
        if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
        {
            RebuildStatus("Layer focus cleared on page '" + page->name + "'.", false);
        }
        else
        {
            RebuildStatus("Layer focus cleared.", false);
        }
    }
}

void EditorApplication::SanitizeLayerFocusForActivePage()
{
    if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        services_.layerFocus.SanitizeFocusState(*page, layoutState_.layerFocusState);
    }
    else
    {
        layoutState_.layerFocusState = {};
    }
}

void EditorApplication::SanitizePageReticleSelectionForCurrentFocus()
{
    if (documentState_.selection.kind != SelectionKind::PageReticle &&
        documentState_.selection.kind != SelectionKind::PageTitle &&
        documentState_.selection.kind != SelectionKind::PageStrobe)
    {
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr || documentState_.selection.pageIndex < 0 || documentState_.selection.pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        documentState_.selection.pageReticleIndex = -1;
        documentState_.selection.pageReticleIndices.clear();
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageTitle)
    {
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
        if (strobe == nullptr)
        {
            SelectPage(documentState_.selection.pageIndex);
        }
        return;
    }

    const std::vector<int> filtered =
        services_.layerFocus.FilterSelectableReticleIndices(*page, documentState_.selection.pageReticleIndices, layoutState_.layerFocusState);
    if (filtered.empty())
    {
        SelectPage(documentState_.selection.pageIndex);
        return;
    }

    documentState_.selection.pageReticleIndices = filtered;
    if (std::find(filtered.begin(), filtered.end(), documentState_.selection.pageReticleIndex) == filtered.end())
    {
        documentState_.selection.pageReticleIndex = filtered.back();
    }
}

bool EditorApplication::IsPageReticleSelectableInCurrentFocus(const mfd::PageDefinition& page,
                                                              const mfd::ReticleGroup& reticle) const
{
    return services_.layerFocus.IsReticleSelectable(page, reticle, layoutState_.layerFocusState);
}

bool EditorApplication::ShouldDimPageReticleInCurrentFocus(const mfd::PageDefinition& page,
                                                           const mfd::ReticleGroup& reticle) const
{
    return services_.layerFocus.ShouldReticleBeDimmed(page, reticle, layoutState_.layerFocusState);
}

std::string EditorApplication::ActiveInsertionLayerId(const mfd::PageDefinition& page) const
{
    if (services_.layerFocus.IsFocusActive(page, layoutState_.layerFocusState))
    {
        return layoutState_.layerFocusState.focusedLayerId;
    }

    return DefaultEditorLayerId(page);
}
