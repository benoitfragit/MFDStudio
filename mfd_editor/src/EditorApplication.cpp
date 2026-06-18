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
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::DefaultPageIndex;
using editor::detail::kPrimitiveTypes;
using editor::detail::kTutorialAircraftTemplateId;
using editor::detail::kTutorialStrobeCursorTemplateId;
using editor::detail::ReticleHasFillCapablePrimitive;
using editor::detail::SeedPrimitiveFillColorIfNeeded;
using editor::detail::SeedReticleFillOverrideIfNeeded;
using editor::detail::SuggestReplacementPageIndex;
using editor::detail::ToColorRgba;
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

char LowerAscii(const char value) noexcept
{
    return (value >= 'A' && value <= 'Z') ? static_cast<char>(value - 'A' + 'a') : value;
}

mfd::Vec2 ApplyNudgeToPosition(mfd::Vec2 position, const mfd::Vec2 delta, const bool snap, const float gridStep)
{
    position.x = std::clamp(position.x + delta.x, -1.0f, 1.0f);
    position.y = std::clamp(position.y + delta.y, -1.0f, 1.0f);
    return snap ? editor::app::SnapToGrid(position, gridStep) : position;
}

float SharedGridStepLogical(const editor::PagePreviewViewOptions& viewOptions) noexcept
{
    return editor::app::SanitizeGridStepLogical(viewOptions.gridStepLogical);
}

mfd::Vec2 SnapToSharedGridIfEnabled(const mfd::Vec2 value, const editor::PagePreviewViewOptions& viewOptions) noexcept
{
    return viewOptions.snapToGrid ? editor::app::SnapToGrid(value, SharedGridStepLogical(viewOptions)) : value;
}

// Draws one shell side-panel visibility checkbox. The checkbox reflects the effective
// visibility, so when the responsive layout auto-collapses a wanted panel on a narrow
// window the entry is shown unchecked and disabled instead of falsely claiming a hidden
// panel is visible. Returns true when the user toggled the visibility preference.
bool DrawShellPanelVisibilityToggle(const char* label,
                                    bool& visiblePreference,
                                    const bool autoCollapsed,
                                    const char* tooltip)
{
    bool effectiveVisible = visiblePreference && !autoCollapsed;
    ImGui::BeginDisabled(autoCollapsed);
    const bool changed = ImGui::Checkbox(label, &effectiveVisible);
    ImGui::EndDisabled();
    if (changed)
    {
        visiblePreference = effectiveVisible;
    }
    ShowItemTooltip(autoCollapsed
                        ? "This panel is temporarily auto-hidden because the window is narrow. Widen the window to bring it back."
                        : tooltip);
    return changed;
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
constexpr const char* kRecoveryFileName = "assets/.editor_recovery.json";

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

void ApplyPointFilterToFont(const Font font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    }
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

bool HasSamePageTitleDisplay(const mfd::PageTitleDisplayDefinition& lhs,
                             const mfd::PageTitleDisplayDefinition& rhs) noexcept
{
    return lhs.visible == rhs.visible &&
           lhs.transform.position.x == rhs.transform.position.x &&
           lhs.transform.position.y == rhs.transform.position.y &&
           lhs.transform.rotationDegrees == rhs.transform.rotationDegrees &&
           lhs.transform.scale.x == rhs.transform.scale.x &&
           lhs.transform.scale.y == rhs.transform.scale.y &&
           lhs.color.r == rhs.color.r &&
           lhs.color.g == rhs.color.g &&
           lhs.color.b == rhs.color.b &&
           lhs.color.a == rhs.color.a &&
           lhs.lineWidth == rhs.lineWidth &&
           lhs.lineStyle == rhs.lineStyle &&
           lhs.decoration == rhs.decoration;
}

bool IsPointInsidePolygon(const std::vector<ImVec2>& polygon, const ImVec2 point) noexcept
{
    if (polygon.size() < 3)
    {
        return false;
    }

    bool inside = false;
    std::size_t previous = polygon.size() - 1U;
    for (std::size_t current = 0; current < polygon.size(); ++current)
    {
        const ImVec2& a = polygon[current];
        const ImVec2& b = polygon[previous];

        const bool intersects =
            ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 0.00001f) + a.x);
        if (intersects)
        {
            inside = !inside;
        }

        previous = current;
    }

    return inside;
}

std::vector<mfd::Vec2> ApproximateEllipsePoints(const float width, const float height, const int segments = 48)
{
    const int segmentCount = std::max(12, segments);
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount));

    for (int index = 0; index < segmentCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segmentCount) * 2.0f * PI;
        points.push_back({std::cos(angle) * halfWidth, std::sin(angle) * halfHeight});
    }

    return points;
}

std::vector<mfd::Vec2> ApproximateArcPoints(const float radius,
                                            const float startAngleDegrees,
                                            const float endAngleDegrees,
                                            const int segments = 48)
{
    const int segmentCount = std::max(2, segments);
    const float safeRadius = std::max(0.0f, std::abs(radius));
    const float startRadians = startAngleDegrees * PI / 180.0f;
    const float sweepRadians = (endAngleDegrees - startAngleDegrees) * PI / 180.0f;
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount) + 1U);

    for (int index = 0; index <= segmentCount; ++index)
    {
        const float factor = static_cast<float>(index) / static_cast<float>(segmentCount);
        const float angle = startRadians + sweepRadians * factor;
        points.push_back({std::cos(angle) * safeRadius, std::sin(angle) * safeRadius});
    }

    return points;
}

template <typename TCallback>
void ForEachApproximateArcPoint(const float radius,
                                const float startAngleDegrees,
                                const float endAngleDegrees,
                                const int segments,
                                TCallback&& callback)
{
    editor::detail::ForEachPrimitiveArcPoint(radius, startAngleDegrees, endAngleDegrees, segments, callback);
}

mfd::Vec2 InverseTransformPoint(const mfd::Vec2& point, const mfd::Transform2D& transform)
{
    const mfd::Vec2 translated = point - transform.position;
    const mfd::Vec2 rotated = mfd::Rotate(translated, -transform.rotationDegrees);

    return {
        std::abs(transform.scale.x) <= 0.0001f ? 0.0f : rotated.x / transform.scale.x,
        std::abs(transform.scale.y) <= 0.0001f ? 0.0f : rotated.y / transform.scale.y};
}

mfd::Vec2 TransformPrimitiveWorldPoint(const mfd::ReticleGroup& reticle,
                                       const mfd::Primitive& primitive,
                                       const mfd::Vec2 localPoint)
{
    return mfd::ApplyPrimitiveWorldTransform(localPoint, primitive, reticle);
}

mfd::Vec2 InversePrimitiveWorldPoint(const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     const mfd::Vec2 worldPoint)
{
    return InverseTransformPoint(worldPoint, mfd::ResolvePrimitiveWorldTransform(primitive, reticle));
}

float Distance(const ImVec2 lhs, const ImVec2 rhs)
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

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

struct PageClipPrimitiveHit
{
    int reticleIndex = -1;
    int primitiveIndex = -1;
    float primitiveDistance = std::numeric_limits<float>::max();
    float reticleDistance = std::numeric_limits<float>::max();
};

