/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Selection deletion, dropped-file import, and sidebar tree-view drawing extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "EditorTutorialController.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::app::IsReticleVisibleInEditor;
using editor::app::PageStrobeDisplayLabel;
using editor::app::PageTitleDecorationLabel;
using editor::ui::ShowItemTooltip;
using json = nlohmann::json;

char LowerAscii(const char value) noexcept
{
    return (value >= 'A' && value <= 'Z') ? static_cast<char>(value - 'A' + 'a') : value;
}

bool ContainsCaseInsensitive(const std::string_view haystack, const std::string_view needle) noexcept
{
    if (needle.empty())
    {
        return true;
    }

    if (needle.size() > haystack.size())
    {
        return false;
    }

    for (std::size_t offset = 0; offset + needle.size() <= haystack.size(); ++offset)
    {
        bool matched = true;
        for (std::size_t index = 0; index < needle.size(); ++index)
        {
            if (LowerAscii(haystack[offset + index]) != LowerAscii(needle[index]))
            {
                matched = false;
                break;
            }
        }

        if (matched)
        {
            return true;
        }
    }

    return false;
}

enum class DroppedJsonDocumentKind
{
    Unknown,
    Window,
    Page
};

const json* FindDroppedJsonField(const json& node, const std::initializer_list<const char*> fieldNames)
{
    if (!node.is_object())
    {
        return nullptr;
    }

    for (const char* fieldName : fieldNames)
    {
        const auto iterator = node.find(fieldName);
        if (iterator != node.end())
        {
            return &(*iterator);
        }
    }

    return nullptr;
}

bool IsDroppedPageFileList(const json& value)
{
    if (!value.is_array())
    {
        return false;
    }

    for (const auto& entry : value)
    {
        if (entry.is_string())
        {
            continue;
        }

        if (entry.is_object() && FindDroppedJsonField(entry, {"file", "path", "json"}) != nullptr)
        {
            continue;
        }

        return false;
    }

    return true;
}

DroppedJsonDocumentKind ClassifyDroppedJsonDocument(const std::filesystem::path& path, std::string* error)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        if (error != nullptr)
        {
            *error = "Unable to open dropped JSON file '" + path.string() + "'.";
        }
        return DroppedJsonDocumentKind::Unknown;
    }

    try
    {
        json document;
        stream >> document;

        if (const json* pages = FindDroppedJsonField(document, {"pageFiles", "pages", "pageJsons"});
            pages != nullptr && IsDroppedPageFileList(*pages))
        {
            return DroppedJsonDocumentKind::Window;
        }

        const json* pageNode = &document;
        if (const json* nestedPage = FindDroppedJsonField(document, {"page"});
            nestedPage != nullptr && nestedPage->is_object())
        {
            pageNode = nestedPage;
        }

        if (FindDroppedJsonField(*pageNode,
                                 {"name", "id", "staticReticles", "strobe", "blinkTypes", "defaultBlinkType", "backgroundColor", "bg"}) !=
            nullptr)
        {
            return DroppedJsonDocumentKind::Page;
        }
    }
    catch (const json::parse_error& exception)
    {
        if (error != nullptr)
        {
            *error = "Unable to parse dropped JSON file '" + path.string() + "' at byte " +
                     std::to_string(exception.byte) + ".";
        }
        return DroppedJsonDocumentKind::Unknown;
    }
    catch (const json::exception& exception)
    {
        if (error != nullptr)
        {
            *error = "Unable to inspect dropped JSON file '" + path.string() + "': " + exception.what();
        }
        return DroppedJsonDocumentKind::Unknown;
    }

    return DroppedJsonDocumentKind::Unknown;
}

std::string Lowercase(const std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char character : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return lowered;
}
}

