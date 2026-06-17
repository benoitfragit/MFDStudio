/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Modal popup drawing and reticle/strobe/layer id generation extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "EditorAssetPathService.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "internal/application/EditorApplicationInternal.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::ShowItemTooltip;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::kPrimitiveTypes;
using editor::app::PrimitiveTypeLabel;

bool ReticleIdExistsExact(const std::vector<mfd::ReticleGroup>& groups, const std::string_view id)
{
    return std::any_of(groups.begin(),
                       groups.end(),
                       [id](const mfd::ReticleGroup& reticle)
                       {
                           return reticle.id == id;
                       });
}

bool PageLayerIdExistsNormalized(const mfd::PageDefinition& page, const std::string_view id)
{
    const std::string normalizedId = mfd::NormalizePageName(id);
    return std::any_of(page.layers.begin(),
                       page.layers.end(),
                       [&normalizedId](const mfd::PageLayerDefinition& layer)
                       {
                           return mfd::NormalizePageName(layer.id) == normalizedId;
                       });
}

std::size_t RemapStrobePrimitiveOverridesById(const mfd::ReticleGroup& previousReticle, mfd::ReticleGroup* nextReticle)
{
    if (nextReticle == nullptr)
    {
        return 0;
    }

    std::size_t unmappedEditablePrimitiveCount = 0;
    for (const mfd::Primitive& previousPrimitive : previousReticle.primitives)
    {
        if (previousPrimitive.id.empty())
        {
            continue;
        }

        mfd::Primitive* nextPrimitive = mfd::FindPrimitive(*nextReticle, previousPrimitive.id);
        if (nextPrimitive == nullptr || nextPrimitive->type != previousPrimitive.type)
        {
            if (std::holds_alternative<mfd::TextGeometry>(previousPrimitive.geometry) ||
                std::holds_alternative<mfd::TimeGeometry>(previousPrimitive.geometry))
            {
                ++unmappedEditablePrimitiveCount;
            }
            continue;
        }

        nextPrimitive->style = previousPrimitive.style;
        if (const auto* previousText = std::get_if<mfd::TextGeometry>(&previousPrimitive.geometry))
        {
            auto* nextText = std::get_if<mfd::TextGeometry>(&nextPrimitive->geometry);
            if (nextText == nullptr)
            {
                ++unmappedEditablePrimitiveCount;
                continue;
            }

            nextText->text = previousText->text;
            nextText->letterSpacing = previousText->letterSpacing;
            continue;
        }

        if (const auto* previousTime = std::get_if<mfd::TimeGeometry>(&previousPrimitive.geometry))
        {
            auto* nextTime = std::get_if<mfd::TimeGeometry>(&nextPrimitive->geometry);
            if (nextTime == nullptr)
            {
                ++unmappedEditablePrimitiveCount;
                continue;
            }

            nextTime->format = previousTime->format;
            nextTime->utc = previousTime->utc;
            nextTime->letterSpacing = previousTime->letterSpacing;
        }
    }

    return unmappedEditablePrimitiveCount;
}
}

