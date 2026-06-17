/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Selection state mutators and accessors extracted from `EditorApplication`.
 */

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "internal/application/EditorApplicationInternal.h"

namespace
{
using editor::app::FindEditorLayer;
using editor::app::IsPageStrobeSelectableInEditor;
using editor::app::RefreshPageBlinkStateForEditor;
using editor::detail::BootstrapEditorLayersForPage;
}

void EditorApplication::SelectPage(const int pageIndex, const bool resetPreviewView)
{
    const int previousPageIndex = documentState_.selection.pageIndex;
    documentState_.selection.kind = SelectionKind::Page;
    documentState_.selection.pageIndex = std::clamp(pageIndex, 0, std::max(0, static_cast<int>(documentState_.loaded.document.pages.size()) - 1));
    if (documentState_.selection.pageIndex != previousPageIndex)
    {
        layoutState_.layerFocusState = {};
    }
    if (mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        BootstrapEditorLayersForPage(*page);
    }
    SanitizeLayerFocusForActivePage();
    documentState_.selection.pageReticleIndex = -1;
    documentState_.selection.pageReticleIndices.clear();
    interactionState_.mode = InteractionMode::None;
    interactionState_.primitiveIndex = -1;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
    if (resetPreviewView)
    {
        ResetPagePreviewView();
    }
}

void EditorApplication::SelectWindow()
{
    documentState_.selection.kind = SelectionKind::Window;
    documentState_.selection.pageReticleIndex = -1;
    documentState_.selection.pageReticleIndices.clear();
    documentState_.selection.libraryReticleId.clear();
    documentState_.selection.libraryBrowserReticleId.clear();
    documentState_.selection.primitiveIndex = -1;
    interactionState_.mode = InteractionMode::None;
    interactionState_.primitiveIndex = -1;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
}

void EditorApplication::SelectPageReticle(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return;
    }

    mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        !IsPageReticleSelectableInCurrentFocus(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)]))
    {
        return;
    }

    documentState_.selection.kind = SelectionKind::PageReticle;
    documentState_.selection.pageIndex = pageIndex;
    documentState_.selection.pageReticleIndex = reticleIndex;
    documentState_.selection.pageReticleIndices = {reticleIndex};
    interactionState_.mode = InteractionMode::None;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.primitiveIndex = -1;
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
}

void EditorApplication::SelectPageTitle(const int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return;
    }

    documentState_.selection.kind = SelectionKind::PageTitle;
    documentState_.selection.pageIndex = pageIndex;
    documentState_.selection.pageReticleIndex = -1;
    documentState_.selection.pageReticleIndices.clear();
    interactionState_.mode = InteractionMode::None;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.primitiveIndex = -1;
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
}

void EditorApplication::SelectPageStrobe(const int pageIndex, const int strobeIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return;
    }

    mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
    if (page.strobes.empty())
    {
        return;
    }

    int resolvedStrobeIndex = strobeIndex;
    if (resolvedStrobeIndex < 0 || resolvedStrobeIndex >= static_cast<int>(page.strobes.size()))
    {
        const mfd::PageStrobeDefinition* activeStrobe = mfd::FindActivePageStrobeDefinition(page);
        if (activeStrobe == nullptr)
        {
            return;
        }

        resolvedStrobeIndex = static_cast<int>(activeStrobe - page.strobes.data());
    }

    if (resolvedStrobeIndex < 0 || resolvedStrobeIndex >= static_cast<int>(page.strobes.size()))
    {
        return;
    }

    if (!IsPageStrobeSelectableInEditor(page, resolvedStrobeIndex))
    {
        return;
    }

    documentState_.selection.kind = SelectionKind::PageStrobe;
    documentState_.selection.pageIndex = pageIndex;
    documentState_.selection.pageReticleIndex = resolvedStrobeIndex;
    documentState_.selection.pageReticleIndices.clear();
    interactionState_.mode = InteractionMode::None;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.primitiveIndex = -1;
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
}

void EditorApplication::TogglePageReticleSelection(const int pageIndex, const int reticleIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return;
    }

    mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        !IsPageReticleSelectableInCurrentFocus(page, page.staticReticles[static_cast<std::size_t>(reticleIndex)]))
    {
        return;
    }

    if (documentState_.selection.kind != SelectionKind::PageReticle || documentState_.selection.pageIndex != pageIndex)
    {
        SelectPageReticle(pageIndex, reticleIndex);
        return;
    }

    auto& indices = documentState_.selection.pageReticleIndices;
    auto iterator = std::find(indices.begin(), indices.end(), reticleIndex);
    if (iterator == indices.end())
    {
        indices.push_back(reticleIndex);
        std::sort(indices.begin(), indices.end());
        documentState_.selection.pageReticleIndex = reticleIndex;
        return;
    }

    if (indices.size() == 1U)
    {
        SelectPage(pageIndex);
        return;
    }

    indices.erase(iterator);
    if (documentState_.selection.pageReticleIndex == reticleIndex)
    {
        documentState_.selection.pageReticleIndex = indices.back();
    }
}

