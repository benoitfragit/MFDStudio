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
#include <cstdio>
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

#include <nlohmann/json.hpp>
#include <rlImGui.h>

#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiStatePersistence.h"
#include "internal/application/EditorApplicationInternal.h"
#include "internal/application/EditorViewportGrid.h"
#include "internal/application/EditorViewportSnap.h"
#include "EditorFileDialogs.h"
#include "EditorReticleExtractionService.h"
#include "EditorReticleUsageHighlightService.h"
#include "EditorSnapping.h"
#include "EditorUiTheme.h"
#include "EditorWorkspaceLayout.h"
#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "mfd/model/Types.h"
#include "Canvas2D.h"
#include "RenderTextureUtils.h"
#include "mfd/render/WindowBranding.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::ApplyEditorTheme;
using editor::ui::DrawVerticalSplitter;
using editor::ui::FormatViewportToolbarInfoLabel;
using editor::ui::ShowItemTooltip;
using editor::detail::ApproximateArcPoints;
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::DefaultPageIndex;
using editor::detail::Distance;
using editor::detail::FallbackPreviewTextSizeLogical;
using editor::detail::kPrimitiveTypes;
using editor::detail::kRecoveryFileName;
using editor::detail::kTutorialAircraftTemplateId;
using editor::detail::kTutorialStrobeCursorTemplateId;
using editor::detail::ResolvePreviewMeasurementFont;
using editor::detail::ReticleHasFillCapablePrimitive;
using editor::detail::SanitizePreviewLetterSpacing;
using editor::detail::SeedPrimitiveFillColorIfNeeded;
using editor::detail::SeedReticleFillOverrideIfNeeded;
using editor::detail::SuggestReplacementPageIndex;
using editor::detail::ToColorRgba;
using editor::detail::TransformPrimitiveWorldPoint;
using editor::detail::VisibleFillColorFromStroke;
using editor::app::ClearBlinkReferencesForRemovedType;
using editor::app::ClearEditorLayerReferences;
using editor::app::CollectClipPrimitiveOptions;
using editor::app::CountBlinkReferences;
using editor::app::CountDynamicLayerBindings;
using editor::app::CountEditorLayerAssignments;
using editor::app::DefaultEditorLayerId;
using editor::app::EffectiveDefaultBlinkTypeIndex;
using editor::app::FindActivePageStrobeIndex;
using editor::app::FindBlinkTypeIndex;
using editor::app::FindEditorLayer;
using editor::app::IsPageStrobeIndexVisibleInEditor;
using editor::app::IsPageStrobeSelectableInEditor;
using editor::app::IsPageStrobeVisibleInEditor;
using editor::app::IsReticleVisibleInEditor;
using editor::app::LineStyleLabel;
using editor::app::MakeUniqueBlinkTypeName;
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
using json = nlohmann::json;

float SharedGridStepLogical(const editor::PagePreviewViewOptions& viewOptions) noexcept
{
    return editor::app::SanitizeGridStepLogical(viewOptions.gridStepLogical);
}

mfd::Vec2 SnapToSharedGridIfEnabled(const mfd::Vec2 value, const editor::PagePreviewViewOptions& viewOptions) noexcept
{
    return viewOptions.snapToGrid ? editor::app::SnapToGrid(value, SharedGridStepLogical(viewOptions)) : value;
}

editor::app::ViewportGridInput MakeViewportGridInput(const editor::app::ViewportState& viewport,
                                                     const editor::PagePreviewViewOptions& viewOptions) noexcept
{
    return editor::app::ViewportGridInput {
        static_cast<int>(viewport.size.x),
        static_cast<int>(viewport.size.y),
        viewport.view,
        SharedGridStepLogical(viewOptions)};
}

constexpr float kSidebarWidth = 320.0f;
constexpr float kInspectorWidth = 360.0f;
constexpr float kMinSidebarWidth = 220.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 420.0f;
constexpr float kMinPageContextWidth = 320.0f;
constexpr float kMinReticleStudioWidth = 320.0f;
constexpr float kLayerInspectorDockWidth = 248.0f;
constexpr float kLayerInspectorPreviewHeight = 84.0f;
constexpr float kPreviewProblemsDockHeight = 176.0f;
constexpr const char* kPagePreviewHelpPopupId = "PagePreviewHelpPopup";
constexpr const char* kLibraryPreviewHelpPopupId = "LibraryPreviewHelpPopup";
constexpr const char* kPagePreviewDisplayPopupId = "PagePreviewDisplayPopup";
constexpr const char* kReticleStudioDisplayPopupId = "ReticleStudioDisplayPopup";
constexpr const char* kUiStateFileName = "assets/.editor_ui_state.json";

void TryApplyEditorWindowIcon()
{
    std::string error;
    const std::filesystem::path iconFile = mfd::ResolveWindowBrandingIconFile();
    mfd::ApplyWindowIconFile(iconFile, &error);
}

Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

ImVec4 ToImGuiColor(const mfd::ColorRgba& color)
{
    return ImVec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f);
}