void EditorApplication::DrawPopups()
{
    using editor::tutorial::TutorialStepId;

    if (workflowState_.recoveryPromptPending && !ImGui::IsPopupOpen("Recover session##recovery"))
    {
        ImGui::OpenPopup("Recover session##recovery");
    }
    if (ImGui::BeginPopupModal("Recover session##recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Unsaved work from a previous session was found.");
        ImGui::TextDisabled("Recover it, or discard the recovery snapshot.");
        ImGui::Separator();
        if (AccentButton("Recover"))
        {
            workflowState_.recoveryPromptPending = false;
            ImGui::CloseCurrentPopup();
            RecoverPreviousSession();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            workflowState_.recoveryPromptPending = false;
            ClearRecoverySnapshot();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (workflowState_.unsavedExitRequested && !ImGui::IsPopupOpen("Unsaved changes##exit"))
    {
        ImGui::OpenPopup("Unsaved changes##exit");
    }
    if (ImGui::BeginPopupModal("Unsaved changes##exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("This window has unsaved changes.");
        ImGui::TextDisabled("Save before quitting, or quit and discard the changes.");
        ImGui::Separator();
        if (AccentButton("Save and quit"))
        {
            if (SaveAll())
            {
                workflowState_.unsavedExitRequested = false;
                workflowState_.exitConfirmed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit without saving"))
        {
            workflowState_.unsavedExitRequested = false;
            workflowState_.exitConfirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (workflowState_.reloadConfirmRequested && !ImGui::IsPopupOpen("Unsaved changes##reload"))
    {
        ImGui::OpenPopup("Unsaved changes##reload");
    }
    if (ImGui::BeginPopupModal("Unsaved changes##reload", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Reloading discards unsaved editor changes.");
        ImGui::Separator();
        if (AccentButton("Save and reload"))
        {
            const bool saved = SaveAll();
            workflowState_.reloadConfirmRequested = false;
            ImGui::CloseCurrentPopup();
            if (saved)
            {
                LoadWindowConfiguration(documentState_.windowFile);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard and reload"))
        {
            workflowState_.reloadConfirmRequested = false;
            ImGui::CloseCurrentPopup();
            LoadWindowConfiguration(documentState_.windowFile);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            workflowState_.reloadConfirmRequested = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (workflowState_.externalReloadPromptPending && !ImGui::IsPopupOpen("Files changed##external"))
    {
        ImGui::OpenPopup("Files changed##external");
    }
    if (ImGui::BeginPopupModal("Files changed##external", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Authored files were changed, removed, or renamed on disk outside the editor.");
        ImGui::TextDisabled(workflowState_.documentDirty
                                ? "Reloading discards your unsaved editor changes."
                                : "Reload to pick up the external changes.");
        ImGui::Separator();
        if (AccentButton("Reload from disk"))
        {
            workflowState_.externalReloadPromptPending = false;
            ImGui::CloseCurrentPopup();
            LoadWindowConfiguration(documentState_.windowFile);
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep editing"))
        {
            workflowState_.externalReloadPromptPending = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (workflowState_.showNewPagePopup)
    {
        ImGui::OpenPopup("Create new page");
        workflowState_.showNewPagePopup = false;
    }
    if (workflowState_.showNewWindowPopup)
    {
        ImGui::OpenPopup("Create new window");
        workflowState_.showNewWindowPopup = false;
    }
    if (workflowState_.showNewLibraryReticlePopup)
    {
        ImGui::OpenPopup("Create new library reticle");
        workflowState_.showNewLibraryReticlePopup = false;
    }

    if (workflowState_.showDuplicateLibraryReticlePopup)
    {
        ImGui::OpenPopup("Duplicate library reticle");
        workflowState_.showDuplicateLibraryReticlePopup = false;
    }

    if (workflowState_.pageImportPopup.openRequested)
    {
        ImGui::OpenPopup("Import page");
        workflowState_.pageImportPopup.openRequested = false;
    }

    if (workflowState_.pageRenamePopup.openRequested)
    {
        ImGui::OpenPopup("Rename page globally");
        workflowState_.pageRenamePopup.openRequested = false;
    }

    if (workflowState_.reticleRenamePopup.openRequested)
    {
        ImGui::OpenPopup("Rename reticle globally");
        workflowState_.reticleRenamePopup.openRequested = false;
    }

    if (workflowState_.reticleExtractionPopup.openRequested)
    {
        ImGui::OpenPopup("Extract as reticle");
        workflowState_.reticleExtractionPopup.openRequested = false;
    }

    if (workflowState_.designExportPopup.openRequested)
    {
        ImGui::OpenPopup("Export design");
        workflowState_.designExportPopup.openRequested = false;
    }

    if (workflowState_.pageManagementPopup.openRequested)
    {
        ImGui::OpenPopup("Manage page");
        workflowState_.pageManagementPopup.openRequested = false;
    }

    if (tutorial_->ConsumeResumePopupRequest())
    {
        ImGui::OpenPopup("Tutorial progress");
    }

    if (ImGui::BeginPopupModal("Tutorial progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("A tutorial progress snapshot already exists. Continue where you stopped or restart from scratch?");
        if (AccentButton("Continue"))
        {
            tutorial_->ResumeFromSavedProgress();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart from scratch"))
        {
            tutorial_->RestartFromScratch();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Create new window", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextDisabled("Window file and runtime parameters");
        ImGui::InputText("Window file", workflowState_.newWindowDraft.windowFile.data(), workflowState_.newWindowDraft.windowFile.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse window file..."))
        {
            BrowseNewWindowFile();
        }
        ShowItemTooltip("Choose the new window JSON path with the native Windows file picker.");
        ImGui::InputText("Window title", workflowState_.newWindowDraft.title.data(), workflowState_.newWindowDraft.title.size());
        int windowSize[2] {workflowState_.newWindowDraft.width, workflowState_.newWindowDraft.height};
        if (ImGui::InputInt2("Size (px)", windowSize))
        {
            workflowState_.newWindowDraft.width = windowSize[0];
            workflowState_.newWindowDraft.height = windowSize[1];
        }
        int windowPosition[2] {workflowState_.newWindowDraft.positionX, workflowState_.newWindowDraft.positionY};
        if (ImGui::InputInt2("Position (px)", windowPosition))
        {
            workflowState_.newWindowDraft.positionX = windowPosition[0];
            workflowState_.newWindowDraft.positionY = windowPosition[1];
        }
        ImGui::InputText("Font file (optional)", workflowState_.newWindowDraft.fontFile.data(), workflowState_.newWindowDraft.fontFile.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse font file..."))
        {
            BrowseNewWindowFontFile();
        }
        ShowItemTooltip("Choose an existing .ttf or .otf font file with the native Windows file picker.");
        ImGui::InputText("Reticle library folder", workflowState_.newWindowDraft.reticleLibraryFolder.data(), workflowState_.newWindowDraft.reticleLibraryFolder.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse reticle folder..."))
        {
            BrowseNewWindowReticleLibraryFolder();
        }
        ShowItemTooltip("Choose where new reticle template JSON files should be saved with the native Windows folder picker.");

        ImGui::SeparatorText("Commands UDP (incoming)");
        ImGui::Checkbox("Expose command UDP", &workflowState_.newWindowDraft.commandUdpExposed);
        if (workflowState_.newWindowDraft.commandUdpExposed)
        {
            ImGui::Checkbox("Enable command UDP", &workflowState_.newWindowDraft.commandUdpEnabled);
            ImGui::InputText("Command address", workflowState_.newWindowDraft.commandAddress.data(), workflowState_.newWindowDraft.commandAddress.size());
            ImGui::InputInt("Command port", &workflowState_.newWindowDraft.commandPort);
            ImGui::InputInt("Command max packet", &workflowState_.newWindowDraft.commandMaxPacketSize);
        }

        ImGui::SeparatorText("Feedback UDP (outgoing)");
        ImGui::Checkbox("Expose feedback UDP", &workflowState_.newWindowDraft.feedbackUdpExposed);
        if (workflowState_.newWindowDraft.feedbackUdpExposed)
        {
            ImGui::Checkbox("Enable feedback UDP", &workflowState_.newWindowDraft.feedbackUdpEnabled);
            ImGui::InputText("Feedback address", workflowState_.newWindowDraft.feedbackAddress.data(), workflowState_.newWindowDraft.feedbackAddress.size());
            ImGui::InputInt("Feedback port", &workflowState_.newWindowDraft.feedbackPort);
            ImGui::InputInt("Feedback max packet", &workflowState_.newWindowDraft.feedbackMaxPacketSize);
            if (ImGui::DragFloat("Fast interval",
                                 &workflowState_.newWindowDraft.feedbackFastIntervalSeconds,
                                 0.001f,
                                 0.001f,
                                 10.0f,
                                 "%.3f s"))
            {
                workflowState_.newWindowDraft.feedbackFastIntervalSeconds =
                    ClampFeedbackFastIntervalSeconds(workflowState_.newWindowDraft.feedbackFastIntervalSeconds);
                workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds =
                    ClampFeedbackHeartbeatIntervalSeconds(
                        workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds,
                        workflowState_.newWindowDraft.feedbackFastIntervalSeconds);
            }
            ShowItemTooltip("Minimum cadence used when the active-page feedback state changes.");
            if (ImGui::DragFloat("Heartbeat interval",
                                 &workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds,
                                 0.001f,
                                 workflowState_.newWindowDraft.feedbackFastIntervalSeconds,
                                 10.0f,
                                 "%.3f s"))
            {
                workflowState_.newWindowDraft.feedbackFastIntervalSeconds =
                    ClampFeedbackFastIntervalSeconds(workflowState_.newWindowDraft.feedbackFastIntervalSeconds);
                workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds =
                    ClampFeedbackHeartbeatIntervalSeconds(
                        workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds,
                        workflowState_.newWindowDraft.feedbackFastIntervalSeconds);
            }
            ShowItemTooltip("Minimum cadence used for unchanged active-page heartbeat snapshots.");
        }

        ImGui::SeparatorText("Initial content");
        ImGui::Checkbox("Create one initial page", &workflowState_.newWindowDraft.createInitialPage);
        if (workflowState_.newWindowDraft.createInitialPage)
        {
            ImGui::InputText("First page name", workflowState_.newWindowDraft.firstPageName.data(), workflowState_.newWindowDraft.firstPageName.size());
            ImGui::InputText("First page title", workflowState_.newWindowDraft.firstPageTitle.data(), workflowState_.newWindowDraft.firstPageTitle.size());
            ImGui::InputText("First page file", workflowState_.newWindowDraft.firstPageFile.data(), workflowState_.newWindowDraft.firstPageFile.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse page file..."))
            {
                BrowseNewWindowFirstPageFile();
            }
            ShowItemTooltip("Choose the first page JSON path with the native Windows file picker.");
            ImGui::ColorEdit4("First page background", &workflowState_.newWindowDraft.firstPageBackground.x);
        }

        ImGui::TextDisabled("Use the repo source assets folders, not the staged runtime copy under _Exec.");

        if (AccentButton("Create window"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_window_create");
            if (CreateNewWindow())
            {
                if (tutorialCreateMatched)
                {
                    tutorial_->CompleteStep();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Build a new in-memory window definition, optionally with one page, then use Save to write JSON files.");
        tutorial_->DrawHalo(
            "popup_window_create",
            "Click Create window",
            "Commit the tutorial window using the prefilled authoring values shown in this dialog.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateWindow)))
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Create new page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Page name", workflowState_.newPageDraft.name.data(), workflowState_.newPageDraft.name.size());
        ShowItemTooltip("Internal page id used in JSON, generated file names and API references.");
        ImGui::InputText("Title", workflowState_.newPageDraft.title.data(), workflowState_.newPageDraft.title.size());
        ShowItemTooltip("Optional human-readable title shown in the editor and runtime UI.");
        ImGui::InputText("File", workflowState_.newPageDraft.fileName.data(), workflowState_.newPageDraft.fileName.size());
        ShowItemTooltip("JSON file path written for this page. The .json extension is added automatically when missing.");
        ImGui::SameLine();
        if (ImGui::Button("Browse page file..."))
        {
            BrowseNewPageFile();
        }
        ShowItemTooltip("Choose the page JSON path with the native Windows file picker.");
        ImGui::ColorEdit4("Background", &workflowState_.newPageDraft.background.x);
        ShowItemTooltip("Initial page background color.");
        ImGui::TextDisabled("Use the repo source assets folders, not the staged runtime copy under _Exec.");

        if (AccentButton("Create page"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_page_create");
            if (CreateNewPage())
            {
                if (tutorialCreateMatched)
                {
                    tutorial_->CompleteStep();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Create the new page and add it to the current window.");
        tutorial_->DrawHalo(
            "popup_page_create",
            "Click Create page",
            "Commit the tutorial page so the walkthrough can move to the next authoring action.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage1)) ||
                tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage2)))
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without creating a page.");
        ImGui::EndPopup();
    }

    DrawPageImportPopup();
    DrawPageRenamePopup();
    DrawReticleRenamePopup();
    DrawReticleExtractionPopup();
    DrawDesignExportPopup();
    DrawPageManagementPopup();

    if (ImGui::BeginPopupModal("Create new library reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Reticle id", workflowState_.newLibraryReticleDraft.id.data(), workflowState_.newLibraryReticleDraft.id.size());
        ShowItemTooltip("Template id used by the shared reticle library.");
        if (ImGui::BeginCombo("Primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)]).c_str()))
        {
            for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
            {
                const bool selected = workflowState_.newLibraryReticleDraft.primitiveTypeIndex == index;
                if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(), selected))
                {
                    workflowState_.newLibraryReticleDraft.primitiveTypeIndex = index;
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose the first primitive that will seed the new reticle template.");

        if (AccentButton("Create reticle"))
        {
            const bool tutorialCreateMatched = tutorial_->MatchesTarget("popup_reticle_create");
            if (CreateNewLibraryReticleFromPrimitive())
            {
                if (tutorialCreateMatched)
                {
                    if (tutorial_->ShouldAdvanceReticleCreatePhase())
                    {
                        tutorial_->AdvancePhase();
                    }
                    else
                    {
                        tutorial_->CompleteStep();
                    }
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ShowItemTooltip("Create the new library reticle and open it in the reticle studio.");
        tutorial_->DrawHalo(
            "popup_reticle_create",
            "Click Create reticle",
            "Create the tutorial template currently prepared in this dialog.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            if (tutorial_->ShouldResetReticleCreatePopupOnCancel())
            {
                tutorial_->ResetPhase();
            }
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without creating a reticle.");
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Duplicate library reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("New reticle id", workflowState_.duplicateLibraryReticleDraft.id.data(), workflowState_.duplicateLibraryReticleDraft.id.size());
        ShowItemTooltip("New template id for the duplicated library reticle.");
        if (AccentButton("Duplicate"))
        {
            DuplicateSelectedLibraryReticle();
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Create a full copy of the selected library reticle under the new id.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ShowItemTooltip("Close this dialog without duplicating the reticle.");
        ImGui::EndPopup();
    }
}

void EditorApplication::SeedNewWindowAssetDraftPaths()
{
    const std::filesystem::path windowFile = std::filesystem::path(workflowState_.newWindowDraft.windowFile.data()).lexically_normal();
    if (windowFile.empty() || !windowFile.is_absolute() || editor::EditorAssetPathService::IsExecStagingPath(windowFile))
    {
        CopyTextBuffer(workflowState_.newWindowDraft.windowFile, documentState_.assetPaths.DefaultAssetPath("assets/windows/new_window.json").string());
    }

    const std::filesystem::path resolvedWindowFile = std::filesystem::path(workflowState_.newWindowDraft.windowFile.data()).lexically_normal();
    const std::filesystem::path reticleFolder = std::filesystem::path(workflowState_.newWindowDraft.reticleLibraryFolder.data()).lexically_normal();
    if (reticleFolder.empty() || !reticleFolder.is_absolute() || editor::EditorAssetPathService::IsExecStagingPath(reticleFolder))
    {
        CopyTextBuffer(workflowState_.newWindowDraft.reticleLibraryFolder, documentState_.assetPaths.DefaultSiblingAssetFile(resolvedWindowFile, "reticles", "").string());
    }

    const std::filesystem::path firstPageFile = std::filesystem::path(workflowState_.newWindowDraft.firstPageFile.data()).lexically_normal();
    if (firstPageFile.empty() || !firstPageFile.is_absolute() || editor::EditorAssetPathService::IsExecStagingPath(firstPageFile))
    {
        CopyTextBuffer(workflowState_.newWindowDraft.firstPageFile, documentState_.assetPaths.DefaultSiblingAssetFile(resolvedWindowFile, "pages", "page1.json").string());
    }
}

void EditorApplication::SeedNewPageAssetDraftPath()
{
    const std::filesystem::path pageFile = std::filesystem::path(workflowState_.newPageDraft.fileName.data()).lexically_normal();
    if (!pageFile.empty() && pageFile.is_absolute() && !editor::EditorAssetPathService::IsExecStagingPath(pageFile))
    {
        return;
    }

    const std::filesystem::path windowFile = documentState_.loaded.window.sourceFile.empty()
                                                 ? documentState_.assetPaths.DefaultAssetPath("assets/windows/new_window.json")
                                                 : documentState_.loaded.window.sourceFile;
    const std::filesystem::path defaultFileName =
        editor::EditorAssetPathService::JsonFileNameOrFallback(pageFile, "new_page.json");
    CopyTextBuffer(workflowState_.newPageDraft.fileName, documentState_.assetPaths.DefaultSiblingAssetFile(windowFile, "pages", defaultFileName.string()).string());
}

mfd::PageStrobeDefinition EditorApplication::MakePageStrobeFromTemplate(
    const mfd::PageDefinition& page,
    const mfd::ReticleGroup& templ,
    const std::optional<mfd::PageStrobeDefinition>& previousStrobe,
    std::size_t* unmappedPrimitiveOverrideCount)
{
    if (unmappedPrimitiveOverrideCount != nullptr)
    {
        *unmappedPrimitiveOverrideCount = 0;
    }

    const std::string baseId =
        previousStrobe.has_value() && !previousStrobe->reticle.id.empty()
            ? previousStrobe->reticle.id
            : (templ.id.empty() ? std::string {"strobe"} : templ.id + "_strobe");

    mfd::PageStrobeDefinition strobe;
    const std::string baseName =
        previousStrobe.has_value() && !previousStrobe->name.empty()
            ? previousStrobe->name
            : (templ.id.empty() ? std::string {"Strobe"} : templ.id);
    strobe.name = MakeUniqueStrobeName(
        page,
        baseName,
        previousStrobe.has_value() ? std::string_view {previousStrobe->name} : std::string_view {});
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle = mfd::InstantiateReticle(
        templ,
        MakeUniquePageReticleId(
            page,
            baseId,
            {},
            previousStrobe.has_value() ? std::string_view {previousStrobe->reticle.id} : std::string_view {}));
    strobe.reticle.visible = true;
    strobe.reticle.drawOnTop = true;

    if (previousStrobe.has_value())
    {
        strobe.name = previousStrobe->name;
        strobe.normalizedName = previousStrobe->normalizedName;
        strobe.reticle.visible = previousStrobe->reticle.visible;
        strobe.reticle.transform = previousStrobe->reticle.transform;
        strobe.reticle.overrides = previousStrobe->reticle.overrides;
        strobe.reticle.blink = previousStrobe->reticle.blink;
        strobe.reticle.clipping = previousStrobe->reticle.clipping;
        strobe.capture = previousStrobe->capture;
        strobe.magnet = previousStrobe->magnet;
        const std::size_t unmappedCount = RemapStrobePrimitiveOverridesById(previousStrobe->reticle, &strobe.reticle);
        if (unmappedPrimitiveOverrideCount != nullptr)
        {
            *unmappedPrimitiveOverrideCount = unmappedCount;
        }
    }

    strobe.reticle.drawOnTop = true;

    return strobe;
}

std::string EditorApplication::MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, const std::string_view baseId)
{
    std::string candidate = std::string(baseId);
    int suffix = 1;

    while (ReticleIdExistsExact(groups, candidate))
    {
        candidate = std::string(baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

std::string EditorApplication::MakeUniquePageReticleId(const mfd::PageDefinition& page,
                                                       const std::string_view baseId,
                                                       const std::string_view excludedReticleId,
                                                       const std::string_view excludedStrobeId)
{
    return editor::detail::MakeUniquePageReticleInstanceId(page, baseId, excludedReticleId, excludedStrobeId);
}

std::string EditorApplication::MakeUniqueStrobeName(const mfd::PageDefinition& page,
                                                    const std::string_view baseName,
                                                    const std::string_view excludedStrobeName)
{
    const std::string stableBase =
        mfd::NormalizePageName(baseName).empty() ? std::string {"Strobe"} : std::string(baseName);
    std::string candidate = stableBase;
    int suffix = 2;
    const std::string excludedNormalizedName = mfd::NormalizePageName(excludedStrobeName);

    while (std::any_of(page.strobes.begin(),
                       page.strobes.end(),
                       [&candidate, &excludedNormalizedName](const mfd::PageStrobeDefinition& strobe)
                       {
                           const std::string normalizedStrobeName = mfd::NormalizePageName(strobe.name);
                           if (normalizedStrobeName.empty())
                           {
                               return false;
                           }

                           if (!excludedNormalizedName.empty() && normalizedStrobeName == excludedNormalizedName)
                           {
                               return false;
                           }

                           return normalizedStrobeName == mfd::NormalizePageName(candidate);
                       }))
    {
        candidate = stableBase + "_" + std::to_string(suffix++);
    }

    return candidate;
}

std::string EditorApplication::MakeUniqueLibraryReticleId(const std::string_view baseId) const
{
    return editor::detail::MakeUniqueLibraryReticleId(documentState_.loaded.document.reticleLibrary, baseId);
}

std::string EditorApplication::MakeUniqueLayerId(const mfd::PageDefinition& page, const std::string_view baseId)
{
    std::string candidate = baseId.empty() ? std::string {"layer"} : std::string(baseId);
    int suffix = 2;

    while (PageLayerIdExistsNormalized(page, candidate))
    {
        candidate = std::string(baseId.empty() ? "layer" : baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}