void EditorApplication::DeleteSelection()
{
    if (documentState_.selection.kind == SelectionKind::Page)
    {
        OpenPageManagementPopup(PageManagementAction::DeleteAsset, documentState_.selection.pageIndex);
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageReticle)
    {
        mfd::PageDefinition* page = ActivePage();
        const std::vector<int> selectedIndices = SelectedPageReticleIndices();
        if (page == nullptr || selectedIndices.empty())
        {
            RebuildStatus("No page reticle selected to delete.", true);
            return;
        }

        PushUndoSnapshot();
        std::vector<int> descendingIndices = selectedIndices;
        std::sort(descendingIndices.begin(), descendingIndices.end(), std::greater<int>());

        std::vector<std::string> removedReticleIds;
        removedReticleIds.reserve(descendingIndices.size());
        for (const int reticleIndex : descendingIndices)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            removedReticleIds.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)].id);
            page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
        }

        SelectPage(documentState_.selection.pageIndex);

        if (removedReticleIds.size() == 1U)
        {
            RebuildStatus("Reticle '" + removedReticleIds.front() + "' removed from page '" + page->name + "'.", false);
        }
        else
        {
            RebuildStatus(
                std::to_string(removedReticleIds.size()) + " reticles removed from page '" + page->name + "'.",
                false);
        }
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageTitle)
    {
        RebuildStatus("The page title is part of the page chrome. Hide it instead of deleting it.", true);
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        mfd::PageDefinition* page = ActivePage();
        mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
        if (page == nullptr || strobe == nullptr)
        {
            RebuildStatus("No page strobe selected to delete.", true);
            return;
        }

        PushUndoSnapshot();
        const std::string removedStrobeId = strobe->reticle.id;
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
        RebuildStatus("Strobe '" + removedStrobeId + "' removed from page '" + page->name + "'.", false);
        return;
    }

    if (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive)
    {
        DeleteSelectedLibraryReticle();
        return;
    }

    RebuildStatus("Select a page, a page reticle, the page strobe, or a library reticle to delete it.", true);
}

void EditorApplication::HandleDroppedFiles()
{
    if (!IsFileDropped())
    {
        return;
    }

    const FilePathList droppedFiles = LoadDroppedFiles();
    std::optional<std::filesystem::path> importedPageFile;
    for (unsigned int index = 0; index < droppedFiles.count; ++index)
    {
        const std::filesystem::path candidate = std::filesystem::path(droppedFiles.paths[index]).lexically_normal();
        if (Lowercase(candidate.extension().string()) == ".json")
        {
            importedPageFile = candidate;
            break;
        }
    }

    if (!importedPageFile.has_value())
    {
        RebuildStatus("Drop one page JSON file to open the import workflow.", true);
        UnloadDroppedFiles(droppedFiles);
        return;
    }

    if (!HasOpenWindow())
    {
        std::string inspectError;
        switch (ClassifyDroppedJsonDocument(*importedPageFile, &inspectError))
        {
        case DroppedJsonDocumentKind::Window:
            LoadWindowConfiguration(*importedPageFile);
            UnloadDroppedFiles(droppedFiles);
            return;
        case DroppedJsonDocumentKind::Page:
            RebuildStatus("Open or create one window before importing a page asset.", true);
            break;
        case DroppedJsonDocumentKind::Unknown:
        default:
            if (!inspectError.empty())
            {
                RebuildStatus(inspectError, true);
            }
            else
            {
                RebuildStatus("Drop one window JSON file or open a window before importing page assets.", true);
            }
            break;
        }

        UnloadDroppedFiles(droppedFiles);
        return;
    }

    OpenPageImportPopup(*importedPageFile);
    if (droppedFiles.count > 1U)
    {
        RebuildStatus("Opened the import workflow for the first dropped JSON file.", false);
    }
    UnloadDroppedFiles(droppedFiles);
}