void ShowHoveredRegionTooltip(const bool hovered, const char* text)
{
    if (!hovered || text == nullptr || text[0] == '\0')
    {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

mfd::Vec2 InverseTransformPoint(const mfd::Vec2& point, const mfd::Transform2D& transform)
{
    const mfd::Vec2 translated = point - transform.position;
    const mfd::Vec2 rotated = mfd::Rotate(translated, -transform.rotationDegrees);

    return {
        std::abs(transform.scale.x) <= 0.0001f ? 0.0f : rotated.x / transform.scale.x,
        std::abs(transform.scale.y) <= 0.0001f ? 0.0f : rotated.y / transform.scale.y};
}

mfd::Vec2 InversePrimitiveWorldPoint(const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     const mfd::Vec2 worldPoint)
{
    return InverseTransformPoint(worldPoint, mfd::ResolvePrimitiveWorldTransform(primitive, reticle));
}

struct LogicalBounds
{
    mfd::Vec2 min {};
    mfd::Vec2 max {};
    mfd::Vec2 center {};
    bool valid = false;
};

struct PageMinimapState
{
    ImVec2 frameMin {};
    ImVec2 frameMax {};
    ImVec2 contentMin {};
    ImVec2 contentMax {};
    ImVec2 contentCenter {};
    mfd::Vec2 logicalMin {};
    mfd::Vec2 logicalMax {};
    mfd::Vec2 logicalCenter {};
    float pixelsPerLogicalUnit = 1.0f;
    bool valid = false;
};

constexpr float kPreviewTextMeasurementPixelsPerLogicalUnit = 1024.0f;

void IncludeLogicalPoint(LogicalBounds& bounds, const mfd::Vec2 point)
{
    if (!bounds.valid)
    {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }

    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
}

void FinalizeLogicalBounds(LogicalBounds& bounds)
{
    if (!bounds.valid)
    {
        return;
    }

    bounds.center = {
        (bounds.min.x + bounds.max.x) * 0.5f,
        (bounds.min.y + bounds.max.y) * 0.5f};
}

void IncludeLogicalBounds(LogicalBounds& bounds, const LogicalBounds& other)
{
    if (!other.valid)
    {
        return;
    }

    IncludeLogicalPoint(bounds, other.min);
    IncludeLogicalPoint(bounds, other.max);
}

LogicalBounds ComputePrimitiveLocalBounds(const mfd::Primitive& primitive)
{
    LogicalBounds bounds;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        primitive,
        [&bounds, &primitive](const mfd::Vec2 localPoint)
        {
            IncludeLogicalPoint(bounds, mfd::ApplyTransform(localPoint, primitive.transform));
        });

    FinalizeLogicalBounds(bounds);
    return bounds;
}

LogicalBounds ComputeReticleLocalBounds(const mfd::ReticleGroup& reticle)
{
    LogicalBounds bounds;

    for (const auto& primitive : reticle.primitives)
    {
        const LogicalBounds primitiveBounds = ComputePrimitiveLocalBounds(primitive);
        if (!primitiveBounds.valid)
        {
            continue;
        }

        IncludeLogicalPoint(bounds, primitiveBounds.min);
        IncludeLogicalPoint(bounds, primitiveBounds.max);
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

mfd::Vec2 ReticleVisualCenterLocal(const mfd::ReticleGroup& reticle)
{
    const LogicalBounds bounds = ComputeReticleLocalBounds(reticle);
    return bounds.valid ? bounds.center : mfd::Vec2 {};
}

LogicalBounds ComputeReticleWorldBounds(const mfd::ReticleGroup& reticle)
{
    LogicalBounds worldBounds;

    for (const auto& primitive : reticle.primitives)
    {
        editor::detail::ForEachPrimitiveBoundsLocalPoint(
            primitive,
            [&worldBounds, &reticle, &primitive](const mfd::Vec2 localPoint)
            {
                IncludeLogicalPoint(worldBounds, TransformPrimitiveWorldPoint(reticle, primitive, localPoint));
            });
    }

    FinalizeLogicalBounds(worldBounds);
    return worldBounds;
}

mfd::PageViewState MakeViewFittingBounds(const LogicalBounds& bounds,
                                         const int width,
                                         const int height,
                                         const float padding = 0.82f) noexcept
{
    mfd::PageViewState view {};
    if (!bounds.valid || width <= 0 || height <= 0)
    {
        return view;
    }

    const float minDimension = static_cast<float>(std::min(width, height));
    if (minDimension <= 0.0f)
    {
        return view;
    }

    const float visibleHalfWidth = static_cast<float>(width) / minDimension;
    const float visibleHalfHeight = static_cast<float>(height) / minDimension;
    const float halfExtentX = std::max(0.05f, (bounds.max.x - bounds.min.x) * 0.5f);
    const float halfExtentY = std::max(0.05f, (bounds.max.y - bounds.min.y) * 0.5f);
    const float fitZoom =
        std::min(visibleHalfWidth / halfExtentX, visibleHalfHeight / halfExtentY) * std::clamp(padding, 0.1f, 1.0f);

    view.center = bounds.center;
    view.zoom = std::clamp(fitZoom, 0.1f, 20.0f);
    return view;
}

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

template <typename ViewportStateT>
LogicalBounds ComputeViewportLogicalBounds(const ViewportStateT& viewport)
{
    LogicalBounds bounds;
    if (!viewport.valid)
    {
        return bounds;
    }

    const std::array<ImVec2, 4> corners {{
        viewport.origin,
        ImVec2(viewport.origin.x + viewport.size.x, viewport.origin.y),
        ImVec2(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y),
        ImVec2(viewport.origin.x, viewport.origin.y + viewport.size.y),
    }};
    for (const ImVec2& corner : corners)
    {
        IncludeLogicalPoint(bounds, viewport.ToLogical(corner));
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

template <typename ViewportStateT>
PageMinimapState ComputePageMinimapState(const mfd::PageDefinition& page, const ViewportStateT& viewport)
{
    PageMinimapState state;
    if (!viewport.valid)
    {
        return state;
    }

    LogicalBounds logicalBounds;
    for (const auto& reticle : page.staticReticles)
    {
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        IncludeLogicalBounds(logicalBounds, ComputeReticleWorldBounds(reticle));
    }

    for (const auto& strobe : page.strobes)
    {
        if (IsPageStrobeVisibleInEditor(page, strobe))
        {
            IncludeLogicalBounds(logicalBounds, ComputeReticleWorldBounds(strobe.reticle));
        }
    }

    IncludeLogicalBounds(logicalBounds, ComputeViewportLogicalBounds(viewport));
    if (!logicalBounds.valid)
    {
        IncludeLogicalPoint(logicalBounds, mfd::Vec2 {-1.0f, -1.0f});
        IncludeLogicalPoint(logicalBounds, mfd::Vec2 {1.0f, 1.0f});
    }

    FinalizeLogicalBounds(logicalBounds);

    const float width = std::max(logicalBounds.max.x - logicalBounds.min.x, 0.001f);
    const float height = std::max(logicalBounds.max.y - logicalBounds.min.y, 0.001f);
    const float logicalPadding = std::max({0.20f, width * 0.12f, height * 0.12f});
    logicalBounds.min.x -= logicalPadding;
    logicalBounds.min.y -= logicalPadding;
    logicalBounds.max.x += logicalPadding;
    logicalBounds.max.y += logicalPadding;
    FinalizeLogicalBounds(logicalBounds);

    const float paddedWidth = std::max(logicalBounds.max.x - logicalBounds.min.x, 0.001f);
    const float paddedHeight = std::max(logicalBounds.max.y - logicalBounds.min.y, 0.001f);
    const ImVec2 frameSize(
        std::clamp(viewport.size.x * 0.24f, 150.0f, 240.0f),
        std::clamp(viewport.size.y * 0.24f, 120.0f, 210.0f));
    constexpr float kFrameMargin = 16.0f;
    constexpr float kInnerPadding = 12.0f;

    state.frameMin = ImVec2(
        viewport.origin.x + viewport.size.x - frameSize.x - kFrameMargin,
        viewport.origin.y + viewport.size.y - frameSize.y - kFrameMargin);
    state.frameMax = ImVec2(state.frameMin.x + frameSize.x, state.frameMin.y + frameSize.y);
    state.contentCenter = ImVec2(
        (state.frameMin.x + state.frameMax.x) * 0.5f,
        (state.frameMin.y + state.frameMax.y) * 0.5f);

    const float usableWidth = std::max(8.0f, frameSize.x - kInnerPadding * 2.0f);
    const float usableHeight = std::max(8.0f, frameSize.y - kInnerPadding * 2.0f);
    state.pixelsPerLogicalUnit = std::min(usableWidth / paddedWidth, usableHeight / paddedHeight);

    const ImVec2 contentSize(paddedWidth * state.pixelsPerLogicalUnit, paddedHeight * state.pixelsPerLogicalUnit);
    state.contentMin = ImVec2(
        state.contentCenter.x - contentSize.x * 0.5f,
        state.contentCenter.y - contentSize.y * 0.5f);
    state.contentMax = ImVec2(
        state.contentCenter.x + contentSize.x * 0.5f,
        state.contentCenter.y + contentSize.y * 0.5f);
    state.logicalMin = logicalBounds.min;
    state.logicalMax = logicalBounds.max;
    state.logicalCenter = logicalBounds.center;
    state.valid = true;
    return state;
}

bool IsPointInsideRect(const ImVec2 point, const ImVec2 min, const ImVec2 max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

struct ViewportToolbarLayout
{
    ImVec2 toolbarMin {};
    ImVec2 toolbarMax {};
    ImVec2 helpButtonPos {};
    ImVec2 resetButtonPos {};
    ImVec2 buttonSize {};
    ImVec2 textPos {};
    std::array<char, 96> infoLabel {};
};

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

ViewportToolbarLayout ComputeViewportToolbarLayout(const ImVec2 viewportOrigin,
                                                   const float zoom,
                                                   const std::optional<mfd::Vec2>& mouseLogical)
{
    ViewportToolbarLayout layout;

    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string infoLabel = FormatViewportToolbarInfoLabel(zoom, mouseLogical);
    std::snprintf(layout.infoLabel.data(), layout.infoLabel.size(), "%s", infoLabel.c_str());

    const ImVec2 buttonLabelSize = ImGui::CalcTextSize("?");
    const ImVec2 textSize = ImGui::CalcTextSize(layout.infoLabel.data());
    layout.buttonSize = ImVec2(
        buttonLabelSize.x + style.FramePadding.x * 2.0f,
        buttonLabelSize.y + style.FramePadding.y * 2.0f);
    layout.helpButtonPos = ImVec2(viewportOrigin.x + 12.0f, viewportOrigin.y + 12.0f);
    layout.resetButtonPos = ImVec2(layout.helpButtonPos.x + layout.buttonSize.x + style.ItemSpacing.x,
                                   layout.helpButtonPos.y);
    layout.textPos = ImVec2(layout.resetButtonPos.x + layout.buttonSize.x + style.ItemSpacing.x,
                            layout.helpButtonPos.y + style.FramePadding.y);
    layout.toolbarMin = layout.helpButtonPos;
    layout.toolbarMax = ImVec2(
        layout.textPos.x + textSize.x,
        layout.helpButtonPos.y + std::max(layout.buttonSize.y, textSize.y + style.FramePadding.y * 2.0f));
    return layout;
}

void DrawViewportHelpPopupContent(const bool libraryPreview)
{
    if (libraryPreview)
    {
        ImGui::TextDisabled("Reticle studio");
        ImGui::Separator();
        ImGui::BulletText("Toolbar R: recenter the studio camera.");
        ImGui::BulletText("Mouse wheel: zoom the studio camera.");
        ImGui::BulletText("Right-drag: pan the studio camera.");
        ImGui::BulletText("Click a primitive: focus it in the studio and inspector.");
        ImGui::BulletText("Left-drag the handles: edit the selected primitive geometry.");
        ImGui::BulletText("View > Grid and Snap to grid: reuse the shared logical placement grid.");
    }
    else
    {
        ImGui::TextDisabled("Page preview");
        ImGui::Separator();
        ImGui::BulletText("Ctrl+click: add or remove one page reticle from the selection.");
        ImGui::BulletText("Click the page strobe to edit it like one regular reticle.");
        ImGui::BulletText("Esc: clear the current page-reticle or strobe selection.");
        ImGui::BulletText("Drag a selected reticle or strobe: move it directly in logical space.");
        ImGui::BulletText("Blue handle: rotate the selected reticle or strobe.");
        ImGui::BulletText("Corner handles: scale the selected reticle or strobe.");
        ImGui::BulletText("Toolbar R: recenter the page camera.");
        ImGui::BulletText("Mouse wheel: zoom the page camera.");
        ImGui::BulletText("Right-drag: pan the page camera.");
        ImGui::BulletText("Right-click: open selection and clipping actions.");
        ImGui::BulletText("Left-drag the minimap viewport: navigate the page.");
        ImGui::BulletText("View > Grid and Snap to grid: align the visible helper grid with reticle drags and nudges.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Global shortcuts");
    ImGui::BulletText("Save: Ctrl+S");
    ImGui::BulletText("Undo: Ctrl+Z");
    if (libraryPreview)
    {
        ImGui::BulletText("Copy / Paste focused primitive: Ctrl+C / Ctrl+V");
        ImGui::BulletText("Copy / Paste focused library reticle: Ctrl+C / Ctrl+V when no primitive is selected");
        ImGui::BulletText("Delete current library reticle: Del");
    }
    else
    {
        ImGui::BulletText("Copy / Cut / Paste selected page reticles: Ctrl+C / Ctrl+X / Ctrl+V");
        ImGui::BulletText("Delete current selection: Del");
    }
}

bool DrawViewportToolbar(const ImVec2 viewportOrigin,
                         const float zoom,
                         const std::optional<mfd::Vec2>& mouseLogical,
                         const char* helpButtonId,
                         const char* resetButtonId,
                         const char* popupId,
                         const bool libraryPreview)
{
    const ViewportToolbarLayout layout = ComputeViewportToolbarLayout(viewportOrigin, zoom, mouseLogical);
    bool resetRequested = false;

    ImGui::SetCursorScreenPos(layout.helpButtonPos);
    if (ImGui::Button(helpButtonId, layout.buttonSize))
    {
        ImGui::OpenPopup(popupId);
    }
    ShowItemTooltip("Open a compact summary of the controls available in this view.");

    ImGui::SetCursorScreenPos(layout.resetButtonPos);
    resetRequested = ImGui::Button(resetButtonId, layout.buttonSize);
    ShowItemTooltip(libraryPreview ? "Recenter the reticle-studio camera on its neutral view."
                                   : "Recenter the page camera on the authored page view.");

    ImGui::SetCursorScreenPos(layout.textPos);
    ImGui::TextDisabled("%s", layout.infoLabel.data());

    ImGui::SetNextWindowPos(
        ImVec2(layout.helpButtonPos.x, layout.helpButtonPos.y + layout.buttonSize.y + 6.0f),
        ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId))
    {
        DrawViewportHelpPopupContent(libraryPreview);
        ImGui::EndPopup();
    }

    return resetRequested;
}

ImVec2 ToMinimapScreen(const PageMinimapState& minimap, const mfd::Vec2 logical)
{
    return ImVec2(
        minimap.contentCenter.x + (logical.x - minimap.logicalCenter.x) * minimap.pixelsPerLogicalUnit,
        minimap.contentCenter.y - (logical.y - minimap.logicalCenter.y) * minimap.pixelsPerLogicalUnit);
}

mfd::Vec2 ToMinimapLogical(const PageMinimapState& minimap, const ImVec2 screen)
{
    const float clampedX = std::clamp(screen.x, minimap.contentMin.x, minimap.contentMax.x);
    const float clampedY = std::clamp(screen.y, minimap.contentMin.y, minimap.contentMax.y);
    return {
        minimap.logicalCenter.x + (clampedX - minimap.contentCenter.x) / minimap.pixelsPerLogicalUnit,
        minimap.logicalCenter.y - (clampedY - minimap.contentCenter.y) / minimap.pixelsPerLogicalUnit};
}

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
    uiState.gridStepLogical = SharedGridStepLogical(layoutState_.pagePreviewViewOptions);
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
                                    ? SharedGridStepLogical(layoutState_.pagePreviewViewOptions)
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

mfd::ColorRgba ScaleAlpha(const mfd::ColorRgba color, const float factor)
{
    const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
    return mfd::ColorRgba {
        color.r,
        color.g,
        color.b,
        static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(static_cast<float>(color.a) * clampedFactor)),
                                             0,
                                             255))};
}

mfd::ReticleGroup MakeDimmedReticlePreviewCopy(const mfd::ReticleGroup& source, const float alphaFactor)
{
    mfd::ReticleGroup copy = source;
    if (copy.overrides.color.has_value())
    {
        copy.overrides.color = ScaleAlpha(*copy.overrides.color, alphaFactor);
    }
    if (copy.overrides.fillColor.has_value())
    {
        copy.overrides.fillColor = ScaleAlpha(*copy.overrides.fillColor, alphaFactor);
    }

    for (mfd::Primitive& primitive : copy.primitives)
    {
        primitive.style.color = ScaleAlpha(primitive.style.color, alphaFactor);
        primitive.style.fillColor = ScaleAlpha(primitive.style.fillColor, alphaFactor);
    }

    return copy;
}

void EditorApplication::RebuildStatus(std::string message, const bool isError)
{
    workflowState_.statusMessage = std::move(message);
    workflowState_.statusIsError = isError;
}

void EditorApplication::EnsurePreviewTexture(const int width, const int height)
{
    previewResources_.EnsureTexture(width, height);
}

void EditorApplication::ReleasePreviewTexture()
{
    previewResources_.ReleaseTexture();
}

void EditorApplication::EnsureTooltipPreviewTexture(const int width, const int height)
{
    previewResources_.EnsureTooltipTexture(width, height);
}

void EditorApplication::ReleaseTooltipPreviewTexture()
{
    previewResources_.ReleaseTooltipTexture();
}

void EditorApplication::ReleaseLayerPreviewTextures() noexcept
{
    previewResources_.ReleaseLayerTextures();
}

void EditorApplication::ReleasePreviewGpuResources() noexcept
{
    previewResources_.ReleaseGpuResources();
}

const RenderTexture2D* EditorApplication::RenderLayerPreviewThumbnail(const std::size_t thumbnailIndex,
                                                                      const mfd::PageDefinition& page,
                                                                      const editor::LayerFocusStripEntry& entry,
                                                                      int width,
                                                                      int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    LayerPreviewTextureSlot& slot = previewResources_.ResolveLayerSlot(thumbnailIndex, width, height);
    if (!slot.ready)
    {
        return nullptr;
    }

    LogicalBounds bounds;

    for (const mfd::ReticleGroup& reticle : page.staticReticles)
    {
        const bool matchesEntry = entry.fullView ? IsReticleVisibleInEditor(page, reticle) : reticle.layerId == entry.layerId;
        if (matchesEntry)
        {
            IncludeLogicalBounds(bounds, ComputeReticleWorldBounds(reticle));
        }
    }

    mfd::PageViewState previewView = bounds.valid ? MakeViewFittingBounds(bounds, width, height) : mfd::PageViewState {};
    previewView.zoom = mfd::SanitizeZoom(previewView.zoom);
    const Color background = ToRayColor(page.backgroundColor);
    const bool drawDimmedLayer = !entry.fullView && !entry.visible;

    BeginTextureMode(slot.texture);
    ClearBackground(background);
    {
        EnsurePreviewFont();
        editor::ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(width,
                             height,
                             previewView,
                             PreviewTextFont(),
                             background,
                             slot.stencilReady,
                             &previewResources_.BezierCache(),
                             &previewResources_.ImageCache(),
                             &previewResources_.TextLayoutCache());

        for (const mfd::ReticleGroup& reticle : page.staticReticles)
        {
            const bool matchesEntry = entry.fullView ? IsReticleVisibleInEditor(page, reticle) : reticle.layerId == entry.layerId;
            if (!matchesEntry)
            {
                continue;
            }

            if (drawDimmedLayer)
            {
                canvas.DrawReticle(MakeDimmedReticlePreviewCopy(reticle, 0.38f));
            }
            else
            {
                canvas.DrawReticle(reticle);
            }
        }
    }
    EndTextureMode();

    return &slot.texture;
}

void EditorApplication::ApplyPreviewFontFile(std::filesystem::path fontFile)
{
    previewResources_.ApplyFontFile(std::move(fontFile));
}

void EditorApplication::EnsurePreviewFont()
{
    previewResources_.EnsureFont();
}

void EditorApplication::ReleasePreviewFont() noexcept
{
    previewResources_.ReleaseFont();
}

const Font* EditorApplication::PreviewTextFont() const noexcept
{
    return previewResources_.TextFont();
}

float EditorApplication::MeasurePreviewTextWidthLogical(const mfd::TextGeometry& geometry)
{
    const mfd::Vec2 fallback = FallbackPreviewTextSizeLogical(geometry);
    if (!IsWindowReady())
    {
        return fallback.x;
    }

    const float fontSizePixels = std::max(
        1.0f,
        std::abs(geometry.fontSize * kPreviewTextMeasurementPixelsPerLogicalUnit));
    const float letterSpacingPixels =
        SanitizePreviewLetterSpacing(geometry.letterSpacing) * kPreviewTextMeasurementPixelsPerLogicalUnit;
    const mfd::CachedTextLayout& layout = previewResources_.TextLayoutCache().ResolveStaticText(
        geometry.text,
        ResolvePreviewMeasurementFont(PreviewTextFont()),
        fontSizePixels,
        letterSpacingPixels,
        geometry.align);
    if (!std::isfinite(layout.size.x) || !std::isfinite(layout.size.y) ||
        layout.size.x <= 0.0f || layout.size.y <= 0.0f)
    {
        return fallback.x;
    }

    return std::max(0.03f, layout.size.x / kPreviewTextMeasurementPixelsPerLogicalUnit);
}

float EditorApplication::MeasurePreviewTextWidthLogical(const mfd::TimeGeometry& geometry)
{
    const mfd::Vec2 fallback = FallbackPreviewTextSizeLogical(geometry);
    if (!IsWindowReady())
    {
        return fallback.x;
    }

    const float fontSizePixels = std::max(
        1.0f,
        std::abs(geometry.fontSize * kPreviewTextMeasurementPixelsPerLogicalUnit));
    const float letterSpacingPixels =
        SanitizePreviewLetterSpacing(geometry.letterSpacing) * kPreviewTextMeasurementPixelsPerLogicalUnit;
    const mfd::CachedTextLayout& layout = previewResources_.TextLayoutCache().ResolveTimeText(
        geometry,
        ResolvePreviewMeasurementFont(PreviewTextFont()),
        fontSizePixels,
        letterSpacingPixels);
    if (!std::isfinite(layout.size.x) || !std::isfinite(layout.size.y) ||
        layout.size.x <= 0.0f || layout.size.y <= 0.0f)
    {
        return fallback.x;
    }

    return std::max(0.03f, layout.size.x / kPreviewTextMeasurementPixelsPerLogicalUnit);
}

void EditorApplication::ResetPagePreviewView() noexcept
{
    layoutState_.pagePreviewView = {};
    if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        layoutState_.pagePreviewView = page->view;
        layoutState_.pagePreviewView.zoom = mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom);
    }
}

void EditorApplication::ResetLibraryPreviewView() noexcept
{
    layoutState_.libraryPreviewView = {};
    layoutState_.libraryPreviewView.zoom = 1.0f;
}

void EditorApplication::DrawPagePreview(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    EnsurePreviewTexture(static_cast<int>(viewport.size.x), static_cast<int>(viewport.size.y));
    if (!previewResources_.TextureReady())
    {
        return;
    }

    BeginTextureMode(previewResources_.Texture());
    ClearBackground(ToRayColor(page->backgroundColor));
    {
        if (layoutState_.pagePreviewViewOptions.showGrid)
        {
            editor::app::DrawViewportGrid(
                MakeViewportGridInput(viewport, layoutState_.pagePreviewViewOptions),
                ToRayColor(page->backgroundColor));
        }

        EnsurePreviewFont();
        editor::ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewResources_.Texture().texture.width,
            previewResources_.Texture().texture.height,
            viewport.view,
            PreviewTextFont(),
            ToRayColor(page->backgroundColor),
            previewResources_.TextureStencilReady(),
            &previewResources_.BezierCache(),
            &previewResources_.ImageCache(),
            &previewResources_.TextLayoutCache());
        services_.pagePreviewDrawOrder.CollectStaticReticleDrawOrder(*page, previewResources_.ScratchStaticReticleOrder());
        for (const int reticleIndex : previewResources_.ScratchStaticReticleOrder())
        {
            const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
            if (!IsReticleVisibleInEditor(*page, reticle))
            {
                continue;
            }

            if (ShouldDimPageReticleInCurrentFocus(*page, reticle))
            {
                canvas.DrawReticle(MakeDimmedReticlePreviewCopy(reticle, 0.30f));
            }
            else
            {
                canvas.DrawReticle(reticle);
            }
        }

        for (const auto& strobe : page->strobes)
        {
            if (!IsPageStrobeVisibleInEditor(*page, strobe))
            {
                continue;
            }

            canvas.DrawReticle(strobe.reticle);
        }

        const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(*page);
        canvas.DrawReticle(titleReticle);
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewResources_.Texture().texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("PagePreviewInput", viewport.size);

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    std::optional<mfd::Vec2> mouseLogical;
    if (IsPointInsideRect(mouse, viewport.origin, viewportMax))
    {
        mouseLogical = viewport.ToLogical(mouse);
    }

    tutorial_->DrawHalo(
        "page_preview_clip_source",
        "Right-click the circle reticle",
        "Open the clipping context menu on the tutorial mask so you can keep only the inside region.");

        if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MFD_LIBRARY_RETICLE"))
        {
            const char* templateId = static_cast<const char*>(payload->Data);
            if (tutorial_->ShouldUseHighlightedAddToPageButton())
            {
                RebuildStatus("Tutorial: use the highlighted Add to active page button for this step.", true);
            }
            else
            {
                const ImVec2 dropMousePosition = ImGui::GetMousePos();
                const bool dropInsideViewport = IsPointInsideRect(dropMousePosition, viewport.origin, viewportMax);
                const mfd::Vec2 rawDropPosition =
                    dropInsideViewport ? viewport.ToLogical(dropMousePosition) : viewport.view.center;
                const mfd::Vec2 dropPosition =
                    SnapToSharedGridIfEnabled(rawDropPosition, layoutState_.pagePreviewViewOptions);
                if (CreatePageReticleInstanceFromTemplate(templateId, dropPosition) && !dropInsideViewport)
                {
                    RebuildStatus("Reticle '" + std::string(templateId) +
                                      "' was dropped outside the viewport and placed at the current page-view center.",
                                  false);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (DrawViewportToolbar(
        viewport.origin,
        mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom),
        mouseLogical,
        "?##PagePreviewHelp",
        "R##PagePreviewRecenter",
        kPagePreviewHelpPopupId,
        false))
    {
        ResetPagePreviewView();
    }
}

void EditorApplication::DrawLibraryPreview(const ViewportState& viewport)
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    EnsurePreviewTexture(static_cast<int>(viewport.size.x), static_cast<int>(viewport.size.y));
    if (!previewResources_.TextureReady())
    {
        return;
    }

    BeginTextureMode(previewResources_.Texture());
    ClearBackground(Color {10, 18, 24, 255});
    {
        if (layoutState_.pagePreviewViewOptions.showGrid)
        {
            editor::app::DrawViewportGrid(
                MakeViewportGridInput(viewport, layoutState_.pagePreviewViewOptions),
                Color {10, 18, 24, 255});
        }

        EnsurePreviewFont();
        editor::ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewResources_.Texture().texture.width,
            previewResources_.Texture().texture.height,
            viewport.view,
            PreviewTextFont(),
            Color {10, 18, 24, 255},
            previewResources_.TextureStencilReady(),
            &previewResources_.BezierCache(),
            &previewResources_.ImageCache(),
            &previewResources_.TextLayoutCache());
        canvas.DrawReticle(*reticle);
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewResources_.Texture().texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("LibraryPreviewInput", viewport.size);
    if (ImGui::IsItemHovered() && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
    {
        if (layoutState_.suppressNextLibraryPreviewContextMenu)
        {
            layoutState_.suppressNextLibraryPreviewContextMenu = false;
        }
        else if (!ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("LibraryPreviewContextMenu");
        }
    }
    if (ImGui::BeginPopup("LibraryPreviewContextMenu"))
    {
        const bool hasSelectedPrimitive = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                                          documentState_.selection.libraryReticleId == reticle->id &&
                                          SelectedLibraryPrimitive() != nullptr;
        if (ImGui::MenuItem(hasSelectedPrimitive ? "Copy primitive" : "Copy reticle", "Ctrl+C"))
        {
            if (hasSelectedPrimitive)
            {
                CopySelectedLibraryPrimitive();
            }
            else
            {
                CopySelectedLibraryReticle();
            }
        }
        ShowItemTooltip(
            hasSelectedPrimitive
                ? "Copy the focused primitive into the reticle-studio clipboard."
                : "Copy the current shared reticle template.");

        const bool canPasteCurrentSelection =
            hasSelectedPrimitive ? clipboardState_.libraryPrimitiveClipboard.has_value() : clipboardState_.libraryReticleClipboard.has_value();
        if (ImGui::MenuItem(hasSelectedPrimitive ? "Paste copied primitive" : "Paste copied reticle",
                            "Ctrl+V",
                            false,
                            canPasteCurrentSelection))
        {
            if (hasSelectedPrimitive)
            {
                PasteCopiedLibraryPrimitive();
            }
            else
            {
                PasteCopiedLibraryReticle();
            }
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip(
            hasSelectedPrimitive
                ? "Paste the copied primitive into the current reticle template."
                : "Paste the copied shared reticle template as one new library entry.");

        ImGui::Separator();

        if (ImGui::MenuItem("Rename reticle globally..."))
        {
            OpenReticleRenamePopup(reticle->id);
        }

        if (ImGui::MenuItem("Delete library reticle", "Del"))
        {
            DeleteSelectedLibraryReticle();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::EndPopup();
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    std::optional<mfd::Vec2> mouseLogical;
    if (IsPointInsideRect(mouse, viewport.origin, viewportMax))
    {
        mouseLogical = viewport.ToLogical(mouse);
    }

    if (DrawViewportToolbar(
        viewport.origin,
        mfd::SanitizeZoom(layoutState_.libraryPreviewView.zoom),
        mouseLogical,
        "?##LibraryPreviewHelp",
        "R##LibraryPreviewRecenter",
        kLibraryPreviewHelpPopupId,
        true))
    {
        ResetLibraryPreviewView();
    }
}

void EditorApplication::DrawPagePreviewHeaderControls(const char* buttonId,
                                                      const bool showProblemsIndicator,
                                                      const bool allowFullscreenToggle)
{
    using editor::tutorial::TutorialStepId;

    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string label =
        showProblemsIndicator && !layoutState_.pagePreviewViewOptions.showProblemsPanel ? "View !" : "View";
    const std::string buttonLabel = label + (buttonId == nullptr ? "##PagePreviewViewMenu" : buttonId);
    const float buttonWidth = ImGui::CalcTextSize(label.c_str()).x + style.FramePadding.x * 2.0f;
    const float fullscreenButtonWidth =
        allowFullscreenToggle ? ImGui::CalcTextSize("[]").x + style.FramePadding.x * 2.0f : 0.0f;
    const float controlsWidth =
        buttonWidth + (allowFullscreenToggle ? style.ItemSpacing.x + fullscreenButtonWidth : 0.0f);

    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - controlsWidth));
    if (ImGui::Button(buttonLabel.c_str()))
    {
        if (tutorial_->MatchesTarget("page_preview_view_menu"))
        {
            tutorial_->AdvancePhase();
        }
        ImGui::OpenPopup(kPagePreviewDisplayPopupId);
    }
    ShowItemTooltip("Toggle page-preview overlays and editor-only helper panels.");
    tutorial_->DrawHalo(
        "page_preview_view_menu",
        "Click View",
        "Open the page-preview helper menu so the coach can walk through the editor-only overlays one by one.");

    if (allowFullscreenToggle)
    {
        ImGui::SameLine();
        if (ImGui::Button("[]##PagePreviewFullscreenToggle"))
        {
            ToggleFullscreenPagePreview();
            if (tutorial_->MatchesTarget("page_preview_fullscreen"))
            {
                if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ToggleFullscreenPreview), 0))
                {
                    tutorial_->AdvancePhase();
                }
                else
                {
                    tutorial_->CompleteStep();
                }
            }
        }
        ShowItemTooltip(services_.fullscreenPreview.IsActive() ? "Exit fullscreen preview" : "Fullscreen page preview");
        tutorial_->DrawHalo(
            "page_preview_fullscreen",
            "Toggle fullscreen preview",
            tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ToggleFullscreenPreview), 0)
                ? "Enter fullscreen preview to focus on the page canvas."
                : "Leave fullscreen preview to restore the normal editor layout.");
    }

    if (ImGui::BeginPopup(kPagePreviewDisplayPopupId))
    {
        const bool layerInspectorChanged = ImGui::Checkbox("Layer Inspector", &layoutState_.pagePreviewViewOptions.showLayerInspector);
        if (layerInspectorChanged && !layoutState_.pagePreviewViewOptions.showLayerInspector)
        {
            ClearLayerFocus(false);
            ReleaseLayerPreviewTextures();
        }
        if (layerInspectorChanged && layoutState_.pagePreviewViewOptions.showLayerInspector &&
            tutorial_->MatchesTarget("page_preview_view_layer_inspector"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_layer_inspector",
            "Enable Layer Inspector",
            "Turn on the layer strip to inspect editor-only layers and their thumbnails.");

        const bool minimapChanged = ImGui::Checkbox("Minimap", &layoutState_.pagePreviewViewOptions.showMinimap);
        if (minimapChanged && layoutState_.pagePreviewViewOptions.showMinimap &&
            tutorial_->MatchesTarget("page_preview_view_minimap"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_minimap",
            "Enable Minimap",
            "Turn on the minimap so page navigation stays readable while zooming and panning.");

        const bool problemsChanged = ImGui::Checkbox("Problems", &layoutState_.pagePreviewViewOptions.showProblemsPanel);
        if (problemsChanged && layoutState_.pagePreviewViewOptions.showProblemsPanel &&
            tutorial_->MatchesTarget("page_preview_view_problems"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_problems",
            "Enable Problems",
            "Dock the validation panel under the preview so diagnostics stay visible while editing.");

        const bool highlightChanged = ImGui::Checkbox("Highlight reticle usages", &layoutState_.pagePreviewViewOptions.highlightReticleUsages);
        if (highlightChanged && layoutState_.pagePreviewViewOptions.highlightReticleUsages &&
            tutorial_->MatchesTarget("page_preview_view_highlight_usages"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_highlight_usages",
            "Enable Highlight reticle usages",
            "Turn on template usage highlighting for the currently selected shared reticle.");
        ImGui::Separator();
        ImGui::Checkbox("Reticle names", &layoutState_.pagePreviewViewOptions.showReticleNames);
        ImGui::Checkbox("Gizmos", &layoutState_.pagePreviewViewOptions.showGizmos);
        ImGui::Checkbox("Grid", &layoutState_.pagePreviewViewOptions.showGrid);
        ShowItemTooltip("Draw a discreet editor-only grid matching the shared logical snapping step.");
        ImGui::Checkbox("Snap to grid", &layoutState_.pagePreviewViewOptions.snapToGrid);
        ShowItemTooltip("Snap page-preview drags and nudges plus reticle-studio edits to the shared logical grid. "
                        "Arrow keys move the page selection; hold Shift for a larger step when snapping is off.");
        if (layoutState_.pagePreviewViewOptions.showGrid || layoutState_.pagePreviewViewOptions.snapToGrid)
        {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Grid step", &layoutState_.pagePreviewViewOptions.gridStepLogical, 0.005f, 0.01f, 0.5f, "%.3f");
            layoutState_.pagePreviewViewOptions.gridStepLogical =
                editor::app::SanitizeGridStepLogical(layoutState_.pagePreviewViewOptions.gridStepLogical);
            ShowItemTooltip("Shared logical spacing reused by the visible grid, page-preview snapping, and reticle-studio snapping.");
        }
        const bool pageContextChanged = ImGui::Checkbox("Page context", &layoutState_.pagePreviewViewOptions.showPageContext);
        if (pageContextChanged && layoutState_.pagePreviewViewOptions.showPageContext &&
            tutorial_->MatchesTarget("page_preview_view_page_context"))
        {
            tutorial_->CompleteStep();
        }
        tutorial_->DrawHalo(
            "page_preview_view_page_context",
            "Enable Page context",
            "Turn on the page-context split so the active page stays visible while you inspect the rest of the workspace.");
        if (showProblemsIndicator && !layoutState_.pagePreviewViewOptions.showProblemsPanel)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Validation issues are available. Enable Problems to inspect them below the preview.");
        }
        ImGui::EndPopup();
    }
    else if (tutorial_->ShouldResetPagePreviewViewPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }
}

void EditorApplication::DrawLibraryPreviewOverlays(const ViewportState& viewport)
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasSelectedPrimitive = documentState_.selection.kind == SelectionKind::LibraryPrimitive &&
                                      documentState_.selection.libraryReticleId == reticle->id &&
                                      documentState_.selection.primitiveIndex >= 0 &&
                                      documentState_.selection.primitiveIndex < static_cast<int>(reticle->primitives.size());

    auto toScreenPoint = [&viewport, reticle](const mfd::Primitive& primitive, const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(TransformPrimitiveWorldPoint(*reticle, primitive, localPoint));
    };

    auto drawHandle = [drawList](const ImVec2 point, const ImU32 color, const float radius = 6.0f)
    {
        drawList->AddCircleFilled(point, radius, color, 16);
        drawList->AddCircle(point, radius, IM_COL32(10, 18, 24, 255), 16, 1.5f);
    };

    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const bool selected = hasSelectedPrimitive && documentState_.selection.primitiveIndex == primitiveIndex;
        if (layoutState_.libraryStudioShowGizmos)
        {
            const ImU32 borderColor = selected ? IM_COL32(255, 212, 110, 255) : IM_COL32(104, 185, 205, 160);
            const ImU32 fillColor = selected ? IM_COL32(255, 212, 110, 32) : IM_COL32(104, 185, 205, 18);
            drawList->AddRectFilled(bounds.min, bounds.max, fillColor, 6.0f);
            drawList->AddRect(bounds.min, bounds.max, borderColor, 6.0f, 0, selected ? 2.2f : 1.3f);
        }

        if (layoutState_.libraryStudioShowPrimitiveLabels)
        {
            const std::string label =
                std::to_string(primitiveIndex + 1) + ". " +
                (primitive.id.empty() ? PrimitiveTypeLabel(primitive.type) : primitive.id);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
            const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
            drawList->AddRectFilled(tagMin, tagMax, selected ? IM_COL32(255, 212, 110, 220) : IM_COL32(33, 49, 59, 210), 4.0f);
            drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                              selected ? IM_COL32(12, 20, 26, 255) : IM_COL32(220, 235, 240, 255),
                              label.c_str());
        }

        if (!selected || !layoutState_.libraryStudioShowGizmos)
        {
            continue;
        }

        const ImVec2 primitiveCenter = toScreenPoint(primitive, {});
        drawHandle(primitiveCenter, IM_COL32(94, 224, 174, 255), 7.0f);

        if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, line->start), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, line->end), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {circle->radius, 0.0f}))),
                                IM_COL32(255, 212, 110, 140),
                                48,
                                1.2f);
            drawHandle(toScreenPoint(primitive, {circle->radius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ring->outerRadius, 0.0f}))),
                                IM_COL32(255, 212, 110, 140),
                                48,
                                1.2f);
            drawList->AddCircle(primitiveCenter,
                                std::max(4.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ring->innerRadius, 0.0f}))),
                                IM_COL32(255, 212, 110, 90),
                                48,
                                1.0f);
            drawHandle(toScreenPoint(primitive, {ring->innerRadius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            drawHandle(toScreenPoint(primitive, {ring->outerRadius, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {-rectangle->width * 0.5f, -rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {rectangle->width * 0.5f, -rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {rectangle->width * 0.5f, rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-rectangle->width * 0.5f, rectangle->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
        {
            drawList->AddCircle(primitiveCenter,
                                std::max(8.0f, Distance(primitiveCenter, toScreenPoint(primitive, {ellipse->width * 0.5f, 0.0f}))),
                                IM_COL32(255, 212, 110, 90),
                                48,
                                1.0f);
            drawHandle(toScreenPoint(primitive, {ellipse->width * 0.5f, 0.0f}), IM_COL32(110, 180, 250, 255), 7.0f);
            drawHandle(toScreenPoint(primitive, {0.0f, ellipse->height * 0.5f}), IM_COL32(110, 180, 250, 255), 7.0f);
            continue;
        }

        if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {-square->width * 0.5f, -square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {square->width * 0.5f, -square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {square->width * 0.5f, square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-square->width * 0.5f, square->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, {0.0f, diamond->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {diamond->width * 0.5f, 0.0f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {0.0f, -diamond->height * 0.5f}), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, {-diamond->width * 0.5f, 0.0f}), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
        {
            drawHandle(toScreenPoint(primitive, triangle->points[0]), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, triangle->points[1]), IM_COL32(255, 140, 92, 255));
            drawHandle(toScreenPoint(primitive, triangle->points[2]), IM_COL32(255, 140, 92, 255));
            continue;
        }

        if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
        {
            for (const auto& point : polyline->points)
            {
                drawHandle(toScreenPoint(primitive, point), IM_COL32(255, 140, 92, 255));
            }
            continue;
        }

        if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
        {
            for (const auto& point : bezier->controlPoints)
            {
                drawHandle(toScreenPoint(primitive, point), IM_COL32(255, 140, 92, 255));
            }
            continue;
        }

        if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
        {
            const std::vector<mfd::Vec2> arcPoints =
                ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
            for (std::size_t index = 0; index + 1U < arcPoints.size(); ++index)
            {
                drawList->AddLine(toScreenPoint(primitive, arcPoints[index]),
                                  toScreenPoint(primitive, arcPoints[index + 1U]),
                                  IM_COL32(255, 212, 110, 110),
                                  1.5f);
            }

            const float middleAngleDegrees = (arc->startAngleDegrees + arc->endAngleDegrees) * 0.5f;
            const float middleAngleRadians = middleAngleDegrees * PI / 180.0f;
            const mfd::Vec2 middlePoint {
                std::cos(middleAngleRadians) * std::abs(arc->radius),
                std::sin(middleAngleRadians) * std::abs(arc->radius)};

            if (!arcPoints.empty())
            {
                drawHandle(toScreenPoint(primitive, arcPoints.front()), IM_COL32(255, 140, 92, 255));
                drawHandle(toScreenPoint(primitive, arcPoints.back()), IM_COL32(255, 140, 92, 255));
            }
            drawHandle(toScreenPoint(primitive, middlePoint), IM_COL32(110, 180, 250, 255), 7.0f);
        }
    }
}

void EditorApplication::DrawPreviewOverlays(const ViewportState& viewport)
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    if (layoutState_.pagePreviewViewOptions.showMinimap)
    {
        DrawPagePreviewMinimap(viewport, *page);
    }

    if (layoutState_.pagePreviewViewOptions.showReticleNames)
    {
        DrawPagePreviewReticleNames(viewport, *page);
    }

    if (layoutState_.pagePreviewViewOptions.highlightReticleUsages)
    {
        DrawReticleUsageHighlightPlaceholder(viewport);
    }

    if (layoutState_.pagePreviewViewOptions.showGizmos)
    {
        DrawPagePreviewGizmos(viewport, *page);
    }
}

void EditorApplication::DrawPagePreviewMinimap(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const PageMinimapState minimap = ComputePageMinimapState(page, viewport);
    if (!minimap.valid)
    {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool mouseInsideMinimap = IsPointInsideRect(mouse, minimap.contentMin, minimap.contentMax);

    drawList->AddRectFilled(minimap.frameMin, minimap.frameMax, IM_COL32(7, 15, 23, 224), 8.0f);
    drawList->AddRect(minimap.frameMin, minimap.frameMax, IM_COL32(68, 118, 152, 255), 8.0f, 0, 1.5f);
    drawList->AddRectFilled(minimap.contentMin, minimap.contentMax, IM_COL32(12, 24, 34, 220), 6.0f);

    if (minimap.logicalMin.x <= 0.0f && minimap.logicalMax.x >= 0.0f)
    {
        const ImVec2 axisBottom = ToMinimapScreen(minimap, mfd::Vec2 {0.0f, minimap.logicalMin.y});
        const ImVec2 axisTop = ToMinimapScreen(minimap, mfd::Vec2 {0.0f, minimap.logicalMax.y});
        drawList->AddLine(axisBottom, axisTop, IM_COL32(52, 79, 96, 255), 1.0f);
    }

    if (minimap.logicalMin.y <= 0.0f && minimap.logicalMax.y >= 0.0f)
    {
        const ImVec2 axisLeft = ToMinimapScreen(minimap, mfd::Vec2 {minimap.logicalMin.x, 0.0f});
        const ImVec2 axisRight = ToMinimapScreen(minimap, mfd::Vec2 {minimap.logicalMax.x, 0.0f});
        drawList->AddLine(axisLeft, axisRight, IM_COL32(52, 79, 96, 255), 1.0f);
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        const LogicalBounds worldBounds = ComputeReticleWorldBounds(reticle);
        if (!worldBounds.valid)
        {
            continue;
        }

        const ImVec2 rectPointA = ToMinimapScreen(minimap, worldBounds.min);
        const ImVec2 rectPointB = ToMinimapScreen(minimap, worldBounds.max);
        const ImVec2 rectMin(std::min(rectPointA.x, rectPointB.x), std::min(rectPointA.y, rectPointB.y));
        const ImVec2 rectMax(std::max(rectPointA.x, rectPointB.x), std::max(rectPointA.y, rectPointB.y));
        const bool selected = std::find(selectedIndices.begin(), selectedIndices.end(), reticleIndex) != selectedIndices.end();

        if (rectMax.x - rectMin.x < 4.0f || rectMax.y - rectMin.y < 4.0f)
        {
            const ImVec2 center = ToMinimapScreen(minimap, worldBounds.center);
            drawList->AddCircleFilled(center,
                                      selected ? 3.5f : 2.5f,
                                      selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(174, 200, 214, 230),
                                      12);
            continue;
        }

        drawList->AddRectFilled(rectMin,
                                rectMax,
                                selected ? IM_COL32(84, 219, 201, 70) : IM_COL32(174, 200, 214, 34),
                                2.0f);
        drawList->AddRect(rectMin,
                          rectMax,
                          selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(174, 200, 214, 190),
                          2.0f,
                          0,
                          selected ? 1.8f : 1.0f);
    }

    for (std::size_t strobeIndex = 0; strobeIndex < page.strobes.size(); ++strobeIndex)
    {
        const mfd::PageStrobeDefinition& strobe = page.strobes[strobeIndex];
        if (!IsPageStrobeVisibleInEditor(page, strobe))
        {
            continue;
        }

        const LogicalBounds worldBounds = ComputeReticleWorldBounds(strobe.reticle);
        if (worldBounds.valid)
        {
            const ImVec2 rectPointA = ToMinimapScreen(minimap, worldBounds.min);
            const ImVec2 rectPointB = ToMinimapScreen(minimap, worldBounds.max);
            const ImVec2 rectMin(std::min(rectPointA.x, rectPointB.x), std::min(rectPointA.y, rectPointB.y));
            const ImVec2 rectMax(std::max(rectPointA.x, rectPointB.x), std::max(rectPointA.y, rectPointB.y));
            const bool selected =
                documentState_.selection.kind == SelectionKind::PageStrobe &&
                SelectedPageStrobeReticle() == &strobe.reticle;

            if (rectMax.x - rectMin.x < 4.0f || rectMax.y - rectMin.y < 4.0f)
            {
                const ImVec2 center = ToMinimapScreen(minimap, worldBounds.center);
                drawList->AddCircleFilled(center,
                                          selected ? 3.5f : 2.5f,
                                          selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(255, 224, 176, 235),
                                          12);
            }
            else
            {
                drawList->AddRectFilled(rectMin,
                                        rectMax,
                                        selected ? IM_COL32(84, 219, 201, 70) : IM_COL32(255, 224, 176, 34),
                                        2.0f);
                drawList->AddRect(rectMin,
                                  rectMax,
                                  selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(255, 224, 176, 210),
                                  2.0f,
                                  0,
                                  selected ? 1.8f : 1.0f);
            }
        }
    }

    const LogicalBounds viewBounds = ComputeViewportLogicalBounds(viewport);
    if (viewBounds.valid)
    {
        const ImVec2 viewA = ToMinimapScreen(minimap, viewBounds.min);
        const ImVec2 viewB = ToMinimapScreen(minimap, viewBounds.max);
        const ImVec2 viewMin(std::min(viewA.x, viewB.x), std::min(viewA.y, viewB.y));
        const ImVec2 viewMax(std::max(viewA.x, viewB.x), std::max(viewA.y, viewB.y));
        drawList->AddRectFilled(viewMin, viewMax, IM_COL32(110, 180, 250, 38), 4.0f);
        drawList->AddRect(viewMin, viewMax, IM_COL32(110, 180, 250, 255), 4.0f, 0, 1.8f);
    }

    const char* minimapLabel = "Minimap";
    const ImVec2 textSize = ImGui::CalcTextSize(minimapLabel);
    drawList->AddText(ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + 8.0f),
                      IM_COL32(216, 233, 246, 255),
                      minimapLabel);
    drawList->AddLine(ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                      ImVec2(minimap.frameMax.x - 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                      IM_COL32(36, 63, 78, 255),
                      1.0f);

    ShowHoveredRegionTooltip(
        mouseInsideMinimap,
        "Minimap navigation for the editor camera.\n"
        "Drag the blue viewport rectangle to pan smoothly.\n"
        "Click elsewhere in the minimap to recenter the editor view.");
}

void EditorApplication::DrawPagePreviewReticleNames(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, reticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const std::string label = reticle.id.empty() ? std::string {"reticle"} : reticle.id;
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
        const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
        const bool selected = HasSelectedPageReticle(documentState_.selection.pageIndex, reticleIndex);
        const bool dimmed = ShouldDimPageReticleInCurrentFocus(page, reticle);
        drawList->AddRectFilled(tagMin,
                                tagMax,
                                selected ? IM_COL32(84, 219, 201, 220)
                                         : (dimmed ? IM_COL32(24, 30, 36, 188) : IM_COL32(33, 49, 59, 210)),
                                4.0f);
        drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                          selected ? IM_COL32(12, 20, 26, 255)
                                   : (dimmed ? IM_COL32(146, 160, 170, 220) : IM_COL32(220, 235, 240, 255)),
                          label.c_str());
    }

    for (std::size_t strobeIndex = 0; strobeIndex < page.strobes.size(); ++strobeIndex)
    {
        const mfd::PageStrobeDefinition& strobe = page.strobes[strobeIndex];
        const mfd::ReticleGroup& strobeReticle = strobe.reticle;
        if (!IsPageStrobeVisibleInEditor(page, strobe))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(strobeReticle, viewport);
        if (bounds.valid)
        {
            const std::string label = "strobe: " + PageStrobeDisplayLabel(page, strobe, strobeIndex);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
            const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
            const bool selected =
                documentState_.selection.kind == SelectionKind::PageStrobe &&
                SelectedPageStrobeReticle() == &strobe.reticle;
            drawList->AddRectFilled(tagMin,
                                    tagMax,
                                    selected ? IM_COL32(84, 219, 201, 220)
                                             : IM_COL32(64, 58, 33, 210),
                                    4.0f);
            drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                              selected ? IM_COL32(12, 20, 26, 255)
                                       : IM_COL32(255, 236, 196, 255),
                              label.c_str());
        }
    }

    const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(page);
    if (titleReticle.visible)
    {
        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(titleReticle, viewport);
        if (bounds.valid)
        {
            const std::string label = "title";
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const ImVec2 tagMin(bounds.min.x + 6.0f, bounds.min.y + 6.0f);
            const ImVec2 tagMax(tagMin.x + textSize.x + 12.0f, tagMin.y + textSize.y + 6.0f);
            const bool selected = IsPageTitleSelected();
            drawList->AddRectFilled(tagMin,
                                    tagMax,
                                    selected ? IM_COL32(84, 219, 201, 220) : IM_COL32(62, 76, 96, 210),
                                    4.0f);
            drawList->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + 3.0f),
                              selected ? IM_COL32(12, 20, 26, 255) : IM_COL32(225, 233, 244, 255),
                              label.c_str());
        }
    }
}

void EditorApplication::DrawLayerInspectorPanel(const mfd::PageDefinition& page)
{
    const editor::LayerFocusStripModel model = services_.layerFocus.BuildStripModel(page, layoutState_.layerFocusState);
    ImGui::TextColored(ImVec4(0.85f, 0.91f, 0.96f, 1.0f), "Layer Inspector");
    ImGui::TextDisabled("Focus one layer without changing JSON.");
    ImGui::Separator();

    const float previewWidth = std::max(72.0f, ImGui::GetContentRegionAvail().x);
    const int previewWidthPixels = std::max(72, static_cast<int>(std::lround(previewWidth)));
    const int previewHeightPixels = static_cast<int>(kLayerInspectorPreviewHeight);

    for (std::size_t index = 0; index < model.entries.size(); ++index)
    {
        const editor::LayerFocusStripEntry& entry = model.entries[index];
        const bool pressed = ImGui::Selectable(entry.label.c_str(), entry.selected);
        if (pressed)
        {
            if (entry.fullView)
            {
                ClearLayerFocus(true);
            }
            else
            {
                layoutState_.layerFocusState = services_.layerFocus.MakeFocusedState(page, entry.layerId);
                SanitizePageReticleSelectionForCurrentFocus();
                RebuildStatus("Layer focus set to '" + entry.layerId + "' on page '" + page.name + "'.", false);
            }
        }

        const std::string metaLabel =
            entry.fullView ? std::to_string(entry.reticleCount) + " page reticle(s)"
                           : std::to_string(entry.reticleCount) + " reticle(s)" +
                                 (entry.visible ? "" : "  hidden in preview");
        ImGui::TextDisabled("%s", metaLabel.c_str());

        if (const RenderTexture2D* previewTexture =
                RenderLayerPreviewThumbnail(index, page, entry, previewWidthPixels, previewHeightPixels);
            previewTexture != nullptr)
        {
            ImGui::Image((ImTextureID)(uintptr_t)previewTexture->texture.id,
                         ImVec2(previewWidth, kLayerInspectorPreviewHeight),
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
        else
        {
            ImGui::Dummy(ImVec2(previewWidth, kLayerInspectorPreviewHeight));
        }

        if (entry.reticleCount == 0U)
        {
            ImGui::TextDisabled("No page reticles currently target this layer.");
        }

        if (!entry.fullView && !entry.visible)
        {
            ShowItemTooltip("This editor layer is currently hidden in the page preview.");
        }

        if (index + 1U < model.entries.size())
        {
            ImGui::Spacing();
            ImGui::Separator();
        }
    }

    previewResources_.TrimLayerTextures(model.entries.size());
}

void EditorApplication::DrawPagePreviewGizmos(const ViewportState& viewport, const mfd::PageDefinition& page)
{
    const auto drawSingleSelectionHandles = [&viewport](const mfd::ReticleGroup& reticle, const ReticleScreenBounds& bounds)
    {
        const mfd::Vec2 visualCenterLogical = mfd::ApplyTransform(ReticleVisualCenterLocal(reticle), reticle.transform);
        const ImVec2 center = viewport.ToScreen(visualCenterLogical);
        const ImVec2 topLeft = bounds.min;
        const ImVec2 topRight(bounds.max.x, bounds.min.y);
        const ImVec2 bottomRight = bounds.max;
        const ImVec2 bottomLeft(bounds.min.x, bounds.max.y);
        const ImVec2 topCenter((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y);
        const ImVec2 rotateHandle(topCenter.x, topCenter.y - 26.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircle(center, 5.0f, IM_COL32(84, 219, 201, 255), 18, 2.0f);
        drawList->AddLine(topCenter, rotateHandle, IM_COL32(110, 180, 250, 255), 1.5f);
        drawList->AddCircle(rotateHandle, 8.0f, IM_COL32(110, 180, 250, 255), 20, 2.0f);
        drawList->AddLine(ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
                          ImVec2(rotateHandle.x + 10.0f, rotateHandle.y - 7.0f),
                          IM_COL32(110, 180, 250, 255),
                          2.0f);
        drawList->AddLine(ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
                          ImVec2(rotateHandle.x + 10.0f, rotateHandle.y + 1.0f),
                          IM_COL32(110, 180, 250, 255),
                          2.0f);

        const auto drawCorner = [drawList](const ImVec2 corner)
        {
            drawList->AddRectFilled(ImVec2(corner.x - 5.5f, corner.y - 5.5f),
                                    ImVec2(corner.x + 5.5f, corner.y + 5.5f),
                                    IM_COL32(255, 193, 92, 255),
                                    2.0f);
        };

        drawCorner(topLeft);
        drawCorner(topRight);
        drawCorner(bottomRight);
        drawCorner(bottomLeft);
    };

    if (documentState_.selection.kind == SelectionKind::PageTitle)
    {
        const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(page);
        if (!titleReticle.visible)
        {
            return;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(titleReticle, viewport);
        if (!bounds.valid)
        {
            return;
        }

        ImGui::GetWindowDrawList()->AddRect(bounds.min, bounds.max, IM_COL32(84, 219, 201, 255), 4.0f, 0, 2.0f);
        drawSingleSelectionHandles(titleReticle, bounds);
        return;
    }

    if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        const mfd::ReticleGroup* strobeReticle = SelectedPageStrobeReticle();
        if (strobeReticle == nullptr || !IsPageStrobeIndexVisibleInEditor(page, documentState_.selection.pageReticleIndex))
        {
            return;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*strobeReticle, viewport);
        if (!bounds.valid)
        {
            return;
        }

        ImGui::GetWindowDrawList()->AddRect(bounds.min, bounds.max, IM_COL32(84, 219, 201, 255), 4.0f, 0, 2.0f);
        drawSingleSelectionHandles(*strobeReticle, bounds);
        return;
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (selectedIndices.empty() || documentState_.selection.kind != SelectionKind::PageReticle)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ReticleScreenBounds selectionBounds;
    for (const int reticleIndex : selectedIndices)
    {
        const mfd::ReticleGroup& selectedReticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(page, selectedReticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(selectedReticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const bool primarySelection = reticleIndex == documentState_.selection.pageReticleIndex;
        drawList->AddRect(bounds.min,
                          bounds.max,
                          primarySelection ? IM_COL32(84, 219, 201, 255) : IM_COL32(84, 219, 201, 150),
                          4.0f,
                          0,
                          primarySelection ? 2.0f : 1.2f);

        if (!selectionBounds.valid)
        {
            selectionBounds = bounds;
        }
        else
        {
            selectionBounds.min.x = std::min(selectionBounds.min.x, bounds.min.x);
            selectionBounds.min.y = std::min(selectionBounds.min.y, bounds.min.y);
            selectionBounds.max.x = std::max(selectionBounds.max.x, bounds.max.x);
            selectionBounds.max.y = std::max(selectionBounds.max.y, bounds.max.y);
        }
    }

    if (!selectionBounds.valid)
    {
        return;
    }

    selectionBounds.center = ImVec2((selectionBounds.min.x + selectionBounds.max.x) * 0.5f,
                                    (selectionBounds.min.y + selectionBounds.max.y) * 0.5f);

    if (selectedIndices.size() != 1U)
    {
        const std::string label = std::to_string(selectedIndices.size()) + " reticles selected";
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 tagMin(selectionBounds.min.x + 8.0f, selectionBounds.min.y + 8.0f);
        const ImVec2 tagMax(tagMin.x + textSize.x + 14.0f, tagMin.y + textSize.y + 8.0f);
        drawList->AddRectFilled(tagMin, tagMax, IM_COL32(7, 15, 23, 220), 5.0f);
        drawList->AddText(ImVec2(tagMin.x + 7.0f, tagMin.y + 4.0f), IM_COL32(216, 233, 246, 255), label.c_str());
        return;
    }

    const mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr || !IsReticleVisibleInEditor(page, *reticle))
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*reticle, viewport);
    if (!bounds.valid)
    {
        return;
    }

    drawSingleSelectionHandles(*reticle, bounds);
}

void EditorApplication::HandlePreviewInteraction(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    auto cancelPreviewInteraction = [this]()
    {
        interactionState_.mode = InteractionMode::None;
        interactionState_.reticleIndex = -1;
        interactionState_.reticleIndices.clear();
        interactionState_.startReticleTransforms.clear();
    };

    ViewportState interactiveViewport = viewport;
    interactiveViewport.view = layoutState_.pagePreviewView;

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    const std::optional<mfd::Vec2> mouseLogical =
        IsPointInsideRect(mouse, viewport.origin, viewportMax) ? std::optional<mfd::Vec2> {viewport.ToLogical(mouse)}
                                                               : std::nullopt;
    const ViewportToolbarLayout toolbarLayout =
        ComputeViewportToolbarLayout(viewport.origin, mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom), mouseLogical);
    const bool mouseInsideViewport = IsPointInsideRect(mouse, viewport.origin, viewportMax);
    const bool mouseInsideToolbar = IsPointInsideRect(mouse, toolbarLayout.toolbarMin, toolbarLayout.toolbarMax);
    const bool anyPopupOpen = ImGui::IsPopupOpen((const char*)nullptr, ImGuiPopupFlags_AnyPopupId);
    if (!mouseInsideViewport || mouseInsideToolbar || anyPopupOpen)
    {
        if (interactionState_.mode != InteractionMode::None)
        {
            const bool interactionButtonReleased =
                interactionState_.mode == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
            if (anyPopupOpen || interactionButtonReleased)
            {
                if (interactionState_.mode == InteractionMode::PanPage)
                {
                    layoutState_.suppressNextPagePreviewContextMenu = false;
                }
                cancelPreviewInteraction();
            }
        }
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionState_.mode == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinPageZoom = 0.1f;
        constexpr float kMaxPageZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom = std::clamp(mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom), kMinPageZoom, kMaxPageZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinPageZoom, kMaxPageZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const mfd::Vec2 mouseLogicalBeforeZoom = interactiveViewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - layoutState_.pagePreviewView.center;
            const float zoomRatio = currentZoom / nextZoom;

            layoutState_.pagePreviewView.zoom = nextZoom;
            layoutState_.pagePreviewView.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
            interactiveViewport.view = layoutState_.pagePreviewView;
        }
    }

    const PageMinimapState minimap =
        layoutState_.pagePreviewViewOptions.showMinimap ? ComputePageMinimapState(*page, interactiveViewport) : PageMinimapState {};
    const bool mouseInsideMinimap =
        layoutState_.pagePreviewViewOptions.showMinimap && minimap.valid && IsPointInsideRect(mouse, minimap.contentMin, minimap.contentMax);
    if (interactionState_.mode == InteractionMode::None && mouseInsideMinimap && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        interactionState_.mode = InteractionMode::NavigateMinimap;
        interactionState_.reticleIndex = -1;

        const LogicalBounds viewBounds = ComputeViewportLogicalBounds(interactiveViewport);
        bool clickedInsideViewRect = false;
        if (viewBounds.valid)
        {
            const ImVec2 viewA = ToMinimapScreen(minimap, viewBounds.min);
            const ImVec2 viewB = ToMinimapScreen(minimap, viewBounds.max);
            const ImVec2 viewMin(std::min(viewA.x, viewB.x), std::min(viewA.y, viewB.y));
            const ImVec2 viewMax(std::max(viewA.x, viewB.x), std::max(viewA.y, viewB.y));
            clickedInsideViewRect = IsPointInsideRect(mouse, viewMin, viewMax);
        }

        const mfd::Vec2 clickedLogical = ToMinimapLogical(minimap, mouse);
        if (clickedInsideViewRect)
        {
            layoutState_.minimapDragOffsetLogical = {
                clickedLogical.x - layoutState_.pagePreviewView.center.x,
                clickedLogical.y - layoutState_.pagePreviewView.center.y};
        }
        else
        {
            layoutState_.pagePreviewView.center = clickedLogical;
            layoutState_.minimapDragOffsetLogical = {};
            interactiveViewport.view = layoutState_.pagePreviewView;
        }
    }

    if (interactionState_.mode == InteractionMode::NavigateMinimap)
    {
        if (!minimap.valid)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }

        const mfd::Vec2 draggedLogical = ToMinimapLogical(minimap, mouse);
        layoutState_.pagePreviewView.center = {
            draggedLogical.x - layoutState_.minimapDragOffsetLogical.x,
            draggedLogical.y - layoutState_.minimapDragOffsetLogical.y};
        if (!leftMouseDown)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
        }
        return;
    }

    if (interactionState_.mode == InteractionMode::None &&
        !mouseInsideMinimap &&
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
    {
        if (layoutState_.suppressNextPagePreviewContextMenu)
        {
            layoutState_.suppressNextPagePreviewContextMenu = false;
            return;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            return;
        }

        // Keep every hovered reticle reachable so overlapping masks can still be clipped intentionally.
        const std::vector<int> hoveredReticleIndices = CollectPageReticlesAt(interactiveViewport, mouse);
        const std::vector<PageClipTarget> hoveredClipTargets = CollectPageClipTargetsAt(interactiveViewport, mouse);
        if (!hoveredReticleIndices.empty())
        {
            if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::ClipCircleOutside), 0))
            {
                const auto tutorialTarget = std::find_if(
                    hoveredClipTargets.begin(),
                    hoveredClipTargets.end(),
                    [page, this](const PageClipTarget& candidate)
                    {
                        const mfd::ReticleGroup& reticle =
                            page->staticReticles[static_cast<std::size_t>(candidate.reticleIndex)];
                        return tutorial_->TrackedReticleId().empty() || reticle.id == tutorial_->TrackedReticleId();
                    });
                if (tutorialTarget == hoveredClipTargets.end())
                {
                    RebuildStatus("Tutorial: right-click the circle reticle created in the previous step.", true);
                    return;
                }

                tutorial_->AdvancePhase();
            }

            if (hoveredReticleIndices.size() == 1U &&
                !HasSelectedPageReticle(documentState_.selection.pageIndex, hoveredReticleIndices.front()))
            {
                SelectPageReticle(documentState_.selection.pageIndex, hoveredReticleIndices.front());
            }

            layoutState_.pagePreviewContextReticleIndices = hoveredReticleIndices;
            layoutState_.pagePreviewContextTargets = hoveredClipTargets;
            ImGui::OpenPopup("PageReticleContextMenu");
            return;
        }
    }

    if (interactionState_.mode == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionState_.mode = InteractionMode::PanPage;
        layoutState_.suppressNextPagePreviewContextMenu = true;
        interactionState_.reticleIndex = -1;
        interactionState_.reticleIndices.clear();
        interactionState_.startReticleTransforms.clear();
    }

    if (interactionState_.mode != InteractionMode::None)
    {
        ApplyMouseTransform(interactiveViewport);
        const bool interactionButtonReleased =
            interactionState_.mode == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
        if (interactionButtonReleased)
        {
            cancelPreviewInteraction();
        }
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const bool additiveSelection = ImGui::GetIO().KeyCtrl;
    const std::optional<int> clickedReticleIndex = FindNearestPageReticle(viewport, mouse);
    const bool singleEditableSelection =
        (documentState_.selection.kind == SelectionKind::PageReticle && SelectedPageReticleCount() == 1) ||
        documentState_.selection.kind == SelectionKind::PageTitle ||
        documentState_.selection.kind == SelectionKind::PageStrobe;
    if (!additiveSelection && page != nullptr && singleEditableSelection)
    {
        const mfd::ReticleGroup* selectedPreviewReticle = nullptr;
        const mfd::Transform2D* selectedTransform = nullptr;
        if (documentState_.selection.kind == SelectionKind::PageTitle)
        {
            selectedPreviewReticle = &BuildPageTitlePreviewReticle(*page);
            if (const mfd::PageTitleDisplayDefinition* titleDisplay = SelectedPageTitleDisplay(); titleDisplay != nullptr)
            {
                selectedTransform = &titleDisplay->transform;
            }
        }
        else if (documentState_.selection.kind == SelectionKind::PageStrobe)
        {
            if (IsPageStrobeIndexVisibleInEditor(*page, documentState_.selection.pageReticleIndex))
            {
                if (const mfd::ReticleGroup* selectedReticle = SelectedPageStrobeReticle(); selectedReticle != nullptr)
                {
                    selectedPreviewReticle = selectedReticle;
                    selectedTransform = &selectedReticle->transform;
                }
            }
        }
        else if (mfd::ReticleGroup* selectedReticle = SelectedPageReticle(); selectedReticle != nullptr)
        {
            selectedPreviewReticle = selectedReticle;
            selectedTransform = &selectedReticle->transform;
        }

        if (selectedPreviewReticle != nullptr &&
            selectedTransform != nullptr &&
            (documentState_.selection.kind == SelectionKind::PageTitle || IsReticleVisibleInEditor(*page, *selectedPreviewReticle)) &&
            selectedPreviewReticle->visible)
        {
            const ReticleScreenBounds selectedBounds = ComputeReticleScreenBounds(*selectedPreviewReticle, viewport);
            if (selectedBounds.valid)
            {
                const std::array<ImVec2, 4> selectedCorners {
                    selectedBounds.min,
                    ImVec2(selectedBounds.max.x, selectedBounds.min.y),
                    selectedBounds.max,
                    ImVec2(selectedBounds.min.x, selectedBounds.max.y)};
                const ImVec2 selectedRotateHandle(
                    (selectedBounds.min.x + selectedBounds.max.x) * 0.5f,
                    selectedBounds.min.y - 26.0f);

                const auto initializeInteraction =
                    [this, &viewport, mouse, selectedTransform, selectedPreviewReticle](const InteractionMode mode,
                                                                                        const ImVec2 cornerScreen)
                {
                    interactionState_.reticleIndex =
                        documentState_.selection.kind == SelectionKind::PageReticle ? documentState_.selection.pageReticleIndex : -1;
                    interactionState_.reticleIndices.clear();
                    interactionState_.startReticleTransforms.clear();
                    if (documentState_.selection.kind == SelectionKind::PageReticle)
                    {
                        interactionState_.reticleIndices = {documentState_.selection.pageReticleIndex};
                        interactionState_.startReticleTransforms = {*selectedTransform};
                    }
                    interactionState_.startTransform = *selectedTransform;
                    interactionState_.startReticleVisualCenterLocal = ReticleVisualCenterLocal(*selectedPreviewReticle);
                    interactionState_.startMouseLogical = viewport.ToLogical(mouse);
                    const mfd::Vec2 interactionPivotLogical =
                        mfd::ApplyTransform(interactionState_.startReticleVisualCenterLocal, interactionState_.startTransform);
                    interactionState_.startAngleDegrees =
                        std::atan2(interactionState_.startMouseLogical.y - interactionPivotLogical.y,
                                   interactionState_.startMouseLogical.x - interactionPivotLogical.x) *
                        180.0f / 3.14159265f;
                    interactionState_.startDistance = std::max(
                        0.001f,
                        std::sqrt(
                            std::pow(interactionState_.startMouseLogical.x - interactionPivotLogical.x, 2.0f) +
                            std::pow(interactionState_.startMouseLogical.y - interactionPivotLogical.y, 2.0f)));
                    interactionState_.startCenterScreen = viewport.ToScreen(interactionPivotLogical);
                    interactionState_.startCornerScreen = cornerScreen;
                    PushUndoSnapshot();
                    interactionState_.mode = mode;
                };

                if (layoutState_.pagePreviewViewOptions.showGizmos)
                {
                    if (Distance(mouse, selectedRotateHandle) <= 16.0f)
                    {
                        initializeInteraction(InteractionMode::RotateReticle, selectedBounds.center);
                        return;
                    }

                    for (const ImVec2 corner : selectedCorners)
                    {
                        if (Distance(mouse, corner) <= 16.0f)
                        {
                            initializeInteraction(InteractionMode::ScaleReticle, corner);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (!additiveSelection &&
        SelectedPageReticleCount() > 1 &&
        clickedReticleIndex.has_value() &&
        HasSelectedPageReticle(documentState_.selection.pageIndex, *clickedReticleIndex))
    {
        // Preserve the current multi-selection when dragging one of its members.
        const std::vector<int> selectedIndices = SelectedPageReticleIndices();
        interactionState_.reticleIndex = documentState_.selection.pageReticleIndex;
        interactionState_.reticleIndices = selectedIndices;
        interactionState_.startReticleTransforms.clear();
        interactionState_.startReticleTransforms.reserve(selectedIndices.size());
        for (const int reticleIndex : selectedIndices)
        {
            interactionState_.startReticleTransforms.push_back(
                page->staticReticles[static_cast<std::size_t>(reticleIndex)].transform);
        }
        interactionState_.startMouseLogical = viewport.ToLogical(mouse);
        PushUndoSnapshot();
        interactionState_.mode = InteractionMode::MoveReticle;
        return;
    }

    UpdateReticleSelectionFromClick(viewport, additiveSelection);
    if (additiveSelection ||
        ((documentState_.selection.kind != SelectionKind::PageReticle || SelectedPageReticleCount() != 1) &&
         documentState_.selection.kind != SelectionKind::PageTitle &&
         documentState_.selection.kind != SelectionKind::PageStrobe))
    {
        return;
    }

    const mfd::ReticleGroup* selectedPreviewReticle = nullptr;
    const mfd::Transform2D* selectedTransform = nullptr;
    if (documentState_.selection.kind == SelectionKind::PageTitle)
    {
        selectedPreviewReticle = &BuildPageTitlePreviewReticle(*page);
        if (const mfd::PageTitleDisplayDefinition* titleDisplay = SelectedPageTitleDisplay(); titleDisplay != nullptr)
        {
            selectedTransform = &titleDisplay->transform;
        }
    }
    else if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        if (IsPageStrobeIndexVisibleInEditor(*page, documentState_.selection.pageReticleIndex))
        {
            if (const mfd::ReticleGroup* reticle = SelectedPageStrobeReticle(); reticle != nullptr)
            {
                selectedPreviewReticle = reticle;
                selectedTransform = &reticle->transform;
            }
        }
    }
    else if (mfd::ReticleGroup* reticle = SelectedPageReticle(); reticle != nullptr)
    {
        selectedPreviewReticle = reticle;
        selectedTransform = &reticle->transform;
    }

    if (selectedPreviewReticle == nullptr || selectedTransform == nullptr || !selectedPreviewReticle->visible)
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*selectedPreviewReticle, viewport);
    if (!bounds.valid)
    {
        return;
    }

    const ImVec2 center = bounds.center;
    const std::array<ImVec2, 4> corners {
        bounds.min,
        ImVec2(bounds.max.x, bounds.min.y),
        bounds.max,
        ImVec2(bounds.min.x, bounds.max.y)};
    const ImVec2 rotateHandle((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y - 26.0f);

    interactionState_.reticleIndex =
        documentState_.selection.kind == SelectionKind::PageReticle ? documentState_.selection.pageReticleIndex : -1;
    interactionState_.reticleIndices.clear();
    interactionState_.startReticleTransforms.clear();
    if (documentState_.selection.kind == SelectionKind::PageReticle)
    {
        interactionState_.reticleIndices = {documentState_.selection.pageReticleIndex};
        interactionState_.startReticleTransforms = {*selectedTransform};
    }
    interactionState_.startTransform = *selectedTransform;
    interactionState_.startReticleVisualCenterLocal = ReticleVisualCenterLocal(*selectedPreviewReticle);
    interactionState_.startMouseLogical = viewport.ToLogical(mouse);
    const mfd::Vec2 interactionPivotLogical =
        mfd::ApplyTransform(interactionState_.startReticleVisualCenterLocal, interactionState_.startTransform);
    interactionState_.startAngleDegrees =
        std::atan2(interactionState_.startMouseLogical.y - interactionPivotLogical.y,
                   interactionState_.startMouseLogical.x - interactionPivotLogical.x) *
        180.0f / 3.14159265f;
    interactionState_.startDistance = std::max(
        0.001f,
        std::sqrt(
            std::pow(interactionState_.startMouseLogical.x - interactionPivotLogical.x, 2.0f) +
            std::pow(interactionState_.startMouseLogical.y - interactionPivotLogical.y, 2.0f)));
    interactionState_.startCenterScreen = viewport.ToScreen(interactionPivotLogical);
    interactionState_.startCornerScreen = center;

    if (layoutState_.pagePreviewViewOptions.showGizmos && Distance(mouse, rotateHandle) <= 16.0f)
    {
        PushUndoSnapshot();
        interactionState_.mode = InteractionMode::RotateReticle;
    }
    else if (mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
             mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        if (layoutState_.pagePreviewViewOptions.showGizmos)
        {
            for (const ImVec2 corner : corners)
            {
                if (Distance(mouse, corner) <= 16.0f)
                {
                    PushUndoSnapshot();
                    interactionState_.mode = InteractionMode::ScaleReticle;
                    interactionState_.startCornerScreen = corner;
                    return;
                }
            }
        }

        PushUndoSnapshot();
        interactionState_.mode = InteractionMode::MoveReticle;
    }
    else
    {
        interactionState_.mode = InteractionMode::None;
        interactionState_.reticleIndex = -1;
        interactionState_.reticleIndices.clear();
        interactionState_.startReticleTransforms.clear();
    }
}

bool EditorApplication::ApplyPageReticleClipping(const int reticleIndex,
                                                 const mfd::ReticleClipMode mode,
                                                 std::string primitiveId)
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        reticleIndex < 0 ||
        reticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return false;
    }

    mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
    if (primitiveId.empty())
    {
        primitiveId = reticle.clipping.primitiveId;
    }

    if (reticle.clipping.mode == mode && reticle.clipping.primitiveId == primitiveId)
    {
        return false;
    }

    PushUndoSnapshot();
    reticle.clipping.mode = mode;
    reticle.clipping.primitiveId = std::move(primitiveId);
    layoutState_.pagePreviewContextReticleIndices = {reticleIndex};
    layoutState_.pagePreviewContextTargets.clear();
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
    {
        if (reticle.primitives[static_cast<std::size_t>(primitiveIndex)].id == reticle.clipping.primitiveId)
        {
            layoutState_.pagePreviewContextTargets.push_back(PageClipTarget {reticleIndex, primitiveIndex});
            break;
        }
    }
    SelectPageReticle(documentState_.selection.pageIndex, reticleIndex);

    if (mode == mfd::ReticleClipMode::None)
    {
        RebuildStatus("Clipping disabled for page reticle '" + reticle.id + "'.", false);
    }
    else
    {
        RebuildStatus(std::string(ReticleClipModeLabel(mode)) + " enabled on primitive '" + reticle.clipping.primitiveId +
                          "' for page reticle '" + reticle.id + "'.",
                      false);
    }

    return true;
}

void EditorApplication::DrawPageReticleContextMenu()
{
    if (!ImGui::BeginPopup("PageReticleContextMenu"))
    {
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr || layoutState_.pagePreviewContextReticleIndices.empty())
    {
        ImGui::TextDisabled("No page reticle is under the mouse.");
        ImGui::EndPopup();
        return;
    }

    auto drawClipItemsForTarget = [this, page](const int reticleIndex, const int primitiveIndex)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            ImGui::TextDisabled("Invalid reticle target.");
            return;
        }

        mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(reticle.primitives.size()))
        {
            ImGui::TextDisabled("Invalid primitive target.");
            return;
        }

        mfd::Primitive& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
        if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive))
        {
            ImGui::TextDisabled("The selected primitive does not support clipping.");
            ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
            return;
        }

        ImGui::TextDisabled("%s", (primitive.id + " (" + PrimitiveTypeLabel(primitive.type) + ")").c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Clip inside",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::Inner &&
                                reticle.clipping.primitiveId == primitive.id))
        {
            ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::Inner, primitive.id);
        }
        ShowItemTooltip("Erase the inside of this convex primitive toward the page background color.");

        if (ImGui::MenuItem("Clip outside",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::Outer &&
                                reticle.clipping.primitiveId == primitive.id))
        {
            const bool tutorialClipMatched = tutorial_->MatchesTarget("context_clip_outer");
            if (ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::Outer, primitive.id) &&
                tutorialClipMatched &&
                (tutorial_->TrackedReticleId().empty() || reticle.id == tutorial_->TrackedReticleId()))
            {
                tutorial_->CompleteStep();
            }
        }
        ShowItemTooltip("Erase everything outside this convex primitive toward the page background color.");
        tutorial_->DrawHalo(
            "context_clip_outer",
            "Click Clip outside",
            "Keep only the inside of the tutorial circle so you can discover page-level masking.");

        if (ImGui::MenuItem("Disable clipping",
                            nullptr,
                            reticle.clipping.mode == mfd::ReticleClipMode::None))
        {
            ApplyPageReticleClipping(reticleIndex, mfd::ReticleClipMode::None, reticle.clipping.primitiveId);
        }
        ShowItemTooltip("Disable clipping for this page reticle.");
    };

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    const bool hasSelectedGroup = !selectedIndices.empty();
    const bool canPasteSelection = !clipboardState_.pageReticleClipboard.empty();
    if (hasSelectedGroup || canPasteSelection)
    {
        if (hasSelectedGroup)
        {
            ImGui::TextDisabled("%d selected reticle%s",
                                static_cast<int>(selectedIndices.size()),
                                selectedIndices.size() == 1U ? "" : "s");
        }
        else
        {
            ImGui::TextDisabled("Reticle clipboard");
        }

        if (ImGui::MenuItem("Copy selection", "Ctrl+C", false, hasSelectedGroup))
        {
            CopySelectedPageReticles();
        }
        ShowItemTooltip("Copy the currently selected page reticle group.");

        if (ImGui::MenuItem("Cut selection", "Ctrl+X", false, hasSelectedGroup))
        {
            CutSelectedPageReticles();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Copy the current selection to the clipboard, then remove it from the page.");

        if (ImGui::MenuItem("Paste copies", "Ctrl+V", false, canPasteSelection))
        {
            PasteCopiedPageReticles();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Paste the current reticle clipboard onto the active page.");

        if (ImGui::MenuItem("Extract as reticle...", nullptr, false, hasSelectedGroup))
        {
            OpenReticleExtractionPopup();
        }
        ShowItemTooltip("Replace the current page-reticle selection with one reusable library template while preserving the visual result.");

        if (ImGui::MenuItem("Delete selection", "Del", false, hasSelectedGroup))
        {
            DeleteSelection();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ShowItemTooltip("Delete the selected page reticle group.");
        ImGui::Separator();
    }

    auto collectClipTargetsForReticle = [this](const int reticleIndex)
    {
        std::vector<PageClipTarget> targets;
        for (const PageClipTarget& target : layoutState_.pagePreviewContextTargets)
        {
            if (target.reticleIndex == reticleIndex)
            {
                targets.push_back(target);
            }
        }
        return targets;
    };

    auto drawReticleContextContent = [this, page, &drawClipItemsForTarget, &collectClipTargetsForReticle](const int reticleIndex)
    {
        if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            ImGui::TextDisabled("Invalid reticle target.");
            return;
        }

        mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        const bool selected = HasSelectedPageReticle(documentState_.selection.pageIndex, reticleIndex);
        if (ImGui::MenuItem("Select only", nullptr, selected))
        {
            SelectPageReticle(documentState_.selection.pageIndex, reticleIndex);
        }
        ShowItemTooltip("Focus only this reticle in the inspector and preview.");

        const char* toggleLabel = selected ? "Remove from selection" : "Add to selection";
        if (ImGui::MenuItem(toggleLabel, "Ctrl+click"))
        {
            TogglePageReticleSelection(documentState_.selection.pageIndex, reticleIndex);
        }
        ShowItemTooltip("Add or remove this reticle from the current multi-selection.");

        const std::vector<PageClipTarget> reticleTargets = collectClipTargetsForReticle(reticleIndex);
        ImGui::Separator();
        if (reticleTargets.empty())
        {
            ImGui::TextDisabled("No convex primitive under the mouse for clipping.");
            ImGui::TextDisabled("Supported mask shapes: triangle, square, rectangle, circle, ellipse.");
            return;
        }

        if (reticleTargets.size() == 1U)
        {
            drawClipItemsForTarget(reticleIndex, reticleTargets.front().primitiveIndex);
            return;
        }

        ImGui::TextDisabled("Clip through one of the hovered primitives:");
        for (const PageClipTarget& target : reticleTargets)
        {
            const mfd::Primitive& primitive =
                reticle.primitives[static_cast<std::size_t>(target.primitiveIndex)];
            const std::string primitiveLabel =
                (primitive.id.empty() ? std::string {"primitive"} : primitive.id) +
                " (" + PrimitiveTypeLabel(primitive.type) + ")##context_primitive_" +
                std::to_string(reticleIndex) + "_" + std::to_string(target.primitiveIndex);
            if (ImGui::BeginMenu(primitiveLabel.c_str()))
            {
                drawClipItemsForTarget(reticleIndex, target.primitiveIndex);
                ImGui::EndMenu();
            }
        }
    };

    if (layoutState_.pagePreviewContextReticleIndices.size() == 1U)
    {
        const int reticleIndex = layoutState_.pagePreviewContextReticleIndices.front();
        if (reticleIndex >= 0 && reticleIndex < static_cast<int>(page->staticReticles.size()))
        {
            const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
            ImGui::TextUnformatted(reticle.id.c_str());
            ImGui::Separator();
            drawReticleContextContent(reticleIndex);
        }
    }
    else
    {
        ImGui::TextDisabled("Reticles under the mouse");
        for (const int reticleIndex : layoutState_.pagePreviewContextReticleIndices)
        {
            if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page->staticReticles.size()))
            {
                continue;
            }

            const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
            std::string label =
                (reticle.id.empty() ? std::string {"reticle"} : reticle.id) +
                "##context_reticle_" + std::to_string(reticleIndex);
            if (ImGui::BeginMenu(label.c_str()))
            {
                drawReticleContextContent(reticleIndex);
                ImGui::EndMenu();
            }
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Right-click lists every hovered reticle, then lets you target clipping per hovered primitive.");

    ImGui::EndPopup();
}

void EditorApplication::HandleLibraryPreviewInteraction(const ViewportState& viewport)
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    auto cancelLibraryPreviewInteraction = [this]()
    {
        interactionState_.mode = InteractionMode::None;
        interactionState_.primitiveIndex = -1;
        interactionState_.handleKind = PrimitiveHandleKind::None;
        interactionState_.handleIndex = -1;
    };

    if (ImGui::IsPopupOpen((const char*)nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        layoutState_.suppressNextLibraryPreviewContextMenu = false;
        cancelLibraryPreviewInteraction();
        return;
    }

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 viewportMax(viewport.origin.x + viewport.size.x, viewport.origin.y + viewport.size.y);
    const std::optional<mfd::Vec2> mouseLogical =
        IsPointInsideRect(mouse, viewport.origin, viewportMax) ? std::optional<mfd::Vec2> {viewport.ToLogical(mouse)}
                                                               : std::nullopt;
    const ViewportToolbarLayout toolbarLayout =
        ComputeViewportToolbarLayout(viewport.origin, mfd::SanitizeZoom(layoutState_.libraryPreviewView.zoom), mouseLogical);
    const bool mouseInsideViewport = IsPointInsideRect(mouse, viewport.origin, viewportMax);
    const bool mouseInsideToolbar = IsPointInsideRect(mouse, toolbarLayout.toolbarMin, toolbarLayout.toolbarMax);
    if (interactionState_.mode == InteractionMode::PanPage)
    {
        if (!rightMouseDown)
        {
            if (!mouseInsideViewport || mouseInsideToolbar)
            {
                layoutState_.suppressNextLibraryPreviewContextMenu = false;
            }
            cancelLibraryPreviewInteraction();
        }
        else
        {
            ApplyMouseTransform(viewport);
        }
        return;
    }

    const bool hasActivePrimitiveInteraction =
        interactionState_.mode == InteractionMode::MovePrimitive || interactionState_.mode == InteractionMode::EditPrimitiveHandle;

    if (hasActivePrimitiveInteraction)
    {
        if (interactionState_.primitiveIndex < 0 || interactionState_.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
        {
            cancelLibraryPreviewInteraction();
            return;
        }

        mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(interactionState_.primitiveIndex)];
        const bool snapToGrid = layoutState_.pagePreviewViewOptions.snapToGrid;
        const float gridStep = SharedGridStepLogical(layoutState_.pagePreviewViewOptions);
        const mfd::Vec2 previewMouseLogical = viewport.ToLogical(ImGui::GetMousePos());

        if (interactionState_.mode == InteractionMode::MovePrimitive)
        {
            primitive.transform.position = editor::app::ResolvePrimitiveMovePosition(
                reticle->transform,
                interactionState_.startPrimitive,
                interactionState_.startMouseLogical,
                previewMouseLogical,
                snapToGrid,
                gridStep);
        }
        else if (interactionState_.mode == InteractionMode::EditPrimitiveHandle)
        {
            const mfd::Vec2 mousePrimitiveLocal = editor::app::ResolvePrimitiveHandleLocalPoint(
                *reticle,
                interactionState_.startPrimitive,
                previewMouseLogical,
                snapToGrid,
                gridStep);

            if (auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex == 0)
                {
                    line->start = mousePrimitiveLocal;
                }
                else if (interactionState_.handleIndex == 1)
                {
                    line->end = mousePrimitiveLocal;
                }
            }
            else if (auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
            {
                circle->radius = std::max(0.001f, std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x +
                                                            mousePrimitiveLocal.y * mousePrimitiveLocal.y));
            }
            else if (auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
            {
                const float radius = std::max(
                    0.001f,
                    std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x + mousePrimitiveLocal.y * mousePrimitiveLocal.y));
                if (interactionState_.handleIndex == 0)
                {
                    ring->innerRadius = std::min(radius, std::max(0.001f, ring->outerRadius - 0.001f));
                }
                else if (interactionState_.handleIndex == 1)
                {
                    ring->outerRadius = std::max(radius, ring->innerRadius + 0.001f);
                }
            }
            else if (auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
            {
                rectangle->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                rectangle->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
            }
            else if (auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex == 0)
                {
                    ellipse->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                }
                else
                {
                    ellipse->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
                }
            }
            else if (auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
            {
                square->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                square->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
            }
            else if (auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
            {
                if ((interactionState_.handleIndex % 2) == 0)
                {
                    diamond->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
                }
                else
                {
                    diamond->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                }
            }
            else if (auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex >= 0 && interactionState_.handleIndex < 3)
                {
                    triangle->points[static_cast<std::size_t>(interactionState_.handleIndex)] = mousePrimitiveLocal;
                }
            }
            else if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex >= 0 &&
                    interactionState_.handleIndex < static_cast<int>(polyline->points.size()))
                {
                    polyline->points[static_cast<std::size_t>(interactionState_.handleIndex)] = mousePrimitiveLocal;
                }
            }
            else if (auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex >= 0 &&
                    interactionState_.handleIndex < static_cast<int>(bezier->controlPoints.size()))
                {
                    bezier->controlPoints[static_cast<std::size_t>(interactionState_.handleIndex)] = mousePrimitiveLocal;
                }
            }
            else if (auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
            {
                if (interactionState_.handleIndex == 2)
                {
                    arc->radius = std::max(
                        0.001f,
                        std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x + mousePrimitiveLocal.y * mousePrimitiveLocal.y));
                }
                else
                {
                    const float angleDegrees = std::atan2(mousePrimitiveLocal.y, mousePrimitiveLocal.x) * 180.0f / PI;
                    if (interactionState_.handleIndex == 0)
                    {
                        arc->startAngleDegrees = angleDegrees;
                    }
                    else if (interactionState_.handleIndex == 1)
                    {
                        arc->endAngleDegrees = angleDegrees;
                    }
                }
            }
        }

        if (!leftMouseDown)
        {
            cancelLibraryPreviewInteraction();
        }
        return;
    }

    if (!mouseInsideViewport || mouseInsideToolbar)
    {
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionState_.mode == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinStudioZoom = 0.1f;
        constexpr float kMaxStudioZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom =
            std::clamp(mfd::SanitizeZoom(layoutState_.libraryPreviewView.zoom), kMinStudioZoom, kMaxStudioZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinStudioZoom, kMaxStudioZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const mfd::Vec2 mouseLogicalBeforeZoom = viewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - layoutState_.libraryPreviewView.center;
            const float zoomRatio = currentZoom / nextZoom;

            layoutState_.libraryPreviewView.zoom = nextZoom;
            layoutState_.libraryPreviewView.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
        }
    }

    if (interactionState_.mode == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionState_.mode = InteractionMode::PanPage;
        layoutState_.suppressNextLibraryPreviewContextMenu = true;
        interactionState_.primitiveIndex = -1;
        interactionState_.handleKind = PrimitiveHandleKind::None;
        interactionState_.handleIndex = -1;
        ApplyMouseTransform(viewport);
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    std::optional<int> bestPrimitiveIndex = FindNearestLibraryPrimitive(viewport, mouse);
    if (!bestPrimitiveIndex.has_value())
    {
        SelectLibraryReticle(reticle->id, false);
        return;
    }

    SelectLibraryPrimitive(reticle->id, *bestPrimitiveIndex);
    mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(*bestPrimitiveIndex)];

    auto toScreenPoint = [&viewport, reticle, &primitive](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(TransformPrimitiveWorldPoint(*reticle, primitive, localPoint));
    };

    interactionState_.primitiveIndex = *bestPrimitiveIndex;
    interactionState_.startPrimitive = primitive;
    interactionState_.startMouseLogical = viewport.ToLogical(mouse);
    interactionState_.startMouseReticleLocal = InverseTransformPoint(interactionState_.startMouseLogical, reticle->transform);
    interactionState_.startMousePrimitiveLocal =
        InversePrimitiveWorldPoint(*reticle, primitive, interactionState_.startMouseLogical);
    interactionState_.handleKind = PrimitiveHandleKind::None;
    interactionState_.handleIndex = -1;

    auto matchesHandle = [mouse](const ImVec2 handlePoint, const float radius = 12.0f)
    {
        return Distance(handlePoint, mouse) <= radius;
    };

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> points {line->start, line->end};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Point;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        if (matchesHandle(toScreenPoint({circle->radius, 0.0f})))
        {
            PushUndoSnapshot();
            interactionState_.mode = InteractionMode::EditPrimitiveHandle;
            interactionState_.handleKind = PrimitiveHandleKind::Radius;
            interactionState_.handleIndex = 0;
            return;
        }
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> handles {{
            {ring->innerRadius, 0.0f},
            {ring->outerRadius, 0.0f},
        }};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Radius;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> corners {{
            {-rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, -rectangle->height * 0.5f},
            {rectangle->width * 0.5f, rectangle->height * 0.5f},
            {-rectangle->width * 0.5f, rectangle->height * 0.5f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(corners[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::RectangleCorner;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 2> handles {{
            {ellipse->width * 0.5f, 0.0f},
            {0.0f, ellipse->height * 0.5f},
        }};
        for (int index = 0; index < 2; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Radius;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> corners {{
            {-square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, -square->height * 0.5f},
            {square->width * 0.5f, square->height * 0.5f},
            {-square->width * 0.5f, square->height * 0.5f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(corners[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::RectangleCorner;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        const std::array<mfd::Vec2, 4> handles {{
            {0.0f, diamond->height * 0.5f},
            {diamond->width * 0.5f, 0.0f},
            {0.0f, -diamond->height * 0.5f},
            {-diamond->width * 0.5f, 0.0f},
        }};
        for (int index = 0; index < 4; ++index)
        {
            if (matchesHandle(toScreenPoint(handles[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::DiamondAxis;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < 3; ++index)
        {
            if (matchesHandle(toScreenPoint(triangle->points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Point;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            if (matchesHandle(toScreenPoint(polyline->points[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Point;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (int index = 0; index < static_cast<int>(bezier->controlPoints.size()); ++index)
        {
            if (matchesHandle(toScreenPoint(bezier->controlPoints[static_cast<std::size_t>(index)])))
            {
                PushUndoSnapshot();
                interactionState_.mode = InteractionMode::EditPrimitiveHandle;
                interactionState_.handleKind = PrimitiveHandleKind::Point;
                interactionState_.handleIndex = index;
                return;
            }
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        const std::vector<mfd::Vec2> arcPoints =
            ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
        const float middleAngleDegrees = (arc->startAngleDegrees + arc->endAngleDegrees) * 0.5f;
        const float middleAngleRadians = middleAngleDegrees * PI / 180.0f;
        const mfd::Vec2 radiusHandle {
            std::cos(middleAngleRadians) * std::abs(arc->radius),
            std::sin(middleAngleRadians) * std::abs(arc->radius)};

        if (!arcPoints.empty() && matchesHandle(toScreenPoint(arcPoints.front())))
        {
            PushUndoSnapshot();
            interactionState_.mode = InteractionMode::EditPrimitiveHandle;
            interactionState_.handleKind = PrimitiveHandleKind::Point;
            interactionState_.handleIndex = 0;
            return;
        }

        if (!arcPoints.empty() && matchesHandle(toScreenPoint(arcPoints.back())))
        {
            PushUndoSnapshot();
            interactionState_.mode = InteractionMode::EditPrimitiveHandle;
            interactionState_.handleKind = PrimitiveHandleKind::Point;
            interactionState_.handleIndex = 1;
            return;
        }

        if (matchesHandle(toScreenPoint(radiusHandle)))
        {
            PushUndoSnapshot();
            interactionState_.mode = InteractionMode::EditPrimitiveHandle;
            interactionState_.handleKind = PrimitiveHandleKind::Radius;
            interactionState_.handleIndex = 2;
            return;
        }
    }

    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
    if (bounds.valid &&
        mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
        mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        PushUndoSnapshot();
        interactionState_.mode = InteractionMode::MovePrimitive;
        return;
    }

    interactionState_.primitiveIndex = -1;
}

void EditorApplication::DrawReticleHoverPreviewTooltip(const mfd::ReticleGroup& reticle,
                                                       const std::string_view label,
                                                       const Color backgroundColor)
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
    {
        return;
    }

    constexpr int kTooltipPreviewWidth = 220;
    constexpr int kTooltipPreviewHeight = 180;

    EnsureTooltipPreviewTexture(kTooltipPreviewWidth, kTooltipPreviewHeight);
    if (!previewResources_.TooltipTextureReady())
    {
        return;
    }

    mfd::ReticleGroup previewReticle = reticle;
    previewReticle.visible = true;
    const mfd::PageViewState previewView =
        MakeViewFittingBounds(ComputeReticleWorldBounds(previewReticle), kTooltipPreviewWidth, kTooltipPreviewHeight);

    BeginTextureMode(previewResources_.TooltipTexture());
    ClearBackground(backgroundColor);
    {
        EnsurePreviewFont();
        editor::ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(kTooltipPreviewWidth,
                             kTooltipPreviewHeight,
                             previewView,
                             PreviewTextFont(),
                             backgroundColor,
                             previewResources_.TooltipTextureStencilReady(),
                             &previewResources_.BezierCache(),
                             &previewResources_.ImageCache(),
                             &previewResources_.TextLayoutCache());
        canvas.DrawReticle(previewReticle);
    }
    EndTextureMode();

    ImGui::BeginTooltip();
    if (!label.empty())
    {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::Separator();
    }
    ImGui::Image(
        (ImTextureID)(uintptr_t)previewResources_.TooltipTexture().texture.id,
        ImVec2(static_cast<float>(kTooltipPreviewWidth), static_cast<float>(kTooltipPreviewHeight)),
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));
    ImGui::TextDisabled("Hover preview");
    ImGui::EndTooltip();
}

void EditorApplication::ApplyMouseTransform(const ViewportState& viewport)
{
    mfd::PageDefinition* page = ActivePage();
    if (interactionState_.mode == InteractionMode::PanPage)
    {
        const float scale = viewport.LogicalScale();
        if (!viewport.valid || scale <= 0.0f)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }

        mfd::PageViewState* targetView = nullptr;
        if (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive)
        {
            targetView = &layoutState_.libraryPreviewView;
        }
        else
        {
            targetView = &layoutState_.pagePreviewView;
        }

        const float zoom = mfd::SanitizeZoom(targetView->zoom);
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        targetView->center.x -= mouseDelta.x / (scale * zoom);
        targetView->center.y += mouseDelta.y / (scale * zoom);
        return;
    }

    mfd::ReticleGroup* reticle = nullptr;
    mfd::Transform2D* reticleTransform = nullptr;
    if (documentState_.selection.kind == SelectionKind::PageTitle)
    {
        mfd::PageTitleDisplayDefinition* titleDisplay = SelectedPageTitleDisplay();
        if (titleDisplay == nullptr)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndex = -1;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }

        reticleTransform = &titleDisplay->transform;
    }
    else if (documentState_.selection.kind == SelectionKind::PageStrobe)
    {
        if (page == nullptr)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndex = -1;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }

        reticle = SelectedPageStrobeReticle();
        if (reticle == nullptr)
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndex = -1;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }
        reticleTransform = &reticle->transform;
    }
    else
    {
        if (interactionState_.reticleIndex < 0)
        {
            return;
        }

        if (page == nullptr || interactionState_.reticleIndex >= static_cast<int>(page->staticReticles.size()))
        {
            interactionState_.mode = InteractionMode::None;
            interactionState_.reticleIndex = -1;
            interactionState_.reticleIndices.clear();
            interactionState_.startReticleTransforms.clear();
            return;
        }

        reticle = &page->staticReticles[static_cast<std::size_t>(interactionState_.reticleIndex)];
        reticleTransform = &reticle->transform;
    }

    const mfd::Vec2 mouseLogical = viewport.ToLogical(ImGui::GetMousePos());

    switch (interactionState_.mode)
    {
    case InteractionMode::PanPage:
        break;

    case InteractionMode::MoveReticle:
    {
        const bool snapToGrid = layoutState_.pagePreviewViewOptions.snapToGrid;
        const float gridStep = SharedGridStepLogical(layoutState_.pagePreviewViewOptions);
        const mfd::Vec2 translationDelta = mouseLogical - interactionState_.startMouseLogical;
        if (!interactionState_.reticleIndices.empty() &&
            interactionState_.reticleIndices.size() == interactionState_.startReticleTransforms.size())
        {
            // Snap the shared delta so a multi-selection keeps its relative layout while moving in
            // grid increments.
            const mfd::Vec2 appliedDelta =
                snapToGrid ? editor::app::SnapToGrid(translationDelta, gridStep) : translationDelta;
            for (std::size_t index = 0; index < interactionState_.reticleIndices.size(); ++index)
            {
                const int movedReticleIndex = interactionState_.reticleIndices[index];
                if (movedReticleIndex < 0 || movedReticleIndex >= static_cast<int>(page->staticReticles.size()))
                {
                    continue;
                }

                mfd::Transform2D nextTransform = interactionState_.startReticleTransforms[index];
                nextTransform.position = nextTransform.position + appliedDelta;
                page->staticReticles[static_cast<std::size_t>(movedReticleIndex)].transform = nextTransform;
            }
        }
        else
        {
            mfd::Vec2 nextPosition = interactionState_.startTransform.position + translationDelta;
            if (snapToGrid)
            {
                nextPosition = editor::app::SnapToGrid(nextPosition, gridStep);
            }
            reticleTransform->position = nextPosition;
        }
        break;
    }

    case InteractionMode::RotateReticle:
    {
        const mfd::Vec2 interactionPivotLogical =
            mfd::ApplyTransform(interactionState_.startReticleVisualCenterLocal, interactionState_.startTransform);
        const float currentAngle =
            std::atan2(mouseLogical.y - interactionPivotLogical.y,
                       mouseLogical.x - interactionPivotLogical.x) *
            180.0f / 3.14159265f;
        const float nextRotationDegrees =
            interactionState_.startTransform.rotationDegrees + (currentAngle - interactionState_.startAngleDegrees);
        *reticleTransform = BuildTransformKeepingLocalPointWorldPosition(
            interactionState_.startTransform,
            interactionState_.startReticleVisualCenterLocal,
            nextRotationDegrees,
            interactionState_.startTransform.scale);
        break;
    }

    case InteractionMode::ScaleReticle:
    {
        const ImVec2 mouseScreen = ImGui::GetMousePos();
        const float startDistance = std::max(8.0f, Distance(interactionState_.startCornerScreen, interactionState_.startCenterScreen));
        const float currentDistance = std::max(4.0f, Distance(mouseScreen, interactionState_.startCenterScreen));
        const float factor = std::clamp(currentDistance / startDistance, 0.1f, 10.0f);
        const mfd::Vec2 nextScale {
            std::max(0.05f, interactionState_.startTransform.scale.x * factor),
            std::max(0.05f, interactionState_.startTransform.scale.y * factor)};
        *reticleTransform = BuildTransformKeepingLocalPointWorldPosition(
            interactionState_.startTransform,
            interactionState_.startReticleVisualCenterLocal,
            interactionState_.startTransform.rotationDegrees,
            nextScale);
        break;
    }

    case InteractionMode::None:
        break;
    }
}

bool EditorApplication::CreatePageReticleInstanceFromTemplate(const std::string_view templateId, const mfd::Vec2 position)
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select a page before dropping a library reticle.", true);
        return false;
    }

    const auto iterator = documentState_.loaded.document.reticleLibrary.find(std::string(templateId));
    if (iterator == documentState_.loaded.document.reticleLibrary.end())
    {
        RebuildStatus("Unknown library reticle: " + std::string(templateId), true);
        return false;
    }

    PushUndoSnapshot();

    const std::string instanceId = MakeUniquePageReticleId(*page, templateId);
    mfd::ReticleGroup instance = mfd::InstantiateReticle(
        iterator->second,
        instanceId,
        mfd::Transform2D {position, 0.0f, {1.0f, 1.0f}},
        {});
    instance.visible = true;
    if (page->name == "Page1" && templateId == kTutorialAircraftTemplateId)
    {
        // Keep the tutorial ownship reference above the circle mask and tracks.
        instance.drawOnTop = true;
    }
    instance.layerId = ActiveInsertionLayerId(*page);

    const LogicalBounds localBounds = ComputeReticleLocalBounds(instance);
    if (localBounds.valid)
    {
        const float width = std::max(0.001f, localBounds.max.x - localBounds.min.x);
        const float height = std::max(0.001f, localBounds.max.y - localBounds.min.y);
        const float maxDimension = std::max(width * std::abs(instance.transform.scale.x),
                                            height * std::abs(instance.transform.scale.y));
        if (maxDimension < 0.12f)
        {
            const float scaleFactor = std::clamp(0.12f / std::max(0.001f, maxDimension), 1.0f, 6.0f);
            instance.transform.scale.x = std::max(0.05f, instance.transform.scale.x * scaleFactor);
            instance.transform.scale.y = std::max(0.05f, instance.transform.scale.y * scaleFactor);
        }

        instance.transform.position =
            position - mfd::Rotate(mfd::Scale(localBounds.center, instance.transform.scale), instance.transform.rotationDegrees);
    }

    page->staticReticles.push_back(std::move(instance));
    SelectPageReticle(documentState_.selection.pageIndex, static_cast<int>(page->staticReticles.size()) - 1);
    RebuildStatus("Reticle '" + std::string(templateId) + "' dropped on page '" + page->name + "'.", false);
    return true;
}
