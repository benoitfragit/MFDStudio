/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Window, page, dynamic-template, strobe, layer and blink inspector implementation extracted from `EditorApplicationInspectors`.
 */

#include <algorithm>
#include <array>
#include <numeric>
#include <string_view>
#include <unordered_map>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "TimeFormatValidation.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::BeginInspectorSection;
using editor::ui::InspectorHelpMarker;
using editor::ui::ShowItemTooltip;
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::kTutorialAircraftTemplateId;
using editor::detail::kTutorialStrobeCursorTemplateId;
using editor::detail::ToColorRgba;
using editor::app::ClearBlinkReferencesForRemovedType;
using editor::app::CountBlinkReferences;
using editor::app::CountDynamicLayerBindings;
using editor::app::CountEditorLayerAssignments;
using editor::app::EffectiveDefaultBlinkTypeIndex;
using editor::app::FindActivePageStrobeIndex;
using editor::app::FindBlinkTypeIndex;
using editor::app::IsReticleVisibleInEditor;
using editor::app::kInvalidBlinkTypeIndex;
using editor::app::MakeUniqueBlinkTypeName;
using editor::app::NextPageDynamicOrderInLayer;
using editor::app::PageHasDynamicTemplateBinding;
using editor::app::PageLayerOrder;
using editor::app::PageStrobeDisplayLabel;
using editor::app::RefreshBlinkBindingForEditor;
using editor::app::RefreshPageBlinkStateForEditor;
using editor::app::RenameBlinkReferences;
using editor::app::RenameEditorLayerReferences;
using editor::app::SetActivePageStrobe;
using editor::app::SuggestPageStrobeDraftName;
using editor::app::SummarizeDynamicLayerBindings;
}