void EditorApplication::SelectLibraryReticle(std::string templateId, const bool resetPreviewView)
{
    documentState_.selection.kind = SelectionKind::LibraryReticle;
    documentState_.selection.libraryReticleId = std::move(templateId);
    documentState_.selection.libraryBrowserReticleId = documentState_.selection.libraryReticleId;
    documentState_.selection.pageReticleIndex = -1;
    documentState_.selection.pageReticleIndices.clear();
    documentState_.selection.primitiveIndex = -1;
    interactionState_.mode = InteractionMode::None;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.primitiveIndex = -1;
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
    if (resetPreviewView)
    {
        ResetLibraryPreviewView();
    }
}

void EditorApplication::SelectLibraryPrimitive(std::string templateId, const int primitiveIndex)
{
    documentState_.selection.kind = SelectionKind::LibraryPrimitive;
    documentState_.selection.libraryReticleId = std::move(templateId);
    documentState_.selection.libraryBrowserReticleId = documentState_.selection.libraryReticleId;
    documentState_.selection.pageReticleIndex = -1;
    documentState_.selection.pageReticleIndices.clear();
    documentState_.selection.primitiveIndex = primitiveIndex;
    interactionState_.mode = InteractionMode::None;
    interactionState_.reticleIndex = -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    interactionState_.primitiveIndex = -1;
    interactionState_.handleIndex = -1;
    interactionState_.handleKind = PrimitiveHandleKind::None;
}

mfd::PageDefinition* EditorApplication::ActivePage() noexcept
{
    if (documentState_.selection.pageIndex < 0 || documentState_.selection.pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return nullptr;
    }

    return &documentState_.loaded.document.pages[static_cast<std::size_t>(documentState_.selection.pageIndex)];
}

const mfd::PageDefinition* EditorApplication::ActivePage() const noexcept
{
    if (documentState_.selection.pageIndex < 0 || documentState_.selection.pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        return nullptr;
    }

    return &documentState_.loaded.document.pages[static_cast<std::size_t>(documentState_.selection.pageIndex)];
}

mfd::ReticleGroup* EditorApplication::SelectedPageReticle() noexcept
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        documentState_.selection.pageReticleIndex < 0 ||
        documentState_.selection.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(documentState_.selection.pageReticleIndex)];
}

const mfd::ReticleGroup* EditorApplication::SelectedPageReticle() const noexcept
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        documentState_.selection.pageReticleIndex < 0 ||
        documentState_.selection.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(documentState_.selection.pageReticleIndex)];
}

mfd::ReticleGroup* EditorApplication::SelectedPageStrobeReticle() noexcept
{
    mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
    if (strobe == nullptr)
    {
        return nullptr;
    }

    return &strobe->reticle;
}

const mfd::ReticleGroup* EditorApplication::SelectedPageStrobeReticle() const noexcept
{
    const mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
    if (strobe == nullptr)
    {
        return nullptr;
    }

    return &strobe->reticle;
}

mfd::PageStrobeDefinition* EditorApplication::SelectedPageStrobe() noexcept
{
    mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind != SelectionKind::PageStrobe ||
        page == nullptr ||
        documentState_.selection.pageReticleIndex < 0 ||
        documentState_.selection.pageReticleIndex >= static_cast<int>(page->strobes.size()))
    {
        return nullptr;
    }

    return &page->strobes[static_cast<std::size_t>(documentState_.selection.pageReticleIndex)];
}

const mfd::PageStrobeDefinition* EditorApplication::SelectedPageStrobe() const noexcept
{
    const mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind != SelectionKind::PageStrobe ||
        page == nullptr ||
        documentState_.selection.pageReticleIndex < 0 ||
        documentState_.selection.pageReticleIndex >= static_cast<int>(page->strobes.size()))
    {
        return nullptr;
    }

    return &page->strobes[static_cast<std::size_t>(documentState_.selection.pageReticleIndex)];
}

mfd::PageTitleDisplayDefinition* EditorApplication::SelectedPageTitleDisplay() noexcept
{
    mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind != SelectionKind::PageTitle || page == nullptr)
    {
        return nullptr;
    }

    return &page->titleDisplay;
}

