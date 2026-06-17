/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Main editor shell implementation (layout, interaction, preview and persistence wiring).
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <rlImGui.h>

#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiStatePersistence.h"
#include "internal/application/EditorApplicationInternal.h"
#include "internal/application/EditorViewportGrid.h"
#include "EditorSnapping.h"
#include "EditorUiTheme.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "mfd/model/Types.h"
#include "mfd/render/WindowBranding.h"

namespace
{
using editor::ui::ApplyEditorTheme;
using editor::detail::CopyTextBuffer;
using editor::detail::kRecoveryFileName;

constexpr float kSidebarWidth = 320.0f;
constexpr float kInspectorWidth = 360.0f;
constexpr float kMinSidebarWidth = 220.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 420.0f;
constexpr float kMinPageContextWidth = 320.0f;
constexpr float kMinReticleStudioWidth = 320.0f;
constexpr float kLayerInspectorDockWidth = 248.0f;
constexpr float kPreviewProblemsDockHeight = 176.0f;
constexpr const char* kReticleStudioDisplayPopupId = "ReticleStudioDisplayPopup";
constexpr const char* kUiStateFileName = "assets/.editor_ui_state.json";

void TryApplyEditorWindowIcon()
{
    std::string error;
    const std::filesystem::path iconFile = mfd::ResolveWindowBrandingIconFile();
    mfd::ApplyWindowIconFile(iconFile, &error);
}

ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
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

bool IsRaylibControlChordPressed(const std::initializer_list<int> keys)
{
    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (!controlDown)
    {
        return false;
    }

    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) ||
        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
    {
        return false;
    }

    for (const int key : keys)
    {
        if (IsKeyPressed(key))
        {
            return true;
        }
    }

    return false;
}
} // namespace

EditorApplication::EditorApplication(std::filesystem::path assetDirectory)
{
    documentState_.assetPaths = editor::EditorAssetPathService(std::move(assetDirectory));
    layoutState_.sidebarWidth = kSidebarWidth;
    layoutState_.inspectorWidth = kInspectorWidth;
    tutorial_ = std::make_unique<EditorTutorialController>(*this);
    CopyTextBuffer(workflowState_.newPageDraft.name, "NewPage");
    CopyTextBuffer(workflowState_.newPageDraft.title, "New Page");
    CopyTextBuffer(workflowState_.newPageDraft.fileName, documentState_.assetPaths.DefaultAssetPath("assets/pages/new_page.json").string());
    CopyTextBuffer(workflowState_.newWindowDraft.windowFile, documentState_.assetPaths.DefaultAssetPath("assets/windows/new_window.json").string());
    CopyTextBuffer(workflowState_.newWindowDraft.title, "New MFD Window");
    CopyTextBuffer(workflowState_.newWindowDraft.reticleLibraryFolder, documentState_.assetPaths.DefaultAssetPath("assets/reticles").string());
    CopyTextBuffer(workflowState_.newWindowDraft.commandAddress, "127.0.0.1");
    CopyTextBuffer(workflowState_.newWindowDraft.feedbackAddress, "127.0.0.1");
    CopyTextBuffer(workflowState_.newWindowDraft.firstPageName, "Page1");
    CopyTextBuffer(workflowState_.newWindowDraft.firstPageTitle, "Page 1");
    CopyTextBuffer(workflowState_.newWindowDraft.firstPageFile, documentState_.assetPaths.DefaultAssetPath("assets/pages/page1.json").string());
    CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, "new_reticle");
    CopyTextBuffer(workflowState_.duplicateLibraryReticleDraft.id, "reticle_copy");
    ResetPagePreviewView();
    ResetLibraryPreviewView();
    RebuildStatus("Open one window asset or create assets to begin authoring.", false);

    const editor::EditorUiPersistentState uiState =
        editor::LoadEditorUiState(documentState_.assetPaths.DefaultAssetPath(kUiStateFileName));
    if (uiState.sidebarWidth.has_value())
    {
        layoutState_.sidebarWidth = *uiState.sidebarWidth;
    }
    if (uiState.inspectorWidth.has_value())
    {
        layoutState_.inspectorWidth = *uiState.inspectorWidth;
    }
    if (uiState.libraryStudioPageWidth.has_value())
    {
        layoutState_.libraryStudioPageWidth = *uiState.libraryStudioPageWidth;
    }
    if (uiState.showGrid.has_value())
    {
        layoutState_.pagePreviewViewOptions.showGrid = *uiState.showGrid;
    }
    if (uiState.snapToGrid.has_value())
    {
        layoutState_.pagePreviewViewOptions.snapToGrid = *uiState.snapToGrid;
    }
    if (uiState.gridStepLogical.has_value())
    {
        layoutState_.pagePreviewViewOptions.gridStepLogical = *uiState.gridStepLogical;
    }
    layoutState_.inspectorSectionOpen = uiState.sectionOpen;
    editor::ui::SetInspectorSectionStateStore(&layoutState_.inspectorSectionOpen);

    std::error_code recoveryExistsError;
    if (std::filesystem::exists(documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName), recoveryExistsError) &&
        !recoveryExistsError)
    {
        // A recovery snapshot left behind means the previous session did not exit cleanly.
        workflowState_.recoveryPromptPending = true;
    }

    tutorial_->LoadProgress();
}

