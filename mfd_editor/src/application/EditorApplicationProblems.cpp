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
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::app::IsPageStrobeVisibleInEditor;
using editor::ui::ShowItemTooltip;

std::string NormalizeEditorIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

std::string PageProblemId(const mfd::PageDefinition& page)
{
    return "page/" + NormalizeEditorIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName);
}

std::string ReticleAssetProblemId(const std::string_view templateId)
{
    return "reticle/" + NormalizeEditorIdentifier(templateId);
}

std::string PageReticleProblemId(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    return PageProblemId(page) + "/reticle/" + NormalizeEditorIdentifier(reticle.id);
}

void PushProblem(std::vector<editor::PagePreviewProblem>& problems,
                 const std::string_view entityId,
                 const std::string_view message)
{
    if (entityId.empty())
    {
        problems.push_back(editor::PagePreviewProblem {std::string(message), std::string {}});
        return;
    }

    problems.push_back(editor::PagePreviewProblem {
        std::string(entityId) + ": " + std::string(message),
        std::string(entityId)});
}

void AppendPrimitiveProblems(std::vector<editor::PagePreviewProblem>& messages,
                             const std::vector<mfd::Primitive>& primitives,
                             const std::string_view ownerId)
{
    std::unordered_set<std::string> primitiveIds;
    for (const mfd::Primitive& primitive : primitives)
    {
        if (primitive.id.empty())
        {
            continue;
        }

        if (!primitiveIds.insert(NormalizeEditorIdentifier(primitive.id)).second)
        {
            PushProblem(messages, ownerId, "Primitive ids must stay unique inside one reticle.");
        }
    }
}
}