constexpr float kPreviewTextMeasurementPixelsPerLogicalUnit = 1024.0f;

float SanitizePreviewLetterSpacing(const float letterSpacing) noexcept
{
    return std::isfinite(letterSpacing) ? letterSpacing : mfd::kDefaultTextLetterSpacing;
}

mfd::Vec2 FallbackPreviewTextSizeLogical(const mfd::TextGeometry& geometry) noexcept
{
    return {
        editor::detail::EstimatedPrimitiveTextWidth(editor::detail::EstimatePrimitiveTextHalfWidth(geometry)),
        editor::detail::EstimatePrimitiveTextHalfHeight(geometry) * 2.0f};
}

mfd::Vec2 FallbackPreviewTextSizeLogical(const mfd::TimeGeometry& geometry) noexcept
{
    return {
        editor::detail::EstimatedPrimitiveTextWidth(editor::detail::EstimatePrimitiveTextHalfWidth(geometry)),
        editor::detail::EstimatePrimitiveTextHalfHeight(geometry) * 2.0f};
}

Font ResolvePreviewMeasurementFont(const Font* const previewFont) noexcept
{
    return previewFont != nullptr ? *previewFont : GetFontDefault();
}

float EditorViewportZoom(const editor::app::ViewportState& viewport) noexcept
{
    return mfd::SanitizeZoom(viewport.view.zoom);
}

float ToEditorViewPixels(const editor::app::ViewportState& viewport, const float logicalValue) noexcept
{
    return logicalValue * viewport.LogicalScale() * EditorViewportZoom(viewport);
}

Vector2 RotateScreenOffset(const Vector2 offset, const float rotationDegrees) noexcept
{
    const float radians = rotationDegrees * PI / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return Vector2 {
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine};
}

void IncludeScreenPoint(editor::app::ReticleScreenBounds& bounds, const ImVec2 point) noexcept
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
    {
        return;
    }

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

class PrimitiveScreenBoundsAccumulator
{
public:
    PrimitiveScreenBoundsAccumulator(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive) noexcept
        : bounds_(bounds)
        , viewport_(viewport)
        , reticle_(reticle)
        , primitive_(primitive)
    {
    }

    void operator()(const mfd::Vec2 localPoint) const
    {
        IncludeScreenPoint(bounds_, viewport_.ToScreen(TransformPrimitiveWorldPoint(reticle_, primitive_, localPoint)));
    }

private:
    editor::app::ReticleScreenBounds& bounds_;
    const editor::app::ViewportState& viewport_;
    const mfd::ReticleGroup& reticle_;
    const mfd::Primitive& primitive_;
};

void IncludeAlignedTextLogicalBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     const float width,
                                     const float halfHeight,
                                     const mfd::Align align)
{
    const float originX = editor::detail::PrimitiveTextOriginX(width, align);
    const float left = -originX;
    const float right = width - originX;

    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {left, -halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {right, -halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {right, halfHeight})));
    IncludeScreenPoint(bounds, viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {left, halfHeight})));
}

bool IncludeTextLayoutScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                   const editor::app::ViewportState& viewport,
                                   const mfd::ReticleGroup& reticle,
                                   const mfd::Primitive& primitive,
                                   const mfd::CachedTextLayout& layout)
{
    const mfd::Transform2D combinedTransform = mfd::ResolvePrimitiveWorldTransform(primitive, reticle);
    if (!std::isfinite(layout.size.x) || !std::isfinite(layout.size.y) ||
        !std::isfinite(layout.origin.x) || !std::isfinite(layout.origin.y) ||
        !std::isfinite(combinedTransform.rotationDegrees) ||
        layout.size.x <= 0.0f || layout.size.y <= 0.0f)
    {
        return false;
    }

    const float left = -layout.origin.x;
    const float right = layout.size.x - layout.origin.x;
    const float top = -layout.origin.y;
    const float bottom = layout.size.y - layout.origin.y;
    const ImVec2 anchor = viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, {}));
    const float screenRotationDegrees = -combinedTransform.rotationDegrees;

    const std::array<Vector2, 4> offsets {{
        {left, top},
        {right, top},
        {right, bottom},
        {left, bottom}}};
    for (const Vector2 offset : offsets)
    {
        const Vector2 rotatedOffset = RotateScreenOffset(offset, screenRotationDegrees);
        IncludeScreenPoint(bounds, ImVec2(anchor.x + rotatedOffset.x, anchor.y + rotatedOffset.y));
    }

    return true;
}

bool IncludeMeasuredTextScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     mfd::TextLayoutCache& textLayoutCache,
                                     const mfd::TextGeometry& text,
                                     const Font* const previewFont)
{
    const Font font = ResolvePreviewMeasurementFont(previewFont);
    const float textScale = mfd::PrimitiveAverageScale(primitive, reticle);
    const float fontSizePixels = std::max(1.0f, std::abs(ToEditorViewPixels(viewport, text.fontSize * textScale)));
    const float letterSpacingPixels = std::isfinite(text.letterSpacing)
                                          ? ToEditorViewPixels(viewport, text.letterSpacing * textScale)
                                          : ToEditorViewPixels(viewport, mfd::kDefaultTextLetterSpacing * textScale);
    const mfd::CachedTextLayout& layout =
        textLayoutCache.ResolveStaticText(text.text, font, fontSizePixels, letterSpacingPixels, text.align);

    return IncludeTextLayoutScreenBounds(bounds, viewport, reticle, primitive, layout);
}