EditorApplication::~EditorApplication()
{
    editor::ui::SetInspectorSectionStateStore(nullptr);

    editor::EditorUiPersistentState uiState;
    uiState.sidebarWidth = layoutState_.sidebarWidth;
    uiState.inspectorWidth = layoutState_.inspectorWidth;
    if (layoutState_.libraryStudioPageWidth > 0.0f)
    {
        uiState.libraryStudioPageWidth = layoutState_.libraryStudioPageWidth;
    }
    uiState.showGrid = layoutState_.pagePreviewViewOptions.showGrid;
    uiState.snapToGrid = layoutState_.pagePreviewViewOptions.snapToGrid;
    uiState.gridStepLogical = editor::app::SanitizeGridStepLogical(layoutState_.pagePreviewViewOptions.gridStepLogical);
    uiState.sectionOpen = layoutState_.inspectorSectionOpen;
    editor::SaveEditorUiState(documentState_.assetPaths.DefaultAssetPath(kUiStateFileName), uiState);

    ReleasePreviewGpuResources();
}

float editor::app::ViewportState::LogicalScale() const noexcept
{
    return 0.5f * std::min(size.x, size.y);
}

ImVec2 editor::app::ViewportState::ToScreen(const mfd::Vec2 logical) const noexcept
{
    const mfd::Vec2 viewed = mfd::ApplyPageView(logical, view);
    return ImVec2(
        origin.x + size.x * 0.5f + viewed.x * LogicalScale(),
        origin.y + size.y * 0.5f - viewed.y * LogicalScale());
}

mfd::Vec2 editor::app::ViewportState::ToLogical(const ImVec2 screen) const noexcept
{
    const float scale = LogicalScale();
    if (!valid || scale <= 0.0f)
    {
        return {};
    }

    const float viewedX = (screen.x - origin.x - size.x * 0.5f) / scale;
    const float viewedY = -(screen.y - origin.y - size.y * 0.5f) / scale;
    const float zoom = mfd::SanitizeZoom(view.zoom);

    return {
        viewedX / zoom + view.center.x,
        viewedY / zoom + view.center.y};
}

int EditorApplication::Run()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1720, 980, "MFDStudio");
    SetExitKey(KEY_NULL);
    TryApplyEditorWindowIcon();
    SetWindowMinSize(1320, 760);
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ApplyEditorTheme();

    while (true)
    {
        if (WindowShouldClose())
        {
            if (HasOpenWindow() && workflowState_.documentDirty)
            {
                // Defer the close so the unsaved-changes modal can offer to save first.
                workflowState_.unsavedExitRequested = true;
            }
            else
            {
                break;
            }
        }

        BeginDrawing();
        ClearBackground(Color {8, 13, 18, 255});

        bool imguiBegun = false;
        try
        {
            rlImGuiBegin();
            imguiBegun = true;
            HandleShortcuts();
            DrawMenuBar();
            DrawRootLayout();
            DrawPopups();
        }
        catch (const std::exception& exception)
        {
            workflowState_.lastRuntimeError = exception.what();
            RebuildStatus(workflowState_.lastRuntimeError, true);
        }
        catch (...)
        {
            workflowState_.lastRuntimeError = "Unknown exception inside mfd_editor";
            RebuildStatus(workflowState_.lastRuntimeError, true);
        }

        if (imguiBegun)
        {
            rlImGuiEnd();
        }

        EndDrawing();

        const float frameSeconds = GetFrameTime();
        autosave_.Advance(frameSeconds);
        if (HasOpenWindow() &&
            !workflowState_.recoveryPromptPending &&
            autosave_.DueForAutosave(editor::EditorAutosaveScheduler::kDefaultIntervalSeconds, workflowState_.documentDirty))
        {
            WriteRecoverySnapshot();
            autosave_.MarkAutosaved();
        }

        assetWatchAccumulatorSeconds_ += frameSeconds;
        if (assetWatchAccumulatorSeconds_ >= 2.0f)
        {
            assetWatchAccumulatorSeconds_ = 0.0f;
            const bool anyPromptPending = workflowState_.recoveryPromptPending ||
                                          workflowState_.unsavedExitRequested ||
                                          workflowState_.reloadConfirmRequested ||
                                          workflowState_.externalReloadPromptPending;
            if (HasOpenWindow() && !anyPromptPending && assetWatcher_.DetectExternalChange())
            {
                workflowState_.externalReloadPromptPending = true;
            }
        }

        if (workflowState_.exitConfirmed)
        {
            break;
        }
    }

    // A clean exit (saved, or an explicit quit-without-saving) must not look like a crash next launch.
    ClearRecoverySnapshot();
    rlImGuiShutdown();
    ReleasePreviewGpuResources();
    CloseWindow();
    return 0;
}