std::vector<editor::PagePreviewProblem> EditorApplication::BuildPagePreviewProblems() const
{
    std::vector<editor::PagePreviewProblem> messages;
    if (!HasOpenWindow())
    {
        return messages;
    }

    if (!documentState_.loaded.document.pages.empty() && documentState_.files.pageFiles.size() != documentState_.loaded.document.pages.size())
    {
        PushProblem(messages, "window", "Page file layout count must match the number of authored pages.");
    }

    std::unordered_set<std::string> pageNames;
    for (const mfd::PageDefinition& page : documentState_.loaded.document.pages)
    {
        const std::string pageId = PageProblemId(page);
        const std::string normalizedPageId =
            page.normalizedName.empty() ? NormalizeEditorIdentifier(page.name) : page.normalizedName;

        if (page.name.empty())
        {
            PushProblem(messages, pageId, "Page name cannot be empty.");
        }

        if (!pageNames.insert(normalizedPageId).second)
        {
            PushProblem(messages, pageId, "Page ids must stay unique.");
        }

        std::unordered_set<std::string> runtimeLayerIds;
        for (const mfd::PageLayerDefinition& layer : page.layers)
        {
            if (layer.id.empty())
            {
                PushProblem(messages, pageId, "Page layer ids cannot be empty.");
                continue;
            }

            if (!runtimeLayerIds.insert(NormalizeEditorIdentifier(layer.id)).second)
            {
                PushProblem(messages, pageId, "Page layer ids must stay unique inside one page.");
            }
        }

        std::unordered_set<std::string> editorLayerIds;
        for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
        {
            if (layer.id.empty())
            {
                PushProblem(messages, pageId, "Editor layer ids cannot be empty.");
                continue;
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(layer.id);
            if (!editorLayerIds.insert(normalizedLayerId).second)
            {
                PushProblem(messages, pageId, "Editor layer ids must stay unique inside one page.");
            }

            if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, pageId, "Editor layer state must reference one runtime page layer.");
            }
        }

        std::unordered_set<std::string> blinkTypeNames;
        for (const mfd::PageBlinkDefinition& blinkType : page.blinkTypes)
        {
            const std::string normalizedBlinkName =
                blinkType.normalizedName.empty() ? NormalizeEditorIdentifier(blinkType.name) : blinkType.normalizedName;
            if (normalizedBlinkName.empty())
            {
                PushProblem(messages, pageId, "Blink type names cannot be empty.");
                continue;
            }

            if (!blinkTypeNames.insert(normalizedBlinkName).second)
            {
                PushProblem(messages, pageId, "Blink type names must stay unique inside one page.");
            }
        }

        if (!page.defaultBlinkTypeName.empty() &&
            mfd::FindPageBlinkDefinition(page, page.defaultBlinkTypeName) == nullptr)
        {
            PushProblem(messages, pageId, "The page default blink type must resolve inside the page blink catalog.");
        }

        std::unordered_set<std::string> dynamicTemplateIds;
        std::unordered_map<std::string, std::unordered_set<int>> dynamicBindingOrdersByLayer;
        for (const mfd::DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
        {
            if (binding.templateId.empty())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must define a template id.");
                continue;
            }

            const std::string normalizedTemplateId = NormalizeEditorIdentifier(binding.templateId);
            if (!dynamicTemplateIds.insert(normalizedTemplateId).second)
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding template ids must stay unique inside one page.");
            }

            if (documentState_.loaded.document.reticleLibrary.find(binding.templateId) == documentState_.loaded.document.reticleLibrary.end())
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding template ids must resolve inside the loaded reticle library.");
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(binding.layerId);
            if (normalizedLayerId.empty())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must define a layer id.");
            }
            else if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, pageId, "Page dynamic reticle bindings must reference one runtime page layer.");
            }

            if (!dynamicBindingOrdersByLayer[normalizedLayerId].insert(binding.orderInLayer).second)
            {
                PushProblem(messages, pageId, "Page dynamic reticle binding orderInLayer values must stay unique inside one layer.");
            }
        }

        std::unordered_set<std::string> reticleIds;
        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            const std::string reticleId = PageReticleProblemId(page, reticle);
            if (reticle.id.empty())
            {
                PushProblem(messages, pageId, "Page reticle ids cannot be empty.");
            }

            if (!reticleIds.insert(NormalizeEditorIdentifier(reticle.id)).second)
            {
                PushProblem(messages, reticleId, "Page reticle ids must stay unique inside one page.");
            }

            if (!reticle.sourceTemplateId.empty() &&
                documentState_.loaded.document.reticleLibrary.find(reticle.sourceTemplateId) == documentState_.loaded.document.reticleLibrary.end())
            {
                PushProblem(messages, reticleId, "Page reticle source template must resolve inside the loaded reticle library.");
            }

            const std::string normalizedLayerId = NormalizeEditorIdentifier(reticle.layerId);
            if (normalizedLayerId.empty())
            {
                PushProblem(messages, reticleId, "Page reticles must define a runtime layer id.");
            }
            else if (runtimeLayerIds.find(normalizedLayerId) == runtimeLayerIds.end())
            {
                PushProblem(messages, reticleId, "Page reticles must reference an existing runtime page layer.");
            }

            AppendPrimitiveProblems(messages, reticle.primitives, reticleId);
            if (reticle.clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(reticle) == nullptr)
            {
                PushProblem(messages, reticleId, "Reticle clipping must reference an existing supported primitive.");
            }

            if (reticle.blink.enabled &&
                !reticle.blink.typeName.empty() &&
                mfd::FindPageBlinkDefinition(page, reticle.blink.typeName) == nullptr)
            {
                PushProblem(messages, reticleId, "Page reticle blink bindings must reference one page-local blink type.");
            }
        }

        for (const auto& strobe : page.strobes)
        {
            if (strobe.reticle.blink.enabled &&
                !strobe.reticle.blink.typeName.empty() &&
                mfd::FindPageBlinkDefinition(page, strobe.reticle.blink.typeName) == nullptr)
            {
                PushProblem(messages,
                            pageId,
                            "Page strobe blink bindings must reference one page-local blink type.");
            }

            if (strobe.reticle.clipping.mode != mfd::ReticleClipMode::None &&
                mfd::ResolveClipPrimitive(strobe.reticle) == nullptr)
            {
                PushProblem(messages, pageId, "Page strobe clipping must reference an existing supported primitive.");
            }
        }
    }

    for (const auto& [templateId, reticle] : documentState_.loaded.document.reticleLibrary)
    {
        const std::string reticleId = ReticleAssetProblemId(templateId);
        if (NormalizeEditorIdentifier(templateId) != NormalizeEditorIdentifier(reticle.id))
        {
            PushProblem(messages, reticleId, "Reticle-library map key and reticle id must stay aligned.");
        }

        if (documentState_.files.templateFiles.find(templateId) == documentState_.files.templateFiles.end())
        {
            PushProblem(messages, reticleId, "Missing template file path for reticle asset.");
        }

        AppendPrimitiveProblems(messages, reticle.primitives, reticleId);
        if (reticle.clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(reticle) == nullptr)
        {
            PushProblem(messages, reticleId, "Reticle clipping must reference an existing supported primitive.");
        }
    }

    return messages;
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