namespace
{
ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

std::string NormalizeEditorIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

int FindPageStrobeIndexByName(const mfd::PageDefinition& page, const std::string_view strobeName) noexcept
{
    const std::string normalizedTarget = NormalizeEditorIdentifier(strobeName);
    if (normalizedTarget.empty())
    {
        return -1;
    }

    for (std::size_t index = 0; index < page.strobes.size(); ++index)
    {
        const mfd::PageStrobeDefinition& strobe = page.strobes[index];
        const std::string normalizedName =
            strobe.normalizedName.empty() ? NormalizeEditorIdentifier(strobe.name) : strobe.normalizedName;
        if (normalizedName == normalizedTarget)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

struct TutorialDynamicTemplateInfo
{
    std::string_view templateId;
    std::string_view preferredLayerId;
    std::string_view targetId;
    const char* label;
    const char* reason;
};

struct DynamicBindingDraftState
{
    std::string templateId;
    std::string layerId;
};

struct StrobeDraftState
{
    std::string name;
    std::string templateId;
};

std::string DescribeRequestedTimeFormat(const std::string_view format)
{
    return format.empty() ? std::string {"<empty>"} : std::string(format);
}

std::string InvalidTimeFormatMessage(const std::string_view format)
{
    return "Rejected invalid time format '" + DescribeRequestedTimeFormat(format) +
           "'. Use complete strftime directives such as %H:%M:%S.";
}
}

void EditorApplication::EditTimeFormatField(const std::string& label,
                                            const char* const tooltip,
                                            mfd::TimeGeometry& geometry)
{
    std::array<char, 128> buffer {};
    CopyTextBuffer(buffer, geometry.format);
    ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
    ShowItemTooltip(tooltip);
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }

    if (!ImGui::IsItemDeactivatedAfterEdit())
    {
        return;
    }

    std::string requestedFormat(buffer.data());
    if (!mfd::detail::IsValidTimeFormatString(requestedFormat))
    {
        RebuildStatus(InvalidTimeFormatMessage(requestedFormat), true);
        return;
    }

    geometry.format = std::move(requestedFormat);
}

void EditorApplication::DrawWindowInspector()
{
    if (!HasOpenWindow())
    {
        ImGui::TextDisabled("No window selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Window");
    ImGui::TextDisabled("Tune the root window asset, transports and runtime feedback cadence.");
    ImGui::TextDisabled("Source file: %s", documentState_.loaded.window.sourceFile.string().c_str());
    ImGui::TextDisabled("Loaded pages: %d", static_cast<int>(documentState_.loaded.document.pages.size()));

    std::array<char, 128> title {};
    std::array<char, kPathTextCapacity> fontFile {};
    std::array<char, kPathTextCapacity> reticleLibraryFolder {};
    CopyTextBuffer(title, documentState_.loaded.window.title);
    CopyTextBuffer(fontFile, documentState_.loaded.window.fontFile.string());
    CopyTextBuffer(reticleLibraryFolder, documentState_.loaded.window.reticleLibraryFolder.string());

    const bool titleChanged = ImGui::InputText("Window title", title.data(), title.size());
    ShowItemTooltip("Human-readable title stored in the root window JSON.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (titleChanged)
    {
        documentState_.loaded.window.title = title.data();
    }

    int windowSize[2] {documentState_.loaded.window.width, documentState_.loaded.window.height};
    const bool sizeChanged = ImGui::InputInt2("Size (px)", windowSize);
    ShowItemTooltip("Initial native window size in pixels.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (sizeChanged)
    {
        documentState_.loaded.window.width = std::max(1, windowSize[0]);
        documentState_.loaded.window.height = std::max(1, windowSize[1]);
    }

    int windowPosition[2] {documentState_.loaded.window.positionX, documentState_.loaded.window.positionY};
    const bool positionChanged = ImGui::InputInt2("Position (px)", windowPosition);
    ShowItemTooltip("Initial native window position in pixels.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        documentState_.loaded.window.positionX = windowPosition[0];
        documentState_.loaded.window.positionY = windowPosition[1];
    }

    int targetFps = documentState_.loaded.window.targetFps;
    const bool targetFpsChanged = ImGui::InputInt("Target FPS", &targetFps);
    ShowItemTooltip("Requested runtime cadence for the host window loop.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (targetFpsChanged)
    {
        documentState_.loaded.window.targetFps = std::max(1, targetFps);
    }

    const bool fontChanged = ImGui::InputText("Font file", fontFile.data(), fontFile.size());
    ShowItemTooltip("Optional font file resolved from the window JSON.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (fontChanged)
    {
        documentState_.loaded.window.fontFile = std::filesystem::path(fontFile.data()).lexically_normal();
        ApplyPreviewFontFile(documentState_.loaded.window.fontFile);
    }

    const bool reticleFolderChanged =
        ImGui::InputText("Reticle library folder", reticleLibraryFolder.data(), reticleLibraryFolder.size());
    ShowItemTooltip("Folder containing the reusable reticle JSON templates referenced by this window.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (reticleFolderChanged)
    {
        documentState_.loaded.window.reticleLibraryFolder = std::filesystem::path(reticleLibraryFolder.data()).lexically_normal();
    }

    ImGui::Spacing();
    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    if (BeginInspectorSection("section_window_commands_udp", "Commands UDP", true, sectionForceOpen))
    {
    bool hasCommandUdp = documentState_.loaded.window.commandTransports.udp.has_value();
    if (ImGui::Checkbox("Expose command UDP", &hasCommandUdp))
    {
        PushUndoSnapshot();
        if (hasCommandUdp)
        {
            documentState_.loaded.window.commandTransports.udp = documentState_.loaded.window.commandTransports.udp.value_or(mfd::WindowUdpCommandTransport {});
        }
        else
        {
            documentState_.loaded.window.commandTransports.udp.reset();
        }
    }
    ShowItemTooltip("Persist one optional UDP command endpoint in the root window JSON.");

    if (documentState_.loaded.window.commandTransports.udp.has_value())
    {
        auto& commandUdp = *documentState_.loaded.window.commandTransports.udp;
        ImGui::Checkbox("Enable command UDP", &commandUdp.enabled);
        ShowItemTooltip("Enable or disable the runtime UDP command listener.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }

        std::array<char, 64> commandAddress {};
        CopyTextBuffer(commandAddress, commandUdp.address);
        const bool commandAddressChanged =
            ImGui::InputText("Command address", commandAddress.data(), commandAddress.size());
        ShowItemTooltip("Numeric IPv4 bind address for incoming command packets.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (commandAddressChanged)
        {
            commandUdp.address = commandAddress.data();
        }

        int commandPort = static_cast<int>(commandUdp.port);
        const bool commandPortChanged = ImGui::InputInt("Command port", &commandPort);
        ShowItemTooltip("UDP port used by the command listener.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (commandPortChanged)
        {
            commandUdp.port = static_cast<std::uint16_t>(
                std::clamp(commandPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        }

        int commandMaxPacketSize = commandUdp.maxPacketSize;
        const bool commandMaxPacketChanged = ImGui::InputInt("Command max packet", &commandMaxPacketSize);
        ShowItemTooltip("Maximum protobuf UDP payload accepted by the command endpoint.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (commandMaxPacketChanged)
        {
            commandUdp.maxPacketSize = std::max(512, commandMaxPacketSize);
        }
    }

    }

    ImGui::Spacing();
    if (BeginInspectorSection("section_window_feedback_udp", "Feedback UDP", true, sectionForceOpen))
    {
    bool hasFeedbackUdp = documentState_.loaded.window.feedbackTransports.udp.has_value();
    if (ImGui::Checkbox("Expose feedback UDP", &hasFeedbackUdp))
    {
        PushUndoSnapshot();
        if (hasFeedbackUdp)
        {
            documentState_.loaded.window.feedbackTransports.udp = documentState_.loaded.window.feedbackTransports.udp.value_or(mfd::WindowUdpFeedbackTransport {});
        }
        else
        {
            documentState_.loaded.window.feedbackTransports.udp.reset();
        }
    }
    ShowItemTooltip("Persist one optional UDP runtime-feedback endpoint in the root window JSON.");

    if (documentState_.loaded.window.feedbackTransports.udp.has_value())
    {
        auto& feedbackUdp = *documentState_.loaded.window.feedbackTransports.udp;
        ImGui::Checkbox("Enable feedback UDP", &feedbackUdp.enabled);
        ShowItemTooltip("Enable or disable the runtime UDP feedback stream.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }

        std::array<char, 64> feedbackAddress {};
        CopyTextBuffer(feedbackAddress, feedbackUdp.address);
        const bool feedbackAddressChanged =
            ImGui::InputText("Feedback address", feedbackAddress.data(), feedbackAddress.size());
        ShowItemTooltip("Numeric IPv4 destination used by the runtime feedback stream.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (feedbackAddressChanged)
        {
            feedbackUdp.address = feedbackAddress.data();
        }

        int feedbackPort = static_cast<int>(feedbackUdp.port);
        const bool feedbackPortChanged = ImGui::InputInt("Feedback port", &feedbackPort);
        ShowItemTooltip("UDP port used by the runtime feedback stream.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (feedbackPortChanged)
        {
            feedbackUdp.port = static_cast<std::uint16_t>(
                std::clamp(feedbackPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        }

        int feedbackMaxPacketSize = feedbackUdp.maxPacketSize;
        const bool feedbackMaxPacketChanged = ImGui::InputInt("Feedback max packet", &feedbackMaxPacketSize);
        ShowItemTooltip("Maximum protobuf UDP payload emitted by the feedback endpoint.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (feedbackMaxPacketChanged)
        {
            feedbackUdp.maxPacketSize = std::max(512, feedbackMaxPacketSize);
        }

        const bool fastIntervalChanged =
            ImGui::DragFloat("Fast interval", &documentState_.loaded.window.feedbackFastIntervalSeconds, 0.001f, 0.001f, 10.0f, "%.3f s");
        ShowItemTooltip("Minimum cadence used when the active-page feedback state changes.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (fastIntervalChanged)
        {
            documentState_.loaded.window.feedbackFastIntervalSeconds =
                ClampFeedbackFastIntervalSeconds(documentState_.loaded.window.feedbackFastIntervalSeconds);
            documentState_.loaded.window.feedbackHeartbeatIntervalSeconds =
                ClampFeedbackHeartbeatIntervalSeconds(
                    documentState_.loaded.window.feedbackHeartbeatIntervalSeconds,
                    documentState_.loaded.window.feedbackFastIntervalSeconds);
        }

        const bool heartbeatIntervalChanged =
            ImGui::DragFloat("Heartbeat interval",
                             &documentState_.loaded.window.feedbackHeartbeatIntervalSeconds,
                             0.001f,
                             documentState_.loaded.window.feedbackFastIntervalSeconds,
                             10.0f,
                             "%.3f s");
        ShowItemTooltip("Minimum cadence used for unchanged active-page heartbeat snapshots.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (heartbeatIntervalChanged)
        {
            documentState_.loaded.window.feedbackFastIntervalSeconds =
                ClampFeedbackFastIntervalSeconds(documentState_.loaded.window.feedbackFastIntervalSeconds);
            documentState_.loaded.window.feedbackHeartbeatIntervalSeconds =
                ClampFeedbackHeartbeatIntervalSeconds(
                    documentState_.loaded.window.feedbackHeartbeatIntervalSeconds,
                    documentState_.loaded.window.feedbackFastIntervalSeconds);
        }

        ImGui::TextDisabled("Fast: %.0f ms", documentState_.loaded.window.feedbackFastIntervalSeconds * 1000.0f);
        ImGui::TextDisabled("Heartbeat: %.0f ms", documentState_.loaded.window.feedbackHeartbeatIntervalSeconds * 1000.0f);
    }
    }
}


void EditorApplication::DrawPageInspector()
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        if (HasOpenWindow() && documentState_.loaded.document.pages.empty())
        {
            ImGui::TextDisabled("No page yet.");
            ImGui::TextWrapped("This window is open, but it does not contain any page yet.");
            if (AccentButton("Create first page##inspector"))
            {
                OpenNewPagePopup();
            }
            ShowItemTooltip("Open the page-creation workflow for this window.");
        }
        else
        {
            ImGui::TextDisabled("No page selected.");
        }
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page");
    ImGui::TextDisabled("Edit the page and work directly in the preview.");

    const bool sectionForceOpen = tutorial_->IsCoachVisible();

    if (ImGui::Button("Page actions..."))
    {
        ImGui::OpenPopup("PageActionsMenu");
    }
    ShowItemTooltip("Remove, delete or rename this page asset.");
    InspectorHelpMarker("Del opens the delete confirmation. Ctrl+V pastes copied reticles.");

    if (ImGui::BeginPopup("PageActionsMenu"))
    {
        if (ImGui::MenuItem("Remove page from window"))
        {
            OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, documentState_.selection.pageIndex);
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Delete page asset..."))
        {
            OpenPageManagementPopup(PageManagementAction::DeleteAsset, documentState_.selection.pageIndex);
            ImGui::EndPopup();
            return;
        }
        if (ImGui::MenuItem("Rename page globally..."))
        {
            OpenPageRenamePopup(documentState_.selection.pageIndex);
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }

    const bool canPasteReticles = !clipboardState_.pageReticleClipboard.empty();
    ImGui::BeginDisabled(!canPasteReticles);
    if (ImGui::Button("Paste copied reticles"))
    {
        PasteCopiedPageReticles();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste copied page reticles onto this page.");
    ImGui::EndDisabled();

    if (canPasteReticles)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Shortcut: Ctrl+V");
    }

    std::array<char, 128> name {};
    std::array<char, 128> title {};
    CopyTextBuffer(name, page->name);
    CopyTextBuffer(title, page->title);
    ImVec4 background = ToImGuiColor(page->backgroundColor);

    ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_ReadOnly);
    ShowItemTooltip("Internal page id used in JSON and API references.");
    ImGui::TextDisabled("Use 'Rename page globally...' to change this id safely.");

    const bool titleChanged = ImGui::InputText("Title", title.data(), title.size());
    ShowItemTooltip("Human-readable title shown in the editor and optionally in the runtime page chrome.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (titleChanged)
    {
        page->title = title.data();
    }

    if (ImGui::Button("Select title chrome"))
    {
        SelectPageTitle(documentState_.selection.pageIndex);
        if (page->name == "Page1" && tutorial_->MatchesTarget("page_select_title_chrome"))
        {
            tutorial_->CompleteStep();
        }
        return;
    }
    ShowItemTooltip("Edit the page title position, scale, visibility and decoration in its dedicated inspector.");
    tutorial_->DrawHalo(
        "page_select_title_chrome",
        "Select the Page1 title chrome",
        "Open the dedicated title inspector. It exposes move, scale, visibility, color, line style, and decoration controls for the generated page title.");

    const bool bgChanged = ImGui::ColorEdit4("Background", &background.x);
    ShowItemTooltip("Preview and runtime background color for this page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (bgChanged)
    {
        page->backgroundColor = ToColorRgba(background);
    }

    const bool centerChanged = ImGui::DragFloat2("View center", &page->view.center.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip(
        "Authored page camera center stored in the JSON.\n"
        "This is different from the temporary mouse-wheel zoom and pan used only by the editor preview.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (centerChanged)
    {
        page->view.center.x = std::clamp(page->view.center.x, -1.0f, 1.0f);
        page->view.center.y = std::clamp(page->view.center.y, -1.0f, 1.0f);
        layoutState_.pagePreviewView = page->view;
        layoutState_.pagePreviewView.zoom = mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom);
    }

    const bool zoomChanged = ImGui::DragFloat("Zoom", &page->view.zoom, 0.02f, 0.1f, 20.0f, "%.3f");
    ShowItemTooltip(
        "Authored default page zoom stored in the JSON.\n"
        "Mouse-wheel zoom in the preview does not change this value.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (zoomChanged)
    {
        page->view.zoom = mfd::SanitizeZoom(page->view.zoom);
        layoutState_.pagePreviewView = page->view;
    }

    bool defaultPage = page->defaultPage;
    if (ImGui::Checkbox("Default page for this window", &defaultPage))
    {
        PushUndoSnapshot();
        if (defaultPage)
        {
            for (auto& candidate : documentState_.loaded.document.pages)
            {
                candidate.defaultPage = false;
            }
        }

        page->defaultPage = defaultPage;
    }
    ShowItemTooltip("Mark this page as the default page opened by the runtime for this window.");

    ImGui::TextDisabled("If no page is marked default, the runtime opens the first page in the window JSON.");

    if (BeginInspectorSection("section_page_blink", "Blink types", false, sectionForceOpen))
    {
        DrawPageBlinkInspector(*page);
    }
    if (BeginInspectorSection("section_page_layers", "Editor layers", false, sectionForceOpen))
    {
        DrawPageLayerInspector(*page);
    }
    if (BeginInspectorSection("section_page_dynamic", "Dynamic reticles", false, sectionForceOpen))
    {
        DrawPageDynamicTemplateInspector(*page);
    }
    if (BeginInspectorSection("section_page_strobe", "Strobe", false, sectionForceOpen))
    {
        DrawPageStrobeInspector(*page);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Static reticles: %d", static_cast<int>(page->staticReticles.size()));
    const int editorVisibleReticleCount = static_cast<int>(std::count_if(
        page->staticReticles.begin(),
        page->staticReticles.end(),
        [page](const mfd::ReticleGroup& reticle)
        {
            return IsReticleVisibleInEditor(*page, reticle);
        }));
    ImGui::TextDisabled("Visible in editor: %d", editorVisibleReticleCount);
}

void EditorApplication::DrawPageDynamicTemplateInspector(mfd::PageDefinition& page)
{
    constexpr std::array<TutorialDynamicTemplateInfo, 1> kTutorialDynamicTemplates {{
        {"mfd_tutorial_radar_track",
         "RadarTrackLayer",
         "page_dynamic_template_mfd_tutorial_radar_track",
         "Add mfd_tutorial_radar_track",
         "Bind the tutorial radar-track template to RadarTrackLayer on Page1 without touching the existing steering cue."},
    }};

    static std::unordered_map<std::string, DynamicBindingDraftState, mfd::TransparentStringHash, mfd::TransparentStringEqual>
        s_bindingDrafts;

    BootstrapEditorLayersForPage(page);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Dynamic reticles");
    ImGui::TextDisabled("Choose one reticle template, choose one page layer, then click Add.");
    ImGui::TextDisabled("These entries are runtime-only bindings. They do not create authored static reticles on the page canvas.");

    std::vector<std::string> templateIds;
    templateIds.reserve(documentState_.loaded.document.reticleLibrary.size());
    for (const auto& entry : documentState_.loaded.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());
    templateIds.erase(std::unique(templateIds.begin(), templateIds.end()), templateIds.end());

    DynamicBindingDraftState& draft = s_bindingDrafts[page.normalizedName];
    if ((draft.layerId.empty() || mfd::FindPageLayerDefinition(page, draft.layerId) == nullptr) && !page.layers.empty())
    {
        draft.layerId = page.layers.front().id;
    }

    const TutorialDynamicTemplateInfo* activeTutorialTemplate = nullptr;
    for (const TutorialDynamicTemplateInfo& tutorialTemplate : kTutorialDynamicTemplates)
    {
        if (tutorial_->MatchesTarget(tutorialTemplate.targetId))
        {
            activeTutorialTemplate = &tutorialTemplate;
            break;
        }
    }

    std::vector<std::string> availableTemplateIds;
    availableTemplateIds.reserve(templateIds.size());
    for (const std::string& templateId : templateIds)
    {
        if (!PageHasDynamicTemplateBinding(page, templateId))
        {
            availableTemplateIds.push_back(templateId);
        }
    }

    if (activeTutorialTemplate != nullptr &&
        std::any_of(availableTemplateIds.begin(),
                    availableTemplateIds.end(),
                    [activeTutorialTemplate](const std::string& candidate)
                    {
                        return mfd::PageNamesEqual(candidate, activeTutorialTemplate->templateId);
                    }))
    {
        draft.templateId = std::string(activeTutorialTemplate->templateId);
    }
    else if (!availableTemplateIds.empty() &&
             !std::any_of(availableTemplateIds.begin(),
                          availableTemplateIds.end(),
                          [&draft](const std::string& candidate)
                          {
                              return candidate == draft.templateId;
                          }))
    {
        draft.templateId = availableTemplateIds.front();
    }
    else if (availableTemplateIds.empty())
    {
        draft.templateId.clear();
    }

    if (activeTutorialTemplate != nullptr &&
        mfd::FindPageLayerDefinition(page, activeTutorialTemplate->preferredLayerId) != nullptr)
    {
        draft.layerId = std::string(activeTutorialTemplate->preferredLayerId);
    }

    ImGui::TextDisabled("Configured on this page: %d", static_cast<int>(page.dynamicReticleBindings.size()));
    ImGui::TextDisabled("Each reticle template can appear at most once in this dynamic list.");

    if (templateIds.empty())
    {
        ImGui::TextDisabled("No library reticle is available yet. Create one first.");
        return;
    }

    if (page.layers.empty())
    {
        ImGui::TextDisabled("No page layer is available yet. Add one in Page layers first.");
        return;
    }

    const bool canAddBinding = !draft.templateId.empty() && !draft.layerId.empty() &&
                               mfd::FindPageLayerDefinition(page, draft.layerId) != nullptr;

    if (ImGui::BeginCombo("Reticle template", draft.templateId.empty() ? "<none>" : draft.templateId.c_str()))
    {
        if (availableTemplateIds.empty())
        {
            ImGui::TextDisabled("All library templates are already bound on this page.");
        }
        else
        {
            for (const std::string& templateId : availableTemplateIds)
            {
                const bool selected = draft.templateId == templateId;
                if (ImGui::Selectable(templateId.c_str(), selected))
                {
                    draft.templateId = templateId;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose one unbound library reticle template to expose as one page-scoped dynamic reticle type.");

    if (ImGui::BeginCombo("Layer", draft.layerId.empty() ? "<none>" : draft.layerId.c_str()))
    {
        for (const mfd::PageLayerDefinition& layer : page.layers)
        {
            const bool selected = draft.layerId == layer.id;
            if (ImGui::Selectable(layer.id.c_str(), selected))
            {
                draft.layerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose which page layer will own runtime instances created from this dynamic reticle type.");

    ImGui::BeginDisabled(!canAddBinding);
    if (AccentButton("Add"))
    {
        if (activeTutorialTemplate != nullptr &&
            !mfd::PageNamesEqual(draft.templateId, activeTutorialTemplate->templateId))
        {
            RebuildStatus("Tutorial: choose '" + std::string(activeTutorialTemplate->templateId) + "' before clicking Add.", true);
        }
        else
        {
            PushUndoSnapshot();
            page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {
                draft.templateId,
                draft.layerId,
                NextPageDynamicOrderInLayer(page, draft.layerId)});

            if (activeTutorialTemplate != nullptr)
            {
                tutorial_->CompleteStep();
            }

            RebuildStatus("Dynamic reticle '" + draft.templateId + "' added on layer '" + draft.layerId +
                              "' for page '" + page.name + "'.",
                          false);
        }
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Add the selected reticle template to the page dynamic-reticle list with the chosen layer.");

    if (activeTutorialTemplate != nullptr &&
        mfd::PageNamesEqual(draft.templateId, activeTutorialTemplate->templateId))
    {
        tutorial_->DrawHalo(
            activeTutorialTemplate->targetId,
            activeTutorialTemplate->label,
            activeTutorialTemplate->reason);
    }

    if (page.dynamicReticleBindings.empty())
    {
        ImGui::TextDisabled("No dynamic reticle is configured on this page yet.");
        return;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Dynamic reticles on this page:");

    std::vector<std::size_t> sortedBindingIndexes(page.dynamicReticleBindings.size());
    std::iota(sortedBindingIndexes.begin(), sortedBindingIndexes.end(), 0U);
    std::sort(sortedBindingIndexes.begin(),
              sortedBindingIndexes.end(),
              [&page](const std::size_t lhsIndex, const std::size_t rhsIndex)
              {
                  const auto& lhs = page.dynamicReticleBindings[lhsIndex];
                  const auto& rhs = page.dynamicReticleBindings[rhsIndex];
                  if (PageLayerOrder(page, lhs.layerId) != PageLayerOrder(page, rhs.layerId))
                  {
                      return PageLayerOrder(page, lhs.layerId) < PageLayerOrder(page, rhs.layerId);
                  }
                  if (lhs.orderInLayer != rhs.orderInLayer)
                  {
                      return lhs.orderInLayer < rhs.orderInLayer;
                  }
                  return lhs.templateId < rhs.templateId;
              });

    for (std::size_t displayIndex = 0; displayIndex < sortedBindingIndexes.size(); ++displayIndex)
    {
        const std::size_t bindingIndex = sortedBindingIndexes[displayIndex];
        mfd::DynamicReticleLayerBinding& binding = page.dynamicReticleBindings[bindingIndex];

        ImGui::PushID(static_cast<int>(bindingIndex));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("%s", binding.templateId.c_str());
        ImGui::TextDisabled("orderInLayer %d", binding.orderInLayer);

        if (ImGui::BeginCombo("Layer", binding.layerId.c_str()))
        {
            for (const mfd::PageLayerDefinition& layer : page.layers)
            {
                const bool selected = binding.layerId == layer.id;
                if (ImGui::Selectable(layer.id.c_str(), selected))
                {
                    PushUndoSnapshot();
                    binding.layerId = layer.id;
                    binding.orderInLayer =
                        NextPageDynamicOrderInLayer(page, binding.layerId, static_cast<int>(bindingIndex));
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which page layer owns runtime instances of this dynamic reticle type.");

        ImGui::SameLine();
        if (ImGui::Button("Remove"))
        {
            PushUndoSnapshot();
            const std::string removedTemplateId = binding.templateId;
            page.dynamicReticleBindings.erase(page.dynamicReticleBindings.begin() + static_cast<std::ptrdiff_t>(bindingIndex));
            RebuildStatus("Dynamic reticle '" + removedTemplateId + "' removed from page '" + page.name + "'.", false);
            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Remove this dynamic reticle type from the page.");

        std::vector<std::size_t> siblingIndexes;
        for (std::size_t siblingIndex : sortedBindingIndexes)
        {
            if (mfd::PageNamesEqual(page.dynamicReticleBindings[siblingIndex].layerId, binding.layerId))
            {
                siblingIndexes.push_back(siblingIndex);
            }
        }

        const auto siblingIterator = std::find(siblingIndexes.begin(), siblingIndexes.end(), bindingIndex);
        const bool hasPreviousSibling = siblingIterator != siblingIndexes.begin() && siblingIterator != siblingIndexes.end();
        const bool hasNextSibling = siblingIterator != siblingIndexes.end() && std::next(siblingIterator) != siblingIndexes.end();

        ImGui::BeginDisabled(!hasPreviousSibling);
        if (ImGui::Button("Move earlier"))
        {
            PushUndoSnapshot();
            mfd::DynamicReticleLayerBinding& previous = page.dynamicReticleBindings[*std::prev(siblingIterator)];
            std::swap(binding.orderInLayer, previous.orderInLayer);
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Swap orderInLayer with the previous dynamic binding on the same runtime layer.");

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasNextSibling);
        if (ImGui::Button("Move later"))
        {
            PushUndoSnapshot();
            mfd::DynamicReticleLayerBinding& next = page.dynamicReticleBindings[*std::next(siblingIterator)];
            std::swap(binding.orderInLayer, next.orderInLayer);
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Swap orderInLayer with the next dynamic binding on the same runtime layer.");

        ImGui::PopID();
    }

}

void EditorApplication::DrawPageStrobeInspector(mfd::PageDefinition& page)
{
    using editor::tutorial::TutorialStepId;

    constexpr std::string_view kTutorialDefaultStrobeNameTargetId = "page_strobe_default_name";
    constexpr std::string_view kTutorialDefaultStrobeTemplateTargetId = "page_strobe_default";
    constexpr std::string_view kTutorialDefaultStrobeAddTargetId = "page_strobe_default_add";
    constexpr std::string_view kTutorialAlternativeStrobeNameTargetId = "page_strobe_alternative_name";
    constexpr std::string_view kTutorialAlternativeStrobeTemplateTargetId = "page_strobe_alternative";
    constexpr std::string_view kTutorialAlternativeStrobeAddTargetId = "page_strobe_alternative_add";
    static std::unordered_map<std::string, StrobeDraftState> s_strobeDrafts;

    const bool isDefaultStrobeTutorialStep = tutorial_->IsStep(static_cast<int>(TutorialStepId::AddPage1DefaultStrobe));
    const bool isAlternativeStrobeTutorialStep =
        tutorial_->IsStep(static_cast<int>(TutorialStepId::AddPage1AlternativeStrobe));

    std::string_view tutorialExpectedStrobeName {};
    if (page.name == "Page1")
    {
        if (isDefaultStrobeTutorialStep)
        {
            tutorialExpectedStrobeName = "Default";
        }
        else if (isAlternativeStrobeTutorialStep)
        {
            tutorialExpectedStrobeName = "Strobe1";
        }
    }

    const std::string_view tutorialTemplateTargetId =
        isDefaultStrobeTutorialStep
            ? kTutorialDefaultStrobeTemplateTargetId
            : (isAlternativeStrobeTutorialStep ? kTutorialAlternativeStrobeTemplateTargetId : std::string_view {});
    const std::string_view tutorialExpectedTemplateId =
        isDefaultStrobeTutorialStep
            ? kTutorialStrobeCursorTemplateId
            : (isAlternativeStrobeTutorialStep ? kTutorialAircraftTemplateId : std::string_view {});
    const std::string_view tutorialAddTargetId =
        isDefaultStrobeTutorialStep
            ? kTutorialDefaultStrobeAddTargetId
            : (isAlternativeStrobeTutorialStep ? kTutorialAlternativeStrobeAddTargetId : std::string_view {});
    const std::string_view tutorialNameTargetId =
        isDefaultStrobeTutorialStep
            ? kTutorialDefaultStrobeNameTargetId
            : (isAlternativeStrobeTutorialStep ? kTutorialAlternativeStrobeNameTargetId : std::string_view {});
    const char* const tutorialTemplateReason =
        isAlternativeStrobeTutorialStep
            ? "Choose the triangle-based aircraft template so Page1 exposes one second strobe that is visibly distinct from Default."
            : "Choose the tutorial cursor template so Page1 gets one first authored strobe with capture and feedback.";
    const char* const tutorialAddReason =
        isAlternativeStrobeTutorialStep
            ? "Commit the second authored strobe so Page1 keeps Default plus one runtime-selectable aircraft-shaped alternative."
            : "Commit the first authored strobe so Page1 has one default strobe before the runtime alternative is added.";

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Strobe");
    ImGui::TextDisabled("Each page exposes a list of named strobes, but only one is active at runtime.");
    ImGui::TextDisabled("Add alternative strobes here, choose the default one, then edit the selected strobe below.");

    std::vector<std::string> templateIds;
    templateIds.reserve(documentState_.loaded.document.reticleLibrary.size());
    for (const auto& entry : documentState_.loaded.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());

    StrobeDraftState& draft = s_strobeDrafts[page.normalizedName.empty() ? page.name : page.normalizedName];
    if (draft.name.empty())
    {
        draft.name = SuggestPageStrobeDraftName(page);
    }
    if (!tutorialExpectedStrobeName.empty())
    {
        draft.name = std::string(tutorialExpectedStrobeName);
    }
    if (!templateIds.empty() &&
        std::find(templateIds.begin(), templateIds.end(), draft.templateId) == templateIds.end())
    {
        draft.templateId = templateIds.front();
    }

    std::array<char, 128> draftNameBuffer {};
    CopyTextBuffer(draftNameBuffer, draft.name);
    if (ImGui::InputText("New strobe name##strobe_draft_name", draftNameBuffer.data(), draftNameBuffer.size()))
    {
        draft.name = draftNameBuffer.data();
    }
    ShowItemTooltip("Name used by the editor, runtime selection, and generated client API.");
    tutorial_->DrawHalo(
        tutorialNameTargetId,
        "Keep the guided strobe name",
        isAlternativeStrobeTutorialStep
            ? "Keep `Strobe1` so the generated client exposes one clear runtime alternative next to Default."
            : "Keep `Default` so the generated client exposes one stable default Page1 strobe.");
    if (tutorial_->MatchesTarget(tutorialNameTargetId) && draft.name == tutorialExpectedStrobeName)
    {
        tutorial_->AdvancePhase();
    }

    if (ImGui::BeginCombo("Reticle template##strobe_draft_template",
                          draft.templateId.empty() ? "<none>" : draft.templateId.c_str()))
    {
        if (templateIds.empty())
        {
            ImGui::TextDisabled("No library reticle is available yet.");
        }
        else
        {
            for (const std::string& templateId : templateIds)
            {
                const bool selected = draft.templateId == templateId;
                if (ImGui::Selectable(templateId.c_str(), selected))
                {
                    draft.templateId = templateId;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose which shared reticle template will be instantiated for the next strobe you add.");
    tutorial_->DrawHalo(
        tutorialTemplateTargetId,
        isAlternativeStrobeTutorialStep ? "Choose mfd_tutorial_aircraft" : "Choose mfd_tutorial_strobe_cursor",
        tutorialTemplateReason);
    if (tutorial_->MatchesTarget(tutorialTemplateTargetId) && draft.templateId == tutorialExpectedTemplateId)
    {
        tutorial_->AdvancePhase();
    }

    const bool canAddStrobe = !draft.name.empty() && !draft.templateId.empty();
    ImGui::BeginDisabled(!canAddStrobe);
    if (AccentButton("Add strobe##page_strobe_add"))
    {
        if (tutorial_->MatchesTarget(tutorialAddTargetId) &&
            draft.name != tutorialExpectedStrobeName)
        {
            RebuildStatus("Tutorial: keep the suggested Page1 strobe name before adding it.", true);
        }
        else if (tutorial_->MatchesTarget(tutorialAddTargetId) && draft.templateId != tutorialExpectedTemplateId)
        {
            RebuildStatus("Tutorial: choose the guided Page1 strobe template before adding it.", true);
        }
        else if (const auto iterator = documentState_.loaded.document.reticleLibrary.find(draft.templateId);
                 iterator != documentState_.loaded.document.reticleLibrary.end())
        {
            PushUndoSnapshot();
            if (tutorial_->MatchesTarget(tutorialAddTargetId) && draft.templateId == kTutorialStrobeCursorTemplateId)
            {
                tutorial_->ConfigureTutorialDefaultStrobeTemplate(iterator->second);
            }

            const bool guidedAdd =
                tutorial_->MatchesTarget(tutorialAddTargetId) && !tutorialExpectedStrobeName.empty();
            const int guidedStrobeIndex =
                guidedAdd ? FindPageStrobeIndexByName(page, tutorialExpectedStrobeName) : -1;

            if (guidedStrobeIndex >= 0)
            {
                mfd::PageStrobeDefinition updatedStrobe = MakePageStrobeFromTemplate(
                    page,
                    iterator->second,
                    page.strobes[static_cast<std::size_t>(guidedStrobeIndex)]);
                updatedStrobe.name = std::string(tutorialExpectedStrobeName);
                updatedStrobe.normalizedName = mfd::NormalizePageName(updatedStrobe.name);
                RefreshBlinkBindingForEditor(page, updatedStrobe.reticle.blink);
                page.strobes[static_cast<std::size_t>(guidedStrobeIndex)] = std::move(updatedStrobe);
                SelectPageStrobe(documentState_.selection.pageIndex, guidedStrobeIndex);
                RebuildStatus("Strobe '" + page.strobes[static_cast<std::size_t>(guidedStrobeIndex)].name +
                                  "' updated on page '" + page.name + "'.",
                              false);
            }
            else
            {
                mfd::PageStrobeDefinition strobe = MakePageStrobeFromTemplate(page, iterator->second, std::nullopt);
                strobe.name = MakeUniqueStrobeName(page, draft.name.empty() ? SuggestPageStrobeDraftName(page) : draft.name);
                strobe.normalizedName = mfd::NormalizePageName(strobe.name);
                RefreshBlinkBindingForEditor(page, strobe.reticle.blink);
                page.strobes.push_back(std::move(strobe));
                if (page.strobes.size() == 1U || page.normalizedActiveStrobeName.empty())
                {
                    SetActivePageStrobe(page, page.strobes.back());
                }

                const int newIndex = static_cast<int>(page.strobes.size()) - 1;
                SelectPageStrobe(documentState_.selection.pageIndex, newIndex);
                RebuildStatus("Strobe '" + page.strobes.back().name + "' added to page '" + page.name + "'.", false);
            }

            draft.name = SuggestPageStrobeDraftName(page);
            if (tutorial_->MatchesTarget(tutorialAddTargetId))
            {
                tutorial_->CompleteStep();
            }
        }
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Instantiate one new strobe from the chosen library template and add it to this page.");
    tutorial_->DrawHalo(
        tutorialAddTargetId,
        "Add the guided strobe",
        tutorialAddReason);

    if (templateIds.empty())
    {
        ImGui::TextDisabled("No library reticle is available yet. Create one first.");
        return;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Configured strobes: %d", static_cast<int>(page.strobes.size()));
    ImGui::TextDisabled("Exactly one strobe is active at runtime. The default choice is stored on the page.");

    bool removedStrobe = false;
    for (std::size_t strobeIndex = 0; strobeIndex < page.strobes.size(); ++strobeIndex)
    {
        mfd::PageStrobeDefinition& strobe = page.strobes[strobeIndex];
        const std::string strobeDisplayLabel = PageStrobeDisplayLabel(page, strobe, strobeIndex);
        const std::string strobeSelectableLabel = strobeDisplayLabel + "##strobe_select";
        const bool selected =
            documentState_.selection.kind == SelectionKind::PageStrobe &&
            documentState_.selection.pageReticleIndex == static_cast<int>(strobeIndex);
        const bool active = page.normalizedActiveStrobeName == strobe.normalizedName ||
                            (page.normalizedActiveStrobeName.empty() && strobeIndex == 0U);

        ImGui::PushID(static_cast<int>(strobeIndex));
        if (ImGui::Selectable(strobeSelectableLabel.c_str(), selected))
        {
            SelectPageStrobe(documentState_.selection.pageIndex, static_cast<int>(strobeIndex));
        }

        if (ImGui::RadioButton("Active by default##strobe_default", active) && !active)
        {
            PushUndoSnapshot();
            SetActivePageStrobe(page, strobe);
            SelectPageStrobe(documentState_.selection.pageIndex, static_cast<int>(strobeIndex));
        }
        ShowItemTooltip("Choose which authored strobe becomes active by default when the page loads.");

        std::array<char, 128> strobeNameBuffer {};
        CopyTextBuffer(strobeNameBuffer, strobe.name);
        if (ImGui::InputText("Name##strobe_name", strobeNameBuffer.data(), strobeNameBuffer.size()))
        {
            const std::string previousNormalizedName = strobe.normalizedName;
            std::string requestedName = strobeNameBuffer.data();
            if (NormalizeEditorIdentifier(requestedName).empty())
            {
                requestedName = "Strobe";
            }

            strobe.name = MakeUniqueStrobeName(page, requestedName, strobe.name);
            strobe.normalizedName = mfd::NormalizePageName(strobe.name);
            if (page.normalizedActiveStrobeName == previousNormalizedName)
            {
                SetActivePageStrobe(page, strobe);
            }
        }
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        ShowItemTooltip("Rename this strobe. Names stay unique within the page.");

        ImGui::TextDisabled("Template: %s",
                            strobe.reticle.sourceTemplateId.empty() ? "<custom strobe>" : strobe.reticle.sourceTemplateId.c_str());

        if (ImGui::Button("Remove strobe##page_strobe_remove"))
        {
            PushUndoSnapshot();
            const std::string removedName = strobe.name;
            const std::string removedNormalizedName = strobe.normalizedName;
            page.strobes.erase(page.strobes.begin() + static_cast<std::ptrdiff_t>(strobeIndex));
            if (page.strobes.empty())
            {
                page.activeStrobeName.clear();
                page.normalizedActiveStrobeName.clear();
                SelectPage(documentState_.selection.pageIndex, false);
            }
            else
            {
                if (page.normalizedActiveStrobeName == removedNormalizedName)
                {
                    SetActivePageStrobe(page, page.strobes.front());
                }
                SelectPageStrobe(documentState_.selection.pageIndex,
                                 std::min<int>(static_cast<int>(strobeIndex), static_cast<int>(page.strobes.size()) - 1));
            }
            RebuildStatus("Strobe '" + removedName + "' removed from page '" + page.name + "'.", false);
            removedStrobe = true;
            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Remove this authored strobe from the page list.");

        ImGui::Separator();
        ImGui::PopID();
    }

    if (removedStrobe)
    {
        return;
    }

    if (page.strobes.empty())
    {
        ImGui::TextDisabled("No strobe assigned to this page.");
        return;
    }

    int editedStrobeIndex = FindActivePageStrobeIndex(page);
    if (documentState_.selection.kind == SelectionKind::PageStrobe &&
        documentState_.selection.pageReticleIndex >= 0 &&
        documentState_.selection.pageReticleIndex < static_cast<int>(page.strobes.size()))
    {
        editedStrobeIndex = documentState_.selection.pageReticleIndex;
    }
    if (editedStrobeIndex < 0 || editedStrobeIndex >= static_cast<int>(page.strobes.size()))
    {
        editedStrobeIndex = 0;
    }

    mfd::PageStrobeDefinition& editedStrobe = page.strobes[static_cast<std::size_t>(editedStrobeIndex)];
    const std::string currentTemplateId = editedStrobe.reticle.sourceTemplateId;

    ImGui::TextDisabled("Editing strobe: %s", editedStrobe.name.c_str());
    if (ImGui::BeginCombo("Strobe template##page_strobe_template",
                          currentTemplateId.empty() ? "<custom strobe>" : currentTemplateId.c_str()))
    {
        for (const std::string& templateId : templateIds)
        {
            const bool selected = currentTemplateId == templateId;
            if (ImGui::Selectable(templateId.c_str(), selected) && !selected)
            {
                if ((tutorial_->MatchesTarget(kTutorialDefaultStrobeTemplateTargetId) ||
                     tutorial_->MatchesTarget(kTutorialAlternativeStrobeTemplateTargetId)) &&
                    templateId != tutorialExpectedTemplateId)
                {
                    RebuildStatus("Tutorial: choose the guided Page1 strobe template.", true);
                    continue;
                }

                const auto iterator = documentState_.loaded.document.reticleLibrary.find(templateId);
                if (iterator != documentState_.loaded.document.reticleLibrary.end())
                {
                    PushUndoSnapshot();
                    const std::optional<mfd::PageStrobeDefinition> previousStrobe = editedStrobe;
                    std::size_t unmappedPrimitiveOverrideCount = 0;
                    editedStrobe = MakePageStrobeFromTemplate(
                        page,
                        iterator->second,
                        previousStrobe,
                        &unmappedPrimitiveOverrideCount);
                    RefreshBlinkBindingForEditor(page, editedStrobe.reticle.blink);
                    if (unmappedPrimitiveOverrideCount > 0)
                    {
                        RebuildStatus("Changed the strobe template, but " +
                                          std::to_string(unmappedPrimitiveOverrideCount) +
                                          " named text/time primitive override(s) could not be remapped.",
                                      true);
                    }
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Replace the selected strobe instance with another shared reticle template while keeping its authored runtime settings.");

    ImGui::TextDisabled("Strobe id: %s", editedStrobe.reticle.id.c_str());
    if (!currentTemplateId.empty())
    {
        ImGui::TextDisabled("Source template: %s", currentTemplateId.c_str());
    }

    const char* captureShapeLabel =
        editedStrobe.capture.shape == mfd::StrobeCaptureShape::Circle ? "Circle" : "Rectangle";
    if (ImGui::BeginCombo("Capture shape", captureShapeLabel))
    {
        const bool circleSelected = editedStrobe.capture.shape == mfd::StrobeCaptureShape::Circle;
        if (ImGui::Selectable("Circle", circleSelected) && !circleSelected)
        {
            PushUndoSnapshot();
            editedStrobe.capture.shape = mfd::StrobeCaptureShape::Circle;
        }
        if (circleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        const bool rectangleSelected = editedStrobe.capture.shape == mfd::StrobeCaptureShape::Rectangle;
        if (ImGui::Selectable("Rectangle", rectangleSelected) && !rectangleSelected)
        {
            PushUndoSnapshot();
            editedStrobe.capture.shape = mfd::StrobeCaptureShape::Rectangle;
        }
        if (rectangleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the capture area shape used by the page strobe.");

    if (editedStrobe.capture.shape == mfd::StrobeCaptureShape::Circle)
    {
        if (ImGui::DragFloat("Capture radius", &editedStrobe.capture.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            editedStrobe.capture.radius = std::max(0.001f, editedStrobe.capture.radius);
        }
        ShowItemTooltip("Radius of the circular capture area around the strobe.");
    }
    else
    {
        if (ImGui::DragFloat2("Capture size", &editedStrobe.capture.size.x, 0.002f, 0.001f, 2.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            editedStrobe.capture.size.x = std::max(0.001f, editedStrobe.capture.size.x);
            editedStrobe.capture.size.y = std::max(0.001f, editedStrobe.capture.size.y);
        }
        ShowItemTooltip("Width and height of the rectangular capture area around the strobe.");
    }

    bool magnetEnabled = editedStrobe.magnet.enabled;
    if (ImGui::Checkbox("Magnet enabled", &magnetEnabled))
    {
        PushUndoSnapshot();
        editedStrobe.magnet.enabled = magnetEnabled;
    }
    ShowItemTooltip("Enable attraction toward nearby dynamic reticles when the strobe moves.");

    if (editedStrobe.magnet.enabled)
    {
        if (ImGui::DragFloat("Magnet radius", &editedStrobe.magnet.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            editedStrobe.magnet.radius = std::max(0.001f, editedStrobe.magnet.radius);
        }
        ShowItemTooltip("Maximum distance used to snap the strobe toward nearby targets.");

        if (ImGui::DragFloat("Magnet strength", &editedStrobe.magnet.strength, 0.01f, 0.0f, 1.0f, "%.2f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            editedStrobe.magnet.strength = std::clamp(editedStrobe.magnet.strength, 0.0f, 1.0f);
        }
        ShowItemTooltip("Blend factor applied when the strobe is attracted toward one target.");

        bool visualShapeEnabled = editedStrobe.magnet.visualShapeEnabled;
        if (ImGui::Checkbox("Change visual shape while magnetized", &visualShapeEnabled))
        {
            PushUndoSnapshot();
            editedStrobe.magnet.visualShapeEnabled = visualShapeEnabled;
        }
        ShowItemTooltip("Optional visual cue only: the authored strobe reticle is kept unless this is enabled.");

        ImGui::BeginDisabled(!editedStrobe.magnet.visualShapeEnabled);
        const char* visualShapeLabel =
            editedStrobe.magnet.visualShape == mfd::StrobeMagnetVisualShape::Circle ? "Circle" : "Square";
        if (ImGui::BeginCombo("Magnetized visual shape", visualShapeLabel))
        {
            const bool circleSelected = editedStrobe.magnet.visualShape == mfd::StrobeMagnetVisualShape::Circle;
            if (ImGui::Selectable("Circle", circleSelected) && !circleSelected)
            {
                PushUndoSnapshot();
                editedStrobe.magnet.visualShape = mfd::StrobeMagnetVisualShape::Circle;
            }
            if (circleSelected)
            {
                ImGui::SetItemDefaultFocus();
            }

            const bool squareSelected = editedStrobe.magnet.visualShape == mfd::StrobeMagnetVisualShape::Square;
            if (ImGui::Selectable("Square", squareSelected) && !squareSelected)
            {
                PushUndoSnapshot();
                editedStrobe.magnet.visualShape = mfd::StrobeMagnetVisualShape::Square;
            }
            if (squareSelected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Shape used only while the runtime reports this strobe as magnetized.");

        if (ImGui::DragFloat("Magnetized visual size",
                             &editedStrobe.magnet.visualShapeSize,
                             0.002f,
                             0.001f,
                             1.0f,
                             "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            editedStrobe.magnet.visualShapeSize = std::max(0.001f, editedStrobe.magnet.visualShapeSize);
        }
        ShowItemTooltip("Logical diameter/side length of the optional magnetized visual cue.");
        ImGui::EndDisabled();
    }
}

void EditorApplication::DrawPageLayerInspector(mfd::PageDefinition& page)
{
    BootstrapEditorLayersForPage(page);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.82f, 0.73f, 0.94f, 1.0f), "Page layers");
    ImGui::TextDisabled("Page layers drive runtime draw order. Visibility below only affects the editor preview.");

    if (AccentButton("Add layer"))
    {
        const bool tutorialRadarTrackLayerMatched = tutorial_->MatchesTarget("inspector_add_radar_track_layer");
        const bool tutorialAddLayerMatched = tutorial_->MatchesTarget("inspector_add_layer");
        if (tutorialRadarTrackLayerMatched && mfd::FindPageLayerDefinition(page, "RadarTrackLayer") != nullptr)
        {
            tutorial_->CompleteStep();
            RebuildStatus("RadarTrackLayer is already present on page '" + page.name + "'.", false);
            return;
        }

        PushUndoSnapshot();
        const std::string newLayerId = tutorialRadarTrackLayerMatched ? std::string {"RadarTrackLayer"}
                                                                      : MakeUniqueLayerId(page, "layer");
        page.layers.push_back(mfd::PageLayerDefinition {newLayerId});
        page.editor.layers.push_back(mfd::EditorLayerDefinition {newLayerId, true});
        if (tutorialRadarTrackLayerMatched)
        {
            tutorial_->CompleteStep();
        }
        else if (tutorialAddLayerMatched)
        {
            tutorial_->SetFocusLayerId(page.layers.back().id);
            tutorial_->AdvancePhase();
        }
        RebuildStatus("Runtime layer added to page '" + page.name + "'.", false);
    }
    ShowItemTooltip("Create one new runtime page layer. The editor also tracks its temporary visibility state.");
    tutorial_->DrawHalo(
        "inspector_add_radar_track_layer",
        "Create RadarTrackLayer",
        "Create one dedicated runtime layer for Page1 radar tracks. The existing steering cue already stays on its own layer.");
    tutorial_->DrawHalo(
        "inspector_add_layer",
        "Click Add layer",
        "Create one extra page layer so the tutorial can show how layer-based authoring visibility works.");

    if (page.layers.empty())
    {
        ImGui::TextDisabled("No runtime layer exists on this page yet.");
        return;
    }

    for (std::size_t index = 0; index < page.layers.size(); ++index)
    {
        mfd::PageLayerDefinition& layer = page.layers[index];
        mfd::EditorLayerDefinition& editorLayer = page.editor.layers[index];
        const std::size_t assignedReticles = CountEditorLayerAssignments(page, layer.id);
        const std::size_t assignedBindings = CountDynamicLayerBindings(page, layer.id);
        const std::string dynamicBindingSummary = SummarizeDynamicLayerBindings(page, layer.id);

        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();
        ImGui::Separator();

        bool visible = editorLayer.visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            editorLayer.visible = visible;
            SanitizePageReticleSelectionForCurrentFocus();
            if (tutorial_->MatchesTarget("inspector_layer_visibility") &&
                layer.id == tutorial_->FocusLayerId() &&
                !editorLayer.visible)
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Show or hide this layer in the editor preview only.");
        if (layer.id == tutorial_->FocusLayerId())
        {
            tutorial_->DrawHalo(
                "inspector_layer_visibility",
                "Click Visible",
                "Hide the layer you just created to confirm that editor visibility stays separate from runtime layer order.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%zu reticle%s | %zu dynamic binding%s",
                            assignedReticles,
                            assignedReticles == 1U ? "" : "s",
                            assignedBindings,
                            assignedBindings == 1U ? "" : "s");

        if (!dynamicBindingSummary.empty())
        {
            ImGui::TextDisabled("Dynamic reticles: %s", dynamicBindingSummary.c_str());
        }

        std::array<char, 128> layerName {};
        CopyTextBuffer(layerName, layer.id);
        const bool nameChanged = ImGui::InputText("Layer id", layerName.data(), layerName.size());
        ShowItemTooltip("Unique layer identifier used by page reticles on this page.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (nameChanged)
        {
            const std::string nextId = layerName.data();
            const std::string normalizedNextId = NormalizeEditorIdentifier(nextId);
            const bool duplicateId = std::any_of(
                page.layers.begin(),
                page.layers.end(),
                [&layer, &normalizedNextId](const mfd::PageLayerDefinition& candidate)
                {
                    return &candidate != &layer && NormalizeEditorIdentifier(candidate.id) == normalizedNextId;
                });

            if (nextId.empty())
            {
                RebuildStatus("Layer id cannot be empty.", true);
            }
            else if (duplicateId)
            {
                RebuildStatus("Layer ids must stay unique inside one page.", true);
            }
            else if (nextId != layer.id)
            {
                const std::string previousId = layer.id;
                layer.id = nextId;
                editorLayer.id = nextId;
                RenameEditorLayerReferences(page, previousId, layer.id);
                if (layoutState_.layerFocusState.focusedLayerId == previousId)
                {
                    layoutState_.layerFocusState.focusedLayerId = layer.id;
                }
                SanitizePageReticleSelectionForCurrentFocus();
            }
        }

        ImGui::BeginDisabled(page.layers.size() <= 1U || assignedReticles > 0U || assignedBindings > 0U);
        if (ImGui::Button("Remove layer"))
        {
            const std::string removedLayerId = layer.id;
            PushUndoSnapshot();
            page.layers.erase(page.layers.begin() + static_cast<std::ptrdiff_t>(index));
            page.editor.layers.erase(page.editor.layers.begin() + static_cast<std::ptrdiff_t>(index));
            SanitizeLayerFocusForActivePage();
            SanitizePageReticleSelectionForCurrentFocus();
            RebuildStatus("Runtime layer '" + removedLayerId + "' removed from page '" + page.name + "'.", false);

            ImGui::EndDisabled();
            ImGui::PopID();
            break;
        }
        ImGui::EndDisabled();
        ShowItemTooltip("Delete one unused runtime layer. A layer cannot be removed while reticles or dynamic bindings still reference it.");

        ImGui::PopID();
    }
}

void EditorApplication::DrawPageBlinkInspector(mfd::PageDefinition& page)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Blink types");
    ImGui::TextDisabled("Blink names are page-local. Same effective duration means same phase.");

    if (AccentButton("Add blink type"))
    {
        PushUndoSnapshot();

        mfd::PageBlinkDefinition blinkType;
        blinkType.name = MakeUniqueBlinkTypeName(page, "blink");
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = 1000;
        page.blinkTypes.push_back(std::move(blinkType));

        if (page.blinkTypes.size() == 1)
        {
            page.defaultBlinkTypeName = page.blinkTypes.front().name;
            page.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
        }

        RefreshPageBlinkStateForEditor(page);
        RebuildStatus("Blink type added to page '" + page.name + "'.", false);
    }
    ShowItemTooltip("Create a named blink rhythm that page reticles can reference.");

    if (page.blinkTypes.empty())
    {
        ImGui::TextDisabled("No blink type defined on this page yet.");
        return;
    }

    const std::size_t effectiveDefaultIndex = EffectiveDefaultBlinkTypeIndex(page);
    const std::string defaultBlinkPreview =
        effectiveDefaultIndex == kInvalidBlinkTypeIndex ? std::string {"<none>"} : page.blinkTypes[effectiveDefaultIndex].name;

    if (ImGui::BeginCombo("Default blink", defaultBlinkPreview.c_str()))
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            const bool selected = index == effectiveDefaultIndex;
            if (ImGui::Selectable(page.blinkTypes[index].name.c_str(), selected))
            {
                PushUndoSnapshot();
                page.defaultBlinkTypeName = page.blinkTypes[index].name;
                page.normalizedDefaultBlinkTypeName = page.blinkTypes[index].normalizedName;
                RefreshPageBlinkStateForEditor(page);
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Fallback blink used when a reticle enables blink without choosing a named type.");

    ImGui::TextDisabled("Used when a reticle enables blink without choosing an explicit type.");

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        auto& blinkType = page.blinkTypes[index];

        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Blink %d", static_cast<int>(index) + 1);
        if (index == effectiveDefaultIndex)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(default)");
        }

        std::array<char, 128> blinkName {};
        CopyTextBuffer(blinkName, blinkType.name);
        const bool nameChanged = ImGui::InputText("Name", blinkName.data(), blinkName.size());
        ShowItemTooltip("Display name stored in the page JSON for this blink definition.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (nameChanged)
        {
            const std::string nextName = blinkName.data();
            const std::string nextNormalizedName = mfd::NormalizePageName(nextName);
            if (nextNormalizedName.empty())
            {
                RebuildStatus("Blink type name cannot be empty.", true);
            }
            else if (const std::size_t existingIndex = FindBlinkTypeIndex(page, nextNormalizedName);
                     existingIndex != kInvalidBlinkTypeIndex && existingIndex != index)
            {
                RebuildStatus("Blink type names must be unique inside one page.", true);
            }
            else if (nextName != blinkType.name)
            {
                const std::string previousNormalizedName = blinkType.normalizedName;
                blinkType.name = nextName;
                blinkType.normalizedName = nextNormalizedName;
                RenameBlinkReferences(page, previousNormalizedName, blinkType.name);
                RefreshPageBlinkStateForEditor(page);
            }
        }

        int durationMs = static_cast<int>(blinkType.durationMs);
        const bool durationChanged = ImGui::DragInt("Duration (ms)", &durationMs, 10.0f, 1, 60000);
        ShowItemTooltip("Blink cycle duration in milliseconds.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (durationChanged)
        {
            blinkType.durationMs = static_cast<std::uint32_t>(std::max(1, durationMs));
            RefreshPageBlinkStateForEditor(page);
        }

        if (ImGui::Button("Remove blink type"))
        {
            const std::string removedName = blinkType.name;
            const std::string removedNormalizedName = blinkType.normalizedName;
            const std::size_t clearedBindingCount = CountBlinkReferences(page, removedNormalizedName);
            const bool removedWasDefault = page.normalizedDefaultBlinkTypeName == removedNormalizedName;

            PushUndoSnapshot();
            page.blinkTypes.erase(page.blinkTypes.begin() + static_cast<std::ptrdiff_t>(index));
            ClearBlinkReferencesForRemovedType(page, removedNormalizedName);

            if (removedWasDefault && !page.blinkTypes.empty())
            {
                page.defaultBlinkTypeName = page.blinkTypes.front().name;
                page.normalizedDefaultBlinkTypeName = page.blinkTypes.front().normalizedName;
            }

            RefreshPageBlinkStateForEditor(page);

            std::string status = "Blink type '" + removedName + "' removed from page '" + page.name + "'.";
            if (clearedBindingCount > 0)
            {
                status += " Cleared " + std::to_string(clearedBindingCount) + " explicit binding(s).";
            }
            RebuildStatus(status, false);

            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Delete this blink definition and clear page reticles that referenced it.");

        ImGui::PopID();
    }
}