const mfd::PageTitleDisplayDefinition* EditorApplication::SelectedPageTitleDisplay() const noexcept
{
    const mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind != SelectionKind::PageTitle || page == nullptr)
    {
        return nullptr;
    }

    return &page->titleDisplay;
}

mfd::ReticleGroup* EditorApplication::SelectedEditablePageReticle() noexcept
{
    return documentState_.selection.kind == SelectionKind::PageStrobe ? SelectedPageStrobeReticle() : SelectedPageReticle();
}

const mfd::ReticleGroup* EditorApplication::SelectedEditablePageReticle() const noexcept
{
    return documentState_.selection.kind == SelectionKind::PageStrobe ? SelectedPageStrobeReticle() : SelectedPageReticle();
}

bool EditorApplication::HasSelectedPageReticle(const int pageIndex, const int reticleIndex) const noexcept
{
    if (documentState_.selection.kind != SelectionKind::PageReticle || documentState_.selection.pageIndex != pageIndex)
    {
        return false;
    }

    const auto& indices = documentState_.selection.pageReticleIndices;
    if (indices.empty())
    {
        return documentState_.selection.pageReticleIndex == reticleIndex;
    }

    return std::find(indices.begin(), indices.end(), reticleIndex) != indices.end();
}

bool EditorApplication::IsPageStrobeSelected() const noexcept
{
    return documentState_.selection.kind == SelectionKind::PageStrobe && SelectedPageStrobeReticle() != nullptr;
}

bool EditorApplication::IsPageTitleSelected() const noexcept
{
    return documentState_.selection.kind == SelectionKind::PageTitle && SelectedPageTitleDisplay() != nullptr;
}

std::vector<int> EditorApplication::SelectedPageReticleIndices() const
{
    std::vector<int> indices;
    if (documentState_.selection.kind != SelectionKind::PageReticle)
    {
        return indices;
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return indices;
    }

    if (!documentState_.selection.pageReticleIndices.empty())
    {
        indices = documentState_.selection.pageReticleIndices;
    }
    else if (documentState_.selection.pageReticleIndex >= 0)
    {
        indices.push_back(documentState_.selection.pageReticleIndex);
    }

    indices.erase(
        std::remove_if(indices.begin(),
                       indices.end(),
                       [page](const int index)
                       {
                           return index < 0 || index >= static_cast<int>(page->staticReticles.size());
                       }),
        indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

int EditorApplication::SelectedPageReticleCount() const
{
    return static_cast<int>(SelectedPageReticleIndices().size());
}

bool EditorApplication::HasOpenWindow() const noexcept
{
    return !documentState_.loaded.window.sourceFile.empty();
}

const std::filesystem::path& EditorApplication::CurrentWindowFile() const noexcept
{
    return documentState_.windowFile;
}

void EditorApplication::CopySelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        const mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
        if (page == nullptr || strobe == nullptr)
        {
            RebuildStatus("Select the page strobe to copy it.", true);
            return;
        }

        clipboardState_.pageReticleClipboard.clear();
        clipboardState_.pageReticleClipboard.push_back(strobe->reticle);
        clipboardState_.pageReticlePasteSerial = 0;
        RebuildStatus("Strobe '" + strobe->reticle.id + "' copied from page '" + page->name + "'.", false);
        return;
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to copy them.", true);
        return;
    }

    clipboardState_.pageReticleClipboard.clear();
    clipboardState_.pageReticleClipboard.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        clipboardState_.pageReticleClipboard.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }

    clipboardState_.pageReticlePasteSerial = 0;
    if (clipboardState_.pageReticleClipboard.size() == 1U)
    {
        RebuildStatus("Reticle '" + clipboardState_.pageReticleClipboard.front().id + "' copied from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(clipboardState_.pageReticleClipboard.size()) + " reticles copied from page '" + page->name + "'.",
            false);
    }
}