bool IncludeMeasuredTimeScreenBounds(editor::app::ReticleScreenBounds& bounds,
                                     const editor::app::ViewportState& viewport,
                                     const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     mfd::TextLayoutCache& textLayoutCache,
                                     const mfd::TimeGeometry& time,
                                     const Font* const previewFont)
{
    const Font font = ResolvePreviewMeasurementFont(previewFont);
    const float textScale = mfd::PrimitiveAverageScale(primitive, reticle);
    const float fontSizePixels = std::max(1.0f, std::abs(ToEditorViewPixels(viewport, time.fontSize * textScale)));
    const float letterSpacingPixels = std::isfinite(time.letterSpacing)
                                          ? ToEditorViewPixels(viewport, time.letterSpacing * textScale)
                                          : ToEditorViewPixels(viewport, mfd::kDefaultTextLetterSpacing * textScale);
    const mfd::CachedTextLayout& layout = textLayoutCache.ResolveTimeText(time, font, fontSizePixels, letterSpacingPixels);

    return IncludeTextLayoutScreenBounds(bounds, viewport, reticle, primitive, layout);
}

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
    if (uiState.sidebarVisible.has_value())
    {
        layoutState_.sidebarVisible = *uiState.sidebarVisible;
    }
    if (uiState.inspectorWidth.has_value())
    {
        layoutState_.inspectorWidth = *uiState.inspectorWidth;
    }
    if (uiState.inspectorVisible.has_value())
    {
        layoutState_.inspectorVisible = *uiState.inspectorVisible;
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
    uiState.sidebarVisible = layoutState_.sidebarVisible;
    uiState.inspectorWidth = layoutState_.inspectorWidth;
    uiState.inspectorVisible = layoutState_.inspectorVisible;
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
    // Keep only a sane technical floor: the responsive shell layout degrades panels
    // progressively, so the full three-column arrangement is no longer required to
    // allow the window to shrink.
    SetWindowMinSize(720, 480);
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

bool EditorApplication::LoadWindowConfiguration(const std::filesystem::path& path)
{
    try
    {
        documentState_.loaded = documentState_.loader.LoadWindowConfiguration(path);
        for (auto& page : documentState_.loaded.document.pages)
        {
            BootstrapEditorLayersForPage(page);
        }
        documentState_.files.pageFiles = documentState_.loaded.window.pageFiles;
        documentState_.files.removedPageFiles.clear();
        documentState_.files.removedTemplateFiles.clear();
        std::string error;
        if (!editor::DiscoverReticleTemplateFiles(documentState_.loaded.window.reticleLibraryFolder, documentState_.files, &error))
        {
            throw std::runtime_error(error);
        }

        ApplyPreviewFontFile(documentState_.loaded.window.fontFile);
        documentState_.selection = {};
        layoutState_.layerFocusState = {};
        SelectPage(DefaultPageIndex(documentState_.loaded.document.pages));
        ResetLibraryPreviewView();
        documentState_.history.Clear();
        InvalidateReticleUsageHighlightCache();
        documentState_.windowFile = path;
        workflowState_.documentDirty = false;
        autosave_.Reset();
        RearmAssetWatcher();
        workflowState_.lastRuntimeError.clear();
        RebuildStatus("Editor loaded '" + documentState_.loaded.window.title + "'.", false);
        return true;
    }
    catch (const std::exception& exception)
    {
        RebuildStatus(exception.what(), true);
        return false;
    }
}

bool EditorApplication::SaveAll()
{
    if (!HasOpenWindow())
    {
        RebuildStatus("No window asset is open yet. Create or open one before saving.", true);
        return false;
    }

    std::string error;
    if (!editor::SaveEditorDocument(documentState_.loaded, documentState_.files, &error))
    {
        RebuildStatus("Save failed: " + error, true);
        return false;
    }

    workflowState_.documentDirty = false;
    autosave_.Reset();
    RearmAssetWatcher();
    ClearRecoverySnapshot();
    RebuildStatus("Window, pages and library saved.", false);
    return true;
}

void EditorApplication::WriteRecoverySnapshot()
{
    if (!HasOpenWindow())
    {
        return;
    }

    try
    {
        const std::string bundle =
            editor::SerializeRecoveryBundleToJsonString(documentState_.loaded, documentState_.files);
        const std::filesystem::path path = documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName);
        if (path.has_parent_path())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(path.parent_path(), directoryError);
        }

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (stream.is_open())
        {
            stream << bundle;
        }
    }
    catch (const std::exception&)
    {
        // Best-effort recovery snapshot: never interrupt authoring if it cannot be written.
    }
}

void EditorApplication::ClearRecoverySnapshot()
{
    std::error_code removeError;
    std::filesystem::remove(documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName), removeError);
}

void EditorApplication::RecoverPreviousSession()
{
    const std::filesystem::path path = documentState_.assetPaths.DefaultAssetPath(kRecoveryFileName);
    std::string bundle;
    try
    {
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        bundle = buffer.str();
    }
    catch (const std::exception&)
    {
        bundle.clear();
    }

    // The recovery file lives at the asset root, so its parent bounds every legitimate authored file.
    const std::filesystem::path assetRoot = path.parent_path();

    std::filesystem::path windowFile;
    std::string error;
    if (!bundle.empty() && editor::RestoreRecoveryBundle(bundle, assetRoot, windowFile, &error) &&
        LoadWindowConfiguration(windowFile))
    {
        RebuildStatus("Recovered unsaved work from the previous session.", false);
        // Only discard the snapshot once the restore and reload both succeeded.
        ClearRecoverySnapshot();
    }
    else if (!error.empty())
    {
        RebuildStatus("Recovery failed (snapshot kept): " + error, true);
    }
    else
    {
        RebuildStatus("Recovery snapshot could not be restored (snapshot kept).", true);
    }
}

std::vector<std::filesystem::path> EditorApplication::CollectWatchedAssetFiles() const
{
    std::vector<std::filesystem::path> files;
    if (!HasOpenWindow())
    {
        return files;
    }

    files.reserve(1U + documentState_.files.pageFiles.size() + documentState_.files.templateFiles.size());
    files.push_back(documentState_.loaded.window.sourceFile);
    for (const std::filesystem::path& pageFile : documentState_.files.pageFiles)
    {
        files.push_back(pageFile);
    }
    for (const auto& entry : documentState_.files.templateFiles)
    {
        files.push_back(entry.second);
    }

    return files;
}

void EditorApplication::RearmAssetWatcher()
{
    assetWatcher_.Watch(CollectWatchedAssetFiles());
    assetWatchAccumulatorSeconds_ = 0.0f;
}

void EditorApplication::Undo()
{
    std::optional<UndoSnapshot> previous = documentState_.history.Undo(CaptureCurrentSnapshot());
    if (!previous.has_value())
    {
        RebuildStatus("Nothing to undo.", true);
        return;
    }

    RestoreSnapshot(std::move(*previous));
    RebuildStatus("Undo applied.", false);
}

void EditorApplication::Redo()
{
    std::optional<UndoSnapshot> next = documentState_.history.Redo(CaptureCurrentSnapshot());
    if (!next.has_value())
    {
        RebuildStatus("Nothing to redo.", true);
        return;
    }

    RestoreSnapshot(std::move(*next));
    RebuildStatus("Redo applied.", false);
}

EditorApplication::UndoSnapshot EditorApplication::CaptureCurrentSnapshot() const
{
    return UndoSnapshot {
        documentState_.loaded,
        documentState_.files,
        documentState_.selection,
        layoutState_.pagePreviewView,
        layoutState_.libraryPreviewView};
}

void EditorApplication::RestoreSnapshot(UndoSnapshot snapshot)
{
    documentState_.loaded = std::move(snapshot.loaded);
    documentState_.files = std::move(snapshot.files);
    documentState_.selection = std::move(snapshot.selection);
    layoutState_.pagePreviewView = snapshot.pagePreviewView;
    layoutState_.pagePreviewView.zoom = mfd::SanitizeZoom(layoutState_.pagePreviewView.zoom);
    layoutState_.libraryPreviewView = snapshot.libraryPreviewView;
    layoutState_.libraryPreviewView.zoom = mfd::SanitizeZoom(layoutState_.libraryPreviewView.zoom);

    if (documentState_.selection.pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        SelectPage(documentState_.loaded.document.pages.empty() ? 0 : static_cast<int>(documentState_.loaded.document.pages.size()) - 1);
    }

    SanitizeLayerFocusForActivePage();
    SanitizePageReticleSelectionForCurrentFocus();
    InvalidateReticleUsageHighlightCache();
    workflowState_.documentDirty = true;
}