void EditorApplication::DeleteSelectedLibraryReticle()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        RebuildStatus("No library reticle selected to delete.", true);
        return;
    }

    const std::string reticleId = reticle->id;
    for (const auto& page : documentState_.loaded.document.pages)
    {
        for (const auto& pageReticle : page.staticReticles)
        {
            if (pageReticle.sourceTemplateId == reticleId)
            {
                RebuildStatus("Cannot delete library reticle '" + reticleId +
                                  "' because page '" + page.name + "' still uses it.",
                              true);
                return;
            }
        }

        if (std::any_of(page.strobes.begin(),
                        page.strobes.end(),
                        [&reticleId](const mfd::PageStrobeDefinition& strobe)
                        {
                            return strobe.reticle.sourceTemplateId == reticleId;
                        }))
        {
            RebuildStatus("Cannot delete library reticle '" + reticleId +
                              "' because one strobe on page '" + page.name + "' still uses it.",
                          true);
            return;
        }

        if (std::any_of(page.dynamicReticleBindings.begin(),
                        page.dynamicReticleBindings.end(),
                        [&reticleId](const mfd::DynamicReticleLayerBinding& binding)
                        {
                            return mfd::PageNamesEqual(binding.templateId, reticleId);
                        }))
        {
            RebuildStatus("Cannot delete library reticle '" + reticleId +
                              "' because page '" + page.name +
                              "' exposes it as one generated dynamic template.",
                          true);
            return;
        }
    }

    PushUndoSnapshot();

    if (const auto fileIt = documentState_.files.templateFiles.find(reticleId); fileIt != documentState_.files.templateFiles.end())
    {
        documentState_.files.removedTemplateFiles.push_back(fileIt->second.lexically_normal());
        documentState_.files.templateFiles.erase(fileIt);
    }

    documentState_.loaded.document.reticleLibrary.erase(reticleId);

    if (documentState_.loaded.document.reticleLibrary.empty())
    {
        SelectPage(std::clamp(documentState_.selection.pageIndex,
                              0,
                              std::max(0, static_cast<int>(documentState_.loaded.document.pages.size()) - 1)));
        RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
        return;
    }

    std::vector<std::string> remainingTemplateIds;
    remainingTemplateIds.reserve(documentState_.loaded.document.reticleLibrary.size());
    for (const auto& entry : documentState_.loaded.document.reticleLibrary)
    {
        remainingTemplateIds.push_back(entry.first);
    }
    std::sort(remainingTemplateIds.begin(), remainingTemplateIds.end());
    SelectLibraryReticle(remainingTemplateIds.front());
    RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
}