void EditorApplication::CutSelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
        if (page == nullptr || strobe == nullptr)
        {
            RebuildStatus("Select the page strobe to cut it.", true);
            return;
        }

        clipboardState_.pageReticleClipboard.clear();
        clipboardState_.pageReticleClipboard.push_back(strobe->reticle);
        clipboardState_.pageReticlePasteSerial = 0;

        PushUndoSnapshot();
        const std::string strobeId = strobe->reticle.id;
        const std::string removedNormalizedName = strobe->normalizedName;
        page->strobes.erase(page->strobes.begin() + documentState_.selection.pageReticleIndex);
        if (page->strobes.empty())
        {
            page->activeStrobeName.clear();
            page->normalizedActiveStrobeName.clear();
        }
        else if (page->normalizedActiveStrobeName == removedNormalizedName)
        {
            page->activeStrobeName = page->strobes.front().name;
            page->normalizedActiveStrobeName = page->strobes.front().normalizedName;
        }
        SelectPage(documentState_.selection.pageIndex);
        RebuildStatus("Strobe '" + strobeId + "' cut from page '" + page->name + "'.", false);
        return;
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to cut them.", true);
        return;
    }

    clipboardState_.pageReticleClipboard.clear();
    clipboardState_.pageReticleClipboard.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        clipboardState_.pageReticleClipboard.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }
    clipboardState_.pageReticlePasteSerial = 0;

    PushUndoSnapshot();
    std::vector<int> descendingIndices = selectedIndices;
    std::sort(descendingIndices.begin(), descendingIndices.end(), std::greater<int>());
    for (const int reticleIndex : descendingIndices)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            continue;
        }

        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
    }

    SelectPage(documentState_.selection.pageIndex);
    if (clipboardState_.pageReticleClipboard.size() == 1U)
    {
        RebuildStatus("Reticle '" + clipboardState_.pageReticleClipboard.front().id + "' cut from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(clipboardState_.pageReticleClipboard.size()) + " reticles cut from page '" + page->name + "'.",
            false);
    }
}

void EditorApplication::PasteCopiedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select a page before pasting copied reticles.", true);
        return;
    }

    if (clipboardState_.pageReticleClipboard.empty())
    {
        RebuildStatus("No copied page reticle is available yet.", true);
        return;
    }

    PushUndoSnapshot();

    ++clipboardState_.pageReticlePasteSerial;
    const float offset = 0.035f * static_cast<float>(clipboardState_.pageReticlePasteSerial);
    std::vector<int> pastedIndices;
    pastedIndices.reserve(clipboardState_.pageReticleClipboard.size());

    for (const auto& sourceReticle : clipboardState_.pageReticleClipboard)
    {
        mfd::ReticleGroup pastedReticle = sourceReticle;
        const std::string baseId = pastedReticle.id.empty() ? std::string {"reticle"} : pastedReticle.id;
        pastedReticle.id = MakeUniquePageReticleId(*page, baseId);
        pastedReticle.transform.position.x = std::clamp(pastedReticle.transform.position.x + offset, -1.0f, 1.0f);
        pastedReticle.transform.position.y = std::clamp(pastedReticle.transform.position.y - offset, -1.0f, 1.0f);
        if (!pastedReticle.layerId.empty() && FindEditorLayer(*page, pastedReticle.layerId) == nullptr)
        {
            pastedReticle.layerId = ActiveInsertionLayerId(*page);
        }
        else if (pastedReticle.layerId.empty())
        {
            pastedReticle.layerId = ActiveInsertionLayerId(*page);
        }

        page->staticReticles.push_back(std::move(pastedReticle));
        pastedIndices.push_back(static_cast<int>(page->staticReticles.size()) - 1);
    }

    RefreshPageBlinkStateForEditor(*page);
    documentState_.selection.kind = SelectionKind::PageReticle;
    documentState_.selection.pageReticleIndices = pastedIndices;
    documentState_.selection.pageReticleIndex = pastedIndices.empty() ? -1 : pastedIndices.back();
    SanitizePageReticleSelectionForCurrentFocus();

    if (pastedIndices.size() == 1U)
    {
        const mfd::ReticleGroup& pastedReticle = page->staticReticles[static_cast<std::size_t>(pastedIndices.front())];
        RebuildStatus("Reticle '" + pastedReticle.id + "' pasted on page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pastedIndices.size()) + " reticles pasted on page '" + page->name + "'.",
            false);
    }
}

mfd::ReticleGroup* EditorApplication::SelectedLibraryReticle() noexcept
{
    const auto iterator = documentState_.loaded.document.reticleLibrary.find(documentState_.selection.libraryReticleId);
    return iterator == documentState_.loaded.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

const mfd::ReticleGroup* EditorApplication::SelectedLibraryReticle() const noexcept
{
    const auto iterator = documentState_.loaded.document.reticleLibrary.find(documentState_.selection.libraryReticleId);
    return iterator == documentState_.loaded.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() noexcept
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        documentState_.selection.primitiveIndex < 0 ||
        documentState_.selection.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(documentState_.selection.primitiveIndex)];
}

const mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() const noexcept
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        documentState_.selection.primitiveIndex < 0 ||
        documentState_.selection.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(documentState_.selection.primitiveIndex)];
}