void EditorApplication::PushUndoSnapshot()
{
    PushUndoSnapshot(CaptureCurrentSnapshot());
}

void EditorApplication::PushUndoSnapshot(UndoSnapshot snapshot)
{
    documentState_.history.Record(std::move(snapshot));
    workflowState_.documentDirty = true;
    InvalidateReticleUsageHighlightCache();
}

void EditorApplication::NudgeSelection(const mfd::Vec2 delta)
{
    if (delta.x == 0.0f && delta.y == 0.0f)
    {
        return;
    }

    const bool snap = layoutState_.pagePreviewViewOptions.snapToGrid;
    const float gridStep = SharedGridStepLogical(layoutState_.pagePreviewViewOptions);

    if (IsPageTitleSelected())
    {
        if (mfd::PageTitleDisplayDefinition* title = SelectedPageTitleDisplay(); title != nullptr)
        {
            PushUndoSnapshot();
            title->transform.position = ApplyNudgeToPosition(title->transform.position, delta, snap, gridStep);
        }
        return;
    }

    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedReticles = SelectedPageReticleIndices();
    if (page != nullptr && !selectedReticles.empty())
    {
        PushUndoSnapshot();
        for (const int index : selectedReticles)
        {
            if (index >= 0 && index < static_cast<int>(page->staticReticles.size()))
            {
                mfd::Vec2& position = page->staticReticles[static_cast<std::size_t>(index)].transform.position;
                position = ApplyNudgeToPosition(position, delta, snap, gridStep);
            }
        }
        return;
    }

    if (mfd::ReticleGroup* editable = SelectedEditablePageReticle(); editable != nullptr)
    {
        PushUndoSnapshot();
        editable->transform.position = ApplyNudgeToPosition(editable->transform.position, delta, snap, gridStep);
    }
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
                DrawReticleHoverPreviewTooltip(titleReticle, "Page title", ToRayColor(page.backgroundColor));
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
                    ToRayColor(page.backgroundColor));
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
                    ToRayColor(page.backgroundColor));
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

void EditorApplication::RebuildStatus(std::string message, const bool isError)
{
    workflowState_.statusMessage = std::move(message);
    workflowState_.statusIsError = isError;
}

void EditorApplication::EnsurePreviewTexture(const int width, const int height)
{
    if (previewState_.previewTextureReady &&
        previewState_.previewTexture.texture.width == width &&
        previewState_.previewTexture.texture.height == height)
    {
        return;
    }

    ReleasePreviewTexture();
    previewState_.previewTextureStencilReady = false;
    previewState_.previewTexture = mfd::LoadRenderTextureWithStencil(width, height, &previewState_.previewTextureStencilReady);
    previewState_.previewTextureReady = previewState_.previewTexture.texture.id != 0;
}

void EditorApplication::ReleasePreviewTexture()
{
    const bool windowReady = IsWindowReady();
    if (previewState_.previewTextureReady)
    {
        if (windowReady)
        {
            UnloadRenderTexture(previewState_.previewTexture);
        }
        previewState_.previewTexture = {};
    }

    previewState_.previewTextureReady = false;
    previewState_.previewTextureStencilReady = false;
}

void EditorApplication::EnsureTooltipPreviewTexture(const int width, const int height)
{
    if (previewState_.tooltipPreviewTextureReady &&
        previewState_.tooltipPreviewTexture.texture.width == width &&
        previewState_.tooltipPreviewTexture.texture.height == height)
    {
        return;
    }

    ReleaseTooltipPreviewTexture();
    previewState_.tooltipPreviewTextureStencilReady = false;
    previewState_.tooltipPreviewTexture =
        mfd::LoadRenderTextureWithStencil(width, height, &previewState_.tooltipPreviewTextureStencilReady);
    previewState_.tooltipPreviewTextureReady = previewState_.tooltipPreviewTexture.texture.id != 0;
}

void EditorApplication::ReleaseTooltipPreviewTexture()
{
    const bool windowReady = IsWindowReady();
    if (previewState_.tooltipPreviewTextureReady)
    {
        if (windowReady)
        {
            UnloadRenderTexture(previewState_.tooltipPreviewTexture);
        }
        previewState_.tooltipPreviewTexture = {};
    }

    previewState_.tooltipPreviewTextureReady = false;
    previewState_.tooltipPreviewTextureStencilReady = false;
}

void EditorApplication::ReleaseLayerPreviewTextures() noexcept
{
    const bool windowReady = IsWindowReady();
    for (LayerPreviewTextureSlot& slot : previewState_.layerPreviewTextures)
    {
        if (slot.ready)
        {
            if (windowReady)
            {
                UnloadRenderTexture(slot.texture);
            }
            slot.texture = {};
        }

        slot.ready = false;
        slot.stencilReady = false;
        slot.width = 0;
        slot.height = 0;
    }

    previewState_.layerPreviewTextures.clear();
}

void EditorApplication::ReleasePreviewGpuResources() noexcept
{
    previewState_.previewBezierCache.Clear();
    previewState_.previewImageCache.Clear();
    previewState_.previewTextLayoutCache.Clear();
    ReleaseLayerPreviewTextures();
    ReleaseTooltipPreviewTexture();
    ReleasePreviewTexture();
    ReleasePreviewFont();
}

const RenderTexture2D* EditorApplication::RenderLayerPreviewThumbnail(const std::size_t thumbnailIndex,
                                                                      const mfd::PageDefinition& page,
                                                                      const editor::LayerFocusStripEntry& entry,
                                                                      int width,
                                                                      int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (thumbnailIndex >= previewState_.layerPreviewTextures.size())
    {
        previewState_.layerPreviewTextures.resize(thumbnailIndex + 1U);
    }

    LayerPreviewTextureSlot& slot = previewState_.layerPreviewTextures[thumbnailIndex];
    if (!slot.ready || slot.width != width || slot.height != height)
    {
        if (slot.ready)
        {
            UnloadRenderTexture(slot.texture);
            slot.texture = {};
            slot.ready = false;
        }

        slot.stencilReady = false;
        slot.texture = mfd::LoadRenderTextureWithStencil(width, height, &slot.stencilReady);
        slot.ready = slot.texture.texture.id != 0;
        slot.width = width;
        slot.height = height;
    }

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
        ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(width,
                             height,
                             previewView,
                             PreviewTextFont(),
                             background,
                             slot.stencilReady,
                             &previewState_.previewBezierCache,
                             &previewState_.previewImageCache,
                             &previewState_.previewTextLayoutCache);

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
    if (!fontFile.empty())
    {
        fontFile = fontFile.lexically_normal();
    }

    if (previewState_.previewFontFile == fontFile)
    {
        return;
    }

    previewState_.previewTextLayoutCache.Clear();
    ReleasePreviewFont();
    previewState_.previewFontFile = std::move(fontFile);
    previewState_.previewFontLoadAttempted = false;
}