void EditorApplication::DrawPageTree()
{
    const editor::ReticleUsageHighlightResult* usageHighlight =
        layoutState_.pagePreviewViewOptions.highlightReticleUsages ? ResolveReticleUsageHighlight() : nullptr;

    const std::string_view sidebarFilter(layoutState_.sidebarFilter.data());
    const bool filterActive = !sidebarFilter.empty() && !tutorial_->IsCoachVisible();

    for (int pageIndex = 0; pageIndex < static_cast<int>(documentState_.loaded.document.pages.size()); ++pageIndex)
    {
        const auto& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
        if (filterActive &&
            !ContainsCaseInsensitive(page.name, sidebarFilter) &&
            !ContainsCaseInsensitive(page.title, sidebarFilter))
        {
            continue;
        }

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (documentState_.selection.kind == SelectionKind::Page && documentState_.selection.pageIndex == pageIndex)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string pageLabel = page.title.empty() ? page.name : page.title;
        const bool pageUsesSelectedReticle =
            usageHighlight != nullptr &&
            std::any_of(usageHighlight->pages.begin(),
                        usageHighlight->pages.end(),
                        [pageIndex](const editor::ReticleUsageHighlightPage& usagePage)
                        {
                            return usagePage.currentPageIndex == pageIndex;
                        });
        if (pageUsesSelectedReticle)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.38f, 1.0f));
        }
        const bool open = ImGui::TreeNodeEx((pageLabel + "##page_" + std::to_string(pageIndex)).c_str(), flags);
        if (pageUsesSelectedReticle)
        {
            ImGui::PopStyleColor();
        }
        ShowItemTooltip("Click to focus the page inspector. Use the arrow or double-click to expand its reticles.");
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            SelectPage(pageIndex);
        }

        if (ImGui::BeginPopupContextItem(("PageContextMenu##" + std::to_string(pageIndex)).c_str()))
        {
            if (ImGui::MenuItem("Rename page globally..."))
            {
                SelectPage(pageIndex);
                OpenPageRenamePopup(pageIndex);
            }

            if (ImGui::MenuItem("Remove page from window"))
            {
                SelectPage(pageIndex);
                OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, pageIndex);
            }

            if (ImGui::MenuItem("Delete page asset..."))
            {
                SelectPage(pageIndex);
                OpenPageManagementPopup(PageManagementAction::DeleteAsset, pageIndex);
            }

            ImGui::EndPopup();
        }

        if (open)
        {
            ImGui::TextDisabled("file: %s", pageIndex < static_cast<int>(documentState_.files.pageFiles.size())
                                              ? documentState_.files.pageFiles[static_cast<std::size_t>(pageIndex)].filename().string().c_str()
                                              : "<missing>");

            {
                ImGuiTreeNodeFlags leafFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (documentState_.selection.kind == SelectionKind::PageTitle && documentState_.selection.pageIndex == pageIndex)
                {
                    leafFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const std::string titleLabel = "title chrome##title_" + std::to_string(pageIndex);
                ImGui::TreeNodeEx(titleLabel.c_str(), leafFlags);
                const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(page);
                DrawReticleHoverPreviewTooltip(
                    titleReticle,
                    "Page title",
                    Color {page.backgroundColor.r, page.backgroundColor.g, page.backgroundColor.b, page.backgroundColor.a});
                if (ImGui::IsItemClicked())
                {
                    SelectPageTitle(pageIndex);
                    if (page.name == "Page1" && tutorial_->MatchesTarget("page_select_title_chrome"))
                    {
                        tutorial_->CompleteStep();
                    }
                }

                ImGui::SameLine();
                ImGui::TextDisabled("[%s%s]",
                                    PageTitleDecorationLabel(page.titleDisplay.decoration),
                                    page.titleDisplay.visible ? "" : " hidden");
            }

            for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
            {
                const auto& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
                const bool reticleSelectable = IsPageReticleSelectableInCurrentFocus(page, reticle);
                ImGuiTreeNodeFlags leafFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (HasSelectedPageReticle(pageIndex, reticleIndex))
                {
                    leafFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const std::string reticleLabel =
                    (reticle.id.empty() ? "reticle" : reticle.id) + "##reticle_" +
                    std::to_string(pageIndex) + "_" + std::to_string(reticleIndex);
                ImGui::BeginDisabled(!reticleSelectable);
                ImGui::TreeNodeEx(reticleLabel.c_str(), leafFlags);
                DrawReticleHoverPreviewTooltip(
                    reticle,
                    std::string("Page reticle: ") + (reticle.id.empty() ? "reticle" : reticle.id),
                    Color {page.backgroundColor.r, page.backgroundColor.g, page.backgroundColor.b, page.backgroundColor.a});
                if (reticleSelectable && ImGui::IsItemClicked())
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        TogglePageReticleSelection(pageIndex, reticleIndex);
                    }
                    else
                    {
                        SelectPageReticle(pageIndex, reticleIndex);
                    }
                }

                if (!reticle.layerId.empty())
                {
                    ImGui::SameLine();
                    const bool layerVisible = IsReticleVisibleInEditor(page, reticle);
                    ImGui::TextDisabled("[%s%s]",
                                        reticle.layerId.c_str(),
                                        layerVisible ? "" : " hidden");
                }
                ImGui::EndDisabled();
            }

            for (std::size_t strobeIndex = 0; strobeIndex < page.strobes.size(); ++strobeIndex)
            {
                const mfd::PageStrobeDefinition& strobe = page.strobes[strobeIndex];
                const mfd::ReticleGroup& strobeReticle = strobe.reticle;
                const bool strobeSelectable = true;
                ImGuiTreeNodeFlags leafFlags =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (documentState_.selection.kind == SelectionKind::PageStrobe &&
                    documentState_.selection.pageIndex == pageIndex &&
                    documentState_.selection.pageReticleIndex == static_cast<int>(strobeIndex))
                {
                    leafFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const std::string strobeLabel =
                    "strobe: " + PageStrobeDisplayLabel(page, strobe, strobeIndex) +
                    "##strobe_" + std::to_string(pageIndex) + "_" + std::to_string(strobeIndex);
                ImGui::BeginDisabled(!strobeSelectable);
                ImGui::TreeNodeEx(strobeLabel.c_str(), leafFlags);
                DrawReticleHoverPreviewTooltip(
                    strobeReticle,
                    std::string("Page strobe: ") + PageStrobeDisplayLabel(page, strobe, strobeIndex),
                    Color {page.backgroundColor.r, page.backgroundColor.g, page.backgroundColor.b, page.backgroundColor.a});
                if (strobeSelectable && ImGui::IsItemClicked())
                {
                    SelectPageStrobe(pageIndex, static_cast<int>(strobeIndex));
                }
                ImGui::EndDisabled();
            }

            ImGui::TreePop();
        }
    }
}

