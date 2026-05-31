/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Editor inspector implementation extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <unordered_map>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::ShowItemTooltip;
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::kPrimitiveTypes;
using editor::detail::kTutorialAircraftTemplateId;
using editor::detail::kTutorialStrobeCursorTemplateId;
using editor::detail::kTutorialPage1OwnshipReticleId;
using editor::detail::ReticleHasFillCapablePrimitive;
using editor::detail::SeedPrimitiveFillColorIfNeeded;
using editor::detail::SeedReticleFillOverrideIfNeeded;
using editor::detail::ToColorRgba;
using editor::detail::VisibleFillColorFromStroke;
using editor::app::ClearBlinkReferencesForRemovedType;
using editor::app::ClearEditorLayerReferences;
using editor::app::CollectClipPrimitiveOptions;
using editor::app::CountBlinkReferences;
using editor::app::CountDynamicLayerBindings;
using editor::app::CountEditorLayerAssignments;
using editor::app::ClipPrimitiveOption;
using editor::app::EffectiveDefaultBlinkTypeIndex;
using editor::app::FindActivePageStrobeIndex;
using editor::app::FindBlinkTypeIndex;
using editor::app::IsReticleVisibleInEditor;
using editor::app::kInvalidBlinkTypeIndex;
using editor::app::LineStyleLabel;
using editor::app::MakeUniqueBlinkTypeName;
using editor::app::NextPageDynamicOrderInLayer;
using editor::app::PageHasDynamicTemplateBinding;
using editor::app::PageLayerOrder;
using editor::app::PageStrobeDisplayLabel;
using editor::app::PageStrobeNameExistsExact;
using editor::app::PageTitleDecorationLabel;
using editor::app::PrimitiveTypeLabel;
using editor::app::RefreshBlinkBindingForEditor;
using editor::app::RefreshPageBlinkStateForEditor;
using editor::app::RenameBlinkReferences;
using editor::app::RenameEditorLayerReferences;
using editor::app::ReticleClipModeLabel;
using editor::app::SetActivePageStrobe;
using editor::app::SuggestPageStrobeDraftName;
using editor::app::SummarizeDynamicLayerBindings;
using editor::app::SupportsPrimitiveLineStyle;
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

class ScopedImGuiId
{
public:
    explicit ScopedImGuiId(const char* id)
    {
        ImGui::PushID(id);
    }

    ~ScopedImGuiId()
    {
        ImGui::PopID();
    }
};

/**
 * @brief Returns the local-space visual center of one reticle template or instance.
 */
mfd::Vec2 ReticleVisualCenterLocal(const mfd::ReticleGroup& reticle)
{
    bool hasBounds = false;
    mfd::Vec2 minPoint {};
    mfd::Vec2 maxPoint {};

    for (const auto& primitive : reticle.primitives)
    {
        editor::detail::ForEachPrimitiveBoundsLocalPoint(
            primitive,
            [&hasBounds, &minPoint, &maxPoint, &primitive](const mfd::Vec2 localPoint)
            {
                const mfd::Vec2 transformedPoint = mfd::ApplyTransform(localPoint, primitive.transform);
                if (!hasBounds)
                {
                    minPoint = transformedPoint;
                    maxPoint = transformedPoint;
                    hasBounds = true;
                    return;
                }

                minPoint.x = std::min(minPoint.x, transformedPoint.x);
                minPoint.y = std::min(minPoint.y, transformedPoint.y);
                maxPoint.x = std::max(maxPoint.x, transformedPoint.x);
                maxPoint.y = std::max(maxPoint.y, transformedPoint.y);
            });
    }

    if (!hasBounds)
    {
        return {};
    }

    return {
        (minPoint.x + maxPoint.x) * 0.5f,
        (minPoint.y + maxPoint.y) * 0.5f};
}

/**
 * @brief Rebuilds one transform while keeping the same local anchor at the same world position.
 */
mfd::Transform2D BuildTransformKeepingLocalPointWorldPosition(const mfd::Transform2D& startTransform,
                                                              const mfd::Vec2 localPoint,
                                                              const float rotationDegrees,
                                                              const mfd::Vec2 scale)
{
    const mfd::Vec2 worldPoint = mfd::ApplyTransform(localPoint, startTransform);
    const mfd::Vec2 offset = mfd::Rotate(mfd::Scale(localPoint, scale), rotationDegrees);

    return mfd::Transform2D {
        worldPoint - offset,
        rotationDegrees,
        scale};
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
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Commands UDP");

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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Feedback UDP");

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

    if (ImGui::Button("Remove page from window"))
    {
        OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, documentState_.selection.pageIndex);
        return;
    }
    ShowItemTooltip("Remove the currently selected page from this window while keeping its authored JSON file.");

    ImGui::SameLine();
    if (ImGui::Button("Delete page asset..."))
    {
        OpenPageManagementPopup(PageManagementAction::DeleteAsset, documentState_.selection.pageIndex);
        return;
    }
    ShowItemTooltip("Remove the page from this window and mark its JSON file for deletion on the next save.");

    ImGui::SameLine();
    if (ImGui::Button("Rename page globally..."))
    {
        OpenPageRenamePopup(documentState_.selection.pageIndex);
        return;
    }
    ShowItemTooltip("Rename this page asset safely across the current asset tree and update every referenced window defaultPage.");

    ImGui::SameLine();
    ImGui::TextDisabled("Shortcut: Suppr opens the delete confirmation");

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

    DrawPageBlinkInspector(*page);
    DrawPageLayerInspector(*page);
    DrawPageDynamicTemplateInspector(*page);
    DrawPageStrobeInspector(*page);

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
            activeTutorialTemplate->targetId.data(),
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
        tutorialNameTargetId.data(),
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
        tutorialTemplateTargetId.data(),
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
        tutorialAddTargetId.data(),
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

void EditorApplication::MoveSelectedPageReticleToIndex(mfd::PageDefinition& page,
                                                       mfd::ReticleGroup& reticle,
                                                       const int targetIndex,
                                                       const char* const action)
{
    const int reticleIndex = documentState_.selection.pageReticleIndex;
    if (reticleIndex < 0 ||
        reticleIndex >= static_cast<int>(page.staticReticles.size()) ||
        targetIndex < 0 ||
        targetIndex >= static_cast<int>(page.staticReticles.size()) ||
        targetIndex == reticleIndex)
    {
        return;
    }

    PushUndoSnapshot();
    const std::string movedReticleId = reticle.id;

    auto movedReticle =
        std::move(page.staticReticles[static_cast<std::size_t>(reticleIndex)]);
    page.staticReticles.erase(page.staticReticles.begin() + reticleIndex);

    const int insertionIndex =
        std::clamp(targetIndex, 0, static_cast<int>(page.staticReticles.size()));

    page.staticReticles.insert(page.staticReticles.begin() + insertionIndex, std::move(movedReticle));
    SelectPageReticle(documentState_.selection.pageIndex, insertionIndex);
    RebuildStatus("Reticle '" + movedReticleId + "' moved " + action + " on page '" + page.name + "'.", false);
}

void EditorApplication::ApplyPageReticleIdEdit(mfd::PageDefinition& page,
                                               mfd::ReticleGroup& reticle,
                                               const std::string_view requestedId)
{
    const std::string previousId = reticle.id;
    reticle.id = MakeUniquePageReticleId(page, requestedId, previousId);

    if (tutorial_->TrackedReticleId() == previousId)
    {
        tutorial_->SetTrackedReticleId(reticle.id);
    }
}