void EditorApplication::EnsurePreviewFont()
{
    if (previewState_.previewFontReady || previewState_.previewFontLoadAttempted || previewState_.previewFontFile.empty() || !IsWindowReady())
    {
        return;
    }

    previewState_.previewFontLoadAttempted = true;
    const std::string path = previewState_.previewFontFile.string();
    Font loadedFont = LoadFont(path.c_str());
    if (loadedFont.texture.id == 0)
    {
        return;
    }

    ApplyPointFilterToFont(loadedFont);
    previewState_.previewFont = loadedFont;
    previewState_.previewFontReady = true;
}

void EditorApplication::ReleasePreviewFont() noexcept
{
    const bool windowReady = IsWindowReady();
    if (previewState_.previewFontReady)
    {
        if (windowReady)
        {
            UnloadFont(previewState_.previewFont);
        }
        previewState_.previewFont = {};
        previewState_.previewFontReady = false;
    }
}

const Font* EditorApplication::PreviewTextFont() const noexcept
{
    return previewState_.previewFontReady ? &previewState_.previewFont : nullptr;
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
    const mfd::CachedTextLayout& layout = previewState_.previewTextLayoutCache.ResolveStaticText(
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
    const mfd::CachedTextLayout& layout = previewState_.previewTextLayoutCache.ResolveTimeText(
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

void EditorApplication::DrawPagePreview(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    EnsurePreviewTexture(static_cast<int>(viewport.size.x), static_cast<int>(viewport.size.y));
    if (!previewState_.previewTextureReady)
    {
        return;
    }

    BeginTextureMode(previewState_.previewTexture);
    ClearBackground(ToRayColor(page->backgroundColor));
    {
        if (layoutState_.pagePreviewViewOptions.showGrid)
        {
            editor::app::DrawViewportGrid(
                MakeViewportGridInput(viewport, layoutState_.pagePreviewViewOptions),
                ToRayColor(page->backgroundColor));
        }

        EnsurePreviewFont();
        ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewState_.previewTexture.texture.width,
            previewState_.previewTexture.texture.height,
            viewport.view,
            PreviewTextFont(),
            ToRayColor(page->backgroundColor),
            previewState_.previewTextureStencilReady,
            &previewState_.previewBezierCache,
            &previewState_.previewImageCache,
            &previewState_.previewTextLayoutCache);
        services_.pagePreviewDrawOrder.CollectStaticReticleDrawOrder(*page, previewState_.orderedStaticReticleIndices);
        for (const int reticleIndex : previewState_.orderedStaticReticleIndices)
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
        (ImTextureID)(uintptr_t)previewState_.previewTexture.texture.id,
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
    if (!previewState_.previewTextureReady)
    {
        return;
    }

    BeginTextureMode(previewState_.previewTexture);
    ClearBackground(Color {10, 18, 24, 255});
    {
        if (layoutState_.pagePreviewViewOptions.showGrid)
        {
            editor::app::DrawViewportGrid(
                MakeViewportGridInput(viewport, layoutState_.pagePreviewViewOptions),
                Color {10, 18, 24, 255});
        }

        EnsurePreviewFont();
        ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(
            previewState_.previewTexture.texture.width,
            previewState_.previewTexture.texture.height,
            viewport.view,
            PreviewTextFont(),
            Color {10, 18, 24, 255},
            previewState_.previewTextureStencilReady,
            &previewState_.previewBezierCache,
            &previewState_.previewImageCache,
            &previewState_.previewTextLayoutCache);
        canvas.DrawReticle(*reticle);
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewState_.previewTexture.texture.id,
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
        // Shell side panels belong to the normal editor layout only. They are offered here,
        // next to the fullscreen button, so the menu bar no longer needs a separate View menu.
        // Fullscreen hides the shell, so the toggles are skipped while it is active.
        const bool showShellPanelToggles = allowFullscreenToggle && !services_.fullscreenPreview.IsActive();
        if (showShellPanelToggles)
        {
            ImGui::TextDisabled("Panels");
            if (DrawShellPanelVisibilityToggle("Sidebar",
                                               layoutState_.sidebarVisible,
                                               layoutState_.sidebarAutoCollapsed,
                                               "Show or hide the left page and reticle selector sidebar.") &&
                tutorial_->MatchesTarget("page_preview_view_sidebar"))
            {
                tutorial_->CompleteStep();
            }
            tutorial_->DrawHalo(
                "page_preview_view_sidebar",
                "Toggle Sidebar",
                "Hide the left page and reticle selector to focus on the page preview. Re-open this menu to show it again.");

            DrawShellPanelVisibilityToggle("Inspector",
                                           layoutState_.inspectorVisible,
                                           layoutState_.inspectorAutoCollapsed,
                                           "Show or hide the right inspector panel.");
            ImGui::Separator();
        }

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

    if (previewState_.layerPreviewTextures.size() > model.entries.size())
    {
        for (std::size_t slotIndex = model.entries.size(); slotIndex < previewState_.layerPreviewTextures.size(); ++slotIndex)
        {
            if (previewState_.layerPreviewTextures[slotIndex].ready)
            {
                UnloadRenderTexture(previewState_.layerPreviewTextures[slotIndex].texture);
            }
        }
        previewState_.layerPreviewTextures.resize(model.entries.size());
    }
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
            NormalizeEditorIdentifier(page.normalizedName.empty() ? page.name : page.normalizedName));
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
            if (NormalizeEditorIdentifier(entry.first) == reticleSegment)
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
    if (!previewState_.tooltipPreviewTextureReady)
    {
        return;
    }

    mfd::ReticleGroup previewReticle = reticle;
    previewReticle.visible = true;
    const mfd::PageViewState previewView =
        MakeViewFittingBounds(ComputeReticleWorldBounds(previewReticle), kTooltipPreviewWidth, kTooltipPreviewHeight);

    BeginTextureMode(previewState_.tooltipPreviewTexture);
    ClearBackground(backgroundColor);
    {
        EnsurePreviewFont();
        ApplyPointFilterToFont(PreviewTextFont() == nullptr ? GetFontDefault() : *PreviewTextFont());
        mfd::Canvas2D canvas(kTooltipPreviewWidth,
                             kTooltipPreviewHeight,
                             previewView,
                             PreviewTextFont(),
                             backgroundColor,
                             previewState_.tooltipPreviewTextureStencilReady,
                             &previewState_.previewBezierCache,
                             &previewState_.previewImageCache,
                             &previewState_.previewTextLayoutCache);
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
        (ImTextureID)(uintptr_t)previewState_.tooltipPreviewTexture.texture.id,
        ImVec2(static_cast<float>(kTooltipPreviewWidth), static_cast<float>(kTooltipPreviewHeight)),
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));
    ImGui::TextDisabled("Hover preview");
    ImGui::EndTooltip();
}

EditorApplication::ReticleScreenBounds EditorApplication::ComputePrimitiveScreenBounds(
    const mfd::ReticleGroup& reticle,
    const mfd::Primitive& primitive,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;
    if (!primitive.style.visible)
    {
        return bounds;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        mfd::TextLayoutCache textLayoutCache;
        if (!IncludeMeasuredTextScreenBounds(
                bounds,
                viewport,
                reticle,
                primitive,
                textLayoutCache,
                *text,
                PreviewTextFont()))
        {
            const mfd::Vec2 size = FallbackPreviewTextSizeLogical(*text);
            IncludeAlignedTextLogicalBounds(bounds, viewport, reticle, primitive, size.x, size.y * 0.5f, text->align);
        }
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        mfd::TextLayoutCache textLayoutCache;
        if (!IncludeMeasuredTimeScreenBounds(
                bounds,
                viewport,
                reticle,
                primitive,
                textLayoutCache,
                *time,
                PreviewTextFont()))
        {
            const mfd::Vec2 size = FallbackPreviewTextSizeLogical(*time);
            IncludeAlignedTextLogicalBounds(bounds, viewport, reticle, primitive, size.x, size.y * 0.5f, time->align);
        }
    }
    else
    {
        const PrimitiveScreenBoundsAccumulator accumulator(bounds, viewport, reticle, primitive);
        editor::detail::ForEachPrimitiveBoundsLocalPoint(primitive, accumulator);
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

EditorApplication::ReticleScreenBounds EditorApplication::ComputeReticleScreenBounds(
    const mfd::ReticleGroup& reticle,
    const ViewportState& viewport) const
{
    ReticleScreenBounds bounds;

    for (const auto& primitive : reticle.primitives)
    {
        const ReticleScreenBounds primitiveBounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
        if (!primitiveBounds.valid)
        {
            continue;
        }

        if (!bounds.valid)
        {
            bounds = primitiveBounds;
            continue;
        }

        bounds.min.x = std::min(bounds.min.x, primitiveBounds.min.x);
        bounds.min.y = std::min(bounds.min.y, primitiveBounds.min.y);
        bounds.max.x = std::max(bounds.max.x, primitiveBounds.max.x);
        bounds.max.y = std::max(bounds.max.y, primitiveBounds.max.y);
    }

    if (bounds.valid)
    {
        bounds.center = ImVec2((bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f);
    }

    return bounds;
}

float EditorApplication::PrimitiveHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                    const mfd::Primitive& primitive,
                                                    const ViewportState& viewport,
                                                    const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    const auto distanceToPoint = [mousePosition](const ImVec2 point)
    {
        const float dx = point.x - mousePosition.x;
        const float dy = point.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    auto distanceToSegment = [mousePosition](const ImVec2 a, const ImVec2 b)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float apx = mousePosition.x - a.x;
        const float apy = mousePosition.y - a.y;
        const float lengthSquared = abx * abx + aby * aby;
        if (lengthSquared <= 0.0001f)
        {
            const float dx = a.x - mousePosition.x;
            const float dy = a.y - mousePosition.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        const float factor = std::clamp((apx * abx + apy * aby) / lengthSquared, 0.0f, 1.0f);
        const ImVec2 projection(a.x + abx * factor, a.y + aby * factor);
        const float dx = projection.x - mousePosition.x;
        const float dy = projection.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    float bestDistance = distanceToPoint(bounds.center);

    auto toScreenPoint = [&viewport, &reticle, &primitive](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(TransformPrimitiveWorldPoint(reticle, primitive, localPoint));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        if (mousePosition.x >= bounds.min.x - 6.0f && mousePosition.x <= bounds.max.x + 6.0f &&
            mousePosition.y >= bounds.min.y - 6.0f && mousePosition.y <= bounds.max.y + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        if (mousePosition.x >= bounds.min.x - 6.0f && mousePosition.x <= bounds.max.x + 6.0f &&
            mousePosition.y >= bounds.min.y - 6.0f && mousePosition.y <= bounds.max.y + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        return std::min(bestDistance, distanceToSegment(toScreenPoint(line->start), toScreenPoint(line->end)));
    }

    if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        const std::array<ImVec2, 4> imageCorners {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
        std::vector<ImVec2> polygon(imageCorners.begin(), imageCorners.end());
        if (IsPointInsidePolygon(polygon, mousePosition))
        {
            return 1.0f;
        }

        for (std::size_t index = 0; index < imageCorners.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1U) % imageCorners.size();
            bestDistance = std::min(bestDistance, distanceToSegment(imageCorners[index], imageCorners[nextIndex]));
        }

        return bestDistance;
    }

    if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 edge = toScreenPoint({circle->radius, 0.0f});
        const float radiusPixels = std::max(1.0f, Distance(center, edge));
        const float centerDistance = distanceToPoint(center);
        bestDistance = std::min(bestDistance, std::abs(centerDistance - radiusPixels));
        if (centerDistance <= radiusPixels + 6.0f)
        {
            bestDistance = std::min(bestDistance, 3.0f);
        }
        return bestDistance;
    }

    if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        const ImVec2 center = toScreenPoint({});
        const ImVec2 innerEdge = toScreenPoint({ring->innerRadius, 0.0f});
        const ImVec2 outerEdge = toScreenPoint({ring->outerRadius, 0.0f});
        float innerRadiusPixels = Distance(center, innerEdge);
        float outerRadiusPixels = std::max(1.0f, Distance(center, outerEdge));
        if (innerRadiusPixels > outerRadiusPixels)
        {
            std::swap(innerRadiusPixels, outerRadiusPixels);
        }

        const float centerDistance = distanceToPoint(center);
        float ringDistance = 0.0f;
        if (centerDistance < innerRadiusPixels)
        {
            ringDistance = innerRadiusPixels - centerDistance;
        }
        else if (centerDistance > outerRadiusPixels)
        {
            ringDistance = centerDistance - outerRadiusPixels;
        }
        else
        {
            ringDistance = std::min(centerDistance - innerRadiusPixels, outerRadiusPixels - centerDistance);
        }

        if (centerDistance >= innerRadiusPixels - 6.0f && centerDistance <= outerRadiusPixels + 6.0f)
        {
            ringDistance = std::min(ringDistance, 2.0f);
        }

        return ringDistance;
    }

    if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        const std::vector<mfd::Vec2> logicalArcPoints =
            ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments);
        std::vector<ImVec2> arcPoints;
        arcPoints.reserve(logicalArcPoints.size());
        for (const auto& point : logicalArcPoints)
        {
            arcPoints.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1U < arcPoints.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(arcPoints[index], arcPoints[index + 1U]));
        }

        if (primitive.style.filled && arcPoints.size() >= 2U)
        {
            std::vector<ImVec2> sectorPoints;
            sectorPoints.reserve(arcPoints.size() + 1U);
            sectorPoints.push_back(toScreenPoint({}));
            sectorPoints.insert(sectorPoints.end(), arcPoints.begin(), arcPoints.end());
            for (std::size_t index = 0; index + 1U < sectorPoints.size(); ++index)
            {
                bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints[index], sectorPoints[index + 1U]));
            }
            bestDistance = std::min(bestDistance, distanceToSegment(sectorPoints.back(), sectorPoints.front()));
            if (IsPointInsidePolygon(sectorPoints, mousePosition))
            {
                bestDistance = std::min(bestDistance, 2.0f);
            }
        }

        return bestDistance;
    }

    std::vector<ImVec2> points;

    if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f}),
            toScreenPoint({rectangle->width * 0.5f, rectangle->height * 0.5f}),
            toScreenPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f})};
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        for (const auto& point : ApproximateEllipsePoints(ellipse->width, ellipse->height))
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, -square->height * 0.5f}),
            toScreenPoint({square->width * 0.5f, square->height * 0.5f}),
            toScreenPoint({-square->width * 0.5f, square->height * 0.5f})};
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({0.0f, diamond->height * 0.5f}),
            toScreenPoint({diamond->width * 0.5f, 0.0f}),
            toScreenPoint({0.0f, -diamond->height * 0.5f}),
            toScreenPoint({-diamond->width * 0.5f, 0.0f})};
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint(triangle->points[0]),
            toScreenPoint(triangle->points[1]),
            toScreenPoint(triangle->points[2])};
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            points.push_back(toScreenPoint(point));
        }

        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }

        if (polyline->closed && points.size() > 2)
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        }

        return bestDistance;
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            points.push_back(toScreenPoint(point));
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        points = {
            toScreenPoint({-image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, -image->height * 0.5f}),
            toScreenPoint({image->width * 0.5f, image->height * 0.5f}),
            toScreenPoint({-image->width * 0.5f, image->height * 0.5f})};
    }

    for (std::size_t index = 0; index < points.size(); ++index)
    {
        bestDistance = std::min(bestDistance, distanceToPoint(points[index]));
        if (index + 1 < points.size())
        {
            bestDistance = std::min(bestDistance, distanceToSegment(points[index], points[index + 1]));
        }
    }

    if (points.size() > 2)
    {
        bestDistance = std::min(bestDistance, distanceToSegment(points.back(), points.front()));
        if (IsPointInsidePolygon(points, mousePosition))
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
    }

    return bestDistance;
}

