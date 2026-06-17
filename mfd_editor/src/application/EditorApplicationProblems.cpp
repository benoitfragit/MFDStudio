/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Problems panel, problem navigation, and reticle usage highlighting extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::app::IsPageStrobeVisibleInEditor;
using editor::ui::ShowItemTooltip;
}

void EditorApplication::NavigateToProblem(const std::string& contextId)
{
    if (contextId.empty() || !HasOpenWindow())
    {
        return;
    }

    if (contextId == "window")
    {
        SelectWindow();
        return;
    }

    std::vector<std::string> normalizedPageIds;
    normalizedPageIds.reserve(documentState_.loaded.document.pages.size());
    for (const mfd::PageDefinition& page : documentState_.loaded.document.pages)
    {
        normalizedPageIds.push_back(
            mfd::NormalizePageName(page.normalizedName.empty() ? page.name : page.normalizedName));
    }

    if (const int pageIndex = editor::MatchProblemPageIndex(normalizedPageIds, contextId); pageIndex >= 0)
    {
        SelectPage(pageIndex);
        return;
    }

    if (contextId.rfind("reticle/", 0) == 0)
    {
        const std::string reticleSegment = contextId.substr(std::string_view("reticle/").size());
        for (const auto& entry : documentState_.loaded.document.reticleLibrary)
        {
            if (mfd::NormalizePageName(entry.first) == reticleSegment)
            {
                SelectLibraryReticle(entry.first);
                return;
            }
        }
    }
}