void EditorApplication::DrawPageReticleInspector()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        ImGui::TextDisabled("No page reticle selected.");
        return;
    }

    if (selectedIndices.size() > 1U)
    {
        ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page reticles");
        ImGui::Text("Page: %s", page->name.c_str());
        ImGui::Text("%d reticles selected", static_cast<int>(selectedIndices.size()));
        ImGui::TextDisabled("Ctrl+click in the page or in the tree to add or remove reticles from the selection.");
        ImGui::Separator();

        if (AccentButton("Copy selection"))
        {
            CopySelectedPageReticles();
        }
        ShowItemTooltip("Copy all selected page reticle instances.");

        ImGui::SameLine();
        if (ImGui::Button("Cut selection"))
        {
            CutSelectedPageReticles();
            return;
        }
        ShowItemTooltip("Copy all selected page reticle instances, then remove them from the page.");

        ImGui::SameLine();
        if (ImGui::Button("Delete from page"))
        {
            DeleteSelection();
            return;
        }
        ShowItemTooltip("Delete all selected reticles from the active page.");

        ImGui::SameLine();
        ImGui::BeginDisabled(clipboardState_.pageReticleClipboard.empty());
        if (ImGui::Button("Paste copies"))
        {
            PasteCopiedPageReticles();
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Extract as reticle..."))
    {
        if (tutorial_->MatchesTarget("page_reticle_extract"))
        {
            tutorial_->AdvancePhase();
        }
        OpenReticleExtractionPopup();
        return;
    }
    ShowItemTooltip("Replace the current selection with one reusable reticle template staged in the shared library.");
    tutorial_->DrawHalo(
        "page_reticle_extract",
        "Click Extract as reticle...",
        "Open the extraction workflow to review how page content can become one reusable library template.");

        ImGui::TextDisabled("Shortcuts: Ctrl+C, Ctrl+X, Ctrl+V, Suppr, Esc");
        ImGui::TextDisabled("Drag one selected reticle in the preview to move the whole group.");
        ImGui::TextDisabled("Direct property editing stays available when a single reticle is selected.");
        return;
    }

    mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr)
    {
        ImGui::TextDisabled("No page reticle selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page reticle");
    ImGui::Text("Page: %s", page->name.c_str());
    std::array<char, 128> reticleId {};
    CopyTextBuffer(reticleId, reticle->id);
    const bool reticleIdChanged = ImGui::InputText("Reticle id", reticleId.data(), reticleId.size());
    ShowItemTooltip("Unique page-local reticle instance id stored in the page JSON. The shared template id stays unchanged.");
    tutorial_->DrawHalo(
        "page_reticle_id",
        "Rename this page reticle",
        "Rename the Page1 ownship instance to `page1_ownship` without changing the shared template id `mfd_tutorial_aircraft`.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (reticleIdChanged)
    {
        ApplyPageReticleIdEdit(*page, *reticle, reticleId.data());
    }
    if (tutorial_->MatchesTarget("page_reticle_id") &&
        page->name == "Page1" &&
        reticle->id == kTutorialPage1OwnshipReticleId)
    {
        tutorial_->CompleteStep();
        return;
    }
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::Separator();

    const int reticleIndex = documentState_.selection.pageReticleIndex;
    const int lastReticleIndex = static_cast<int>(page->staticReticles.size()) - 1;

    if (ImGui::Button("Delete from page"))
    {
        DeleteSelection();
        return;
    }
    ShowItemTooltip("Delete this reticle instance from the active page.");

    ImGui::SameLine();
    if (AccentButton("Copy"))
    {
        CopySelectedPageReticles();
    }
    ShowItemTooltip("Copy this page reticle instance.");

    ImGui::SameLine();
    if (ImGui::Button("Cut"))
    {
        CutSelectedPageReticles();
        return;
    }
    ShowItemTooltip("Copy this page reticle instance, then remove it from the page.");

    ImGui::SameLine();
    ImGui::BeginDisabled(clipboardState_.pageReticleClipboard.empty());
    if (ImGui::Button("Paste copies"))
    {
        PasteCopiedPageReticles();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste copied page reticles onto the active page.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Extract as reticle..."))
    {
        if (tutorial_->MatchesTarget("page_reticle_extract"))
        {
            tutorial_->AdvancePhase();
        }
        OpenReticleExtractionPopup();
        return;
    }
    ShowItemTooltip("Extract this page reticle as a reusable library template, then replace it with one template instance.");
    tutorial_->DrawHalo(
        "page_reticle_extract",
        "Click Extract as reticle...",
        "Open the extraction workflow to review how one page reticle can be promoted into the shared library.");

    if (!reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end() &&
        ImGui::Button("Edit source template"))
    {
        SelectLibraryReticle(reticle->sourceTemplateId);
        RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
        return;
    }
    if (!reticle->sourceTemplateId.empty() && documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end())
    {
        ShowItemTooltip("Open the shared template that this page reticle instance was created from.");
    }

    if (!reticle->sourceTemplateId.empty() && documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end())
    {
        ImGui::SameLine();
    }

    ImGui::TextDisabled("Shortcut: Suppr");
    ImGui::TextDisabled("Cut / copy / paste: Ctrl+X / Ctrl+C / Ctrl+V");
    ImGui::TextDisabled("Esc clears the current page-reticle selection.");
    ImGui::TextDisabled("Draw order: %d / %d", reticleIndex + 1, std::max(1, static_cast<int>(page->staticReticles.size())));

    const std::string currentLayerLabel = reticle->layerId.empty() ? std::string {"<missing>"} : reticle->layerId;
    if (ImGui::BeginCombo("Editor layer", currentLayerLabel.c_str()))
    {
        for (const auto& layer : page->editor.layers)
        {
            const bool selected = reticle->layerId == layer.id;
            const std::string label = layer.id + (layer.visible ? "" : " (hidden)");
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                reticle->layerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Assign this page reticle to an editor-only layer.");
    ImGui::SameLine();
    if (ImGui::Button("Page layers..."))
    {
        SelectPage(documentState_.selection.pageIndex);
        RebuildStatus("Layer editor opened for page '" + page->name + "'.", false);
        return;
    }
    ShowItemTooltip("Open the page inspector to edit the available editor-only layers.");
    ImGui::TextDisabled("Hidden layers stay editable in the inspector but are not rendered in the editor preview.");

    const bool canMoveBackward = reticleIndex > 0;
    const bool canMoveForward = reticleIndex >= 0 && reticleIndex < lastReticleIndex;

    ImGui::BeginDisabled(!canMoveBackward);
    if (ImGui::Button("Send to back", ImVec2(130.0f, 0.0f)))
    {
        MoveSelectedPageReticleToIndex(*page, *reticle, 0, "to the back");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle to the first draw-order slot on the page.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveBackward);
    if (ImGui::Button("Step back", ImVec2(110.0f, 0.0f)))
    {
        MoveSelectedPageReticleToIndex(*page, *reticle, reticleIndex - 1, "backward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step earlier in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Step forward", ImVec2(130.0f, 0.0f)))
    {
        MoveSelectedPageReticleToIndex(*page, *reticle, reticleIndex + 1, "forward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step later in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Bring to front", ImVec2(130.0f, 0.0f)))
    {
        MoveSelectedPageReticleToIndex(*page, *reticle, lastReticleIndex, "to the front");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle to the last draw-order slot on the page.");
    ImGui::EndDisabled();

    ImGui::Separator();

    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this page reticle instance is rendered.");
    }

    {
        bool drawOnTop = reticle->drawOnTop;
        if (ImGui::Checkbox("Draw on top", &drawOnTop))
        {
            PushUndoSnapshot();
            reticle->drawOnTop = drawOnTop;
        }
        ShowItemTooltip("Render this page reticle after regular page reticles while keeping the strobe on top.");
    }

    DrawPageReticleBlinkInspector(*page, *reticle);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Clipping");
    const std::vector<ClipPrimitiveOption> clipOptions = CollectClipPrimitiveOptions(*reticle);
    if (clipOptions.empty())
    {
        ImGui::TextDisabled("No supported convex primitive with an id is available for clipping.");
        ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
    }
    else
    {
        std::string currentClipPrimitiveLabel = reticle->clipping.primitiveId.empty()
                                                    ? std::string {"<select primitive>"}
                                                    : std::string {"<missing primitive>"};
        for (const auto& option : clipOptions)
        {
            if (option.primitiveId == reticle->clipping.primitiveId)
            {
                currentClipPrimitiveLabel = option.label;
                break;
            }
        }

        if (ImGui::BeginCombo("Clip primitive", currentClipPrimitiveLabel.c_str()))
        {
            for (const auto& option : clipOptions)
            {
                const bool selected = option.primitiveId == reticle->clipping.primitiveId;
                if (ImGui::Selectable(option.label.c_str(), selected))
                {
                    ApplyPageReticleClipping(documentState_.selection.pageReticleIndex, reticle->clipping.mode, option.primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which convex primitive erases the inside or the outside toward the page background.");

        const char* currentModeLabel = ReticleClipModeLabel(reticle->clipping.mode);
        if (ImGui::BeginCombo("Clip mode", currentModeLabel))
        {
            const std::array modes {
                mfd::ReticleClipMode::None,
                mfd::ReticleClipMode::Inner,
                mfd::ReticleClipMode::Outer};

            for (const mfd::ReticleClipMode mode : modes)
            {
                const bool selected = reticle->clipping.mode == mode;
                if (ImGui::Selectable(ReticleClipModeLabel(mode), selected))
                {
                    const std::string primitiveId =
                        reticle->clipping.primitiveId.empty() ? clipOptions.front().primitiveId : reticle->clipping.primitiveId;
                    ApplyPageReticleClipping(documentState_.selection.pageReticleIndex, mode, primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip(
            "Inner clipping erases the inside of the selected shape. Outer clipping erases everything outside it.");

        if (reticle->clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(*reticle) == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.42f, 1.0f), "The current clip primitive is missing or unsupported.");
        }

        ImGui::TextDisabled("The selected primitive erases toward the page background when this reticle is drawn.");
    }

    const bool positionChanged = ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip("Logical position of this page reticle on the active page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        reticle->transform.position.x = std::clamp(reticle->transform.position.x, -1.0f, 1.0f);
        reticle->transform.position.y = std::clamp(reticle->transform.position.y, -1.0f, 1.0f);
    }

    const mfd::Transform2D rotationStartTransform = reticle->transform;
    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            ReticleVisualCenterLocal(*reticle),
            reticle->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the reticle visual center.");

    const mfd::Transform2D scaleStartTransform = reticle->transform;
    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            ReticleVisualCenterLocal(*reticle),
            scaleStartTransform.rotationDegrees,
            reticle->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to this page reticle instance.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Override the template stroke color for this page reticle instance.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Override the template stroke thickness for this page reticle instance.");

    for (auto& primitive : reticle->primitives)
    {
        auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry);
        if ((text == nullptr && time == nullptr) || primitive.id.empty())
        {
            continue;
        }

        if (text != nullptr)
        {
            std::array<char, 128> buffer {};
            CopyTextBuffer(buffer, text->text);
            const std::string label = "Text##" + primitive.id;
            const bool changed = ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
            ShowItemTooltip("Override the literal text for this text primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (changed)
            {
                text->text = buffer.data();
            }

            float letterSpacing = text->letterSpacing;
            const std::string spacingLabel = "Letter spacing##" + primitive.id;
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the letter spacing for this text primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                text->letterSpacing = letterSpacing;
            }
            continue;
        }

        if (time != nullptr)
        {
            std::array<char, 128> format {};
            CopyTextBuffer(format, time->format);
            const std::string formatLabel = "Time format##" + primitive.id;
            const bool formatChanged = ImGui::InputText(formatLabel.c_str(), format.data(), format.size());
            ShowItemTooltip("Override the strftime-style format used by this time primitive.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (formatChanged)
            {
                time->format = format.data();
            }

            bool utc = time->utc;
            const std::string utcLabel = "UTC##" + primitive.id;
            if (ImGui::Checkbox(utcLabel.c_str(), &utc))
            {
                PushUndoSnapshot();
                time->utc = utc;
            }
            ShowItemTooltip("Render this time primitive in UTC instead of local time.");

            float letterSpacing = time->letterSpacing;
            const std::string spacingLabel = "Letter spacing##" + primitive.id;
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the character spacing for this time primitive on the page instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                time->letterSpacing = letterSpacing;
            }
        }
    }

}

mfd::Vec2 EditorApplication::PageTitleVisualCenterLocal(const mfd::PageDefinition& page) const
{
    return ReticleVisualCenterLocal(BuildPageTitlePreviewReticle(page));
}

void EditorApplication::DrawSelectedPageTitleInspector()
{
    mfd::PageDefinition* page = ActivePage();
    mfd::PageTitleDisplayDefinition* titleDisplay = SelectedPageTitleDisplay();
    if (page == nullptr || titleDisplay == nullptr)
    {
        ImGui::TextDisabled("No page title selected.");
        return;
    }

    const std::string displayedTitle = mfd::ResolvePageDisplayTitleText(page->name, page->title);
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page title");
    ImGui::Text("Page: %s", page->name.c_str());
    ImGui::TextWrapped("Displayed text: %s", displayedTitle.c_str());
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::TextDisabled("Edit the title text from the page inspector.");
    ImGui::Separator();

    bool visible = titleDisplay->visible;
    if (ImGui::Checkbox("Visible", &visible))
    {
        PushUndoSnapshot();
        titleDisplay->visible = visible;
    }
    ShowItemTooltip("Show or hide both the title text and its decoration.");

    if (ImGui::Button("Edit page properties"))
    {
        SelectPage(documentState_.selection.pageIndex, false);
        return;
    }
    ShowItemTooltip("Return to the page inspector to edit the page name and title text.");

    const char* currentDecoration = PageTitleDecorationLabel(titleDisplay->decoration);
    const bool decorationComboOpen = ImGui::BeginCombo("Decoration", currentDecoration);
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("page_title_decoration"))
    {
        tutorial_->AdvancePhase();
    }
    tutorial_->DrawHalo(
        "page_title_decoration",
        "Open Decoration",
        "Open the title decoration chooser. This inspector is the dedicated place to frame, move, scale, hide, or recolor the Page1 title.");
    if (decorationComboOpen)
    {
        const std::array decorations {
            mfd::PageTitleDecoration::Underline,
            mfd::PageTitleDecoration::Frame,
            mfd::PageTitleDecoration::None};

        for (const mfd::PageTitleDecoration decoration : decorations)
        {
            const bool selected = titleDisplay->decoration == decoration;
            if (ImGui::Selectable(PageTitleDecorationLabel(decoration), selected) && !selected)
            {
                PushUndoSnapshot();
                titleDisplay->decoration = decoration;
                if (page->name == "Page1" &&
                    decoration == mfd::PageTitleDecoration::Frame &&
                    tutorial_->MatchesTarget("page_title_decoration_frame"))
                {
                    tutorial_->CompleteStep();
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
            if (decoration == mfd::PageTitleDecoration::Frame)
            {
                tutorial_->DrawHalo(
                    "page_title_decoration_frame",
                    "Choose Frame",
                    "Frame the Page1 title so the generated chrome becomes a boxed heading. You can still tune its color, line style, and transform afterwards.");
                if (selected &&
                    page->name == "Page1" &&
                    tutorial_->MatchesTarget("page_title_decoration_frame"))
                {
                    tutorial_->CompleteStep();
                }
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose whether the title is underlined, boxed, or rendered without decoration.");

    ImVec4 color = ToImGuiColor(titleDisplay->color);
    if (ImGui::ColorEdit4("Color", &color.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->color = ToColorRgba(color);
    }
    ShowItemTooltip("Shared color applied to the title text and its decoration.");

    ImGui::BeginDisabled(titleDisplay->decoration == mfd::PageTitleDecoration::None);
    const char* currentLineStyle = LineStyleLabel(titleDisplay->lineStyle);
    if (ImGui::BeginCombo("Line style", currentLineStyle))
    {
        const std::array lineStyles {
            mfd::LineStyle::Solid,
            mfd::LineStyle::Dotted,
            mfd::LineStyle::Dashed};

        for (const mfd::LineStyle lineStyle : lineStyles)
        {
            const bool selected = titleDisplay->lineStyle == lineStyle;
            if (ImGui::Selectable(LineStyleLabel(lineStyle), selected) && !selected)
            {
                PushUndoSnapshot();
                titleDisplay->lineStyle = lineStyle;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Outline pattern used by the underline or frame.");

    float lineWidth = titleDisplay->lineWidth;
    if (ImGui::DragFloat("Line width", &lineWidth, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->lineWidth = std::max(0.0005f, lineWidth);
    }
    ShowItemTooltip("Stroke thickness used by the underline or the frame.");
    ImGui::EndDisabled();

    const bool positionChanged =
        ImGui::DragFloat2("Position", &titleDisplay->transform.position.x, 0.01f, -4.0f, 4.0f, "%.3f");
    ShowItemTooltip("Logical page-space anchor of the title chrome.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        if (!std::isfinite(titleDisplay->transform.position.x))
        {
            titleDisplay->transform.position.x = 0.0f;
        }
        if (!std::isfinite(titleDisplay->transform.position.y))
        {
            titleDisplay->transform.position.y = 0.0f;
        }
    }

    const mfd::Transform2D rotationStartTransform = titleDisplay->transform;
    if (ImGui::DragFloat("Rotation", &titleDisplay->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            PageTitleVisualCenterLocal(*page),
            titleDisplay->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the title chrome visual center.");

    const mfd::Transform2D scaleStartTransform = titleDisplay->transform;
    if (ImGui::DragFloat2("Scale", &titleDisplay->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        titleDisplay->transform.scale.x = std::max(0.05f, std::abs(titleDisplay->transform.scale.x));
        titleDisplay->transform.scale.y = std::max(0.05f, std::abs(titleDisplay->transform.scale.y));
        titleDisplay->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            PageTitleVisualCenterLocal(*page),
            scaleStartTransform.rotationDegrees,
            titleDisplay->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to the generated title chrome.");
}

void EditorApplication::ApplySelectedPageStrobeClipping(mfd::ReticleGroup& reticle,
                                                        const mfd::ReticleClipMode mode,
                                                        std::string primitiveId)
{
    if (primitiveId.empty())
    {
        primitiveId = reticle.clipping.primitiveId;
    }

    if (reticle.clipping.mode == mode && reticle.clipping.primitiveId == primitiveId)
    {
        return;
    }

    PushUndoSnapshot();
    reticle.clipping.mode = mode;
    reticle.clipping.primitiveId = std::move(primitiveId);
    if (mode == mfd::ReticleClipMode::None)
    {
        RebuildStatus("Clipping disabled for page strobe '" + reticle.id + "'.", false);
    }
    else
    {
        RebuildStatus(std::string(ReticleClipModeLabel(mode)) + " enabled on primitive '" +
                          reticle.clipping.primitiveId + "' for page strobe '" + reticle.id + "'.",
                      false);
    }
}

void EditorApplication::DrawSelectedPageStrobeInspector()
{
    mfd::PageDefinition* page = ActivePage();
    mfd::ReticleGroup* reticle = SelectedPageStrobeReticle();
    const mfd::PageStrobeDefinition* strobe = SelectedPageStrobe();
    if (page == nullptr || reticle == nullptr || strobe == nullptr)
    {
        ImGui::TextDisabled("No page strobe selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page strobe");
    ImGui::Text("Page: %s", page->name.c_str());
    ImGui::Text("Name: %s", strobe->name.c_str());
    ImGui::Text("Strobe id: %s", reticle->id.c_str());
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::Separator();

    if (!reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end() &&
        ImGui::Button("Edit source template"))
    {
        SelectLibraryReticle(reticle->sourceTemplateId);
        RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
        return;
    }
    if (!reticle->sourceTemplateId.empty() &&
        documentState_.loaded.document.reticleLibrary.find(reticle->sourceTemplateId) != documentState_.loaded.document.reticleLibrary.end())
    {
        ShowItemTooltip("Open the shared template that this page strobe instance was created from.");
        ImGui::SameLine();
    }

    ImGui::TextDisabled("Shortcut: Suppr removes the page strobe.");
    ImGui::TextDisabled("Esc clears the current page-strobe selection.");
    ImGui::TextDisabled("The page strobe renders after regular page reticles.");

    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this page strobe instance is rendered.");
    }

    DrawPageReticleBlinkInspector(*page, *reticle);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Clipping");
    const std::vector<ClipPrimitiveOption> clipOptions = CollectClipPrimitiveOptions(*reticle);

    if (clipOptions.empty())
    {
        ImGui::TextDisabled("No supported convex primitive with an id is available for clipping.");
        ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
    }
    else
    {
        std::string currentClipPrimitiveLabel = reticle->clipping.primitiveId.empty()
                                                    ? std::string {"<select primitive>"}
                                                    : std::string {"<missing primitive>"};
        for (const auto& option : clipOptions)
        {
            if (option.primitiveId == reticle->clipping.primitiveId)
            {
                currentClipPrimitiveLabel = option.label;
                break;
            }
        }

        if (ImGui::BeginCombo("Clip primitive", currentClipPrimitiveLabel.c_str()))
        {
            for (const auto& option : clipOptions)
            {
                const bool selected = option.primitiveId == reticle->clipping.primitiveId;
                if (ImGui::Selectable(option.label.c_str(), selected))
                {
                    ApplySelectedPageStrobeClipping(*reticle, reticle->clipping.mode, option.primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose which convex primitive erases the inside or the outside toward the page background.");

        const char* currentModeLabel = ReticleClipModeLabel(reticle->clipping.mode);
        if (ImGui::BeginCombo("Clip mode", currentModeLabel))
        {
            const std::array modes {
                mfd::ReticleClipMode::None,
                mfd::ReticleClipMode::Inner,
                mfd::ReticleClipMode::Outer};

            for (const mfd::ReticleClipMode mode : modes)
            {
                const bool selected = reticle->clipping.mode == mode;
                if (ImGui::Selectable(ReticleClipModeLabel(mode), selected))
                {
                    const std::string primitiveId =
                        reticle->clipping.primitiveId.empty() ? clipOptions.front().primitiveId : reticle->clipping.primitiveId;
                    ApplySelectedPageStrobeClipping(*reticle, mode, primitiveId);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ShowItemTooltip(
            "Inner clipping erases the inside of the selected shape. Outer clipping erases everything outside it.");

        if (reticle->clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(*reticle) == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.42f, 1.0f), "The current clip primitive is missing or unsupported.");
        }

        ImGui::TextDisabled("The selected primitive erases toward the page background when this strobe is drawn.");
    }

    const bool positionChanged = ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f");
    ShowItemTooltip("Logical position of this page strobe on the active page.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (positionChanged)
    {
        reticle->transform.position.x = std::clamp(reticle->transform.position.x, -1.0f, 1.0f);
        reticle->transform.position.y = std::clamp(reticle->transform.position.y, -1.0f, 1.0f);
    }

    const mfd::Transform2D rotationStartTransform = reticle->transform;
    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            rotationStartTransform,
            ReticleVisualCenterLocal(*reticle),
            reticle->transform.rotationDegrees,
            rotationStartTransform.scale);
    }
    ShowItemTooltip("Rotation in degrees around the strobe visual center.");

    const mfd::Transform2D scaleStartTransform = reticle->transform;
    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
        reticle->transform = BuildTransformKeepingLocalPointWorldPosition(
            scaleStartTransform,
            ReticleVisualCenterLocal(*reticle),
            scaleStartTransform.rotationDegrees,
            reticle->transform.scale);
    }
    ShowItemTooltip("Per-axis scale applied to this page strobe instance.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Override the template stroke color for this page strobe instance.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Override the template stroke thickness for this page strobe instance.");

    int unnamedTextPrimitiveIndex = 0;
    int unnamedTimePrimitiveIndex = 0;
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        auto& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry);
        if (text == nullptr && time == nullptr)
        {
            continue;
        }

        if (text != nullptr)
        {
            const bool hasPrimitiveId = !primitive.id.empty();
            const int fallbackIndex = unnamedTextPrimitiveIndex++;
            std::array<char, 128> buffer {};
            CopyTextBuffer(buffer, text->text);
            const std::string label = hasPrimitiveId ? "Text##strobe_" + primitive.id
                                                     : "Text #" + std::to_string(fallbackIndex) + "##strobe_text_" +
                                                           std::to_string(primitiveIndex);
            const bool changed = ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
            ShowItemTooltip("Override the literal text for this text primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (changed)
            {
                text->text = buffer.data();
            }

            float letterSpacing = text->letterSpacing;
            const std::string spacingLabel = hasPrimitiveId ? "Letter spacing##strobe_" + primitive.id
                                                            : "Letter spacing #" + std::to_string(fallbackIndex) +
                                                                  "##strobe_text_spacing_" + std::to_string(primitiveIndex);
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the letter spacing for this text primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                text->letterSpacing = letterSpacing;
            }
            continue;
        }

        if (time != nullptr)
        {
            const bool hasPrimitiveId = !primitive.id.empty();
            const int fallbackIndex = unnamedTimePrimitiveIndex++;
            std::array<char, 128> format {};
            CopyTextBuffer(format, time->format);
            const std::string formatLabel = hasPrimitiveId ? "Time format##strobe_" + primitive.id
                                                           : "Time format #" + std::to_string(fallbackIndex) +
                                                                 "##strobe_time_" + std::to_string(primitiveIndex);
            const bool formatChanged = ImGui::InputText(formatLabel.c_str(), format.data(), format.size());
            ShowItemTooltip("Override the strftime-style format used by this time primitive on the page strobe.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (formatChanged)
            {
                time->format = format.data();
            }

            bool utc = time->utc;
            const std::string utcLabel = hasPrimitiveId ? "UTC##strobe_" + primitive.id
                                                        : "UTC #" + std::to_string(fallbackIndex) + "##strobe_time_utc_" +
                                                              std::to_string(primitiveIndex);
            if (ImGui::Checkbox(utcLabel.c_str(), &utc))
            {
                PushUndoSnapshot();
                time->utc = utc;
            }
            ShowItemTooltip("Render this time primitive in UTC instead of local time.");

            float letterSpacing = time->letterSpacing;
            const std::string spacingLabel = hasPrimitiveId ? "Letter spacing##strobe_" + primitive.id
                                                            : "Letter spacing #" + std::to_string(fallbackIndex) +
                                                                  "##strobe_time_spacing_" +
                                                                  std::to_string(primitiveIndex);
            const bool spacingChanged =
                ImGui::DragFloat(spacingLabel.c_str(), &letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f");
            ShowItemTooltip("Override the character spacing for this time primitive on the page strobe instance.");
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (spacingChanged)
            {
                time->letterSpacing = letterSpacing;
            }
        }
    }

    DrawPageStrobeInspector(*page);
}

void EditorApplication::DrawPageReticleBlinkInspector(mfd::PageDefinition& page, mfd::ReticleGroup& reticle)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.81f, 0.52f, 1.0f), "Blink");
    ImGui::TextDisabled("Blink is managed by the page, not by the reticle template.");

    if (page.blinkTypes.empty())
    {
        ImGui::TextDisabled("This page has no blink type yet. Add one in the page inspector.");
        return;
    }

    bool blinkEnabled = reticle.blink.enabled;
    if (ImGui::Checkbox("Blink enabled", &blinkEnabled))
    {
        PushUndoSnapshot();
        if (!blinkEnabled)
        {
            reticle.blink = {};
        }
        else
        {
            reticle.blink.enabled = true;
        }

        RefreshPageBlinkStateForEditor(page);
    }
    ShowItemTooltip("Enable or disable blinking for this page reticle instance.");

    if (!reticle.blink.enabled)
    {
        ImGui::TextDisabled("Enable blink to use the page default or pick a named page type.");
        return;
    }

    const std::size_t effectiveDefaultIndex = EffectiveDefaultBlinkTypeIndex(page);
    const std::string defaultBlinkName =
        effectiveDefaultIndex == kInvalidBlinkTypeIndex ? std::string {"<none>"} : page.blinkTypes[effectiveDefaultIndex].name;
    const std::string currentSelection =
        reticle.blink.typeName.empty() ? "<page default: " + defaultBlinkName + ">" : reticle.blink.typeName;

    if (ImGui::BeginCombo("Blink type", currentSelection.c_str()))
    {
        const std::string defaultItem = "<page default> - " + defaultBlinkName;
        const bool defaultSelected = reticle.blink.typeName.empty();
        if (ImGui::Selectable(defaultItem.c_str(), defaultSelected))
        {
            PushUndoSnapshot();
            reticle.blink.enabled = true;
            reticle.blink.typeName.clear();
            reticle.blink.normalizedTypeName.clear();
            RefreshPageBlinkStateForEditor(page);
        }
        if (defaultSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const auto& blinkType : page.blinkTypes)
        {
            const bool selected = reticle.blink.normalizedTypeName == blinkType.normalizedName;
            const std::string label =
                blinkType.name + " - " + std::to_string(blinkType.durationMs) + " ms";
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                reticle.blink.enabled = true;
                reticle.blink.typeName = blinkType.name;
                reticle.blink.normalizedTypeName = blinkType.normalizedName;
                RefreshPageBlinkStateForEditor(page);
            }
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose a page-local blink type, or keep using the page default blink.");

    ImGui::TextDisabled("Current effective duration: %u ms", reticle.blink.durationMs);
}

void EditorApplication::DrawLibraryReticleInspector()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        ImGui::TextDisabled("Select a library reticle.");
        return;
    }

    const ScopedImGuiId scopedId("LibraryReticleInspector");

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Library reticle");
    ImGui::Text("Template id: %s", reticle->id.c_str());
    if (const auto fileIt = documentState_.files.templateFiles.find(reticle->id); fileIt != documentState_.files.templateFiles.end())
    {
        ImGui::TextDisabled("File: %s", fileIt->second.filename().string().c_str());
    }
    ImGui::TextDisabled("Drag this reticle from the library tree to the page preview, or edit it directly in the reticle studio.");

    const bool canAddToPage = ActivePage() != nullptr;
    if (!canAddToPage)
    {
        ImGui::BeginDisabled();
    }
    if (AccentButton("Add to active page"))
    {
        const bool tutorialAddMatched = tutorial_->MatchesTarget("library_add_to_page");
        const mfd::PageDefinition* page = ActivePage();
        const mfd::Vec2 dropPosition = page == nullptr ? mfd::Vec2 {} : layoutState_.pagePreviewView.center;
        std::string tutorialError;
        if (tutorialAddMatched && !tutorial_->ValidateAddToPage(page, *reticle, tutorialError))
        {
            RebuildStatus(tutorialError, true);
            return;
        }
        if (CreatePageReticleInstanceFromTemplate(reticle->id, dropPosition) && tutorialAddMatched)
        {
            if (const mfd::ReticleGroup* createdReticle = SelectedPageReticle(); createdReticle != nullptr)
            {
                tutorial_->SetTrackedReticleId(createdReticle->id);
            }
            tutorial_->CompleteStep();
        }
    }
    ShowItemTooltip("Instantiate this template on the active page at the current editor camera center.");
    tutorial_->DrawHalo(
        "library_add_to_page",
        "Click Add to active page",
        tutorial_->LibraryAddToPageHaloReason().data());
    if (!canAddToPage)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (AccentButton("Copy"))
    {
        CopySelectedLibraryReticle();
    }
    ShowItemTooltip("Copy this shared reticle template.");

    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboardState_.libraryReticleClipboard.has_value());
    if (ImGui::Button("Paste copy"))
    {
        PasteCopiedLibraryReticle();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste the copied shared reticle template as one new library entry.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Rename reticle globally..."))
    {
        OpenReticleRenamePopup(reticle->id);
        return;
    }
    ShowItemTooltip("Rename this shared reticle template safely across the current asset tree and every page that references it.");

    ImGui::SameLine();
    if (ImGui::Button("Delete library reticle"))
    {
        DeleteSelectedLibraryReticle();
        return;
    }
    ShowItemTooltip("Delete this shared reticle template from the library.");

    ImGui::TextDisabled("Template shortcuts: Ctrl+C / Ctrl+V while the reticle stays focused.");

    ImGui::Separator();

    {
        bool visible = reticle->visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            reticle->visible = visible;
        }
        ShowItemTooltip("Toggle whether this template is visible by default.");
    }

    {
        bool drawOnTop = reticle->drawOnTop;
        if (ImGui::Checkbox("Draw on top", &drawOnTop))
        {
            PushUndoSnapshot();
            reticle->drawOnTop = drawOnTop;
        }
        ShowItemTooltip("Default draw tier used when this template is instantiated on a page.");
    }

    if (ImGui::DragFloat2("Position", &reticle->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Default logical position inside the reticle template.");

    if (ImGui::DragFloat("Rotation", &reticle->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Default template rotation in degrees.");

    if (ImGui::DragFloat2("Scale", &reticle->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->transform.scale.x = std::max(0.05f, reticle->transform.scale.x);
        reticle->transform.scale.y = std::max(0.05f, reticle->transform.scale.y);
    }
    ShowItemTooltip("Default per-axis scale applied to this template.");

    ImVec4 stroke = ToImGuiColor(reticle->overrides.color.value_or(mfd::ColorRgba {0, 255, 102, 255}));
    if (ImGui::ColorEdit4("Default stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Default stroke color inherited by page instances unless they override it.");

    float thickness = reticle->overrides.thickness.value_or(0.0042f);
    if (ImGui::DragFloat("Default thickness", &thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        reticle->overrides.thickness = std::max(0.0005f, thickness);
    }
    ShowItemTooltip("Default stroke thickness inherited by page instances unless they override it.");

    if (ReticleHasFillCapablePrimitive(*reticle))
    {
        const mfd::ColorRgba fallbackFillColor =
            VisibleFillColorFromStroke(reticle->overrides.color.value_or(mfd::PrimitiveStyle {}.color));
        ImVec4 fill = ToImGuiColor(reticle->overrides.fillColor.value_or(fallbackFillColor));
        if (ImGui::ColorEdit4("Default fill", &fill.x))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            reticle->overrides.fillColor = ToColorRgba(fill);
        }
        ShowItemTooltip("Default fill color inherited by fill-capable primitives unless they override it locally.");

        bool filled = reticle->overrides.filled.value_or(false);
        if (ImGui::Checkbox("Default filled", &filled))
        {
            PushUndoSnapshot();
            reticle->overrides.filled = filled;
            SeedReticleFillOverrideIfNeeded(
                reticle->overrides,
                reticle->overrides.color.value_or(mfd::PrimitiveStyle {}.color));
        }
        ShowItemTooltip("Default filled state inherited by fill-capable primitives unless they override it locally.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Primitives");
    ImGui::TextDisabled("Click a primitive below or directly in the studio preview to focus and edit it.");

    ImGui::BeginChild("PrimitiveCatalog", ImVec2(0.0f, 210.0f), true);
    for (int index = 0; index < static_cast<int>(reticle->primitives.size()); ++index)
    {
        const auto& primitive = reticle->primitives[static_cast<std::size_t>(index)];
        const bool selected = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                              documentState_.selection.libraryReticleId == reticle->id &&
                              documentState_.selection.primitiveIndex == index;
        const std::string header =
            std::to_string(index + 1) + ". " +
            (primitive.id.empty() ? PrimitiveTypeLabel(primitive.type) : primitive.id);
        if (ImGui::Selectable((header + "##primitive_" + std::to_string(index)).c_str(), selected))
        {
            SelectLibraryPrimitive(reticle->id, index);
        }
        ShowItemTooltip("Click to focus this primitive in the inspector and the reticle studio.");

        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", PrimitiveTypeLabel(primitive.type).c_str());
    }
    ImGui::EndChild();

    const bool hasSelectedPrimitive = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                                      documentState_.selection.libraryReticleId == reticle->id &&
                                      SelectedLibraryPrimitive() != nullptr;
    ImGui::BeginDisabled(!hasSelectedPrimitive);
    if (AccentButton("Copy selected primitive"))
    {
        CopySelectedLibraryPrimitive();
    }
    ShowItemTooltip("Copy the focused primitive into the reticle-studio clipboard.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboardState_.libraryPrimitiveClipboard.has_value());
    if (ImGui::Button("Paste copied primitive"))
    {
        PasteCopiedLibraryPrimitive();
    }
    ShowItemTooltip("Paste the copied primitive into this reticle template.");
    ImGui::EndDisabled();

    if (hasSelectedPrimitive || clipboardState_.libraryPrimitiveClipboard.has_value())
    {
        ImGui::TextDisabled("Primitive shortcuts: Ctrl+C / Ctrl+V");
    }

    if (ImGui::BeginCombo("Add primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)]).c_str()))
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(),
                                  workflowState_.newLibraryReticleDraft.primitiveTypeIndex == index))
            {
                workflowState_.newLibraryReticleDraft.primitiveTypeIndex = index;
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the primitive type that will be appended to this reticle.");

    if (AccentButton("Append primitive"))
    {
        const bool tutorialAppendMatched = tutorial_->MatchesTarget("library_append_primitive");
        const mfd::PrimitiveType primitiveType =
            kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)];
        if (tutorialAppendMatched)
        {
            std::string tutorialError;
            if (!tutorial_->ValidateAppendPrimitive(*reticle, primitiveType, tutorialError))
            {
                RebuildStatus(tutorialError, true);
                return;
            }
        }

        PushUndoSnapshot();
        mfd::ReticleGroup seed = MakePrimitiveReticle("seed", primitiveType);
        mfd::Primitive primitive = seed.primitives.front();
        primitive.id = "primitive_" + std::to_string(reticle->primitives.size() + 1);
        if (tutorialAppendMatched)
        {
            tutorial_->ConfigureAppendedPrimitive(primitive);
        }
        reticle->primitives.push_back(std::move(primitive));
        SelectLibraryPrimitive(reticle->id, static_cast<int>(reticle->primitives.size()) - 1);
        if (tutorialAppendMatched)
        {
            tutorial_->CompleteStep();
        }
    }
    ShowItemTooltip("Append a new primitive of the selected type to this reticle.");
    tutorial_->DrawHalo(
        "library_append_primitive",
        "Click Append primitive",
        tutorial_->LibraryAppendPrimitiveHaloReason().data());

    ImGui::SameLine();
    if (ImGui::Button("Remove selected primitive"))
    {
        mfd::Primitive* primitive = SelectedLibraryPrimitive();
        if (primitive != nullptr)
        {
            PushUndoSnapshot();
            reticle->primitives.erase(reticle->primitives.begin() + documentState_.selection.primitiveIndex);
            documentState_.selection.kind = SelectionKind::LibraryReticle;
            documentState_.selection.primitiveIndex = -1;
        }
    }
    ShowItemTooltip("Delete the currently selected primitive from this reticle.");

}

void EditorApplication::EditPointArrayField(const char* const label, mfd::Vec2& value)
{
    if (ImGui::DragFloat2(label, &value.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
}

void EditorApplication::DrawLibraryPrimitiveInspector()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    mfd::Primitive* primitive = SelectedLibraryPrimitive();
    if (reticle == nullptr || primitive == nullptr)
    {
        ImGui::TextDisabled("Select a primitive inside a library reticle.");
        return;
    }

    const ScopedImGuiId scopedId("LibraryPrimitiveInspector");

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Primitive");
    ImGui::TextDisabled("Green handle moves the primitive. Orange handles edit geometry directly in the studio.");
    if (AccentButton("Copy primitive"))
    {
        CopySelectedLibraryPrimitive();
    }
    ShowItemTooltip("Copy this primitive into the reticle-studio clipboard.");

    ImGui::SameLine();
    ImGui::BeginDisabled(!clipboardState_.libraryPrimitiveClipboard.has_value());
    if (ImGui::Button("Paste copied primitive"))
    {
        PasteCopiedLibraryPrimitive();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste the copied primitive into the current reticle template.");
    ImGui::EndDisabled();
    ImGui::TextDisabled("Shortcut: Ctrl+C / Ctrl+V");

    std::array<char, 128> primitiveId {};
    CopyTextBuffer(primitiveId, primitive->id);
    const bool idChanged = ImGui::InputText("Primitive id", primitiveId.data(), primitiveId.size());
    ShowItemTooltip("Primitive identifier stored in the template JSON.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (idChanged)
    {
        primitive->id = primitiveId.data();
    }

    ImGui::TextDisabled("Type: %s", PrimitiveTypeLabel(primitive->type).c_str());
    {
        bool visible = primitive->style.visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            primitive->style.visible = visible;
        }
        ShowItemTooltip("Toggle whether this primitive is rendered inside the template.");
    }

    {
        bool exposed = primitive->exposed;
        const bool tutorialExposedSelected =
            tutorial_->IsExposedPrimitiveTutorialSelection(documentState_.selection.libraryReticleId, primitive->id);
        const bool tutorialAlternativeStrobeLabelSelected =
            tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id);
        if (ImGui::Checkbox("Exposed", &exposed))
        {
            PushUndoSnapshot();
            primitive->exposed = exposed;

            if (exposed && tutorial_->MatchesTarget("primitive_exposed_checkbox") && tutorialExposedSelected)
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Expose this primitive through the generated client API so runtime code can drive it directly.");
        if (tutorialExposedSelected)
        {
            tutorial_->DrawHalo(
                "primitive_exposed_checkbox",
                "Enable Exposed",
                tutorialAlternativeStrobeLabelSelected
                    ? "Expose the aircraft label so the generated API can mutate one primitive on the active Page1 strobe."
                    : "Expose the fill rectangle so the generated API can animate the progress bar without raw ids.");
        }
    }

    {
        bool rotationSensitive = primitive->reticleRotationSensitive;
        if (ImGui::Checkbox("Affected by reticle rotation", &rotationSensitive))
        {
            PushUndoSnapshot();
            primitive->reticleRotationSensitive = rotationSensitive;

            if (!rotationSensitive &&
                tutorial_->MatchesTarget("primitive_reticle_rotation_checkbox") &&
                tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
            {
                tutorial_->AdvancePhase();
            }
        }
        ShowItemTooltip(
            "When disabled, parent page-reticle or strobe rotation no longer rotates this primitive. "
            "Explicit primitive rotation still applies.");
        if (tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
        {
            tutorial_->DrawHalo(
                "primitive_reticle_rotation_checkbox",
                "Disable reticle rotation inheritance",
                "Keep the aircraft label upright even when the alternative Page1 strobe rotates.");
        }
    }

    {
        bool scaleSensitive = primitive->reticleScaleSensitive;
        if (ImGui::Checkbox("Affected by reticle scale", &scaleSensitive))
        {
            PushUndoSnapshot();
            primitive->reticleScaleSensitive = scaleSensitive;

            if (!scaleSensitive &&
                tutorial_->MatchesTarget("primitive_reticle_scale_checkbox") &&
                tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip(
            "When disabled, parent page-reticle or strobe scaling no longer scales this primitive. "
            "Explicit primitive scale still applies.");
        if (tutorial_->IsAlternativeStrobeLabelSelection(documentState_.selection.libraryReticleId, primitive->id))
        {
            tutorial_->DrawHalo(
                "primitive_reticle_scale_checkbox",
                "Disable reticle scale inheritance",
                "Keep the aircraft label size stable even when the alternative Page1 strobe scales.");
        }
    }

    if (ImGui::DragFloat2("Position", &primitive->transform.position.x, 0.01f, -1.0f, 1.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Logical position of this primitive inside the reticle template.");

    if (ImGui::DragFloat("Rotation", &primitive->transform.rotationDegrees, 0.25f, -360.0f, 360.0f, "%.2f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
    }
    ShowItemTooltip("Primitive rotation in degrees.");

    if (ImGui::DragFloat2("Scale", &primitive->transform.scale.x, 0.01f, 0.05f, 10.0f, "%.3f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->transform.scale.x = std::max(0.05f, primitive->transform.scale.x);
        primitive->transform.scale.y = std::max(0.05f, primitive->transform.scale.y);
    }
    ShowItemTooltip("Per-axis scale applied to this primitive.");

    ImVec4 stroke = ToImGuiColor(primitive->style.color);
    if (ImGui::ColorEdit4("Stroke", &stroke.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.color = ToColorRgba(stroke);
    }
    ShowItemTooltip("Stroke color used to render this primitive.");

    if (ImGui::DragFloat("Thickness", &primitive->style.thickness, 0.0002f, 0.0005f, 0.05f, "%.4f"))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.thickness = std::max(0.0005f, primitive->style.thickness);
    }
    ShowItemTooltip("Stroke thickness used by this primitive.");

    if (SupportsPrimitiveLineStyle(primitive->type))
    {
        if (ImGui::BeginCombo("Line style", LineStyleLabel(primitive->style.lineStyle)))
        {
            constexpr std::array<mfd::LineStyle, 3> kLineStyles {{
                mfd::LineStyle::Solid,
                mfd::LineStyle::Dotted,
                mfd::LineStyle::Dashed}};
            for (const mfd::LineStyle candidate : kLineStyles)
            {
                const bool selected = primitive->style.lineStyle == candidate;
                if (ImGui::Selectable(LineStyleLabel(candidate), selected))
                {
                    PushUndoSnapshot();
                    primitive->style.lineStyle = candidate;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose whether this primitive outline is solid, dotted, or dashed.");
    }

    if (mfd::SupportsFilledPrimitive(*primitive))
    {
        const bool fillColorOverridden = reticle->overrides.fillColor.has_value();
        const bool filledStateOverridden = reticle->overrides.filled.has_value();
        if (fillColorOverridden || filledStateOverridden)
        {
            const mfd::PrimitiveStyle effectiveStyle = mfd::MergeStyle(primitive->style, reticle->overrides);
            ImGui::Spacing();
            ImGui::TextDisabled("Effective fill preview");
            if (fillColorOverridden)
            {
                ImGui::TextDisabled("Fill color comes from the reticle default: #%02X%02X%02X%02X",
                                    effectiveStyle.fillColor.r,
                                    effectiveStyle.fillColor.g,
                                    effectiveStyle.fillColor.b,
                                    effectiveStyle.fillColor.a);
            }
            if (filledStateOverridden)
            {
                ImGui::TextDisabled("Filled state comes from the reticle default: %s",
                                    effectiveStyle.filled ? "enabled" : "disabled");
            }
            ImGui::TextDisabled("Edit the reticle Default fill / Default filled controls to change this preview.");
        }

        ImGui::BeginDisabled(fillColorOverridden);
        ImVec4 fill = ToImGuiColor(primitive->style.fillColor);
        if (ImGui::ColorEdit4(fillColorOverridden ? "Local fill" : "Fill", &fill.x))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            primitive->style.fillColor = ToColorRgba(fill);
        }
        ImGui::EndDisabled();
        ShowItemTooltip(fillColorOverridden
                            ? "This primitive fill color is masked by the reticle default fill color."
                            : "Fill color used when this primitive supports filled rendering.");

        ImGui::BeginDisabled(filledStateOverridden);
        bool filled = primitive->style.filled;
        if (ImGui::Checkbox(filledStateOverridden ? "Local filled" : "Filled", &filled))
        {
            PushUndoSnapshot();
            primitive->style.filled = filled;
            SeedPrimitiveFillColorIfNeeded(primitive->style);
        }
        ImGui::EndDisabled();
        ShowItemTooltip(filledStateOverridden
                            ? "This primitive filled state is masked by the reticle default filled state."
                            : "Toggle filled rendering for primitives that support it.");
    }

    if (auto* text = std::get_if<mfd::TextGeometry>(&primitive->geometry))
    {
        std::array<char, 128> buffer {};
        CopyTextBuffer(buffer, text->text);
        const bool changed = ImGui::InputText("Text", buffer.data(), buffer.size());
        ShowItemTooltip("Literal text displayed by this text primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (changed)
        {
            text->text = buffer.data();
        }
        if (ImGui::DragFloat("Font size", &text->fontSize, 0.002f, 0.01f, 0.25f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Logical font size used for this text primitive.");
        if (ImGui::DragFloat("Letter spacing", &text->letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Additional spacing inserted between letters.");
        return;
    }

    if (auto* time = std::get_if<mfd::TimeGeometry>(&primitive->geometry))
    {
        std::array<char, 128> buffer {};
        CopyTextBuffer(buffer, time->format);
        const bool changed = ImGui::InputText("Format", buffer.data(), buffer.size());
        ShowItemTooltip("strftime-style format string used by this time primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (changed)
        {
            time->format = buffer.data();
        }

        bool utc = time->utc;
        if (ImGui::Checkbox("UTC", &utc))
        {
            PushUndoSnapshot();
            time->utc = utc;
        }
        ShowItemTooltip("Render the time in UTC instead of local time.");

        if (ImGui::DragFloat("Font size", &time->fontSize, 0.002f, 0.01f, 0.25f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Logical font size used for this time primitive.");
        if (ImGui::DragFloat("Letter spacing", &time->letterSpacing, 0.0005f, -0.05f, 0.10f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Additional spacing inserted between characters.");
        return;
    }

    if (auto* line = std::get_if<mfd::LineGeometry>(&primitive->geometry))
    {
        EditPointArrayField("Start", line->start);
        EditPointArrayField("End", line->end);
        return;
    }

    if (auto* circle = std::get_if<mfd::CircleGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Radius", &circle->radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            circle->radius = std::max(0.001f, circle->radius);
        }
        ShowItemTooltip("Circle radius in logical units.");
        return;
    }

    if (auto* ring = std::get_if<mfd::RingGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Inner radius", &ring->innerRadius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->innerRadius = std::clamp(ring->innerRadius, 0.001f, std::max(0.001f, ring->outerRadius - 0.001f));
        }
        ShowItemTooltip("Inner radius of the ring in logical units.");

        if (ImGui::DragFloat("Outer radius", &ring->outerRadius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->outerRadius = std::max(ring->innerRadius + 0.001f, ring->outerRadius);
        }
        ShowItemTooltip("Outer radius of the ring in logical units.");

        if (ImGui::DragInt("Segments", &ring->segments, 1.0f, 8, 256))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ring->segments = std::clamp(ring->segments, 8, 256);
        }
        ShowItemTooltip("Number of segments used to approximate the ring circles.");
        return;
    }

    if (auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &rectangle->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            rectangle->width = std::max(0.001f, rectangle->width);
            rectangle->height = std::max(0.001f, rectangle->height);
        }
        ShowItemTooltip("Rectangle width and height in logical units.");
        return;
    }

    if (auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &ellipse->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            ellipse->width = std::max(0.001f, ellipse->width);
            ellipse->height = std::max(0.001f, ellipse->height);
        }
        ShowItemTooltip("Ellipse width and height in logical units.");
        return;
    }

    if (auto* square = std::get_if<mfd::SquareGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &square->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            square->width = std::max(0.001f, square->width);
            square->height = std::max(0.001f, square->height);
        }
        ShowItemTooltip("Square width and height in logical units.");
        return;
    }

    if (auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat2("Size", &diamond->width, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            diamond->width = std::max(0.001f, diamond->width);
            diamond->height = std::max(0.001f, diamond->height);
        }
        ShowItemTooltip("Diamond width and height in logical units.");
        return;
    }

    if (auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive->geometry))
    {
        EditPointArrayField("Point A", triangle->points[0]);
        EditPointArrayField("Point B", triangle->points[1]);
        EditPointArrayField("Point C", triangle->points[2]);
        return;
    }

    if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive->geometry))
    {
        bool closed = polyline->closed;
        if (ImGui::Checkbox("Closed", &closed))
        {
            PushUndoSnapshot();
            polyline->closed = closed;
            if (!polyline->closed)
            {
                primitive->style.filled = false;
            }
        }
        ShowItemTooltip("Close the polyline by linking the last point back to the first.");

        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            const std::string label = "Point " + std::to_string(index + 1);
            EditPointArrayField(label.c_str(), polyline->points[static_cast<std::size_t>(index)]);
        }

        if (ImGui::Button("Add point"))
        {
            PushUndoSnapshot();
            polyline->points.push_back({});
        }
        ShowItemTooltip("Append one new point to the end of the polyline.");
        ImGui::SameLine();
        if (ImGui::Button("Remove last point") && !polyline->points.empty())
        {
            PushUndoSnapshot();
            polyline->points.pop_back();
        }
        ShowItemTooltip("Remove the last point from the polyline.");
        return;
    }

    if (auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive->geometry))
    {
        for (int index = 0; index < static_cast<int>(bezier->controlPoints.size()); ++index)
        {
            const std::string label = "Control " + std::to_string(index + 1);
            EditPointArrayField(label.c_str(), bezier->controlPoints[static_cast<std::size_t>(index)]);
        }

        if (ImGui::Button("Add control point"))
        {
            PushUndoSnapshot();
            bezier->controlPoints.push_back({});
        }
        ShowItemTooltip("Append one new control point to this bezier curve.");
        ImGui::SameLine();
        if (ImGui::Button("Remove last control point") && !bezier->controlPoints.empty())
        {
            PushUndoSnapshot();
            bezier->controlPoints.pop_back();
        }
        ShowItemTooltip("Remove the last control point from this bezier curve.");

        if (ImGui::DragInt("Segments", &bezier->segments, 1.0f, 2, 128))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            bezier->segments = std::clamp(bezier->segments, 2, 128);
        }
        ShowItemTooltip("Number of line segments used to approximate the bezier curve.");
        return;
    }

    if (auto* arc = std::get_if<mfd::ArcGeometry>(&primitive->geometry))
    {
        if (ImGui::DragFloat("Radius", &arc->radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            arc->radius = std::max(0.001f, arc->radius);
        }
        ShowItemTooltip("Arc radius in logical units.");

        if (ImGui::DragFloat("Start angle", &arc->startAngleDegrees, 0.5f, -720.0f, 720.0f, "%.1f deg"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Arc start angle in degrees.");

        if (ImGui::DragFloat("End angle", &arc->endAngleDegrees, 0.5f, -720.0f, 720.0f, "%.1f deg"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
        ShowItemTooltip("Arc end angle in degrees.");

        if (ImGui::DragInt("Segments", &arc->segments, 1.0f, 2, 256))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            arc->segments = std::clamp(arc->segments, 2, 256);
        }
        ShowItemTooltip("Number of line segments used to approximate the arc.");
        return;
    }

    if (auto* image = std::get_if<mfd::ImageGeometry>(&primitive->geometry))
    {
        std::array<char, kPathTextCapacity> imagePath {};
        CopyTextBuffer(imagePath, image->file.string());
        const bool pathChanged = ImGui::InputText("Image file", imagePath.data(), imagePath.size());
        ShowItemTooltip("Path to the raster image displayed by this primitive.");
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        if (pathChanged)
        {
            image->file = std::filesystem::path(imagePath.data()).lexically_normal();
        }

        if (ImGui::DragFloat2("Size", &image->width, 0.002f, 0.001f, 2.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            image->width = std::max(0.001f, image->width);
            image->height = std::max(0.001f, image->height);
        }
        ShowItemTooltip("Logical size of the image before the primitive scale is applied.");
    }

}