float EditorApplication::ReticleHitDistancePixels(const mfd::ReticleGroup& reticle,
                                                  const ViewportState& viewport,
                                                  const ImVec2 mousePosition) const
{
    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
    if (!bounds.valid)
    {
        return std::numeric_limits<float>::max();
    }

    float bestDistance = Distance(bounds.center, mousePosition);
    for (const auto& primitive : reticle.primitives)
    {
        bestDistance = std::min(bestDistance, PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition));
    }

    return bestDistance;
}

std::optional<EditorApplication::PageReticleHit> EditorApplication::BuildPageReticleHit(
    const mfd::PageDefinition& page,
    const ViewportState& viewport,
    const ImVec2 mousePosition,
    const editor::PagePreviewHitTarget target,
    const mfd::ReticleGroup& reticle,
    const editor::PagePreviewDrawOrderKey drawOrder) const
{
    const bool isPageTitle = target.kind == editor::PagePreviewHitKind::PageTitle;
    const bool isPageStrobe = target.kind == editor::PagePreviewHitKind::PageStrobe;
    if ((!isPageTitle && !IsReticleVisibleInEditor(page, reticle)) ||
        (!isPageTitle && !isPageStrobe && !IsPageReticleSelectableInCurrentFocus(page, reticle)) ||
        (isPageTitle && !reticle.visible))
    {
        return std::nullopt;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
    if (!bounds.valid)
    {
        return std::nullopt;
    }

    float distance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
    const bool mouseInsideBounds =
        mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
        mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
    const bool directHit = distance <= 6.0f;
    if (mouseInsideBounds)
    {
        distance = std::min(distance, 4.0f);
    }

    if (!mouseInsideBounds && !directHit && distance > 36.0f)
    {
        return std::nullopt;
    }

    const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
    return PageReticleHit {target, distance, area, directHit, mouseInsideBounds, drawOrder};
}

bool EditorApplication::PreferPageReticleHit(const PageReticleHit& lhs, const PageReticleHit& rhs) noexcept
{
    if (lhs.drawOrder < rhs.drawOrder)
    {
        return false;
    }

    if (rhs.drawOrder < lhs.drawOrder)
    {
        return true;
    }
    if (lhs.directHit != rhs.directHit)
    {
        return lhs.directHit && !rhs.directHit;
    }
    if (lhs.boundsHit != rhs.boundsHit)
    {
        return lhs.boundsHit && !rhs.boundsHit;
    }
    if (std::abs(lhs.distance - rhs.distance) > 0.25f)
    {
        return lhs.distance < rhs.distance;
    }
    if (std::abs(lhs.area - rhs.area) > 0.5f)
    {
        return lhs.area < rhs.area;
    }

    if (lhs.target.kind != rhs.target.kind)
    {
        return static_cast<int>(lhs.target.kind) > static_cast<int>(rhs.target.kind);
    }

    return lhs.target.index > rhs.target.index;
}

const mfd::ReticleGroup& EditorApplication::BuildPageTitlePreviewReticle(const mfd::PageDefinition& page) const
{
    if (!previewState_.pageTitlePreviewReticleCache.valid ||
        previewState_.pageTitlePreviewReticleCache.pageName != page.name ||
        previewState_.pageTitlePreviewReticleCache.pageTitle != page.title ||
        !HasSamePageTitleDisplay(previewState_.pageTitlePreviewReticleCache.display, page.titleDisplay))
    {
        previewState_.pageTitlePreviewReticleCache.pageName = page.name;
        previewState_.pageTitlePreviewReticleCache.pageTitle = page.title;
        previewState_.pageTitlePreviewReticleCache.display = page.titleDisplay;
        previewState_.pageTitlePreviewReticleCache.reticle =
            mfd::BuildPageTitleDisplayReticle(page.name, page.title, page.titleDisplay);
        previewState_.pageTitlePreviewReticleCache.valid = true;
    }

    return previewState_.pageTitlePreviewReticleCache.reticle;
}

void EditorApplication::UpdateReticleSelectionFromClick(const ViewportState& viewport, const bool additiveSelection)
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    const ImVec2 mousePosition = ImGui::GetMousePos();
    std::optional<PageReticleHit> bestHit;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        std::optional<PageReticleHit> candidate = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::StaticReticle(reticleIndex),
            reticle,
            services_.pagePreviewDrawOrder.BuildStaticReticleDrawOrderKey(*page, reticleIndex));
        if (candidate.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*candidate, *bestHit)))
        {
            bestHit = std::move(candidate);
        }
    }

    for (int strobeIndex = 0; strobeIndex < static_cast<int>(page->strobes.size()); ++strobeIndex)
    {
        if (!IsPageStrobeIndexVisibleInEditor(*page, strobeIndex))
        {
            continue;
        }

        std::optional<PageReticleHit> candidate = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::PageStrobe(strobeIndex),
            page->strobes[static_cast<std::size_t>(strobeIndex)].reticle,
            services_.pagePreviewDrawOrder.BuildStrobeDrawOrderKey(static_cast<std::size_t>(strobeIndex)));
        if (candidate.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*candidate, *bestHit)))
        {
            bestHit = std::move(candidate);
        }
    }

    const mfd::ReticleGroup& titleReticle = BuildPageTitlePreviewReticle(*page);
    if (titleReticle.visible)
    {
        std::optional<PageReticleHit> titleHit = BuildPageReticleHit(
            *page,
            viewport,
            mousePosition,
            editor::PagePreviewHitTarget::PageTitle(),
            titleReticle,
            services_.pagePreviewDrawOrder.BuildTitleDrawOrderKey());
        if (titleHit.has_value() && (!bestHit.has_value() || PreferPageReticleHit(*titleHit, *bestHit)))
        {
            bestHit = std::move(titleHit);
        }
    }

    if (!bestHit.has_value())
    {
        if (!additiveSelection)
        {
            SelectPage(documentState_.selection.pageIndex, false);
        }
        return;
    }

    if (bestHit->target.kind == editor::PagePreviewHitKind::PageTitle)
    {
        SelectPageTitle(documentState_.selection.pageIndex);
    }
    else if (bestHit->target.kind == editor::PagePreviewHitKind::PageStrobe)
    {
        SelectPageStrobe(documentState_.selection.pageIndex, bestHit->target.index);
    }
    else
    {
        if (additiveSelection)
        {
            TogglePageReticleSelection(documentState_.selection.pageIndex, bestHit->target.index);
        }
        else
        {
            SelectPageReticle(documentState_.selection.pageIndex, bestHit->target.index);
        }
    }
}