void EditorApplication::HandleShortcuts()
{
    HandleDroppedFiles();

    const ImGuiIO& io = ImGui::GetIO();
    const bool hasSelectedLibraryPrimitive =
        documentState_.selection.kind == SelectionKind::LibraryPrimitive && SelectedLibraryPrimitive() != nullptr;
    const bool hasFocusedLibraryReticle =
        (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive) &&
        SelectedLibraryReticle() != nullptr;

    if (!io.WantTextInput && CanToggleFullscreenPagePreview() && ImGui::IsKeyPressed(ImGuiKey_F11))
    {
        ToggleFullscreenPagePreview();
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_S}))
    {
        const bool saveSucceeded = SaveAll();
        if (editor::detail::ShouldAdvanceTutorialOnSuccessfulSave(
                saveSucceeded,
                tutorial_->IsStep(static_cast<int>(editor::tutorial::TutorialStepId::SaveTutorialAssets)),
                false))
        {
            tutorial_->CompleteStep();
        }
    }

    const bool undoShortcutTriggered =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_Z, KEY_W});
    const bool redoShortcutTriggered =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_Y});
    if (redoShortcutTriggered)
    {
        Redo();
    }
    else if (undoShortcutTriggered)
    {
        Undo();
    }

    if (!io.WantTextInput)
    {
        const float nudgeStep = layoutState_.pagePreviewViewOptions.snapToGrid
                                    ? editor::app::SanitizeGridStepLogical(layoutState_.pagePreviewViewOptions.gridStepLogical)
                                    : (io.KeyShift ? 0.05f : 0.01f);
        const mfd::Vec2 nudge = editor::app::ArrowNudgeDelta(
            ImGui::IsKeyPressed(ImGuiKey_LeftArrow),
            ImGui::IsKeyPressed(ImGuiKey_RightArrow),
            ImGui::IsKeyPressed(ImGuiKey_UpArrow),
            ImGui::IsKeyPressed(ImGuiKey_DownArrow),
            nudgeStep);
        if (nudge.x != 0.0f || nudge.y != 0.0f)
        {
            NudgeSelection(nudge);
        }
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_C})))
    {
        if (hasSelectedLibraryPrimitive)
        {
            CopySelectedLibraryPrimitive();
        }
        else if (hasFocusedLibraryReticle)
        {
            CopySelectedLibraryReticle();
        }
        else
        {
            CopySelectedPageReticles();
        }
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_X})))
    {
        CutSelectedPageReticles();
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_V})))
    {
        if (hasSelectedLibraryPrimitive)
        {
            PasteCopiedLibraryPrimitive();
        }
        else if (hasFocusedLibraryReticle)
        {
            PasteCopiedLibraryReticle();
        }
        else
        {
            PasteCopiedPageReticles();
        }
    }

    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        DeleteSelection();
    }

    if (!io.WantTextInput &&
        !ImGui::IsPopupOpen((const char*)nullptr, ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        if (services_.fullscreenPreview.IsActive())
        {
            ToggleFullscreenPagePreview();
            return;
        }

        if (const mfd::PageDefinition* page = ActivePage();
            page != nullptr && services_.layerFocus.IsFocusActive(*page, layoutState_.layerFocusState))
        {
            ClearLayerFocus(true);
            return;
        }

        if (documentState_.selection.kind == SelectionKind::PageReticle && !SelectedPageReticleIndices().empty())
        {
            SelectPage(documentState_.selection.pageIndex, false);
            RebuildStatus("Page reticle selection cleared.", false);
        }
        else if (documentState_.selection.kind == SelectionKind::PageTitle)
        {
            SelectPage(documentState_.selection.pageIndex, false);
            RebuildStatus("Page title selection cleared.", false);
        }
        else if (documentState_.selection.kind == SelectionKind::PageStrobe)
        {
            SelectPage(documentState_.selection.pageIndex, false);
            RebuildStatus("Page strobe selection cleared.", false);
        }
    }
}

void EditorApplication::RebuildStatus(std::string message, const bool isError)
{
    workflowState_.statusMessage = std::move(message);
    workflowState_.statusIsError = isError;
}