void EditorApplication::DrawProblemsPanel(const std::vector<editor::PagePreviewProblem>& problems)
{
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.67f, 1.0f), "Problems");
    if (!problems.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu)", problems.size());
    }
    ImGui::TextDisabled("Validation diagnostics for the current editor state. Click a problem to select it.");
    ImGui::Separator();

    if (ImGui::BeginChild("PagePreviewProblemsScrollRegion", ImVec2(0.0f, 0.0f), false))
    {
        if (problems.empty())
        {
            ImGui::TextDisabled("No validation problems detected.");
        }
        else
        {
            for (std::size_t index = 0; index < problems.size(); ++index)
            {
                const editor::PagePreviewProblem& problem = problems[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.44f, 1.0f), "%02zu.", index + 1U);
                ImGui::SameLine();
                const float wrapPos = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::PushTextWrapPos(wrapPos);
                ImGui::TextUnformatted(problem.message.c_str());
                ImGui::PopTextWrapPos();
                if (!problem.contextId.empty())
                {
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                    ShowItemTooltip("Click to select the entity this problem refers to.");
                    if (ImGui::IsItemClicked())
                    {
                        NavigateToProblem(problem.contextId);
                    }
                }
                if (index + 1U < problems.size())
                {
                    ImGui::Spacing();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void EditorApplication::DrawReticleUsageHighlightPlaceholder(const ViewportState& viewport)
{
    const editor::ReticleUsageHighlightResult* usageHighlight = ResolveReticleUsageHighlight();
    if (usageHighlight == nullptr)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const mfd::PageDefinition* page = ActivePage();
    const editor::ReticleUsageHighlightPage* currentPageHighlight = nullptr;
    if (page != nullptr)
    {
        const auto iterator = std::find_if(usageHighlight->pages.begin(),
                                           usageHighlight->pages.end(),
                                           [this](const editor::ReticleUsageHighlightPage& usagePage)
                                           {
                                               return usagePage.currentPageIndex == documentState_.selection.pageIndex;
                                           });
        if (iterator != usageHighlight->pages.end())
        {
            currentPageHighlight = &(*iterator);
        }
    }

    if (page != nullptr && currentPageHighlight != nullptr)
    {
        for (const int reticleIndex : currentPageHighlight->matchingReticleIndices)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            const ReticleScreenBounds bounds =
                ComputeReticleScreenBounds(page->staticReticles[static_cast<std::size_t>(reticleIndex)], viewport);
            if (!bounds.valid)
            {
                continue;
            }

            drawList->AddRectFilled(bounds.min, bounds.max, IM_COL32(255, 210, 102, 24), 5.0f);
            drawList->AddRect(bounds.min, bounds.max, IM_COL32(255, 210, 102, 255), 5.0f, 0, 2.2f);
        }

        if (currentPageHighlight->matchingStrobe)
        {
            for (const auto& strobe : page->strobes)
            {
                if (!IsPageStrobeVisibleInEditor(*page, strobe))
                {
                    continue;
                }

                const ReticleScreenBounds strobeBounds = ComputeReticleScreenBounds(strobe.reticle, viewport);
                if (!strobeBounds.valid)
                {
                    continue;
                }

                drawList->AddRectFilled(strobeBounds.min, strobeBounds.max, IM_COL32(255, 210, 102, 18), 5.0f);
                drawList->AddRect(strobeBounds.min, strobeBounds.max, IM_COL32(255, 210, 102, 220), 5.0f, 0, 2.0f);
            }
        }
    }

    const bool noUsage = !usageHighlight->hasUsage;
    const std::string header = noUsage
                                   ? "No page currently uses '" + usageHighlight->templateId + "'."
                                   : std::to_string(usageHighlight->pages.size()) + " page" +
                                         (usageHighlight->pages.size() == 1U ? "" : "s") +
                                         " use '" + usageHighlight->templateId + "'.";
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const std::size_t shownPages = std::min<std::size_t>(4U, usageHighlight->pages.size());
    const float panelWidth = 320.0f;
    const float panelHeight = 42.0f + lineHeight * static_cast<float>(shownPages) + (shownPages > 0U ? 6.0f : 0.0f);
    const ImVec2 panelMin(viewport.origin.x + 12.0f, viewport.origin.y + viewport.size.y - panelHeight - 12.0f);
    const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(33, 49, 59, 220), 6.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(255, 210, 102, noUsage ? 160 : 220), 6.0f, 0, 1.5f);
    drawList->AddText(ImVec2(panelMin.x + 10.0f, panelMin.y + 8.0f),
                      noUsage ? IM_COL32(220, 235, 240, 255) : IM_COL32(255, 224, 176, 255),
                      header.c_str());

    float currentY = panelMin.y + 8.0f + lineHeight;
    for (std::size_t index = 0; index < shownPages; ++index)
    {
        const editor::ReticleUsageHighlightPage& usagePage = usageHighlight->pages[index];
        const std::string label = std::string("- ") + usagePage.pageName;
        drawList->AddText(ImVec2(panelMin.x + 12.0f, currentY), IM_COL32(220, 235, 240, 255), label.c_str());
        currentY += lineHeight;
    }

    if (usageHighlight->pages.size() > shownPages)
    {
        const std::string more = "+" + std::to_string(usageHighlight->pages.size() - shownPages) + " more";
        drawList->AddText(ImVec2(panelMin.x + 12.0f, currentY), IM_COL32(170, 186, 198, 255), more.c_str());
    }
}

const editor::ReticleUsageHighlightResult* EditorApplication::ResolveReticleUsageHighlight()
{
    const mfd::ReticleGroup* selectedReticle = SelectedLibraryReticle();
    if (!layoutState_.pagePreviewViewOptions.highlightReticleUsages || selectedReticle == nullptr)
    {
        return nullptr;
    }

    std::filesystem::path templateFile = documentState_.loaded.window.reticleLibraryFolder;
    if (const auto iterator = documentState_.files.templateFiles.find(documentState_.selection.libraryReticleId); iterator != documentState_.files.templateFiles.end())
    {
        templateFile = iterator->second;
    }

    const std::filesystem::path assetsRoot = documentState_.assetPaths.ResolveAssetRootForPath(templateFile);
    if (!previewState_.reticleUsageHighlightCache.dirty &&
        mfd::PageNamesEqual(previewState_.reticleUsageHighlightCache.templateId, selectedReticle->id) &&
        previewState_.reticleUsageHighlightCache.assetsRoot == assetsRoot)
    {
        return &previewState_.reticleUsageHighlightCache.result;
    }

    previewState_.reticleUsageHighlightCache.result = services_.reticleUsageHighlight.BuildHighlight(
        documentState_.loaded,
        documentState_.files,
        editor::ReticleUsageHighlightRequest {
            selectedReticle->id,
            assetsRoot});
    previewState_.reticleUsageHighlightCache.templateId = selectedReticle->id;
    previewState_.reticleUsageHighlightCache.assetsRoot = assetsRoot;
    previewState_.reticleUsageHighlightCache.dirty = false;
    return &previewState_.reticleUsageHighlightCache.result;
}

void EditorApplication::InvalidateReticleUsageHighlightCache() noexcept
{
    previewState_.reticleUsageHighlightCache.dirty = true;
}