std::vector<int> EditorApplication::CollectPageReticlesAt(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageReticleHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle) || !IsPageReticleSelectableInCurrentFocus(*page, reticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(reticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        const bool mouseInsideBounds =
            mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
        const bool directHit = distance <= 6.0f;
        if (mouseInsideBounds)
        {
            distance = std::min(distance, 4.0f);
        }

        if (!mouseInsideBounds && !directHit && distance > 36.0f)
        {
            continue;
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        hits.push_back(PageReticleHit {
            editor::PagePreviewHitTarget::StaticReticle(reticleIndex),
            distance,
            area,
            directHit,
            mouseInsideBounds,
            services_.pagePreviewDrawOrder.BuildStaticReticleDrawOrderKey(*page, reticleIndex)});
    }

    std::sort(hits.begin(),
              hits.end(),
              PreferPageReticleHit);

    std::vector<int> reticleIndices;
    reticleIndices.reserve(hits.size());
    for (const PageReticleHit& hit : hits)
    {
        reticleIndices.push_back(hit.target.index);
    }
    return reticleIndices;
}

std::optional<int> EditorApplication::FindNearestPageReticle(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const std::vector<int> hits = CollectPageReticlesAt(viewport, mousePosition);
    return hits.empty() ? std::nullopt : std::optional<int> {hits.front()};
}

std::vector<EditorApplication::PageClipTarget> EditorApplication::CollectPageClipTargetsAt(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    std::vector<PageClipPrimitiveHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle))
        {
            continue;
        }

        const float reticleDistance = ReticleHitDistancePixels(reticle, viewport, mousePosition);
        for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
        {
            const auto& primitive = reticle.primitives[static_cast<std::size_t>(primitiveIndex)];
            if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive) || !primitive.style.visible)
            {
                continue;
            }

            const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(reticle, primitive, viewport);
            if (!bounds.valid)
            {
                continue;
            }

            float primitiveDistance = PrimitiveHitDistancePixels(reticle, primitive, viewport, mousePosition);
            const bool mouseInsideBounds =
                mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
                mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f;
            if (mouseInsideBounds)
            {
                primitiveDistance = std::min(primitiveDistance, 3.0f);
            }

            if (!mouseInsideBounds && primitiveDistance > 10.0f)
            {
                continue;
            }

            hits.push_back(PageClipPrimitiveHit {
                reticleIndex,
                primitiveIndex,
                primitiveDistance,
                reticleDistance});
        }
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const PageClipPrimitiveHit& lhs, const PageClipPrimitiveHit& rhs)
              {
                  if (std::abs(lhs.primitiveDistance - rhs.primitiveDistance) > 0.25f)
                  {
                      return lhs.primitiveDistance < rhs.primitiveDistance;
                  }
                  if (std::abs(lhs.reticleDistance - rhs.reticleDistance) > 0.25f)
                  {
                      return lhs.reticleDistance < rhs.reticleDistance;
                  }
                  if (lhs.reticleIndex != rhs.reticleIndex)
                  {
                      return lhs.reticleIndex > rhs.reticleIndex;
                  }
                  return lhs.primitiveIndex > rhs.primitiveIndex;
              });

    std::vector<PageClipTarget> targets;
    targets.reserve(hits.size());
    for (const PageClipPrimitiveHit& hit : hits)
    {
        targets.push_back(PageClipTarget {hit.reticleIndex, hit.primitiveIndex});
    }
    return targets;
}