void EditorApplication::DrawLibraryTree()
{
    const std::string_view sidebarFilter(layoutState_.sidebarFilter.data());
    const bool filterActive = !sidebarFilter.empty() && !tutorial_->IsCoachVisible();

    std::vector<std::string> templateIds;
    templateIds.reserve(documentState_.loaded.document.reticleLibrary.size());
    for (const auto& entry : documentState_.loaded.document.reticleLibrary)
    {
        if (filterActive && !ContainsCaseInsensitive(entry.first, sidebarFilter))
        {
            continue;
        }

        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());

    if (templateIds.empty())
    {
        ImGui::TextDisabled(filterActive ? "No reticle matches the filter." : "No library reticle yet.");
        return;
    }

    for (const auto& templateId : templateIds)
    {
        const bool selected =
            documentState_.selection.libraryBrowserReticleId == templateId ||
            (((documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive) &&
              documentState_.selection.libraryReticleId == templateId) &&
             documentState_.selection.libraryBrowserReticleId.empty());
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx((templateId + "##library").c_str(), flags);
        const auto libraryIt = documentState_.loaded.document.reticleLibrary.find(templateId);
        if (libraryIt != documentState_.loaded.document.reticleLibrary.end())
        {
            DrawReticleHoverPreviewTooltip(
                libraryIt->second,
                std::string("Library reticle: ") + templateId,
                Color {10, 18, 24, 255});
        }
        if (ImGui::IsItemClicked())
        {
            documentState_.selection.libraryBrowserReticleId = templateId;
            if (documentState_.selection.kind != SelectionKind::LibraryReticle && documentState_.selection.kind != SelectionKind::LibraryPrimitive)
            {
                documentState_.selection.libraryReticleId = templateId;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                SelectLibraryReticle(templateId);
            }
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("MFD_LIBRARY_RETICLE", templateId.c_str(), templateId.size() + 1);
            ImGui::Text("Drop '%s' on the page", templateId.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem(("LibraryReticleContextMenu##" + templateId).c_str()))
        {
            if (ImGui::MenuItem("Copy reticle", "Ctrl+C"))
            {
                SelectLibraryReticle(templateId);
                CopySelectedLibraryReticle();
            }

            if (ImGui::MenuItem("Paste copied reticle", "Ctrl+V", false, clipboardState_.libraryReticleClipboard.has_value()))
            {
                SelectLibraryReticle(templateId, false);
                PasteCopiedLibraryReticle();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Rename reticle globally..."))
            {
                SelectLibraryReticle(templateId);
                OpenReticleRenamePopup(templateId);
            }

            if (ImGui::MenuItem("Delete library reticle"))
            {
                SelectLibraryReticle(templateId);
                DeleteSelectedLibraryReticle();
            }

            ImGui::EndPopup();
        }
    }
}