std::optional<EditorApplication::PageClipTarget> EditorApplication::FindNearestPageClipPrimitive(
    const ViewportState& viewport,
    const ImVec2 mousePosition) const
{
    const std::vector<PageClipTarget> targets = CollectPageClipTargetsAt(viewport, mousePosition);
    return targets.empty() ? std::nullopt : std::optional<PageClipTarget> {targets.front()};
}

std::optional<int> EditorApplication::FindNearestLibraryPrimitive(const ViewportState& viewport,
                                                                  const ImVec2 mousePosition) const
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return std::nullopt;
    }

    float bestDistance = 32.0f;
    float bestArea = std::numeric_limits<float>::max();
    std::optional<int> bestIndex;

    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle->primitives.size()); ++primitiveIndex)
    {
        const mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(primitiveIndex)];
        const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        float distance = PrimitiveHitDistancePixels(*reticle, primitive, viewport, mousePosition);
        if (mousePosition.x >= bounds.min.x - 8.0f && mousePosition.x <= bounds.max.x + 8.0f &&
            mousePosition.y >= bounds.min.y - 8.0f && mousePosition.y <= bounds.max.y + 8.0f)
        {
            distance = std::min(distance, 3.0f);
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        if (distance < bestDistance - 0.25f ||
            (std::abs(distance - bestDistance) <= 0.25f && area < bestArea))
        {
            bestDistance = distance;
            bestArea = area;
            bestIndex = primitiveIndex;
        }
    }

    return bestIndex;
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
        NormalizeEditorIdentifier(baseName).empty() ? std::string {"Strobe"} : std::string(baseName);
    std::string candidate = stableBase;
    int suffix = 2;
    const std::string excludedNormalizedName = NormalizeEditorIdentifier(excludedStrobeName);

    while (std::any_of(page.strobes.begin(),
                       page.strobes.end(),
                       [&candidate, &excludedNormalizedName](const mfd::PageStrobeDefinition& strobe)
                       {
                           const std::string normalizedStrobeName = NormalizeEditorIdentifier(strobe.name);
                           if (normalizedStrobeName.empty())
                           {
                               return false;
                           }

                           if (!excludedNormalizedName.empty() && normalizedStrobeName == excludedNormalizedName)
                           {
                               return false;
                           }

                           return normalizedStrobeName == NormalizeEditorIdentifier(candidate);
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

