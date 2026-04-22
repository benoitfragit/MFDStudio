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
#include <functional>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <rlImGui.h>

#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorFileDialogs.h"
#include "EditorUiTheme.h"
#include "mfd/model/Types.h"
#include "mfd/render/Canvas2D.h"
#include "mfd/render/RenderTextureUtils.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::ApplyEditorTheme;
using editor::ui::DrawVerticalSplitter;
using editor::ui::ShowItemTooltip;

constexpr float kSidebarWidth = 320.0f;
constexpr float kInspectorWidth = 360.0f;
constexpr float kMinSidebarWidth = 220.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 420.0f;
constexpr float kMinPageContextWidth = 320.0f;
constexpr float kMinReticleStudioWidth = 320.0f;
constexpr std::string_view kTutorialStrobeCursorTemplateId = "mfd_tutorial_strobe_cursor";

std::optional<std::filesystem::path> FindProjectRoot(const std::filesystem::path& start)
{
    std::filesystem::path current = std::filesystem::absolute(start);

    while (true)
    {
        if (std::filesystem::exists(current / "CMakeLists.txt") &&
            std::filesystem::exists(current / "assets") &&
            std::filesystem::exists(current / "mfd_editor"))
        {
            return current;
        }

        if (current == current.root_path())
        {
            break;
        }

        current = current.parent_path();
    }

    return std::nullopt;
}

std::filesystem::path DefaultProjectAssetFolder(const std::string_view relativeAssetFolder)
{
    if (const auto projectRoot = FindProjectRoot(std::filesystem::current_path()); projectRoot.has_value())
    {
        return (*projectRoot / std::filesystem::path(relativeAssetFolder)).lexically_normal();
    }

    return std::filesystem::absolute(std::filesystem::path(relativeAssetFolder)).lexically_normal();
}

std::filesystem::path DefaultSiblingAssetFile(const std::filesystem::path& anchorFile,
                                              const std::string_view siblingFolder,
                                              const std::string_view fileName)
{
    if (anchorFile.empty())
    {
        return (DefaultProjectAssetFolder(std::string("assets/") + std::string(siblingFolder)) /
                std::filesystem::path(fileName))
            .lexically_normal();
    }

    const std::filesystem::path anchorFolder = anchorFile.parent_path();
    const std::filesystem::path candidateRoot =
        anchorFolder.filename() == std::filesystem::path(siblingFolder)
            ? anchorFolder
            : (anchorFolder.has_parent_path() ? anchorFolder.parent_path() / std::filesystem::path(siblingFolder)
                                              : anchorFolder / std::filesystem::path(siblingFolder));
    return (candidateRoot / std::filesystem::path(fileName)).lexically_normal();
}

std::filesystem::path JsonFileNameOrFallback(const std::filesystem::path& candidate, const std::string_view fallbackFileName)
{
    std::filesystem::path fileName = candidate.filename();
    if (fileName.empty() || fileName == "." || fileName == "..")
    {
        fileName = std::filesystem::path(fallbackFileName);
    }

    if (fileName.extension().empty())
    {
        fileName += ".json";
    }

    return fileName;
}

bool PathContainsSegment(const std::filesystem::path& path, const std::string_view segment)
{
    if (segment.empty())
    {
        return false;
    }

    std::string normalizedSegment(segment);
    std::transform(normalizedSegment.begin(),
                   normalizedSegment.end(),
                   normalizedSegment.begin(),
                   [](const unsigned char ch)
                   {
                       return static_cast<char>(std::tolower(ch));
                   });

    for (const auto& part : path)
    {
        std::string candidate = part.string();
        std::transform(candidate.begin(),
                       candidate.end(),
                       candidate.begin(),
                       [](const unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });
        if (candidate == normalizedSegment)
        {
            return true;
        }
    }

    return false;
}

bool IsExecStagingPath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path absolutePath =
        path.is_absolute() ? path.lexically_normal() : std::filesystem::absolute(path).lexically_normal();
    return PathContainsSegment(absolutePath, "_Exec");
}

int FindPageIndexByName(const mfd::LoadedWindowConfiguration& loaded, const std::string_view pageName)
{
    for (int index = 0; index < static_cast<int>(loaded.document.pages.size()); ++index)
    {
        const auto& page = loaded.document.pages[static_cast<std::size_t>(index)];
        if (page.name == pageName)
        {
            return index;
        }
    }

    return -1;
}

int FindPageReticleIndexById(const mfd::PageDefinition& page, const std::string_view reticleId)
{
    for (int index = 0; index < static_cast<int>(page.staticReticles.size()); ++index)
    {
        if (page.staticReticles[static_cast<std::size_t>(index)].id == reticleId)
        {
            return index;
        }
    }

    return -1;
}

template <std::size_t N>
void CopyTextBuffer(std::array<char, N>& destination, const std::string_view value)
{
    std::snprintf(destination.data(), destination.size(), "%.*s", static_cast<int>(value.size()), value.data());
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

mfd::ColorRgba ToColorRgba(const ImVec4& color)
{
    auto toByte = [](const float value) -> std::uint8_t
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    return mfd::ColorRgba {
        toByte(color.x),
        toByte(color.y),
        toByte(color.z),
        toByte(color.w)};
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

std::string PrimitiveTypeLabel(const mfd::PrimitiveType type)
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
        return "Text";
    case mfd::PrimitiveType::Time:
        return "Time";
    case mfd::PrimitiveType::Line:
        return "Line";
    case mfd::PrimitiveType::Circle:
        return "Circle";
    case mfd::PrimitiveType::Ring:
        return "Ring";
    case mfd::PrimitiveType::Rectangle:
        return "Rectangle";
    case mfd::PrimitiveType::Ellipse:
        return "Ellipse";
    case mfd::PrimitiveType::Square:
        return "Square";
    case mfd::PrimitiveType::Diamond:
        return "Diamond";
    case mfd::PrimitiveType::Triangle:
        return "Triangle";
    case mfd::PrimitiveType::Polyline:
        return "Polyline";
    case mfd::PrimitiveType::Bezier:
        return "Bezier";
    }

    return "Primitive";
}

const char* ReticleClipModeLabel(const mfd::ReticleClipMode mode) noexcept
{
    switch (mode)
    {
    case mfd::ReticleClipMode::Inner:
        return "Inner clipping";
    case mfd::ReticleClipMode::Outer:
        return "Outer clipping";
    case mfd::ReticleClipMode::None:
    default:
        return "Disabled";
    }
}

struct ClipPrimitiveOption
{
    std::string primitiveId;
    std::string label;
};

std::vector<ClipPrimitiveOption> CollectClipPrimitiveOptions(const mfd::ReticleGroup& reticle)
{
    std::vector<ClipPrimitiveOption> options;
    std::unordered_set<std::string> seenPrimitiveIds;

    for (int index = 0; index < static_cast<int>(reticle.primitives.size()); ++index)
    {
        const auto& primitive = reticle.primitives[static_cast<std::size_t>(index)];
        if (primitive.id.empty() || !mfd::SupportsReticleClipPrimitive(primitive))
        {
            continue;
        }

        if (!seenPrimitiveIds.insert(primitive.id).second)
        {
            continue;
        }

        options.push_back(ClipPrimitiveOption {
            primitive.id,
            primitive.id + " (" + PrimitiveTypeLabel(primitive.type) + ")"});
    }

    return options;
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

float EstimatedTextHalfWidth(const std::string_view text, const float fontSize, const float letterSpacing)
{
    const std::size_t characterCount = std::max<std::size_t>(1, text.size());
    const float glyphWidth = fontSize * static_cast<float>(characterCount) * 0.30f;
    const float spacingWidth = std::max(0.0f, letterSpacing) *
                               static_cast<float>(characterCount > 0 ? characterCount - 1U : 0U) * 0.5f;
    return std::max(0.03f, glyphWidth + spacingWidth);
}

float EstimatedTextHalfWidth(const mfd::TextGeometry& text)
{
    return EstimatedTextHalfWidth(text.text, text.fontSize, text.letterSpacing);
}

float EstimatedTextHalfWidth(const mfd::TimeGeometry& time)
{
    return EstimatedTextHalfWidth(time.format.empty() ? std::string_view {"%H:%M:%S"} : std::string_view {time.format},
                                  time.fontSize,
                                  time.letterSpacing);
}

float EstimatedTextHalfHeight(const float fontSize)
{
    return std::max(0.02f, fontSize * 0.50f);
}

float EstimatedTextHalfHeight(const mfd::TextGeometry& text)
{
    return EstimatedTextHalfHeight(text.fontSize);
}

float EstimatedTextHalfHeight(const mfd::TimeGeometry& time)
{
    return EstimatedTextHalfHeight(time.fontSize);
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

mfd::Vec2 InverseTransformPoint(const mfd::Vec2& point, const mfd::Transform2D& transform)
{
    const mfd::Vec2 translated = point - transform.position;
    const mfd::Vec2 rotated = mfd::Rotate(translated, -transform.rotationDegrees);

    return {
        std::abs(transform.scale.x) <= 0.0001f ? 0.0f : rotated.x / transform.scale.x,
        std::abs(transform.scale.y) <= 0.0001f ? 0.0f : rotated.y / transform.scale.y};
}

float Distance(const ImVec2 lhs, const ImVec2 rhs)
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
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
    if (!primitive.style.visible)
    {
        return bounds;
    }

    auto includeTransformedPoint = [&](const mfd::Vec2 localPoint)
    {
        IncludeLogicalPoint(bounds, mfd::ApplyTransform(localPoint, primitive.transform));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(line->start);
        includeTransformedPoint(line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, circle->radius});
        includeTransformedPoint({-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, ring->outerRadius});
        includeTransformedPoint({-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, rectangle->height * 0.5f});
        includeTransformedPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, ellipse->height * 0.5f});
        includeTransformedPoint({-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, square->height * 0.5f});
        includeTransformedPoint({-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({0.0f, diamond->height * 0.5f});
        includeTransformedPoint({diamond->width * 0.5f, 0.0f});
        includeTransformedPoint({0.0f, -diamond->height * 0.5f});
        includeTransformedPoint({-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(triangle->points[0]);
        includeTransformedPoint(triangle->points[1]);
        includeTransformedPoint(triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            includeTransformedPoint(point);
        }
    }

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
    const LogicalBounds localBounds = ComputeReticleLocalBounds(reticle);
    if (!localBounds.valid)
    {
        return {};
    }

    LogicalBounds worldBounds;
    const std::array<mfd::Vec2, 4> corners {{
        {localBounds.min.x, localBounds.min.y},
        {localBounds.max.x, localBounds.min.y},
        {localBounds.max.x, localBounds.max.y},
        {localBounds.min.x, localBounds.max.y},
    }};
    for (const mfd::Vec2& corner : corners)
    {
        IncludeLogicalPoint(worldBounds, mfd::ApplyTransform(corner, reticle.transform));
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

constexpr std::array<mfd::PrimitiveType, 11> kPrimitiveTypes {
    mfd::PrimitiveType::Text,
    mfd::PrimitiveType::Time,
    mfd::PrimitiveType::Line,
    mfd::PrimitiveType::Circle,
    mfd::PrimitiveType::Rectangle,
    mfd::PrimitiveType::Ellipse,
    mfd::PrimitiveType::Square,
    mfd::PrimitiveType::Diamond,
    mfd::PrimitiveType::Triangle,
    mfd::PrimitiveType::Polyline,
    mfd::PrimitiveType::Bezier};

constexpr std::size_t kInvalidBlinkTypeIndex = std::numeric_limits<std::size_t>::max();

bool ConfigureTutorialStrobeLinePrimitive(mfd::Primitive& primitive, const bool vertical) noexcept
{
    if (primitive.type != mfd::PrimitiveType::Line)
    {
        return false;
    }

    auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry);
    if (line == nullptr)
    {
        return false;
    }

    primitive.id = vertical ? "vertical_line" : "horizontal_line";
    primitive.style.thickness = 0.0038f;
    line->start = vertical ? mfd::Vec2 {0.0f, -0.055f} : mfd::Vec2 {-0.055f, 0.0f};
    line->end = vertical ? mfd::Vec2 {0.0f, 0.055f} : mfd::Vec2 {0.055f, 0.0f};
    return true;
}

int DefaultPageIndex(const std::vector<mfd::PageDefinition>& pages) noexcept
{
    for (int index = 0; index < static_cast<int>(pages.size()); ++index)
    {
        if (pages[static_cast<std::size_t>(index)].defaultPage)
        {
            return index;
        }
    }

    return 0;
}

std::size_t FindBlinkTypeIndex(const mfd::PageDefinition& page, const std::string_view blinkTypeName)
{
    const std::string normalizedBlinkTypeName = mfd::NormalizePageName(blinkTypeName);
    if (normalizedBlinkTypeName.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        if (page.blinkTypes[index].normalizedName == normalizedBlinkTypeName)
        {
            return index;
        }
    }

    return kInvalidBlinkTypeIndex;
}

std::size_t EffectiveDefaultBlinkTypeIndex(const mfd::PageDefinition& page)
{
    if (page.blinkTypes.empty())
    {
        return kInvalidBlinkTypeIndex;
    }

    if (!page.normalizedDefaultBlinkTypeName.empty())
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            if (page.blinkTypes[index].normalizedName == page.normalizedDefaultBlinkTypeName)
            {
                return index;
            }
        }
    }

    return 0;
}

std::string MakeUniqueBlinkTypeName(const mfd::PageDefinition& page, std::string_view baseName)
{
    std::string candidate = baseName.empty() ? std::string {"blink"} : std::string(baseName);
    int suffix = 2;

    while (FindBlinkTypeIndex(page, candidate) != kInvalidBlinkTypeIndex)
    {
        candidate = std::string(baseName.empty() ? "blink" : baseName) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

void RenameBlinkReference(mfd::ReticleBlinkState& blink,
                          const std::string_view previousNormalizedName,
                          const std::string& nextName,
                          const std::string& nextNormalizedName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName != previousNormalizedName)
    {
        return;
    }

    blink.typeName = nextName;
    blink.normalizedTypeName = nextNormalizedName;
}

void ClearBlinkReference(mfd::ReticleBlinkState& blink, const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    const std::string currentNormalizedName =
        blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
    if (currentNormalizedName == removedNormalizedName)
    {
        blink = {};
    }
}

std::size_t CountBlinkReferences(const mfd::PageDefinition& page, const std::string_view normalizedBlinkTypeName)
{
    if (normalizedBlinkTypeName.empty())
    {
        return 0;
    }

    const auto matches = [&normalizedBlinkTypeName](const mfd::ReticleBlinkState& blink)
    {
        const std::string currentNormalizedName =
            blink.normalizedTypeName.empty() ? mfd::NormalizePageName(blink.typeName) : blink.normalizedTypeName;
        return currentNormalizedName == normalizedBlinkTypeName;
    };

    std::size_t count = 0;
    for (const auto& reticle : page.staticReticles)
    {
        if (matches(reticle.blink))
        {
            ++count;
        }
    }

    if (page.strobe.has_value() && matches(page.strobe->reticle.blink))
    {
        ++count;
    }

    return count;
}

void RenameBlinkReferences(mfd::PageDefinition& page,
                           const std::string_view previousNormalizedName,
                           const std::string& nextName)
{
    if (previousNormalizedName.empty())
    {
        return;
    }

    const std::string nextNormalizedName = mfd::NormalizePageName(nextName);
    if (page.normalizedDefaultBlinkTypeName == previousNormalizedName)
    {
        page.defaultBlinkTypeName = nextName;
        page.normalizedDefaultBlinkTypeName = nextNormalizedName;
    }

    for (auto& reticle : page.staticReticles)
    {
        RenameBlinkReference(reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }

    if (page.strobe.has_value())
    {
        RenameBlinkReference(page.strobe->reticle.blink, previousNormalizedName, nextName, nextNormalizedName);
    }
}

void ClearBlinkReferencesForRemovedType(mfd::PageDefinition& page, const std::string_view removedNormalizedName)
{
    if (removedNormalizedName.empty())
    {
        return;
    }

    if (page.normalizedDefaultBlinkTypeName == removedNormalizedName)
    {
        page.defaultBlinkTypeName.clear();
        page.normalizedDefaultBlinkTypeName.clear();
    }

    for (auto& reticle : page.staticReticles)
    {
        ClearBlinkReference(reticle.blink, removedNormalizedName);
    }

    if (page.strobe.has_value())
    {
        ClearBlinkReference(page.strobe->reticle.blink, removedNormalizedName);
    }
}

void RefreshBlinkBindingForEditor(const mfd::PageDefinition& page, mfd::ReticleBlinkState& blink)
{
    blink.normalizedTypeName = blink.typeName.empty() ? std::string {} : mfd::NormalizePageName(blink.typeName);

    if (!blink.typeName.empty())
    {
        const std::size_t blinkTypeIndex = FindBlinkTypeIndex(page, blink.normalizedTypeName);
        blink.durationMs = blinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[blinkTypeIndex].durationMs;
        return;
    }

    if (!blink.enabled)
    {
        blink.durationMs = 0;
        return;
    }

    const std::size_t defaultBlinkTypeIndex = EffectiveDefaultBlinkTypeIndex(page);
    blink.durationMs =
        defaultBlinkTypeIndex == kInvalidBlinkTypeIndex ? 0U : page.blinkTypes[defaultBlinkTypeIndex].durationMs;
}

void RefreshPageBlinkStateForEditor(mfd::PageDefinition& page)
{
    for (auto& blinkType : page.blinkTypes)
    {
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = std::max<std::uint32_t>(1U, blinkType.durationMs);
    }

    page.normalizedDefaultBlinkTypeName =
        page.defaultBlinkTypeName.empty() ? std::string {} : mfd::NormalizePageName(page.defaultBlinkTypeName);

    for (auto& reticle : page.staticReticles)
    {
        RefreshBlinkBindingForEditor(page, reticle.blink);
    }

    if (page.strobe.has_value())
    {
        RefreshBlinkBindingForEditor(page, page.strobe->reticle.blink);
    }
}

const mfd::EditorLayerDefinition* FindEditorLayer(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return nullptr;
    }

    const auto iterator = std::find_if(page.editor.layers.begin(),
                                       page.editor.layers.end(),
                                       [layerId](const mfd::EditorLayerDefinition& layer)
                                       {
                                           return layer.id == layerId;
                                       });
    return iterator == page.editor.layers.end() ? nullptr : &(*iterator);
}

bool IsReticleVisibleInEditor(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle)
{
    if (const mfd::EditorLayerDefinition* layer = FindEditorLayer(page, reticle.editor.layerId); layer != nullptr)
    {
        return layer->visible;
    }

    return true;
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

    if (page.strobe.has_value())
    {
        IncludeLogicalBounds(logicalBounds, ComputeReticleWorldBounds(page.strobe->reticle));
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

std::size_t CountEditorLayerAssignments(const mfd::PageDefinition& page, const std::string_view layerId)
{
    if (layerId.empty())
    {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(
        page.staticReticles.begin(),
        page.staticReticles.end(),
        [layerId](const mfd::ReticleGroup& reticle)
        {
            return reticle.editor.layerId == layerId;
        }));
}

void RenameEditorLayerReferences(mfd::PageDefinition& page,
                                 const std::string_view previousLayerId,
                                 const std::string& nextLayerId)
{
    if (previousLayerId.empty() || previousLayerId == nextLayerId)
    {
        return;
    }

    for (auto& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId == previousLayerId)
        {
            reticle.editor.layerId = nextLayerId;
        }
    }
}

std::size_t ClearEditorLayerReferences(mfd::PageDefinition& page, const std::string_view removedLayerId)
{
    if (removedLayerId.empty())
    {
        return 0;
    }

    std::size_t clearedCount = 0;
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId == removedLayerId)
        {
            reticle.editor.layerId.clear();
            ++clearedCount;
        }
    }

    return clearedCount;
}

std::string DefaultEditorLayerId(const mfd::PageDefinition& page)
{
    const auto visibleLayer = std::find_if(page.editor.layers.begin(),
                                           page.editor.layers.end(),
                                           [](const mfd::EditorLayerDefinition& layer)
                                           {
                                               return layer.visible && !layer.id.empty();
                                           });
    if (visibleLayer != page.editor.layers.end())
    {
        return visibleLayer->id;
    }

    const auto firstLayer = std::find_if(page.editor.layers.begin(),
                                         page.editor.layers.end(),
                                         [](const mfd::EditorLayerDefinition& layer)
                                         {
                                             return !layer.id.empty();
                                         });
    return firstLayer == page.editor.layers.end() ? std::string {} : firstLayer->id;
}

void BootstrapEditorLayersForPage(mfd::PageDefinition& page)
{
    if (!page.editor.layers.empty())
    {
        return;
    }

    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
    for (auto& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId.empty())
        {
            reticle.editor.layerId = page.editor.layers.front().id;
        }
    }
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

EditorApplication::EditorApplication()
{
    sidebarWidth_ = kSidebarWidth;
    inspectorWidth_ = kInspectorWidth;
    tutorial_ = std::make_unique<EditorTutorialController>(*this);
    CopyTextBuffer(newPageDraft_.name, "NewPage");
    CopyTextBuffer(newPageDraft_.title, "New Page");
    CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/new_page.json").string());
    CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/new_window.json").string());
    CopyTextBuffer(newWindowDraft_.title, "New MFD Window");
    CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultProjectAssetFolder("assets/reticles").string());
    CopyTextBuffer(newWindowDraft_.commandAddress, "127.0.0.1");
    CopyTextBuffer(newWindowDraft_.feedbackAddress, "127.0.0.1");
    CopyTextBuffer(newWindowDraft_.firstPageName, "Page1");
    CopyTextBuffer(newWindowDraft_.firstPageTitle, "Page 1");
    CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultProjectAssetFolder("assets/pages/page1.json").string());
    CopyTextBuffer(newLibraryReticleDraft_.id, "new_reticle");
    CopyTextBuffer(duplicateLibraryReticleDraft_.id, "reticle_copy");
    ResetPagePreviewView();
    ResetLibraryPreviewView();
    RebuildStatus("Open one window asset or create assets to begin authoring.", false);
    tutorial_->LoadProgress();
}

EditorApplication::~EditorApplication()
{
    ReleaseTooltipPreviewTexture();
    ReleasePreviewTexture();
    ReleasePreviewFont();
}

float EditorApplication::ViewportState::LogicalScale() const noexcept
{
    return 0.5f * std::min(size.x, size.y);
}

ImVec2 EditorApplication::ViewportState::ToScreen(const mfd::Vec2 logical) const noexcept
{
    const mfd::Vec2 viewed = mfd::ApplyPageView(logical, view);
    return ImVec2(
        origin.x + size.x * 0.5f + viewed.x * LogicalScale(),
        origin.y + size.y * 0.5f - viewed.y * LogicalScale());
}

mfd::Vec2 EditorApplication::ViewportState::ToLogical(const ImVec2 screen) const noexcept
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
    SetWindowMinSize(1320, 760);
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ApplyEditorTheme();

    while (!WindowShouldClose())
    {
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
            tutorial_->DrawCoach();
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
            RebuildStatus(lastRuntimeError_, true);
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception inside mfd_editor";
            RebuildStatus(lastRuntimeError_, true);
        }

        if (imguiBegun)
        {
            rlImGuiEnd();
        }

        EndDrawing();
    }

    rlImGuiShutdown();
    ReleaseTooltipPreviewTexture();
    ReleasePreviewTexture();
    ReleasePreviewFont();
    CloseWindow();
    return 0;
}

bool EditorApplication::LoadWindowConfiguration(const std::filesystem::path& path)
{
    try
    {
        loaded_ = loader_.LoadWindowConfiguration(path);
        for (auto& page : loaded_.document.pages)
        {
            BootstrapEditorLayersForPage(page);
        }
        files_.pageFiles = loaded_.window.pageFiles;
        files_.removedPageFiles.clear();
        files_.removedTemplateFiles.clear();
        std::string error;
        if (!editor::DiscoverReticleTemplateFiles(loaded_.window.reticleLibraryFolder, files_, &error))
        {
            throw std::runtime_error(error);
        }

        ApplyPreviewFontFile(loaded_.window.fontFile);
        selection_ = {};
        SelectPage(DefaultPageIndex(loaded_.document.pages));
        ResetLibraryPreviewView();
        undoStack_.clear();
        windowFile_ = path;
        lastRuntimeError_.clear();
        RebuildStatus("Editor loaded '" + loaded_.window.title + "'.", false);
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
    if (!editor::SaveEditorDocument(loaded_, files_, &error))
    {
        RebuildStatus("Save failed: " + error, true);
        return false;
    }

    RebuildStatus("Window, pages and library saved.", false);
    return true;
}

void EditorApplication::Undo()
{
    if (undoStack_.empty())
    {
        RebuildStatus("Nothing to undo.", true);
        return;
    }

    UndoSnapshot snapshot = std::move(undoStack_.back());
    undoStack_.pop_back();
    loaded_ = std::move(snapshot.loaded);
    files_ = std::move(snapshot.files);
    selection_ = std::move(snapshot.selection);
    ResetPagePreviewView();
    ResetLibraryPreviewView();

    if (selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        SelectPage(loaded_.document.pages.empty() ? 0 : static_cast<int>(loaded_.document.pages.size()) - 1);
    }

    RebuildStatus("Undo applied.", false);
}

void EditorApplication::PushUndoSnapshot()
{
    if (undoStack_.size() >= 64)
    {
        undoStack_.erase(undoStack_.begin());
    }

    undoStack_.push_back(UndoSnapshot {loaded_, files_, selection_});
}

void EditorApplication::HandleShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_S}))
    {
        SaveAll();
    }

    const bool undoShortcutTriggered =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverActive) ||
        IsRaylibControlChordPressed({KEY_Z, KEY_W});
    if (undoShortcutTriggered)
    {
        Undo();
    }

    if (!io.WantTextInput &&
        (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal) ||
         IsRaylibControlChordPressed({KEY_C})))
    {
        CopySelectedPageReticles();
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
        PasteCopiedPageReticles();
    }

    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        DeleteSelection();
    }

    if (!io.WantTextInput &&
        !ImGui::IsPopupOpen((const char*)nullptr, ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape) &&
        selection_.kind == SelectionKind::PageReticle &&
        !SelectedPageReticleIndices().empty())
    {
        SelectPage(selection_.pageIndex);
        RebuildStatus("Page reticle selection cleared.", false);
    }
}

void EditorApplication::DeleteSelection()
{
    if (selection_.kind == SelectionKind::Page)
    {
        DeleteActivePage();
        return;
    }

    if (selection_.kind == SelectionKind::PageReticle)
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

        SelectPage(selection_.pageIndex);

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

    if (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive)
    {
        DeleteSelectedLibraryReticle();
        return;
    }

    RebuildStatus("Select a page, a page reticle or a library reticle to delete it.", true);
}

void EditorApplication::DeleteActivePage()
{
    if (loaded_.document.pages.empty() ||
        selection_.pageIndex < 0 ||
        selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        RebuildStatus("No page selected to delete.", true);
        return;
    }

    PushUndoSnapshot();

    const int removedPageIndex = selection_.pageIndex;
    const std::string removedPageName = loaded_.document.pages[static_cast<std::size_t>(removedPageIndex)].name;
    if (removedPageIndex < static_cast<int>(files_.pageFiles.size()))
    {
        files_.removedPageFiles.push_back(files_.pageFiles[static_cast<std::size_t>(removedPageIndex)].lexically_normal());
        files_.pageFiles.erase(files_.pageFiles.begin() + removedPageIndex);
    }

    loaded_.document.pages.erase(loaded_.document.pages.begin() + removedPageIndex);
    loaded_.window.pageFiles = files_.pageFiles;

    if (loaded_.document.pages.empty())
    {
        selection_ = {};
        RebuildStatus("Page '" + removedPageName + "' deleted. The window has no pages now.", false);
        return;
    }

    const int nextPageIndex = std::min(removedPageIndex, static_cast<int>(loaded_.document.pages.size()) - 1);
    selection_.pageIndex = nextPageIndex;
    selection_.pageReticleIndex = -1;
    if (selection_.kind == SelectionKind::Page || selection_.kind == SelectionKind::PageReticle)
    {
        SelectPage(nextPageIndex);
    }

    RebuildStatus("Page '" + removedPageName + "' deleted.", false);
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
    for (const auto& page : loaded_.document.pages)
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

        if (page.strobe.has_value() && page.strobe->reticle.sourceTemplateId == reticleId)
        {
            RebuildStatus("Cannot delete library reticle '" + reticleId +
                              "' because strobe on page '" + page.name + "' still uses it.",
                          true);
            return;
        }
    }

    PushUndoSnapshot();

    if (const auto fileIt = files_.templateFiles.find(reticleId); fileIt != files_.templateFiles.end())
    {
        files_.removedTemplateFiles.push_back(fileIt->second.lexically_normal());
        files_.templateFiles.erase(fileIt);
    }

    loaded_.document.reticleLibrary.erase(reticleId);

    if (loaded_.document.reticleLibrary.empty())
    {
        SelectPage(std::clamp(selection_.pageIndex,
                              0,
                              std::max(0, static_cast<int>(loaded_.document.pages.size()) - 1)));
        RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
        return;
    }

    std::vector<std::string> remainingTemplateIds;
    remainingTemplateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& [templateId, group] : loaded_.document.reticleLibrary)
    {
        (void)group;
        remainingTemplateIds.push_back(templateId);
    }
    std::sort(remainingTemplateIds.begin(), remainingTemplateIds.end());
    SelectLibraryReticle(remainingTemplateIds.front());
    RebuildStatus("Library reticle '" + reticleId + "' deleted.", false);
}

void EditorApplication::DrawMenuBar()
{
    using editor::tutorial::TutorialStepId;
    const bool hasOpenWindow = HasOpenWindow();

    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    const bool fileMenuOpen = ImGui::BeginMenu("File");
    tutorial_->DrawHalo("menu_file", "Click File", "Open the top-level document actions used by this tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_file"))
    {
        tutorial_->AdvancePhase();
    }
    if (fileMenuOpen)
    {
        const bool newWindowRequested = ImGui::MenuItem("New window from scratch");
        ShowItemTooltip("Create a brand-new window JSON and optional first page directly from the editor.");
        tutorial_->DrawHalo(
            "menu_file_new_window",
            "Click New window from scratch",
            "Open the creation dialog prefilled with the tutorial window settings.");
        if (newWindowRequested)
        {
            if (tutorial_->MatchesTarget("menu_file_new_window"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewWindowPopup();
        }

        const bool openWindowRequested = ImGui::MenuItem("Open window asset...");
        ShowItemTooltip("Browse to one authored window JSON through the native file explorer.");
        if (openWindowRequested)
        {
            OpenWindowAssetFromFileExplorer();
        }

        ImGui::Separator();
        const bool saveRequested = ImGui::MenuItem("Save", "Ctrl+S", false, hasOpenWindow);
        ShowItemTooltip("Write the window file, page files and reticle template files back to disk.");
        tutorial_->DrawHalo(
            "menu_file_save",
            "Click Save",
            "Persist the authored tutorial assets before moving to the code review steps.");
        if (saveRequested)
        {
            const bool tutorialSaveMatched = tutorial_->MatchesTarget("menu_file_save");
            if (SaveAll() && tutorialSaveMatched)
            {
                tutorial_->CompleteStep();
            }
        }

        const bool canReloadCurrent = hasOpenWindow && std::filesystem::exists(windowFile_);
        const bool reloadRequested = ImGui::MenuItem("Reload current", nullptr, false, canReloadCurrent);
        ShowItemTooltip("Reload the current window asset from disk and discard unsaved editor changes.");
        if (reloadRequested)
        {
            LoadWindowConfiguration(windowFile_);
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreateWindow), 1) ||
             tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::SaveTutorialAssets), 1))
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        const bool undoRequested = ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack_.empty());
        ShowItemTooltip("Restore the previous editor snapshot.");
        if (undoRequested)
        {
            Undo();
        }

        const bool hasPageReticleSelection = !SelectedPageReticleIndices().empty();
        const bool copyReticlesRequested =
            ImGui::MenuItem("Copy selected page reticles", "Ctrl+C", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances so they can be pasted on the active page.");
        if (copyReticlesRequested)
        {
            CopySelectedPageReticles();
        }

        const bool cutReticlesRequested =
            ImGui::MenuItem("Cut selected page reticles", "Ctrl+X", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances into the clipboard, then remove them from the page.");
        if (cutReticlesRequested)
        {
            CutSelectedPageReticles();
        }

        const bool canPastePageReticles = ActivePage() != nullptr && !pageReticleClipboard_.empty();
        const bool pasteReticlesRequested =
            ImGui::MenuItem("Paste page reticles", "Ctrl+V", false, canPastePageReticles);
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        if (pasteReticlesRequested)
        {
            PasteCopiedPageReticles();
        }

        const bool deleteReticlesRequested =
            ImGui::MenuItem("Delete selected page reticles", "Del", false, hasPageReticleSelection);
        ShowItemTooltip("Remove the selected page reticle instances from the active page.");
        if (deleteReticlesRequested)
        {
            DeleteSelection();
        }
        ImGui::EndMenu();
    }

    const bool pageMenuOpen = ImGui::BeginMenu("Page", hasOpenWindow);
    tutorial_->DrawHalo("menu_page", "Click Page", "Open the page-authoring actions used by the current tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_page"))
    {
        tutorial_->AdvancePhase();
    }
    if (pageMenuOpen)
    {
        const bool newPageRequested = ImGui::MenuItem("New page");
        ShowItemTooltip("Create a new page and its backing JSON file.");
        tutorial_->DrawHalo(
            "menu_page_new",
            "Click New page",
            "Open the page dialog with the tutorial page values already prepared for you.");
        if (newPageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewPagePopup();
        }

        const bool deletePageRequested = ImGui::MenuItem("Delete current page", "Del", false, ActivePage() != nullptr);
        ShowItemTooltip("Delete the currently selected page from the window definition.");
        if (deletePageRequested)
        {
            DeleteActivePage();
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreatePage1), 1) ||
             tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreatePage2), 1))
    {
        tutorial_->ResetPhase();
    }

    const bool reticleMenuOpen = ImGui::BeginMenu("Reticle", hasOpenWindow);
    tutorial_->DrawHalo("menu_reticle", "Click Reticle", "Open the reticle-template actions used by the tutorial.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_reticle"))
    {
        tutorial_->AdvancePhase();
    }
    if (reticleMenuOpen)
    {
        const bool newLibraryReticleRequested = ImGui::MenuItem("New library reticle from primitive");
        ShowItemTooltip("Create a new shared reticle template.");
        tutorial_->DrawHalo(
            "menu_reticle_new",
            "Click New library reticle from primitive",
            "Open the reticle dialog and create the tutorial template shown in this step.");
        if (newLibraryReticleRequested)
        {
            if (tutorial_->MatchesTarget("menu_reticle_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewLibraryReticlePopup();
        }

        const bool hasFocusedLibraryReticle =
            (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive) &&
            SelectedLibraryReticle() != nullptr;

        const bool duplicateReticleRequested =
            ImGui::MenuItem("Duplicate selected library reticle", nullptr, false, hasFocusedLibraryReticle);
        ShowItemTooltip("Duplicate the focused library reticle under a new template id.");
        if (duplicateReticleRequested)
        {
            OpenDuplicateLibraryReticlePopup();
        }

        const bool deleteReticleRequested =
            ImGui::MenuItem("Delete selected library reticle", "Del", false, hasFocusedLibraryReticle);
        ShowItemTooltip("Delete the focused library reticle template from the shared library.");
        if (deleteReticleRequested)
        {
            DeleteSelectedLibraryReticle();
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreateRadarTrackReticle), 1) ||
             tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreateCircleReticle), 1) ||
             tutorial_->IsStepPhase(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle), 1))
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Help"))
    {
        const bool tutorialRequested = ImGui::MenuItem("Tutorial", nullptr, tutorial_->IsCoachVisible());
        ShowItemTooltip("Open the guided discovery mode for the editor and tutorial assets.");
        if (tutorialRequested)
        {
            tutorial_->OpenFlow();
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("|");
    const char* titleLabel = hasOpenWindow && !loaded_.window.title.empty() ? loaded_.window.title.c_str() : "No asset open";
    ImGui::Text("%s", titleLabel);
    if (hasOpenWindow)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", windowFile_.filename().string().c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", statusMessage_.c_str());

    ImGui::EndMainMenuBar();
}

void EditorApplication::DrawRootLayout()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(
        "MFD Editor Root",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    const float totalWidth = ImGui::GetContentRegionAvail().x;
    const float totalHeight = ImGui::GetContentRegionAvail().y;
    const float maxSidebarWidth = std::max(kMinSidebarWidth,
                                           totalWidth - inspectorWidth_ - kMinWorkspaceWidth - 2.0f * editor::ui::kPaneSplitterWidth);
    sidebarWidth_ = std::clamp(sidebarWidth_, kMinSidebarWidth, maxSidebarWidth);

    const float maxInspectorWidth = std::max(kMinInspectorWidth,
                                             totalWidth - sidebarWidth_ - kMinWorkspaceWidth - 2.0f * editor::ui::kPaneSplitterWidth);
    inspectorWidth_ = std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth);

    ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth_, 0.0f), true);
    DrawSidebar();
    ImGui::EndChild();

    ImGui::SameLine();
    if (DrawVerticalSplitter("##SidebarSplitter", totalHeight))
    {
        const float nextSidebarWidth = sidebarWidth_ + ImGui::GetIO().MouseDelta.x;
        const float nextMaxSidebarWidth = std::max(kMinSidebarWidth,
                                                   totalWidth - inspectorWidth_ - kMinWorkspaceWidth -
                                                       2.0f * editor::ui::kPaneSplitterWidth);
        sidebarWidth_ = std::clamp(nextSidebarWidth, kMinSidebarWidth, nextMaxSidebarWidth);
    }

    ImGui::SameLine();

    const float workspaceWidth =
        std::max(kMinWorkspaceWidth, totalWidth - sidebarWidth_ - inspectorWidth_ - 2.0f * editor::ui::kPaneSplitterWidth);
    ImGui::BeginChild("Workspace", ImVec2(workspaceWidth, 0.0f), true);
    DrawWorkspace();
    ImGui::EndChild();

    ImGui::SameLine();
    if (DrawVerticalSplitter("##InspectorSplitter", totalHeight))
    {
        const float nextInspectorWidth = inspectorWidth_ - ImGui::GetIO().MouseDelta.x;
        const float nextMaxInspectorWidth = std::max(kMinInspectorWidth,
                                                     totalWidth - sidebarWidth_ - kMinWorkspaceWidth -
                                                         2.0f * editor::ui::kPaneSplitterWidth);
        inspectorWidth_ = std::clamp(nextInspectorWidth, kMinInspectorWidth, nextMaxInspectorWidth);
    }

    ImGui::SameLine();
    ImGui::BeginChild("Inspector", ImVec2(0.0f, 0.0f), true);
    DrawInspector();
    ImGui::EndChild();

    ImGui::End();
}

void EditorApplication::DrawSidebar()
{
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "MFD Editor");
    ImGui::TextDisabled("Work directly in the page visualization.");
    ImGui::Separator();

    if (!HasOpenWindow())
    {
        ImGui::TextWrapped("No authored window is open yet.");
        ImGui::Spacing();
        ImGui::TextDisabled("Use File > Open window asset... or File > New window from scratch.");

        if (!lastRuntimeError_.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", lastRuntimeError_.c_str());
        }
        return;
    }

    DrawPageTree();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawLibraryTree();

    if (!lastRuntimeError_.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", lastRuntimeError_.c_str());
    }
}

void EditorApplication::DrawWorkspace()
{
    if (!HasOpenWindow())
    {
        DrawEmptyWorkspacePlaceholder();
        return;
    }

    const bool libraryStudioVisible =
        selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive;

    if (libraryStudioVisible)
    {
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float totalHeight = ImGui::GetContentRegionAvail().y;

        if (libraryStudioPageWidth_ <= 0.0f)
        {
            libraryStudioPageWidth_ = std::max(kMinPageContextWidth, totalWidth * 0.56f);
        }

        const float maxPageWidth = std::max(kMinPageContextWidth,
                                            totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
        libraryStudioPageWidth_ = std::clamp(libraryStudioPageWidth_, kMinPageContextWidth, maxPageWidth);
        const float pageWidth = libraryStudioPageWidth_;

        ImGui::BeginChild("PageContextPanel", ImVec2(pageWidth, 0.0f), true);
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page context");
        ImGui::TextDisabled("Keep drag & drop and page composition visible while editing the library reticle.");
        ImGui::Separator();

        ViewportState pageViewport;
        pageViewport.origin = ImGui::GetCursorScreenPos();
        pageViewport.size = ImGui::GetContentRegionAvail();
        pageViewport.valid = pageViewport.size.x > 8.0f && pageViewport.size.y > 8.0f;
        if (const mfd::PageDefinition* activePage = ActivePage(); activePage != nullptr)
        {
            pageViewport.view = pagePreviewView_;
        }

        if (pageViewport.valid && ActivePage() != nullptr)
        {
            DrawPagePreview(pageViewport);
            if (selection_.kind == SelectionKind::PageReticle)
            {
                DrawPreviewOverlays(pageViewport);
                HandlePreviewInteraction(pageViewport);
            }
            DrawPageReticleContextMenu();
        }
        else
        {
            ImGui::TextDisabled("No active page to preview.");
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##WorkspaceSplitter", totalHeight))
        {
            const float nextPageWidth = libraryStudioPageWidth_ + ImGui::GetIO().MouseDelta.x;
            const float nextMaxPageWidth = std::max(kMinPageContextWidth,
                                                    totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
            libraryStudioPageWidth_ = std::clamp(nextPageWidth, kMinPageContextWidth, nextMaxPageWidth);
        }

        ImGui::SameLine();
        ImGui::BeginChild("ReticleStudioPanel", ImVec2(0.0f, 0.0f), true);
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Reticle studio");
        ImGui::TextDisabled("Click a primitive to focus it, drag the handles to edit its geometry.");
        ImGui::Separator();

        ViewportState studioViewport;
        studioViewport.origin = ImGui::GetCursorScreenPos();
        studioViewport.size = ImGui::GetContentRegionAvail();
        studioViewport.valid = studioViewport.size.x > 8.0f && studioViewport.size.y > 8.0f;
        studioViewport.view = libraryPreviewView_;

        if (studioViewport.valid)
        {
            DrawLibraryPreview(studioViewport);
            DrawLibraryPreviewOverlays(studioViewport);
            HandleLibraryPreviewInteraction(studioViewport);
        }
        ImGui::EndChild();
        return;
    }

    ViewportState viewport;
    viewport.origin = ImGui::GetCursorScreenPos();
    viewport.size = ImGui::GetContentRegionAvail();
    viewport.valid = viewport.size.x > 8.0f && viewport.size.y > 8.0f;

    const mfd::PageDefinition* activePage = ActivePage();
    if (activePage != nullptr)
    {
        viewport.view = pagePreviewView_;
    }

    if (!viewport.valid)
    {
        return;
    }

    if (ActivePage() != nullptr)
    {
        DrawPagePreview(viewport);
        DrawPreviewOverlays(viewport);
        HandlePreviewInteraction(viewport);
        DrawPageReticleContextMenu();
    }
    else
    {
        ImGui::TextDisabled("No active page to preview.");
    }
}

void EditorApplication::DrawEmptyWorkspacePlaceholder()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x <= 8.0f || available.y <= 8.0f)
    {
        return;
    }

    const char* headline = "Open or create assets";
    const char* description =
        "Start with a new authored window or browse to an existing window JSON.\n"
        "Nothing is loaded automatically when the editor starts.";

    const ImVec2 headlineSize = ImGui::CalcTextSize(headline);
    const ImVec2 descriptionSize = ImGui::CalcTextSize(description);
    const float buttonRowWidth = 420.0f;
    const float totalHeight = headlineSize.y + descriptionSize.y + 92.0f;
    const ImVec2 start(
        std::max(0.0f, (available.x - std::max(std::max(headlineSize.x, descriptionSize.x), buttonRowWidth)) * 0.5f),
        std::max(0.0f, (available.y - totalHeight) * 0.5f));

    ImGui::SetCursorPos(start);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "%s", headline);
    ImGui::SetCursorPosX(start.x);
    ImGui::TextDisabled("%s", description);
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(start.x);
    if (AccentButton("Open window asset..."))
    {
        OpenWindowAssetFromFileExplorer();
    }

    ImGui::SetCursorPosX(start.x);
    if (ImGui::Button("New window from scratch", ImVec2(220.0f, 0.0f)))
    {
        OpenNewWindowPopup();
    }
}

void EditorApplication::DrawInspector()
{
    if (!HasOpenWindow())
    {
        ImGui::TextDisabled("No asset is open.");
        ImGui::TextWrapped("Open one existing window asset or create a new window to edit pages, reticles and strobe settings.");
        return;
    }

    switch (selection_.kind)
    {
    case SelectionKind::Page:
        DrawPageInspector();
        break;
    case SelectionKind::PageReticle:
        DrawPageReticleInspector();
        break;
    case SelectionKind::LibraryReticle:
        DrawLibraryReticleInspector();
        break;
    case SelectionKind::LibraryPrimitive:
        DrawLibraryReticleInspector();
        ImGui::Separator();
        DrawLibraryPrimitiveInspector();
        break;
    }
}

void EditorApplication::DrawPageTree()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Pages");
    ShowItemTooltip("Browse authored pages and select a page or one of its page reticles.");

    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded_.document.pages.size()); ++pageIndex)
    {
        const auto& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selection_.kind == SelectionKind::Page && selection_.pageIndex == pageIndex)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string pageLabel = page.title.empty() ? page.name : page.title;
        const bool open = ImGui::TreeNodeEx((pageLabel + "##page_" + std::to_string(pageIndex)).c_str(), flags);
        ShowItemTooltip("Click to focus the page inspector. Use the arrow or double-click to expand its reticles.");
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            SelectPage(pageIndex);
        }

        if (open)
        {
            ImGui::TextDisabled("file: %s", pageIndex < static_cast<int>(files_.pageFiles.size())
                                              ? files_.pageFiles[static_cast<std::size_t>(pageIndex)].filename().string().c_str()
                                              : "<missing>");

            for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
            {
                const auto& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
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
                ImGui::TreeNodeEx(reticleLabel.c_str(), leafFlags);
                DrawReticleHoverPreviewTooltip(
                    reticle,
                    std::string("Page reticle: ") + (reticle.id.empty() ? "reticle" : reticle.id),
                    ToRayColor(page.backgroundColor));
                if (ImGui::IsItemClicked())
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

                if (!reticle.editor.layerId.empty())
                {
                    ImGui::SameLine();
                    const bool layerVisible = IsReticleVisibleInEditor(page, reticle);
                    ImGui::TextDisabled("[%s%s]",
                                        reticle.editor.layerId.c_str(),
                                        layerVisible ? "" : " hidden");
                }
            }

            ImGui::TreePop();
        }
    }
}

void EditorApplication::DrawLibraryTree()
{
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Reticle library");
    ShowItemTooltip("Shared reticle templates that can be edited in the studio or dropped onto pages.");

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& [templateId, reticle] : loaded_.document.reticleLibrary)
    {
        (void)reticle;
        templateIds.push_back(templateId);
    }
    std::sort(templateIds.begin(), templateIds.end());

    for (const auto& templateId : templateIds)
    {
        const bool selected =
            selection_.libraryBrowserReticleId == templateId ||
            (((selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive) &&
              selection_.libraryReticleId == templateId) &&
             selection_.libraryBrowserReticleId.empty());
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx((templateId + "##library").c_str(), flags);
        const auto libraryIt = loaded_.document.reticleLibrary.find(templateId);
        if (libraryIt != loaded_.document.reticleLibrary.end())
        {
            DrawReticleHoverPreviewTooltip(
                libraryIt->second,
                std::string("Library reticle: ") + templateId,
                Color {10, 18, 24, 255});
        }
        if (ImGui::IsItemClicked())
        {
            selection_.libraryBrowserReticleId = templateId;
            if (selection_.kind != SelectionKind::LibraryReticle && selection_.kind != SelectionKind::LibraryPrimitive)
            {
                selection_.libraryReticleId = templateId;
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
    }

    if (AccentButton("New reticle"))
    {
        OpenNewLibraryReticlePopup();
    }
    ShowItemTooltip("Create a new library reticle seeded with one primitive.");
}

void EditorApplication::OpenNewPagePopup()
{
    SeedNewPageAssetDraftPath();
    showNewPagePopup_ = true;
}

void EditorApplication::OpenNewWindowPopup()
{
    SeedNewWindowAssetDraftPaths();
    showNewWindowPopup_ = true;
}

bool EditorApplication::OpenWindowAssetFromFileExplorer()
{
    const std::filesystem::path initialFolder =
        HasOpenWindow() && windowFile_.has_parent_path() ? windowFile_.parent_path() : DefaultProjectAssetFolder("assets/windows");
    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenWindowAssetFileDialog(initialFolder, &error);
    if (!selectedFile.has_value())
    {
        if (!error.empty())
        {
            RebuildStatus(error, true);
        }
        return false;
    }

    return LoadWindowConfiguration(*selectedFile);
}

void EditorApplication::OpenAssetFolderPicker(const AssetFolderPickerTarget target)
{
    assetFolderPickerTarget_ = target;

    auto folderFromConfiguredPath = [](const std::filesystem::path& configuredPath) -> std::filesystem::path
    {
        if (configuredPath.empty())
        {
            return {};
        }

        return configuredPath.has_extension() ? configuredPath.parent_path() : configuredPath;
    };

    switch (target)
    {
    case AssetFolderPickerTarget::WindowFile:
        assetFolderPickerCurrentFolder_ =
            folderFromConfiguredPath(std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/windows");
        }
        break;

    case AssetFolderPickerTarget::ReticleLibraryFolder:
        assetFolderPickerCurrentFolder_ =
            folderFromConfiguredPath(std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/reticles");
        }
        break;

    case AssetFolderPickerTarget::FirstPageFile:
        assetFolderPickerCurrentFolder_ =
            folderFromConfiguredPath(std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
        }
        break;

    case AssetFolderPickerTarget::NewPageFile:
        assetFolderPickerCurrentFolder_ =
            folderFromConfiguredPath(std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal());
        if (assetFolderPickerCurrentFolder_.empty())
        {
            assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
        }
        break;

    case AssetFolderPickerTarget::None:
    default:
        assetFolderPickerCurrentFolder_.clear();
        break;
    }

    if (assetFolderPickerCurrentFolder_.is_relative())
    {
        assetFolderPickerCurrentFolder_ = std::filesystem::absolute(assetFolderPickerCurrentFolder_).lexically_normal();
    }

    showAssetFolderPickerPopup_ = true;
}

void EditorApplication::OpenNewLibraryReticlePopup()
{
    showNewLibraryReticlePopup_ = true;
}

void EditorApplication::OpenDuplicateLibraryReticlePopup()
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    CopyTextBuffer(duplicateLibraryReticleDraft_.id, reticle->id + "_copy");
    showDuplicateLibraryReticlePopup_ = true;
}

void EditorApplication::RebuildStatus(std::string message, const bool isError)
{
    statusMessage_ = std::move(message);
    statusIsError_ = isError;
}

void EditorApplication::EnsurePreviewTexture(const int width, const int height)
{
    if (previewTextureReady_ &&
        previewTexture_.texture.width == width &&
        previewTexture_.texture.height == height)
    {
        return;
    }

    ReleasePreviewTexture();
    previewTextureStencilReady_ = false;
    previewTexture_ = mfd::LoadRenderTextureWithStencil(width, height, &previewTextureStencilReady_);
    previewTextureReady_ = previewTexture_.texture.id != 0;
}

void EditorApplication::ReleasePreviewTexture()
{
    if (previewTextureReady_)
    {
        UnloadRenderTexture(previewTexture_);
        previewTexture_ = {};
    }

    previewTextureReady_ = false;
    previewTextureStencilReady_ = false;
}

void EditorApplication::EnsureTooltipPreviewTexture(const int width, const int height)
{
    if (tooltipPreviewTextureReady_ &&
        tooltipPreviewTexture_.texture.width == width &&
        tooltipPreviewTexture_.texture.height == height)
    {
        return;
    }

    ReleaseTooltipPreviewTexture();
    tooltipPreviewTextureStencilReady_ = false;
    tooltipPreviewTexture_ =
        mfd::LoadRenderTextureWithStencil(width, height, &tooltipPreviewTextureStencilReady_);
    tooltipPreviewTextureReady_ = tooltipPreviewTexture_.texture.id != 0;
}

void EditorApplication::ReleaseTooltipPreviewTexture()
{
    if (tooltipPreviewTextureReady_)
    {
        UnloadRenderTexture(tooltipPreviewTexture_);
        tooltipPreviewTexture_ = {};
    }

    tooltipPreviewTextureReady_ = false;
    tooltipPreviewTextureStencilReady_ = false;
}

void EditorApplication::ApplyPreviewFontFile(std::filesystem::path fontFile)
{
    if (!fontFile.empty())
    {
        fontFile = fontFile.lexically_normal();
    }

    if (previewFontFile_ == fontFile)
    {
        return;
    }

    ReleasePreviewFont();
    previewFontFile_ = std::move(fontFile);
    previewFontLoadAttempted_ = false;
}

void EditorApplication::EnsurePreviewFont()
{
    if (previewFontReady_ || previewFontLoadAttempted_ || previewFontFile_.empty() || !IsWindowReady())
    {
        return;
    }

    previewFontLoadAttempted_ = true;
    const std::string path = previewFontFile_.string();
    Font loadedFont = LoadFont(path.c_str());
    if (loadedFont.texture.id == 0)
    {
        return;
    }

    previewFont_ = loadedFont;
    previewFontReady_ = true;
}

void EditorApplication::ReleasePreviewFont() noexcept
{
    if (previewFontReady_)
    {
        UnloadFont(previewFont_);
        previewFont_ = {};
        previewFontReady_ = false;
    }
}

const Font* EditorApplication::PreviewTextFont() const noexcept
{
    return previewFontReady_ ? &previewFont_ : nullptr;
}

void EditorApplication::ResetPagePreviewView() noexcept
{
    pagePreviewView_ = {};
    if (const mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        pagePreviewView_ = page->view;
        pagePreviewView_.zoom = mfd::SanitizeZoom(pagePreviewView_.zoom);
    }
}

void EditorApplication::ResetLibraryPreviewView() noexcept
{
    libraryPreviewView_ = {};
    libraryPreviewView_.zoom = 1.0f;
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
    if (!previewTextureReady_)
    {
        return;
    }

    BeginTextureMode(previewTexture_);
    ClearBackground(ToRayColor(page->backgroundColor));
    {
        EnsurePreviewFont();
        mfd::Canvas2D canvas(
            previewTexture_.texture.width,
            previewTexture_.texture.height,
            viewport.view,
            PreviewTextFont(),
            ToRayColor(page->backgroundColor),
            previewTextureStencilReady_);
        for (const auto& reticle : page->staticReticles)
        {
            if (!IsReticleVisibleInEditor(*page, reticle))
            {
                continue;
            }

            canvas.DrawReticle(reticle);
        }

        if (page->strobe.has_value())
        {
            canvas.DrawReticle(page->strobe->reticle);
        }
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewTexture_.texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::InvisibleButton("PagePreviewInput", viewport.size);
    ShowItemTooltip(
        "Editor-only page preview.\n"
        "Ctrl+click adds or removes one reticle from the current selection.\n"
        "Esc clears the current page-reticle selection.\n"
        "Drag one selected reticle to move the whole selected group.\n"
        "Mouse wheel zooms the editor camera only.\n"
        "Right-drag pans the editor camera.\n"
        "Right-click one or more reticles to open a context menu with selection actions and clipping submenus.\n"
        "Left-drag the minimap viewport to navigate without changing authored reticle data.");
    tutorial_->DrawHalo(
        "page_preview_clip_source",
        "Right-click the circle reticle",
        "Open the clipping context menu on the tutorial mask so you can keep only the outside region.");

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MFD_LIBRARY_RETICLE"))
        {
            const char* templateId = static_cast<const char*>(payload->Data);
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::AddCircleReticleToPage1)))
            {
                RebuildStatus("Tutorial: use the highlighted Add to active page button for this step.", true);
            }
            else
            {
                CreatePageReticleInstanceFromTemplate(templateId, viewport.ToLogical(ImGui::GetMousePos()));
            }
        }
        ImGui::EndDragDropTarget();
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
    if (!previewTextureReady_)
    {
        return;
    }

    BeginTextureMode(previewTexture_);
    ClearBackground(Color {10, 18, 24, 255});
    {
        EnsurePreviewFont();
        mfd::Canvas2D canvas(
            previewTexture_.texture.width,
            previewTexture_.texture.height,
            viewport.view,
            PreviewTextFont(),
            Color {10, 18, 24, 255},
            previewTextureStencilReady_);
        canvas.DrawReticle(*reticle);
    }
    EndTextureMode();

    ImGui::Image(
        (ImTextureID)(uintptr_t)previewTexture_.texture.id,
        viewport.size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));

    ImGui::SetCursorScreenPos(viewport.origin);
    ImGui::InvisibleButton("LibraryPreviewInput", viewport.size);
    ShowItemTooltip(
        "Editor-only reticle studio preview.\n"
        "Mouse wheel zooms the studio camera only.\n"
        "Right-drag pans the preview.\n"
        "Left-drag green and orange handles to edit the selected primitive.");
}

void EditorApplication::DrawLibraryPreviewOverlays(const ViewportState& viewport)
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasSelectedPrimitive = selection_.kind == SelectionKind::LibraryPrimitive &&
                                      selection_.libraryReticleId == reticle->id &&
                                      selection_.primitiveIndex >= 0 &&
                                      selection_.primitiveIndex < static_cast<int>(reticle->primitives.size());

    auto toScreenPoint = [&](const mfd::Primitive& primitive, const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle->transform));
    };

    auto drawHandle = [&](const ImVec2 point, const ImU32 color, const float radius = 6.0f)
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

        const bool selected = hasSelectedPrimitive && selection_.primitiveIndex == primitiveIndex;
        const ImU32 borderColor = selected ? IM_COL32(255, 212, 110, 255) : IM_COL32(104, 185, 205, 160);
        const ImU32 fillColor = selected ? IM_COL32(255, 212, 110, 32) : IM_COL32(104, 185, 205, 18);
        drawList->AddRectFilled(bounds.min, bounds.max, fillColor, 6.0f);
        drawList->AddRect(bounds.min, bounds.max, borderColor, 6.0f, 0, selected ? 2.2f : 1.3f);

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

        if (!selected)
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
        }
    }
}

void EditorApplication::DrawPreviewOverlays(const ViewportState& viewport)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (ImGui::IsItemHovered())
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (mouse.x >= viewport.origin.x &&
            mouse.x <= viewport.origin.x + viewport.size.x &&
            mouse.y >= viewport.origin.y &&
            mouse.y <= viewport.origin.y + viewport.size.y)
        {
            const mfd::Vec2 logical = viewport.ToLogical(mouse);
            char coordinates[96] {};
            std::snprintf(coordinates,
                          sizeof(coordinates),
                          "View X %+0.3f  Y %+0.3f",
                          static_cast<double>(logical.x),
                          static_cast<double>(logical.y));

            const ImVec2 textSize = ImGui::CalcTextSize(coordinates);
            const ImVec2 labelMin(viewport.origin.x + 12.0f, viewport.origin.y + 12.0f);
            const ImVec2 labelMax(labelMin.x + textSize.x + 16.0f, labelMin.y + textSize.y + 10.0f);
            drawList->AddRectFilled(labelMin, labelMax, IM_COL32(7, 15, 23, 224), 6.0f);
            drawList->AddRect(labelMin, labelMax, IM_COL32(76, 132, 168, 255), 6.0f, 0, 1.5f);
            drawList->AddText(ImVec2(labelMin.x + 8.0f, labelMin.y + 5.0f),
                              IM_COL32(216, 233, 246, 255),
                              coordinates);
        }
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page != nullptr)
    {
        const PageMinimapState minimap = ComputePageMinimapState(*page, viewport);
        if (minimap.valid)
        {
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
            for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
            {
                const mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
                if (!IsReticleVisibleInEditor(*page, reticle))
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
                ImVec2 rectMin(std::min(rectPointA.x, rectPointB.x), std::min(rectPointA.y, rectPointB.y));
                ImVec2 rectMax(std::max(rectPointA.x, rectPointB.x), std::max(rectPointA.y, rectPointB.y));
                const bool selected =
                    std::find(selectedIndices.begin(), selectedIndices.end(), reticleIndex) != selectedIndices.end();

                if (rectMax.x - rectMin.x < 4.0f || rectMax.y - rectMin.y < 4.0f)
                {
                    const ImVec2 center = ToMinimapScreen(minimap, worldBounds.center);
                    drawList->AddCircleFilled(center, selected ? 3.5f : 2.5f, selected ? IM_COL32(84, 219, 201, 255)
                                                                                       : IM_COL32(174, 200, 214, 230),
                                              12);
                    continue;
                }

                drawList->AddRectFilled(
                    rectMin,
                    rectMax,
                    selected ? IM_COL32(84, 219, 201, 70) : IM_COL32(174, 200, 214, 34),
                    2.0f);
                drawList->AddRect(
                    rectMin,
                    rectMax,
                    selected ? IM_COL32(84, 219, 201, 255) : IM_COL32(174, 200, 214, 190),
                    2.0f,
                    0,
                    selected ? 1.8f : 1.0f);
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
            drawList->AddText(
                ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + 8.0f),
                IM_COL32(216, 233, 246, 255),
                minimapLabel);
            drawList->AddLine(
                ImVec2(minimap.frameMin.x + 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                ImVec2(minimap.frameMax.x - 10.0f, minimap.frameMin.y + textSize.y + 12.0f),
                IM_COL32(36, 63, 78, 255),
                1.0f);

            ShowHoveredRegionTooltip(
                mouseInsideMinimap,
                "Minimap navigation for the editor camera.\n"
                "Drag the blue viewport rectangle to pan smoothly.\n"
                "Click elsewhere in the minimap to recenter the editor view.");
        }
    }

    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty() || selection_.kind != SelectionKind::PageReticle)
    {
        return;
    }

    ReticleScreenBounds selectionBounds;
    for (const int reticleIndex : selectedIndices)
    {
        const mfd::ReticleGroup& selectedReticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, selectedReticle))
        {
            continue;
        }

        const ReticleScreenBounds bounds = ComputeReticleScreenBounds(selectedReticle, viewport);
        if (!bounds.valid)
        {
            continue;
        }

        const bool primarySelection = reticleIndex == selection_.pageReticleIndex;
        drawList->AddRect(
            bounds.min,
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

    selectionBounds.center = ImVec2(
        (selectionBounds.min.x + selectionBounds.max.x) * 0.5f,
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
    if (reticle == nullptr || !IsReticleVisibleInEditor(*page, *reticle))
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*reticle, viewport);
    if (!bounds.valid)
    {
        return;
    }

    const mfd::Vec2 visualCenterLogical = mfd::ApplyTransform(ReticleVisualCenterLocal(*reticle), reticle->transform);
    const ImVec2 center = viewport.ToScreen(visualCenterLogical);
    const ImVec2 topLeft = bounds.min;
    const ImVec2 topRight(bounds.max.x, bounds.min.y);
    const ImVec2 bottomRight = bounds.max;
    const ImVec2 bottomLeft(bounds.min.x, bounds.max.y);
    const ImVec2 topCenter((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y);
    const ImVec2 rotateHandle(topCenter.x, topCenter.y - 26.0f);

    drawList->AddCircle(center, 5.0f, IM_COL32(84, 219, 201, 255), 18, 2.0f);
    drawList->AddLine(topCenter, rotateHandle, IM_COL32(110, 180, 250, 255), 1.5f);
    drawList->AddCircle(rotateHandle, 8.0f, IM_COL32(110, 180, 250, 255), 20, 2.0f);
    drawList->AddLine(
        ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
        ImVec2(rotateHandle.x + 10.0f, rotateHandle.y - 7.0f),
        IM_COL32(110, 180, 250, 255),
        2.0f);
    drawList->AddLine(
        ImVec2(rotateHandle.x + 5.0f, rotateHandle.y - 3.0f),
        ImVec2(rotateHandle.x + 10.0f, rotateHandle.y + 1.0f),
        IM_COL32(110, 180, 250, 255),
        2.0f);

    const auto drawCorner = [&](const ImVec2 corner)
    {
        drawList->AddRectFilled(
            ImVec2(corner.x - 5.5f, corner.y - 5.5f),
            ImVec2(corner.x + 5.5f, corner.y + 5.5f),
            IM_COL32(255, 193, 92, 255),
            2.0f);
    };

    drawCorner(topLeft);
    drawCorner(topRight);
    drawCorner(bottomRight);
    drawCorner(bottomLeft);
}

void EditorApplication::HandlePreviewInteraction(const ViewportState& viewport)
{
    using editor::tutorial::TutorialStepId;

    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return;
    }

    ViewportState interactiveViewport = viewport;
    interactiveViewport.view = pagePreviewView_;

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (!ImGui::IsItemHovered())
    {
        const bool interactionButtonReleased =
            interactionMode_ == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
        if (interactionMode_ != InteractionMode::None && interactionButtonReleased)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndex_ = -1;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionMode_ == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinPageZoom = 0.1f;
        constexpr float kMaxPageZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom = std::clamp(mfd::SanitizeZoom(pagePreviewView_.zoom), kMinPageZoom, kMaxPageZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinPageZoom, kMaxPageZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const mfd::Vec2 mouseLogicalBeforeZoom = interactiveViewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - pagePreviewView_.center;
            const float zoomRatio = currentZoom / nextZoom;

            pagePreviewView_.zoom = nextZoom;
            pagePreviewView_.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
            interactiveViewport.view = pagePreviewView_;
        }
    }

    const PageMinimapState minimap = ComputePageMinimapState(*page, interactiveViewport);
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool mouseInsideMinimap = minimap.valid && IsPointInsideRect(mouse, minimap.contentMin, minimap.contentMax);
    if (interactionMode_ == InteractionMode::None && mouseInsideMinimap && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        interactionMode_ = InteractionMode::NavigateMinimap;
        interactionReticleIndex_ = -1;

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
            minimapDragOffsetLogical_ = {
                clickedLogical.x - pagePreviewView_.center.x,
                clickedLogical.y - pagePreviewView_.center.y};
        }
        else
        {
            pagePreviewView_.center = clickedLogical;
            minimapDragOffsetLogical_ = {};
            interactiveViewport.view = pagePreviewView_;
        }
    }

    if (interactionMode_ == InteractionMode::NavigateMinimap)
    {
        if (!minimap.valid)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
            return;
        }

        const mfd::Vec2 draggedLogical = ToMinimapLogical(minimap, mouse);
        pagePreviewView_.center = {
            draggedLogical.x - minimapDragOffsetLogical_.x,
            draggedLogical.y - minimapDragOffsetLogical_.y};
        if (!leftMouseDown)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    if (interactionMode_ == InteractionMode::None &&
        !mouseInsideMinimap &&
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) &&
        !ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
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
                !HasSelectedPageReticle(selection_.pageIndex, hoveredReticleIndices.front()))
            {
                SelectPageReticle(selection_.pageIndex, hoveredReticleIndices.front());
            }

            pagePreviewContextReticleIndices_ = hoveredReticleIndices;
            pagePreviewContextTargets_ = hoveredClipTargets;
            ImGui::OpenPopup("PageReticleContextMenu");
            return;
        }
    }

    if (interactionMode_ == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionMode_ = InteractionMode::PanPage;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
    }

    if (interactionMode_ != InteractionMode::None)
    {
        ApplyMouseTransform(interactiveViewport);
        const bool interactionButtonReleased =
            interactionMode_ == InteractionMode::PanPage ? !rightMouseDown : !leftMouseDown;
        if (interactionButtonReleased)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndex_ = -1;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
        }
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const bool additiveSelection = ImGui::GetIO().KeyCtrl;
    const std::optional<int> clickedReticleIndex = FindNearestPageReticle(viewport, mouse);
    if (!additiveSelection && page != nullptr && SelectedPageReticleCount() == 1)
    {
        mfd::ReticleGroup* selectedReticle = SelectedPageReticle();
        if (selectedReticle != nullptr && IsReticleVisibleInEditor(*page, *selectedReticle))
        {
            const ReticleScreenBounds selectedBounds = ComputeReticleScreenBounds(*selectedReticle, viewport);
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

                const auto initializeInteraction = [&](const InteractionMode mode, const ImVec2 cornerScreen)
                {
                    interactionReticleIndex_ = selection_.pageReticleIndex;
                    interactionReticleIndices_ = {selection_.pageReticleIndex};
                    interactionStartReticleTransforms_ = {selectedReticle->transform};
                    interactionStartTransform_ = selectedReticle->transform;
                    interactionStartReticleVisualCenterLocal_ = ReticleVisualCenterLocal(*selectedReticle);
                    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
                    const mfd::Vec2 interactionPivotLogical =
                        mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
                    interactionStartAngleDegrees_ =
                        std::atan2(interactionStartMouseLogical_.y - interactionPivotLogical.y,
                                   interactionStartMouseLogical_.x - interactionPivotLogical.x) *
                        180.0f / 3.14159265f;
                    interactionStartDistance_ = std::max(
                        0.001f,
                        std::sqrt(
                            std::pow(interactionStartMouseLogical_.x - interactionPivotLogical.x, 2.0f) +
                            std::pow(interactionStartMouseLogical_.y - interactionPivotLogical.y, 2.0f)));
                    interactionStartCenterScreen_ = viewport.ToScreen(interactionPivotLogical);
                    interactionStartCornerScreen_ = cornerScreen;
                    PushUndoSnapshot();
                    interactionMode_ = mode;
                };

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

    if (!additiveSelection &&
        SelectedPageReticleCount() > 1 &&
        clickedReticleIndex.has_value() &&
        HasSelectedPageReticle(selection_.pageIndex, *clickedReticleIndex))
    {
        // Preserve the current multi-selection when dragging one of its members.
        const std::vector<int> selectedIndices = SelectedPageReticleIndices();
        interactionReticleIndex_ = selection_.pageReticleIndex;
        interactionReticleIndices_ = selectedIndices;
        interactionStartReticleTransforms_.clear();
        interactionStartReticleTransforms_.reserve(selectedIndices.size());
        for (const int reticleIndex : selectedIndices)
        {
            interactionStartReticleTransforms_.push_back(
                page->staticReticles[static_cast<std::size_t>(reticleIndex)].transform);
        }
        interactionStartMouseLogical_ = viewport.ToLogical(mouse);
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MoveReticle;
        return;
    }

    UpdateReticleSelectionFromClick(viewport, additiveSelection);
    if (additiveSelection || SelectedPageReticleCount() != 1)
    {
        return;
    }

    mfd::ReticleGroup* reticle = SelectedPageReticle();
    if (reticle == nullptr)
    {
        return;
    }

    const ReticleScreenBounds bounds = ComputeReticleScreenBounds(*reticle, viewport);
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

    auto distance = [](const ImVec2 lhs, const ImVec2 rhs) -> float
    {
        const float dx = lhs.x - rhs.x;
        const float dy = lhs.y - rhs.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    interactionReticleIndex_ = selection_.pageReticleIndex;
    interactionReticleIndices_ = {selection_.pageReticleIndex};
    interactionStartReticleTransforms_ = {reticle->transform};
    interactionStartTransform_ = reticle->transform;
    interactionStartReticleVisualCenterLocal_ = ReticleVisualCenterLocal(*reticle);
    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
    const mfd::Vec2 interactionPivotLogical =
        mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
    interactionStartAngleDegrees_ =
        std::atan2(interactionStartMouseLogical_.y - interactionPivotLogical.y,
                   interactionStartMouseLogical_.x - interactionPivotLogical.x) *
        180.0f / 3.14159265f;
    interactionStartDistance_ = std::max(
        0.001f,
        std::sqrt(
            std::pow(interactionStartMouseLogical_.x - interactionPivotLogical.x, 2.0f) +
            std::pow(interactionStartMouseLogical_.y - interactionPivotLogical.y, 2.0f)));
    interactionStartCenterScreen_ = viewport.ToScreen(interactionPivotLogical);
    interactionStartCornerScreen_ = center;

    if (distance(mouse, rotateHandle) <= 16.0f)
    {
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::RotateReticle;
    }
    else if (mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
             mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        for (const ImVec2 corner : corners)
        {
            if (distance(mouse, corner) <= 16.0f)
            {
                PushUndoSnapshot();
                interactionMode_ = InteractionMode::ScaleReticle;
                interactionStartCornerScreen_ = corner;
                return;
            }
        }

        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MoveReticle;
    }
    else
    {
        interactionMode_ = InteractionMode::None;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
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
    pagePreviewContextReticleIndices_ = {reticleIndex};
    pagePreviewContextTargets_.clear();
    for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
    {
        if (reticle.primitives[static_cast<std::size_t>(primitiveIndex)].id == reticle.clipping.primitiveId)
        {
            pagePreviewContextTargets_.push_back(PageClipTarget {reticleIndex, primitiveIndex});
            break;
        }
    }
    SelectPageReticle(selection_.pageIndex, reticleIndex);

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
    if (page == nullptr || pagePreviewContextReticleIndices_.empty())
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
            "Keep only the outside of the tutorial circle so you can discover page-level masking.");

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
    const bool canPasteSelection = !pageReticleClipboard_.empty();
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
        for (const PageClipTarget& target : pagePreviewContextTargets_)
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
        const bool selected = HasSelectedPageReticle(selection_.pageIndex, reticleIndex);
        if (ImGui::MenuItem("Select only", nullptr, selected))
        {
            SelectPageReticle(selection_.pageIndex, reticleIndex);
        }
        ShowItemTooltip("Focus only this reticle in the inspector and preview.");

        const char* toggleLabel = selected ? "Remove from selection" : "Add to selection";
        if (ImGui::MenuItem(toggleLabel, "Ctrl+click"))
        {
            TogglePageReticleSelection(selection_.pageIndex, reticleIndex);
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

    if (pagePreviewContextReticleIndices_.size() == 1U)
    {
        const int reticleIndex = pagePreviewContextReticleIndices_.front();
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
        for (const int reticleIndex : pagePreviewContextReticleIndices_)
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

    const bool leftMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool rightMouseDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (interactionMode_ == InteractionMode::PanPage)
    {
        if (!rightMouseDown)
        {
            interactionMode_ = InteractionMode::None;
        }
        else
        {
            ApplyMouseTransform(viewport);
        }
        return;
    }

    const bool hasActivePrimitiveInteraction =
        interactionMode_ == InteractionMode::MovePrimitive || interactionMode_ == InteractionMode::EditPrimitiveHandle;

    if (hasActivePrimitiveInteraction)
    {
        if (interactionPrimitiveIndex_ < 0 || interactionPrimitiveIndex_ >= static_cast<int>(reticle->primitives.size()))
        {
            interactionMode_ = InteractionMode::None;
            interactionPrimitiveIndex_ = -1;
            interactionHandleKind_ = PrimitiveHandleKind::None;
            interactionHandleIndex_ = -1;
            return;
        }

        mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(interactionPrimitiveIndex_)];
        const mfd::Vec2 mouseLogical = viewport.ToLogical(ImGui::GetMousePos());
        const mfd::Vec2 mouseReticleLocal = InverseTransformPoint(mouseLogical, reticle->transform);

        if (interactionMode_ == InteractionMode::MovePrimitive)
        {
            primitive.transform.position =
                interactionStartPrimitive_.transform.position + (mouseReticleLocal - interactionStartMouseReticleLocal_);
        }
        else if (interactionMode_ == InteractionMode::EditPrimitiveHandle)
        {
            const mfd::Vec2 mousePrimitiveLocal = InverseTransformPoint(mouseReticleLocal, interactionStartPrimitive_.transform);

            if (auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ == 0)
                {
                    line->start = mousePrimitiveLocal;
                }
                else if (interactionHandleIndex_ == 1)
                {
                    line->end = mousePrimitiveLocal;
                }
            }
            else if (auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
            {
                circle->radius = std::max(0.001f, std::sqrt(mousePrimitiveLocal.x * mousePrimitiveLocal.x +
                                                            mousePrimitiveLocal.y * mousePrimitiveLocal.y));
            }
            else if (auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
            {
                rectangle->width = std::max(0.001f, std::abs(mousePrimitiveLocal.x) * 2.0f);
                rectangle->height = std::max(0.001f, std::abs(mousePrimitiveLocal.y) * 2.0f);
            }
            else if (auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ == 0)
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
                if ((interactionHandleIndex_ % 2) == 0)
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
                if (interactionHandleIndex_ >= 0 && interactionHandleIndex_ < 3)
                {
                    triangle->points[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
            else if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ >= 0 &&
                    interactionHandleIndex_ < static_cast<int>(polyline->points.size()))
                {
                    polyline->points[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
            else if (auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
            {
                if (interactionHandleIndex_ >= 0 &&
                    interactionHandleIndex_ < static_cast<int>(bezier->controlPoints.size()))
                {
                    bezier->controlPoints[static_cast<std::size_t>(interactionHandleIndex_)] = mousePrimitiveLocal;
                }
            }
        }

        if (!leftMouseDown)
        {
            interactionMode_ = InteractionMode::None;
            interactionPrimitiveIndex_ = -1;
            interactionHandleKind_ = PrimitiveHandleKind::None;
            interactionHandleIndex_ = -1;
        }
        return;
    }

    if (!ImGui::IsItemHovered())
    {
        return;
    }

    const float wheelDelta = ImGui::GetIO().MouseWheel;
    if (interactionMode_ == InteractionMode::None && std::abs(wheelDelta) > 0.0001f)
    {
        constexpr float kMinStudioZoom = 0.1f;
        constexpr float kMaxStudioZoom = 20.0f;
        constexpr float kWheelZoomStep = 1.12f;

        const float currentZoom =
            std::clamp(mfd::SanitizeZoom(libraryPreviewView_.zoom), kMinStudioZoom, kMaxStudioZoom);
        const float nextZoom =
            std::clamp(currentZoom * std::pow(kWheelZoomStep, wheelDelta), kMinStudioZoom, kMaxStudioZoom);
        if (std::abs(nextZoom - currentZoom) > 0.0001f)
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const mfd::Vec2 mouseLogicalBeforeZoom = viewport.ToLogical(mouse);
            const mfd::Vec2 viewedOffset = mouseLogicalBeforeZoom - libraryPreviewView_.center;
            const float zoomRatio = currentZoom / nextZoom;

            libraryPreviewView_.zoom = nextZoom;
            libraryPreviewView_.center = {
                mouseLogicalBeforeZoom.x - viewedOffset.x * zoomRatio,
                mouseLogicalBeforeZoom.y - viewedOffset.y * zoomRatio};
        }
    }

    if (interactionMode_ == InteractionMode::None && rightMouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        interactionMode_ = InteractionMode::PanPage;
        interactionPrimitiveIndex_ = -1;
        interactionHandleKind_ = PrimitiveHandleKind::None;
        interactionHandleIndex_ = -1;
        ApplyMouseTransform(viewport);
        return;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    std::optional<int> bestPrimitiveIndex = FindNearestLibraryPrimitive(viewport, mouse);
    if (!bestPrimitiveIndex.has_value())
    {
        SelectLibraryReticle(reticle->id);
        return;
    }

    SelectLibraryPrimitive(reticle->id, *bestPrimitiveIndex);
    mfd::Primitive& primitive = reticle->primitives[static_cast<std::size_t>(*bestPrimitiveIndex)];

    auto toScreenPoint = [&](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle->transform));
    };

    interactionPrimitiveIndex_ = *bestPrimitiveIndex;
    interactionStartPrimitive_ = primitive;
    interactionStartMouseLogical_ = viewport.ToLogical(mouse);
    interactionStartMouseReticleLocal_ = InverseTransformPoint(interactionStartMouseLogical_, reticle->transform);
    interactionStartMousePrimitiveLocal_ = InverseTransformPoint(interactionStartMouseReticleLocal_, primitive.transform);
    interactionHandleKind_ = PrimitiveHandleKind::None;
    interactionHandleIndex_ = -1;

    auto matchesHandle = [&](const ImVec2 handlePoint, const float radius = 12.0f)
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        if (matchesHandle(toScreenPoint({circle->radius, 0.0f})))
        {
            PushUndoSnapshot();
            interactionMode_ = InteractionMode::EditPrimitiveHandle;
            interactionHandleKind_ = PrimitiveHandleKind::Radius;
            interactionHandleIndex_ = 0;
            return;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::RectangleCorner;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Radius;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::RectangleCorner;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::DiamondAxis;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
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
                interactionMode_ = InteractionMode::EditPrimitiveHandle;
                interactionHandleKind_ = PrimitiveHandleKind::Point;
                interactionHandleIndex_ = index;
                return;
            }
        }
    }

    const ReticleScreenBounds bounds = ComputePrimitiveScreenBounds(*reticle, primitive, viewport);
    if (bounds.valid &&
        mouse.x >= bounds.min.x - 8.0f && mouse.x <= bounds.max.x + 8.0f &&
        mouse.y >= bounds.min.y - 8.0f && mouse.y <= bounds.max.y + 8.0f)
    {
        PushUndoSnapshot();
        interactionMode_ = InteractionMode::MovePrimitive;
        return;
    }

    interactionPrimitiveIndex_ = -1;
}

void EditorApplication::SelectPage(const int pageIndex)
{
    selection_.kind = SelectionKind::Page;
    selection_.pageIndex = std::clamp(pageIndex, 0, std::max(0, static_cast<int>(loaded_.document.pages.size()) - 1));
    if (mfd::PageDefinition* page = ActivePage(); page != nullptr)
    {
        BootstrapEditorLayersForPage(*page);
    }
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    interactionMode_ = InteractionMode::None;
    interactionPrimitiveIndex_ = -1;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
    ResetPagePreviewView();
}

void EditorApplication::SelectPageReticle(const int pageIndex, const int reticleIndex)
{
    selection_.kind = SelectionKind::PageReticle;
    selection_.pageIndex = pageIndex;
    selection_.pageReticleIndex = reticleIndex;
    selection_.pageReticleIndices = {reticleIndex};
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
}

void EditorApplication::TogglePageReticleSelection(const int pageIndex, const int reticleIndex)
{
    if (selection_.kind != SelectionKind::PageReticle || selection_.pageIndex != pageIndex)
    {
        SelectPageReticle(pageIndex, reticleIndex);
        return;
    }

    auto& indices = selection_.pageReticleIndices;
    auto iterator = std::find(indices.begin(), indices.end(), reticleIndex);
    if (iterator == indices.end())
    {
        indices.push_back(reticleIndex);
        std::sort(indices.begin(), indices.end());
        selection_.pageReticleIndex = reticleIndex;
        return;
    }

    if (indices.size() == 1U)
    {
        SelectPage(pageIndex);
        return;
    }

    indices.erase(iterator);
    if (selection_.pageReticleIndex == reticleIndex)
    {
        selection_.pageReticleIndex = indices.back();
    }
}

void EditorApplication::SelectLibraryReticle(std::string templateId)
{
    selection_.kind = SelectionKind::LibraryReticle;
    selection_.libraryReticleId = std::move(templateId);
    selection_.libraryBrowserReticleId = selection_.libraryReticleId;
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    selection_.primitiveIndex = -1;
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
    ResetLibraryPreviewView();
}

void EditorApplication::SelectLibraryPrimitive(std::string templateId, const int primitiveIndex)
{
    selection_.kind = SelectionKind::LibraryPrimitive;
    selection_.libraryReticleId = std::move(templateId);
    selection_.libraryBrowserReticleId = selection_.libraryReticleId;
    selection_.pageReticleIndex = -1;
    selection_.pageReticleIndices.clear();
    selection_.primitiveIndex = primitiveIndex;
    interactionMode_ = InteractionMode::None;
    interactionReticleIndex_ = -1;
    interactionReticleIndices_.clear();
    interactionStartReticleTransforms_.clear();
    interactionPrimitiveIndex_ = -1;
    interactionHandleIndex_ = -1;
    interactionHandleKind_ = PrimitiveHandleKind::None;
}

mfd::PageDefinition* EditorApplication::ActivePage() noexcept
{
    if (selection_.pageIndex < 0 || selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return nullptr;
    }

    return &loaded_.document.pages[static_cast<std::size_t>(selection_.pageIndex)];
}

const mfd::PageDefinition* EditorApplication::ActivePage() const noexcept
{
    if (selection_.pageIndex < 0 || selection_.pageIndex >= static_cast<int>(loaded_.document.pages.size()))
    {
        return nullptr;
    }

    return &loaded_.document.pages[static_cast<std::size_t>(selection_.pageIndex)];
}

mfd::ReticleGroup* EditorApplication::SelectedPageReticle() noexcept
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        selection_.pageReticleIndex < 0 ||
        selection_.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(selection_.pageReticleIndex)];
}

const mfd::ReticleGroup* EditorApplication::SelectedPageReticle() const noexcept
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr ||
        selection_.pageReticleIndex < 0 ||
        selection_.pageReticleIndex >= static_cast<int>(page->staticReticles.size()))
    {
        return nullptr;
    }

    return &page->staticReticles[static_cast<std::size_t>(selection_.pageReticleIndex)];
}

bool EditorApplication::HasSelectedPageReticle(const int pageIndex, const int reticleIndex) const noexcept
{
    if (selection_.kind != SelectionKind::PageReticle || selection_.pageIndex != pageIndex)
    {
        return false;
    }

    const auto& indices = selection_.pageReticleIndices;
    if (indices.empty())
    {
        return selection_.pageReticleIndex == reticleIndex;
    }

    return std::find(indices.begin(), indices.end(), reticleIndex) != indices.end();
}

std::vector<int> EditorApplication::SelectedPageReticleIndices() const
{
    std::vector<int> indices;
    if (selection_.kind != SelectionKind::PageReticle)
    {
        return indices;
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return indices;
    }

    if (!selection_.pageReticleIndices.empty())
    {
        indices = selection_.pageReticleIndices;
    }
    else if (selection_.pageReticleIndex >= 0)
    {
        indices.push_back(selection_.pageReticleIndex);
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
    return !loaded_.window.sourceFile.empty();
}

void EditorApplication::CopySelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to copy them.", true);
        return;
    }

    pageReticleClipboard_.clear();
    pageReticleClipboard_.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        pageReticleClipboard_.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }

    pageReticlePasteSerial_ = 0;
    if (pageReticleClipboard_.size() == 1U)
    {
        RebuildStatus("Reticle '" + pageReticleClipboard_.front().id + "' copied from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pageReticleClipboard_.size()) + " reticles copied from page '" + page->name + "'.",
            false);
    }
}

void EditorApplication::CutSelectedPageReticles()
{
    mfd::PageDefinition* page = ActivePage();
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (page == nullptr || selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles to cut them.", true);
        return;
    }

    pageReticleClipboard_.clear();
    pageReticleClipboard_.reserve(selectedIndices.size());
    for (const int reticleIndex : selectedIndices)
    {
        pageReticleClipboard_.push_back(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
    }
    pageReticlePasteSerial_ = 0;

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

    SelectPage(selection_.pageIndex);
    if (pageReticleClipboard_.size() == 1U)
    {
        RebuildStatus("Reticle '" + pageReticleClipboard_.front().id + "' cut from page '" + page->name + "'.", false);
    }
    else
    {
        RebuildStatus(
            std::to_string(pageReticleClipboard_.size()) + " reticles cut from page '" + page->name + "'.",
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

    if (pageReticleClipboard_.empty())
    {
        RebuildStatus("No copied page reticle is available yet.", true);
        return;
    }

    PushUndoSnapshot();

    ++pageReticlePasteSerial_;
    const float offset = 0.035f * static_cast<float>(pageReticlePasteSerial_);
    std::vector<int> pastedIndices;
    pastedIndices.reserve(pageReticleClipboard_.size());

    for (const auto& sourceReticle : pageReticleClipboard_)
    {
        mfd::ReticleGroup pastedReticle = sourceReticle;
        const std::string baseId = pastedReticle.id.empty() ? std::string {"reticle"} : pastedReticle.id;
        pastedReticle.id = MakeUniqueReticleId(page->staticReticles, baseId);
        pastedReticle.transform.position.x = std::clamp(pastedReticle.transform.position.x + offset, -1.0f, 1.0f);
        pastedReticle.transform.position.y = std::clamp(pastedReticle.transform.position.y - offset, -1.0f, 1.0f);
        if (!pastedReticle.editor.layerId.empty() && FindEditorLayer(*page, pastedReticle.editor.layerId) == nullptr)
        {
            pastedReticle.editor.layerId = DefaultEditorLayerId(*page);
        }

        page->staticReticles.push_back(std::move(pastedReticle));
        pastedIndices.push_back(static_cast<int>(page->staticReticles.size()) - 1);
    }

    RefreshPageBlinkStateForEditor(*page);
    selection_.kind = SelectionKind::PageReticle;
    selection_.pageReticleIndices = pastedIndices;
    selection_.pageReticleIndex = pastedIndices.empty() ? -1 : pastedIndices.back();

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
    const auto iterator = loaded_.document.reticleLibrary.find(selection_.libraryReticleId);
    return iterator == loaded_.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

const mfd::ReticleGroup* EditorApplication::SelectedLibraryReticle() const noexcept
{
    const auto iterator = loaded_.document.reticleLibrary.find(selection_.libraryReticleId);
    return iterator == loaded_.document.reticleLibrary.end() ? nullptr : &iterator->second;
}

mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() noexcept
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        selection_.primitiveIndex < 0 ||
        selection_.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(selection_.primitiveIndex)];
}

const mfd::Primitive* EditorApplication::SelectedLibraryPrimitive() const noexcept
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr ||
        selection_.primitiveIndex < 0 ||
        selection_.primitiveIndex >= static_cast<int>(reticle->primitives.size()))
    {
        return nullptr;
    }

    return &reticle->primitives[static_cast<std::size_t>(selection_.primitiveIndex)];
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
    if (!tooltipPreviewTextureReady_)
    {
        return;
    }

    mfd::ReticleGroup previewReticle = reticle;
    previewReticle.visible = true;
    const mfd::PageViewState previewView =
        MakeViewFittingBounds(ComputeReticleWorldBounds(previewReticle), kTooltipPreviewWidth, kTooltipPreviewHeight);

    BeginTextureMode(tooltipPreviewTexture_);
    ClearBackground(backgroundColor);
    {
        EnsurePreviewFont();
        mfd::Canvas2D canvas(kTooltipPreviewWidth,
                             kTooltipPreviewHeight,
                             previewView,
                             PreviewTextFont(),
                             backgroundColor,
                             tooltipPreviewTextureStencilReady_);
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
        (ImTextureID)(uintptr_t)tooltipPreviewTexture_.texture.id,
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

    auto includeLogicalPoint = [&](const mfd::Vec2 logicalPoint)
    {
        const ImVec2 screenPoint = viewport.ToScreen(logicalPoint);
        if (!bounds.valid)
        {
            bounds.min = screenPoint;
            bounds.max = screenPoint;
            bounds.valid = true;
            return;
        }

        bounds.min.x = std::min(bounds.min.x, screenPoint.x);
        bounds.min.y = std::min(bounds.min.y, screenPoint.y);
        bounds.max.x = std::max(bounds.max.x, screenPoint.x);
        bounds.max.y = std::max(bounds.max.y, screenPoint.y);
    };

    auto includeTransformedPoint = [&](const mfd::Vec2 localPoint)
    {
        includeLogicalPoint(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle.transform));
    };

    if (!primitive.style.visible)
    {
        return bounds;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        includeTransformedPoint({-halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, -halfHeight});
        includeTransformedPoint({halfWidth, halfHeight});
        includeTransformedPoint({-halfWidth, halfHeight});
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(line->start);
        includeTransformedPoint(line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, -circle->radius});
        includeTransformedPoint({circle->radius, circle->radius});
        includeTransformedPoint({-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, -ring->outerRadius});
        includeTransformedPoint({ring->outerRadius, ring->outerRadius});
        includeTransformedPoint({-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, -rectangle->height * 0.5f});
        includeTransformedPoint({rectangle->width * 0.5f, rectangle->height * 0.5f});
        includeTransformedPoint({-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, -ellipse->height * 0.5f});
        includeTransformedPoint({ellipse->width * 0.5f, ellipse->height * 0.5f});
        includeTransformedPoint({-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({-square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, -square->height * 0.5f});
        includeTransformedPoint({square->width * 0.5f, square->height * 0.5f});
        includeTransformedPoint({-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        includeTransformedPoint({0.0f, diamond->height * 0.5f});
        includeTransformedPoint({diamond->width * 0.5f, 0.0f});
        includeTransformedPoint({0.0f, -diamond->height * 0.5f});
        includeTransformedPoint({-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        includeTransformedPoint(triangle->points[0]);
        includeTransformedPoint(triangle->points[1]);
        includeTransformedPoint(triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            includeTransformedPoint(point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            includeTransformedPoint(point);
        }
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

    const auto distanceToPoint = [&](const ImVec2 point)
    {
        const float dx = point.x - mousePosition.x;
        const float dy = point.y - mousePosition.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    auto distanceToSegment = [&](const ImVec2 a, const ImVec2 b)
    {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float apx = mousePosition.x - a.x;
        const float apy = mousePosition.y - a.y;
        const float lengthSquared = abx * abx + aby * aby;
        if (lengthSquared <= 0.0001f)
        {
            return distanceToPoint(a);
        }

        const float factor = std::clamp((apx * abx + apy * aby) / lengthSquared, 0.0f, 1.0f);
        const ImVec2 projection(a.x + abx * factor, a.y + aby * factor);
        return distanceToPoint(projection);
    };

    float bestDistance = distanceToPoint(bounds.center);

    auto toScreenPoint = [&](const mfd::Vec2 localPoint)
    {
        return viewport.ToScreen(mfd::ApplyTransform(mfd::ApplyTransform(localPoint, primitive.transform), reticle.transform));
    };

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        const ImVec2 tl = toScreenPoint({-halfWidth, -halfHeight});
        const ImVec2 br = toScreenPoint({halfWidth, halfHeight});
        const float minX = std::min(tl.x, br.x);
        const float maxX = std::max(tl.x, br.x);
        const float minY = std::min(tl.y, br.y);
        const float maxY = std::max(tl.y, br.y);
        if (mousePosition.x >= minX - 6.0f && mousePosition.x <= maxX + 6.0f &&
            mousePosition.y >= minY - 6.0f && mousePosition.y <= maxY + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        const ImVec2 tl = toScreenPoint({-halfWidth, -halfHeight});
        const ImVec2 br = toScreenPoint({halfWidth, halfHeight});
        const float minX = std::min(tl.x, br.x);
        const float maxX = std::max(tl.x, br.x);
        const float minY = std::min(tl.y, br.y);
        const float maxY = std::max(tl.y, br.y);
        if (mousePosition.x >= minX - 6.0f && mousePosition.x <= maxX + 6.0f &&
            mousePosition.y >= minY - 6.0f && mousePosition.y <= maxY + 6.0f)
        {
            bestDistance = std::min(bestDistance, 2.0f);
        }
        return bestDistance;
    }

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        return std::min(bestDistance, distanceToSegment(toScreenPoint(line->start), toScreenPoint(line->end)));
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

void EditorApplication::UpdateReticleSelectionFromClick(const ViewportState& viewport, const bool additiveSelection)
{
    if (const auto reticleIndex = FindNearestPageReticle(viewport, ImGui::GetMousePos()); reticleIndex.has_value())
    {
        if (additiveSelection)
        {
            TogglePageReticleSelection(selection_.pageIndex, *reticleIndex);
        }
        else
        {
            SelectPageReticle(selection_.pageIndex, *reticleIndex);
        }
    }
    else if (!additiveSelection)
    {
        SelectPage(selection_.pageIndex);
    }
}

std::vector<int> EditorApplication::CollectPageReticlesAt(const ViewportState& viewport, const ImVec2 mousePosition) const
{
    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        return {};
    }

    struct ReticleHit
    {
        int reticleIndex = -1;
        float distance = std::numeric_limits<float>::max();
        float area = std::numeric_limits<float>::max();
    };

    std::vector<ReticleHit> hits;
    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page->staticReticles.size()); ++reticleIndex)
    {
        const auto& reticle = page->staticReticles[static_cast<std::size_t>(reticleIndex)];
        if (!IsReticleVisibleInEditor(*page, reticle))
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
        if (mouseInsideBounds)
        {
            distance = std::min(distance, 4.0f);
        }

        if (!mouseInsideBounds && distance > 36.0f)
        {
            continue;
        }

        const float area = std::max(1.0f, (bounds.max.x - bounds.min.x) * (bounds.max.y - bounds.min.y));
        hits.push_back(ReticleHit {reticleIndex, distance, area});
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const ReticleHit& lhs, const ReticleHit& rhs)
              {
                  if (std::abs(lhs.distance - rhs.distance) > 0.25f)
                  {
                      return lhs.distance < rhs.distance;
                  }
                  if (std::abs(lhs.area - rhs.area) > 0.5f)
                  {
                      return lhs.area < rhs.area;
                  }
                  return lhs.reticleIndex > rhs.reticleIndex;
              });

    std::vector<int> reticleIndices;
    reticleIndices.reserve(hits.size());
    for (const ReticleHit& hit : hits)
    {
        reticleIndices.push_back(hit.reticleIndex);
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

    struct PrimitiveHit
    {
        PageClipTarget target {};
        float primitiveDistance = std::numeric_limits<float>::max();
        float reticleDistance = std::numeric_limits<float>::max();
    };

    std::vector<PrimitiveHit> hits;
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

            hits.push_back(PrimitiveHit {
                PageClipTarget {reticleIndex, primitiveIndex},
                primitiveDistance,
                reticleDistance});
        }
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const PrimitiveHit& lhs, const PrimitiveHit& rhs)
              {
                  if (std::abs(lhs.primitiveDistance - rhs.primitiveDistance) > 0.25f)
                  {
                      return lhs.primitiveDistance < rhs.primitiveDistance;
                  }
                  if (std::abs(lhs.reticleDistance - rhs.reticleDistance) > 0.25f)
                  {
                      return lhs.reticleDistance < rhs.reticleDistance;
                  }
                  if (lhs.target.reticleIndex != rhs.target.reticleIndex)
                  {
                      return lhs.target.reticleIndex > rhs.target.reticleIndex;
                  }
                  return lhs.target.primitiveIndex > rhs.target.primitiveIndex;
              });

    std::vector<PageClipTarget> targets;
    targets.reserve(hits.size());
    for (const PrimitiveHit& hit : hits)
    {
        targets.push_back(hit.target);
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
    if (interactionMode_ == InteractionMode::PanPage)
    {
        const float scale = viewport.LogicalScale();
        if (!viewport.valid || scale <= 0.0f)
        {
            interactionMode_ = InteractionMode::None;
            interactionReticleIndices_.clear();
            interactionStartReticleTransforms_.clear();
            return;
        }

        mfd::PageViewState* targetView = nullptr;
        if (selection_.kind == SelectionKind::LibraryReticle || selection_.kind == SelectionKind::LibraryPrimitive)
        {
            targetView = &libraryPreviewView_;
        }
        else
        {
            targetView = &pagePreviewView_;
        }

        const float zoom = mfd::SanitizeZoom(targetView->zoom);
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        targetView->center.x -= mouseDelta.x / (scale * zoom);
        targetView->center.y += mouseDelta.y / (scale * zoom);
        return;
    }

    if (interactionReticleIndex_ < 0)
    {
        return;
    }

    if (page == nullptr || interactionReticleIndex_ >= static_cast<int>(page->staticReticles.size()))
    {
        interactionMode_ = InteractionMode::None;
        interactionReticleIndex_ = -1;
        interactionReticleIndices_.clear();
        interactionStartReticleTransforms_.clear();
        return;
    }

    mfd::ReticleGroup& reticle = page->staticReticles[static_cast<std::size_t>(interactionReticleIndex_)];
    const mfd::Vec2 mouseLogical = viewport.ToLogical(ImGui::GetMousePos());

    switch (interactionMode_)
    {
    case InteractionMode::PanPage:
        break;

    case InteractionMode::MoveReticle:
    {
        const mfd::Vec2 translationDelta = mouseLogical - interactionStartMouseLogical_;
        if (!interactionReticleIndices_.empty() &&
            interactionReticleIndices_.size() == interactionStartReticleTransforms_.size())
        {
            // Apply one shared translation delta to the exact transform snapshot captured at drag start.
            for (std::size_t index = 0; index < interactionReticleIndices_.size(); ++index)
            {
                const int movedReticleIndex = interactionReticleIndices_[index];
                if (movedReticleIndex < 0 || movedReticleIndex >= static_cast<int>(page->staticReticles.size()))
                {
                    continue;
                }

                mfd::Transform2D nextTransform = interactionStartReticleTransforms_[index];
                nextTransform.position = nextTransform.position + translationDelta;
                page->staticReticles[static_cast<std::size_t>(movedReticleIndex)].transform = nextTransform;
            }
        }
        else
        {
            reticle.transform.position = interactionStartTransform_.position + translationDelta;
        }
        break;
    }

    case InteractionMode::RotateReticle:
    {
        const mfd::Vec2 interactionPivotLogical =
            mfd::ApplyTransform(interactionStartReticleVisualCenterLocal_, interactionStartTransform_);
        const float currentAngle =
            std::atan2(mouseLogical.y - interactionPivotLogical.y,
                       mouseLogical.x - interactionPivotLogical.x) *
            180.0f / 3.14159265f;
        const float nextRotationDegrees =
            interactionStartTransform_.rotationDegrees + (currentAngle - interactionStartAngleDegrees_);
        reticle.transform = BuildTransformKeepingLocalPointWorldPosition(
            interactionStartTransform_,
            interactionStartReticleVisualCenterLocal_,
            nextRotationDegrees,
            interactionStartTransform_.scale);
        break;
    }

    case InteractionMode::ScaleReticle:
    {
        const ImVec2 mouseScreen = ImGui::GetMousePos();
        const float startDistance = std::max(8.0f, Distance(interactionStartCornerScreen_, interactionStartCenterScreen_));
        const float currentDistance = std::max(4.0f, Distance(mouseScreen, interactionStartCenterScreen_));
        const float factor = std::clamp(currentDistance / startDistance, 0.1f, 10.0f);
        const mfd::Vec2 nextScale {
            std::max(0.05f, interactionStartTransform_.scale.x * factor),
            std::max(0.05f, interactionStartTransform_.scale.y * factor)};
        reticle.transform = BuildTransformKeepingLocalPointWorldPosition(
            interactionStartTransform_,
            interactionStartReticleVisualCenterLocal_,
            interactionStartTransform_.rotationDegrees,
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

    if (showNewPagePopup_)
    {
        ImGui::OpenPopup("Create new page");
        showNewPagePopup_ = false;
    }
    if (showNewWindowPopup_)
    {
        ImGui::OpenPopup("Create new window");
        showNewWindowPopup_ = false;
    }
    if (showAssetFolderPickerPopup_)
    {
        ImGui::OpenPopup("Choose asset folder");
        showAssetFolderPickerPopup_ = false;
    }

    if (showNewLibraryReticlePopup_)
    {
        ImGui::OpenPopup("Create new library reticle");
        showNewLibraryReticlePopup_ = false;
    }

    if (showDuplicateLibraryReticlePopup_)
    {
        ImGui::OpenPopup("Duplicate library reticle");
        showDuplicateLibraryReticlePopup_ = false;
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
        ImGui::InputText("Window file", newWindowDraft_.windowFile.data(), newWindowDraft_.windowFile.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse window folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::WindowFile);
        }
        ShowItemTooltip("Choose the folder that will receive the new window JSON. Prefer the source repo assets folder, not _Exec.");
        ImGui::InputText("Window title", newWindowDraft_.title.data(), newWindowDraft_.title.size());
        int windowSize[2] {newWindowDraft_.width, newWindowDraft_.height};
        if (ImGui::InputInt2("Size (px)", windowSize))
        {
            newWindowDraft_.width = windowSize[0];
            newWindowDraft_.height = windowSize[1];
        }
        int windowPosition[2] {newWindowDraft_.positionX, newWindowDraft_.positionY};
        if (ImGui::InputInt2("Position (px)", windowPosition))
        {
            newWindowDraft_.positionX = windowPosition[0];
            newWindowDraft_.positionY = windowPosition[1];
        }
        ImGui::InputText("Font file (optional)", newWindowDraft_.fontFile.data(), newWindowDraft_.fontFile.size());
        ImGui::InputText("Reticle library folder", newWindowDraft_.reticleLibraryFolder.data(), newWindowDraft_.reticleLibraryFolder.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse reticle folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::ReticleLibraryFolder);
        }
        ShowItemTooltip("Choose where new reticle template JSON files should be saved.");

        ImGui::SeparatorText("Commands UDP (incoming)");
        ImGui::Checkbox("Enable command UDP", &newWindowDraft_.commandUdpEnabled);
        ImGui::InputText("Command address", newWindowDraft_.commandAddress.data(), newWindowDraft_.commandAddress.size());
        ImGui::InputInt("Command port", &newWindowDraft_.commandPort);
        ImGui::InputInt("Command max packet", &newWindowDraft_.commandMaxPacketSize);

        ImGui::SeparatorText("Feedback UDP (outgoing)");
        ImGui::Checkbox("Enable feedback UDP", &newWindowDraft_.feedbackUdpEnabled);
        ImGui::InputText("Feedback address", newWindowDraft_.feedbackAddress.data(), newWindowDraft_.feedbackAddress.size());
        ImGui::InputInt("Feedback port", &newWindowDraft_.feedbackPort);
        ImGui::InputInt("Feedback max packet", &newWindowDraft_.feedbackMaxPacketSize);

        ImGui::SeparatorText("Initial content");
        ImGui::Checkbox("Create one initial page", &newWindowDraft_.createInitialPage);
        if (newWindowDraft_.createInitialPage)
        {
            ImGui::InputText("First page name", newWindowDraft_.firstPageName.data(), newWindowDraft_.firstPageName.size());
            ImGui::InputText("First page title", newWindowDraft_.firstPageTitle.data(), newWindowDraft_.firstPageTitle.size());
            ImGui::InputText("First page file", newWindowDraft_.firstPageFile.data(), newWindowDraft_.firstPageFile.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse page folder..."))
            {
                OpenAssetFolderPicker(AssetFolderPickerTarget::FirstPageFile);
            }
            ShowItemTooltip("Choose the folder that will receive the first page JSON.");
            ImGui::ColorEdit4("First page background", &newWindowDraft_.firstPageBackground.x);
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
        ImGui::InputText("Page name", newPageDraft_.name.data(), newPageDraft_.name.size());
        ShowItemTooltip("Internal page id used in JSON, generated file names and API references.");
        ImGui::InputText("Title", newPageDraft_.title.data(), newPageDraft_.title.size());
        ShowItemTooltip("Optional human-readable title shown in the editor and runtime UI.");
        ImGui::InputText("File", newPageDraft_.fileName.data(), newPageDraft_.fileName.size());
        ShowItemTooltip("JSON file path written for this page. The .json extension is added automatically when missing.");
        ImGui::SameLine();
        if (ImGui::Button("Browse page folder..."))
        {
            OpenAssetFolderPicker(AssetFolderPickerTarget::NewPageFile);
        }
        ShowItemTooltip("Choose the folder that will receive the new page JSON. Prefer the source repo assets folder, not _Exec.");
        ImGui::ColorEdit4("Background", &newPageDraft_.background.x);
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

    DrawAssetFolderPickerPopup();

    if (ImGui::BeginPopupModal("Create new library reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Reticle id", newLibraryReticleDraft_.id.data(), newLibraryReticleDraft_.id.size());
        ShowItemTooltip("Template id used by the shared reticle library.");
        if (ImGui::BeginCombo("Primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)]).c_str()))
        {
            for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
            {
                const bool selected = newLibraryReticleDraft_.primitiveTypeIndex == index;
                if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(), selected))
                {
                    newLibraryReticleDraft_.primitiveTypeIndex = index;
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
                    if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
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
            if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateRadarTrackReticle)) ||
                tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateCircleReticle)) ||
                tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
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
        ImGui::InputText("New reticle id", duplicateLibraryReticleDraft_.id.data(), duplicateLibraryReticleDraft_.id.size());
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

void EditorApplication::DrawAssetFolderPickerPopup()
{
    if (!ImGui::BeginPopupModal("Choose asset folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const auto defaultFolderForTarget = [this]() -> std::filesystem::path
    {
        switch (assetFolderPickerTarget_)
        {
        case AssetFolderPickerTarget::WindowFile:
            return DefaultProjectAssetFolder("assets/windows");
        case AssetFolderPickerTarget::ReticleLibraryFolder:
            return DefaultProjectAssetFolder("assets/reticles");
        case AssetFolderPickerTarget::FirstPageFile:
        case AssetFolderPickerTarget::NewPageFile:
            return DefaultProjectAssetFolder("assets/pages");
        case AssetFolderPickerTarget::None:
        default:
            return DefaultProjectAssetFolder("assets");
        }
    };

    if (assetFolderPickerCurrentFolder_.empty())
    {
        assetFolderPickerCurrentFolder_ = defaultFolderForTarget();
    }
    if (assetFolderPickerCurrentFolder_.is_relative())
    {
        assetFolderPickerCurrentFolder_ = std::filesystem::absolute(assetFolderPickerCurrentFolder_).lexically_normal();
    }

    ImGui::TextWrapped("Choose where the new asset should be written. Keep authored JSON files in the source assets tree, not in _Exec.");
    ImGui::Separator();
    ImGui::TextDisabled("Current folder");
    ImGui::TextWrapped("%s", assetFolderPickerCurrentFolder_.string().c_str());

    if (ImGui::Button("Project assets"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets");
    }
    ImGui::SameLine();
    if (ImGui::Button("Windows"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/windows");
    }
    ImGui::SameLine();
    if (ImGui::Button("Pages"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/pages");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reticles"))
    {
        assetFolderPickerCurrentFolder_ = DefaultProjectAssetFolder("assets/reticles");
    }

    const bool canGoUp = assetFolderPickerCurrentFolder_.has_parent_path() &&
                         assetFolderPickerCurrentFolder_ != assetFolderPickerCurrentFolder_.root_path();
    ImGui::BeginDisabled(!canGoUp);
    if (ImGui::Button("Up"))
    {
        assetFolderPickerCurrentFolder_ = assetFolderPickerCurrentFolder_.parent_path();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::BeginChild("AssetFolderList", ImVec2(560.0f, 260.0f), true);
    std::vector<std::filesystem::path> childFolders;
    std::error_code directoryError;
    if (std::filesystem::is_directory(assetFolderPickerCurrentFolder_, directoryError))
    {
        for (const auto& entry : std::filesystem::directory_iterator(assetFolderPickerCurrentFolder_, directoryError))
        {
            if (entry.is_directory())
            {
                childFolders.push_back(entry.path());
            }
        }
    }
    std::sort(childFolders.begin(), childFolders.end());
    for (const std::filesystem::path& childFolder : childFolders)
    {
        const std::string label = childFolder.filename().string();
        if (ImGui::Selectable(label.c_str(), false))
        {
            assetFolderPickerCurrentFolder_ = childFolder.lexically_normal();
        }
    }
    if (childFolders.empty())
    {
        ImGui::TextDisabled("No subfolders in this directory.");
    }
    ImGui::EndChild();

    const bool blockedFolder = IsExecStagingPath(assetFolderPickerCurrentFolder_);
    if (blockedFolder)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.38f, 0.38f, 1.0f),
            "This folder is inside _Exec. Choose the source assets tree instead.");
    }

    if (AccentButton("Use this folder"))
    {
        if (!blockedFolder)
        {
            switch (assetFolderPickerTarget_)
            {
            case AssetFolderPickerTarget::WindowFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newWindowDraft_.windowFile.data()),
                    "new_window.json");
                CopyTextBuffer(newWindowDraft_.windowFile, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::ReticleLibraryFolder:
                CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, assetFolderPickerCurrentFolder_.lexically_normal().string());
                break;

            case AssetFolderPickerTarget::FirstPageFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newWindowDraft_.firstPageFile.data()),
                    "page1.json");
                CopyTextBuffer(newWindowDraft_.firstPageFile, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::NewPageFile:
            {
                const std::filesystem::path fileName = JsonFileNameOrFallback(
                    std::filesystem::path(newPageDraft_.fileName.data()),
                    "new_page.json");
                CopyTextBuffer(newPageDraft_.fileName, (assetFolderPickerCurrentFolder_ / fileName).lexically_normal().string());
                break;
            }

            case AssetFolderPickerTarget::None:
            default:
                break;
            }

            assetFolderPickerTarget_ = AssetFolderPickerTarget::None;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        assetFolderPickerTarget_ = AssetFolderPickerTarget::None;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::SeedNewWindowAssetDraftPaths()
{
    const std::filesystem::path windowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    if (windowFile.empty() || !windowFile.is_absolute() || IsExecStagingPath(windowFile))
    {
        CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/new_window.json").string());
    }

    const std::filesystem::path resolvedWindowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    const std::filesystem::path reticleFolder = std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal();
    if (reticleFolder.empty() || !reticleFolder.is_absolute() || IsExecStagingPath(reticleFolder))
    {
        CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultSiblingAssetFile(resolvedWindowFile, "reticles", "").string());
    }

    const std::filesystem::path firstPageFile = std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal();
    if (firstPageFile.empty() || !firstPageFile.is_absolute() || IsExecStagingPath(firstPageFile))
    {
        CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultSiblingAssetFile(resolvedWindowFile, "pages", "page1.json").string());
    }
}

void EditorApplication::SeedNewPageAssetDraftPath()
{
    const std::filesystem::path pageFile = std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal();
    if (!pageFile.empty() && pageFile.is_absolute() && !IsExecStagingPath(pageFile))
    {
        return;
    }

    const std::filesystem::path windowFile = loaded_.window.sourceFile.empty()
                                                 ? DefaultProjectAssetFolder("assets/windows/new_window.json")
                                                 : loaded_.window.sourceFile;
    const std::filesystem::path defaultFileName = JsonFileNameOrFallback(pageFile, "new_page.json");
    CopyTextBuffer(newPageDraft_.fileName, DefaultSiblingAssetFile(windowFile, "pages", defaultFileName.string()).string());
}

void EditorApplication::PrepareTutorialStep()
{
    using editor::tutorial::TutorialStepId;

    tutorial_->ClampStepIndex();
    tutorial_->ResetPhase();
    tutorial_->ClearFocusLayer();
    tutorial_->ResetFileReviewView();

    const auto setPrimitiveDraft = [&](const mfd::PrimitiveType primitiveType)
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (kPrimitiveTypes[static_cast<std::size_t>(index)] == primitiveType)
            {
                newLibraryReticleDraft_.primitiveTypeIndex = index;
                break;
            }
        }
    };

    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(tutorial_->StepIndex())];
    if (editor::tutorial::IsFileReviewStep(step))
    {
        return;
    }

    switch (tutorial_->StepIndex())
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        tutorial_->ClearTrackedReticle();
        CopyTextBuffer(newWindowDraft_.windowFile, DefaultProjectAssetFolder("assets/windows/mfd_tutorial.json").string());
        CopyTextBuffer(newWindowDraft_.title, "MFD Tutorial");
        newWindowDraft_.width = 480;
        newWindowDraft_.height = 480;
        newWindowDraft_.positionX = 120;
        newWindowDraft_.positionY = 80;
        CopyTextBuffer(newWindowDraft_.fontFile, "");
        CopyTextBuffer(newWindowDraft_.reticleLibraryFolder, DefaultProjectAssetFolder("assets/reticles").string());
        newWindowDraft_.commandUdpEnabled = true;
        CopyTextBuffer(newWindowDraft_.commandAddress, "127.0.0.1");
        newWindowDraft_.commandPort = 49000;
        newWindowDraft_.commandMaxPacketSize = 65507;
        newWindowDraft_.feedbackUdpEnabled = true;
        CopyTextBuffer(newWindowDraft_.feedbackAddress, "127.0.0.1");
        newWindowDraft_.feedbackPort = 49001;
        newWindowDraft_.feedbackMaxPacketSize = 65507;
        newWindowDraft_.createInitialPage = false;
        CopyTextBuffer(newWindowDraft_.firstPageName, "Page1");
        CopyTextBuffer(newWindowDraft_.firstPageTitle, "Page 1");
        CopyTextBuffer(newWindowDraft_.firstPageFile, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page1.json").string());
        newWindowDraft_.firstPageBackground = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, "mfd_tutorial_radar_track");
        setPrimitiveDraft(mfd::PrimitiveType::Diamond);
        break;
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, "mfd_tutorial_circle");
        setPrimitiveDraft(mfd::PrimitiveType::Circle);
        break;
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        CopyTextBuffer(newLibraryReticleDraft_.id, kTutorialStrobeCursorTemplateId);
        setPrimitiveDraft(mfd::PrimitiveType::Line);
        break;
    case static_cast<int>(TutorialStepId::CreatePage1):
        CopyTextBuffer(newPageDraft_.name, "Page1");
        CopyTextBuffer(newPageDraft_.title, "Page 1");
        CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page1.json").string());
        newPageDraft_.background = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::AssignPage1StrobeTemplate):
        if (const int pageIndex = FindPageIndexByName(loaded_, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
        }
        break;
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
    {
        if (const int pageIndex = FindPageIndexByName(loaded_, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
        }
        if (loaded_.document.reticleLibrary.find("mfd_tutorial_circle") != loaded_.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_circle");
        }
        break;
    }
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
    {
        if (const int pageIndex = FindPageIndexByName(loaded_, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            if (!tutorial_->TrackedReticleId().empty())
            {
                const mfd::PageDefinition& page = loaded_.document.pages[static_cast<std::size_t>(pageIndex)];
                if (const int reticleIndex = FindPageReticleIndexById(page, tutorial_->TrackedReticleId()); reticleIndex >= 0)
                {
                    SelectPageReticle(pageIndex, reticleIndex);
                }
            }
        }
        break;
    }
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        if (const int pageIndex = FindPageIndexByName(loaded_, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
        }
        break;
    case static_cast<int>(TutorialStepId::CreatePage2):
        CopyTextBuffer(newPageDraft_.name, "Page2");
        CopyTextBuffer(newPageDraft_.title, "Page 2");
        CopyTextBuffer(newPageDraft_.fileName, DefaultProjectAssetFolder("assets/pages/mfd_tutorial_page2.json").string());
        newPageDraft_.background = ImVec4(0.04f, 0.08f, 0.14f, 1.0f);
        break;
    default:
        break;
    }
}

bool EditorApplication::CreateNewWindow()
{
    const std::filesystem::path windowFile = std::filesystem::path(newWindowDraft_.windowFile.data()).lexically_normal();
    if (windowFile.empty())
    {
        RebuildStatus("Window file cannot be empty.", true);
        return false;
    }
    if (IsExecStagingPath(windowFile))
    {
        RebuildStatus("Choose a source assets folder for the window JSON, not a staged _Exec folder.", true);
        return false;
    }

    const std::string windowTitle = newWindowDraft_.title.data();
    if (windowTitle.empty())
    {
        RebuildStatus("Window title cannot be empty.", true);
        return false;
    }

    if (newWindowDraft_.width <= 0 || newWindowDraft_.height <= 0)
    {
        RebuildStatus("Window size must be strictly positive.", true);
        return false;
    }

    const std::filesystem::path windowBaseFolder = windowFile.parent_path();

    mfd::LoadedWindowConfiguration next {};
    next.window.sourceFile = windowFile;
    next.window.title = windowTitle;
    next.window.width = std::max(1, newWindowDraft_.width);
    next.window.height = std::max(1, newWindowDraft_.height);
    next.window.positionX = newWindowDraft_.positionX;
    next.window.positionY = newWindowDraft_.positionY;
    next.window.targetFps = 60;

    const std::filesystem::path fontPath = std::filesystem::path(newWindowDraft_.fontFile.data()).lexically_normal();
    if (!fontPath.empty())
    {
        next.window.fontFile = fontPath.is_relative() ? (windowBaseFolder / fontPath).lexically_normal() : fontPath;
    }

    const std::filesystem::path reticleFolder =
        std::filesystem::path(newWindowDraft_.reticleLibraryFolder.data()).lexically_normal();
    const std::filesystem::path effectiveReticleFolder =
        reticleFolder.empty() ? DefaultSiblingAssetFile(windowFile, "reticles", "") : reticleFolder;
    if (IsExecStagingPath(effectiveReticleFolder))
    {
        RebuildStatus("Choose a source assets folder for the reticle library, not a staged _Exec folder.", true);
        return false;
    }
    next.window.reticleLibraryFolder = effectiveReticleFolder.is_relative()
                                           ? (windowBaseFolder / effectiveReticleFolder).lexically_normal()
                                           : effectiveReticleFolder;

    mfd::WindowUdpCommandTransport commandUdp {};
    commandUdp.enabled = newWindowDraft_.commandUdpEnabled;
    commandUdp.address = newWindowDraft_.commandAddress.data();
    commandUdp.port = std::max(0, newWindowDraft_.commandPort);
    commandUdp.maxPacketSize = std::max(512, newWindowDraft_.commandMaxPacketSize);
    next.window.commandTransports.udp = commandUdp;

    mfd::WindowUdpFeedbackTransport feedbackUdp {};
    feedbackUdp.enabled = newWindowDraft_.feedbackUdpEnabled;
    feedbackUdp.address = newWindowDraft_.feedbackAddress.data();
    feedbackUdp.port = std::max(0, newWindowDraft_.feedbackPort);
    feedbackUdp.maxPacketSize = std::max(512, newWindowDraft_.feedbackMaxPacketSize);
    next.window.feedbackTransports.udp = feedbackUdp;

    editor::EditorFileLayout nextFiles {};
    if (newWindowDraft_.createInitialPage)
    {
        const std::string pageName = newWindowDraft_.firstPageName.data();
        if (pageName.empty())
        {
            RebuildStatus("Initial page name cannot be empty.", true);
            return false;
        }

        mfd::PageDefinition page {};
        page.name = pageName;
        page.normalizedName = mfd::NormalizePageName(pageName);
        page.title = newWindowDraft_.firstPageTitle.data();
        page.backgroundColor = ToColorRgba(newWindowDraft_.firstPageBackground);
        page.defaultPage = true;
        page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});
        next.document.pages.push_back(page);

        std::filesystem::path pageFile = std::filesystem::path(newWindowDraft_.firstPageFile.data()).lexically_normal();
        if (pageFile.empty())
        {
            pageFile = editor::DefaultPageFilePath(windowFile, page.name);
        }
        if (pageFile.is_relative())
        {
            pageFile = (windowBaseFolder / pageFile).lexically_normal();
        }
        if (IsExecStagingPath(pageFile))
        {
            RebuildStatus("Choose a source assets folder for the first page JSON, not a staged _Exec folder.", true);
            return false;
        }
        nextFiles.pageFiles.push_back(pageFile);
        next.window.pageFiles = nextFiles.pageFiles;
    }

    PushUndoSnapshot();
    loaded_ = std::move(next);
    files_ = std::move(nextFiles);
    windowFile_ = loaded_.window.sourceFile;
    ApplyPreviewFontFile(loaded_.window.fontFile);
    SelectPage(DefaultPageIndex(loaded_.document.pages));
    RebuildStatus("New window draft created. Use File > Save to write JSON files on disk.", false);
    return true;
}

bool EditorApplication::CreateNewPage()
{
    const std::string pageName = newPageDraft_.name.data();
    if (pageName.empty())
    {
        RebuildStatus("Page name cannot be empty.", true);
        return false;
    }

    std::filesystem::path pageFile = std::filesystem::path(newPageDraft_.fileName.data()).lexically_normal();
    if (pageFile.empty())
    {
        pageFile = editor::DefaultPageFilePath(loaded_.window.sourceFile, pageName);
    }
    if (pageFile.is_relative())
    {
        pageFile = (loaded_.window.sourceFile.parent_path() / pageFile).lexically_normal();
    }
    if (pageFile.extension().empty())
    {
        pageFile += ".json";
    }
    if (IsExecStagingPath(pageFile))
    {
        RebuildStatus("Choose a source assets folder for the page JSON, not a staged _Exec folder.", true);
        return false;
    }

    PushUndoSnapshot();

    mfd::PageDefinition page;
    page.name = pageName;
    page.normalizedName = mfd::NormalizePageName(pageName);
    page.title = newPageDraft_.title.data();
    page.backgroundColor = ToColorRgba(newPageDraft_.background);
    page.editor.layers.push_back(mfd::EditorLayerDefinition {"layer", true});

    loaded_.document.pages.push_back(page);
    files_.pageFiles.push_back(pageFile.lexically_normal());
    loaded_.window.pageFiles = files_.pageFiles;

    SelectPage(static_cast<int>(loaded_.document.pages.size()) - 1);
    RebuildStatus("Page '" + page.name + "' created.", false);
    return true;
}

bool EditorApplication::CreateNewLibraryReticleFromPrimitive()
{
    using editor::tutorial::TutorialStepId;

    const std::string reticleId = newLibraryReticleDraft_.id.data();
    if (reticleId.empty())
    {
        RebuildStatus("Library reticle id cannot be empty.", true);
        return false;
    }

    const mfd::PrimitiveType primitiveType =
        kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)];
    if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)))
    {
        if (reticleId != kTutorialStrobeCursorTemplateId)
        {
            RebuildStatus("Tutorial: keep the reticle id set to 'mfd_tutorial_strobe_cursor'.", true);
            return false;
        }

        if (primitiveType != mfd::PrimitiveType::Line)
        {
            RebuildStatus("Tutorial: create the strobe cursor from a Line primitive.", true);
            return false;
        }
    }

    PushUndoSnapshot();
    mfd::ReticleGroup reticle = MakePrimitiveReticle(
        reticleId,
        primitiveType);
    if (tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateStrobeCursorReticle)) &&
        reticle.id == kTutorialStrobeCursorTemplateId &&
        !reticle.primitives.empty())
    {
        (void)ConfigureTutorialStrobeLinePrimitive(reticle.primitives.front(), false);
    }
    loaded_.document.reticleLibrary[reticle.id] = reticle;
    files_.templateFiles[reticle.id] = editor::DefaultTemplateFilePath(loaded_.window.reticleLibraryFolder, reticle.id);
    SelectLibraryReticle(reticle.id);
    RebuildStatus("Library reticle '" + reticle.id + "' created.", false);
    return true;
}

void EditorApplication::DuplicateSelectedLibraryReticle()
{
    const mfd::ReticleGroup* source = SelectedLibraryReticle();
    if (source == nullptr)
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    const std::string newId = duplicateLibraryReticleDraft_.id.data();
    if (newId.empty())
    {
        RebuildStatus("Duplicate id cannot be empty.", true);
        return;
    }

    PushUndoSnapshot();
    mfd::ReticleGroup copy = *source;
    copy.id = newId;
    copy.sourceTemplateId.clear();
    loaded_.document.reticleLibrary[newId] = copy;
    files_.templateFiles[newId] = editor::DefaultTemplateFilePath(loaded_.window.reticleLibraryFolder, newId);
    SelectLibraryReticle(newId);
    RebuildStatus("Library reticle duplicated as '" + newId + "'.", false);
}

bool EditorApplication::CreatePageReticleInstanceFromTemplate(const std::string_view templateId, const mfd::Vec2 position)
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select a page before dropping a library reticle.", true);
        return false;
    }

    const auto iterator = loaded_.document.reticleLibrary.find(std::string(templateId));
    if (iterator == loaded_.document.reticleLibrary.end())
    {
        RebuildStatus("Unknown library reticle: " + std::string(templateId), true);
        return false;
    }

    PushUndoSnapshot();

    const std::string instanceId = MakeUniqueReticleId(page->staticReticles, templateId);
    mfd::ReticleGroup instance = mfd::InstantiateReticle(
        iterator->second,
        instanceId,
        mfd::Transform2D {position, 0.0f, {1.0f, 1.0f}},
        {});
    instance.visible = true;
    instance.editor.layerId = DefaultEditorLayerId(*page);

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
    SelectPageReticle(selection_.pageIndex, static_cast<int>(page->staticReticles.size()) - 1);
    RebuildStatus("Reticle '" + std::string(templateId) + "' dropped on page '" + page->name + "'.", false);
    return true;
}

mfd::ReticleGroup EditorApplication::MakePrimitiveReticle(std::string id, const mfd::PrimitiveType primitiveType)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);

    mfd::Primitive primitive;
    primitive.id = "primitive_01";
    primitive.type = primitiveType;

    switch (primitiveType)
    {
    case mfd::PrimitiveType::Text:
        primitive.geometry = mfd::TextGeometry {"TXT", 0.06f};
        break;
    case mfd::PrimitiveType::Time:
        primitive.geometry = mfd::TimeGeometry {"%H:%M:%S", false, 0.06f};
        break;
    case mfd::PrimitiveType::Line:
        primitive.geometry = mfd::LineGeometry {{-0.10f, 0.0f}, {0.10f, 0.0f}};
        break;
    case mfd::PrimitiveType::Circle:
        primitive.geometry = mfd::CircleGeometry {0.10f};
        break;
    case mfd::PrimitiveType::Rectangle:
        primitive.geometry = mfd::RectangleGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Ellipse:
        primitive.geometry = mfd::EllipseGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Square:
        primitive.geometry = mfd::SquareGeometry {0.20f, 0.20f};
        break;
    case mfd::PrimitiveType::Diamond:
        primitive.geometry = mfd::DiamondGeometry {0.22f, 0.22f};
        break;
    case mfd::PrimitiveType::Triangle:
        primitive.geometry = mfd::TriangleGeometry {{{{-0.10f, -0.08f}, {0.10f, -0.08f}, {0.0f, 0.12f}}}};
        break;
    case mfd::PrimitiveType::Polyline:
        primitive.geometry = mfd::PolylineGeometry {{{{-0.10f, -0.10f}, {0.0f, 0.12f}, {0.10f, -0.10f}}}, false};
        break;
    case mfd::PrimitiveType::Bezier:
        primitive.geometry = mfd::BezierGeometry {{{{-0.12f, -0.12f}, {-0.04f, 0.12f}, {0.04f, -0.12f}, {0.12f, 0.12f}}}, 32};
        break;
    }

    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::PageStrobeDefinition EditorApplication::MakePageStrobeFromTemplate(
    const mfd::PageDefinition& page,
    const mfd::ReticleGroup& templ,
    const std::optional<mfd::PageStrobeDefinition>& previousStrobe)
{
    const std::string baseId =
        previousStrobe.has_value() && !previousStrobe->reticle.id.empty()
            ? previousStrobe->reticle.id
            : (templ.id.empty() ? std::string {"strobe"} : templ.id + "_strobe");

    mfd::PageStrobeDefinition strobe;
    strobe.reticle = mfd::InstantiateReticle(templ, MakeUniqueReticleId(page.staticReticles, baseId));
    strobe.reticle.visible = true;

    if (previousStrobe.has_value())
    {
        strobe.reticle.visible = previousStrobe->reticle.visible;
        strobe.reticle.transform = previousStrobe->reticle.transform;
        strobe.reticle.overrides = previousStrobe->reticle.overrides;
        strobe.reticle.blink = previousStrobe->reticle.blink;
        strobe.reticle.clipping = previousStrobe->reticle.clipping;
        strobe.reticle.editor = previousStrobe->reticle.editor;
        strobe.capture = previousStrobe->capture;
        strobe.magnet = previousStrobe->magnet;
    }

    return strobe;
}

std::string EditorApplication::MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, const std::string_view baseId)
{
    std::string candidate = std::string(baseId);
    int suffix = 1;

    auto exists = [&](const std::string& id)
    {
        return std::any_of(groups.begin(),
                           groups.end(),
                           [&id](const mfd::ReticleGroup& reticle)
                           {
                               return reticle.id == id;
                           });
    };

    while (exists(candidate))
    {
        candidate = std::string(baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

std::string EditorApplication::MakeUniqueLayerId(const mfd::PageDefinition& page, const std::string_view baseId)
{
    std::string candidate = baseId.empty() ? std::string {"layer"} : std::string(baseId);
    int suffix = 2;

    auto exists = [&page](const std::string& id)
    {
        return std::any_of(page.editor.layers.begin(),
                           page.editor.layers.end(),
                           [&id](const mfd::EditorLayerDefinition& layer)
                           {
                               return layer.id == id;
                           });
    };

    while (exists(candidate))
    {
        candidate = std::string(baseId.empty() ? "layer" : baseId) + "_" + std::to_string(suffix++);
    }

    return candidate;
}

void EditorApplication::DrawPageInspector()
{
    mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        ImGui::TextDisabled("No page selected.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Page");
    ImGui::TextDisabled("Edit the page and work directly in the preview.");

    if (ImGui::Button("Delete page"))
    {
        DeleteActivePage();
        return;
    }
    ShowItemTooltip("Delete the currently selected page from the window definition.");

    ImGui::SameLine();
    ImGui::TextDisabled("Shortcut: Suppr when the page is selected");

    const bool canPasteReticles = !pageReticleClipboard_.empty();
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

    const bool nameChanged = ImGui::InputText("Name", name.data(), name.size());
    ShowItemTooltip("Internal page id used in JSON and API references.");
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (nameChanged)
    {
        page->name = name.data();
        page->normalizedName = mfd::NormalizePageName(page->name);
    }

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
        pagePreviewView_ = page->view;
        pagePreviewView_.zoom = mfd::SanitizeZoom(pagePreviewView_.zoom);
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
        pagePreviewView_ = page->view;
    }

    bool defaultPage = page->defaultPage;
    if (ImGui::Checkbox("Default page for this window", &defaultPage))
    {
        PushUndoSnapshot();
        if (defaultPage)
        {
            for (auto& candidate : loaded_.document.pages)
            {
                candidate.defaultPage = false;
            }
        }

        page->defaultPage = defaultPage;
    }
    ShowItemTooltip("Mark this page as the default page opened by the runtime for this window.");

    ImGui::TextDisabled("If no page is marked default, the runtime opens the first page in the window JSON.");

    DrawPageBlinkInspector(*page);
    DrawPageStrobeInspector(*page);
    DrawPageLayerInspector(*page);

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

void EditorApplication::DrawPageStrobeInspector(mfd::PageDefinition& page)
{
    using editor::tutorial::TutorialStepId;

    constexpr std::string_view kTutorialStrobeTargetId = "page_strobe_template";
    constexpr std::string_view kTutorialStrobeReticleId = "tutorial_strobe";

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Strobe");
    ImGui::TextDisabled("Assign one library reticle template as the optional page strobe.");
    ImGui::TextDisabled("Each page can expose at most one strobe.");

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded_.document.reticleLibrary.size());
    for (const auto& [templateId, reticle] : loaded_.document.reticleLibrary)
    {
        (void)reticle;
        templateIds.push_back(templateId);
    }
    std::sort(templateIds.begin(), templateIds.end());

    const std::string currentTemplateId =
        page.strobe.has_value() ? page.strobe->reticle.sourceTemplateId : std::string {};
    const char* currentTemplateLabel =
        !page.strobe.has_value() ? "None" : (currentTemplateId.empty() ? "<custom strobe>" : currentTemplateId.c_str());

    if (ImGui::BeginCombo("Strobe template", currentTemplateLabel))
    {
        const bool noneSelected = !page.strobe.has_value();
        if (ImGui::Selectable("None", noneSelected) && page.strobe.has_value())
        {
            PushUndoSnapshot();
            page.strobe.reset();
            RebuildStatus("Strobe removed from page '" + page.name + "'.", false);
        }
        if (noneSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const std::string& templateId : templateIds)
        {
            const bool selected = page.strobe.has_value() && currentTemplateId == templateId;
            if (ImGui::Selectable(templateId.c_str(), selected) && !selected)
            {
                if (tutorial_->MatchesTarget(kTutorialStrobeTargetId) && templateId != kTutorialStrobeCursorTemplateId)
                {
                    RebuildStatus("Tutorial: choose 'mfd_tutorial_strobe_cursor' as the Page1 strobe template.", true);
                    continue;
                }

                const auto iterator = loaded_.document.reticleLibrary.find(templateId);
                if (iterator != loaded_.document.reticleLibrary.end())
                {
                    PushUndoSnapshot();
                    page.strobe = MakePageStrobeFromTemplate(page, iterator->second, page.strobe);
                    if (tutorial_->MatchesTarget(kTutorialStrobeTargetId))
                    {
                        page.strobe->reticle.id = std::string(kTutorialStrobeReticleId);
                    }
                    RefreshBlinkBindingForEditor(page, page.strobe->reticle.blink);
                    RebuildStatus("Strobe template '" + templateId + "' assigned to page '" + page.name + "'.", false);
                    if (tutorial_->MatchesTarget(kTutorialStrobeTargetId))
                    {
                        tutorial_->CompleteStep();
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
    ShowItemTooltip("Choose which shared reticle template is instantiated as the page strobe.");
    tutorial_->DrawHalo(
        kTutorialStrobeTargetId.data(),
        "Choose mfd_tutorial_strobe_cursor",
        "Assign the cross cursor as the real Page1 strobe before the tutorial client starts driving it.");

    ImGui::BeginDisabled(!page.strobe.has_value());
    if (ImGui::Button("Remove strobe") && page.strobe.has_value())
    {
        PushUndoSnapshot();
        page.strobe.reset();
        RebuildStatus("Strobe removed from page '" + page.name + "'.", false);
    }
    ImGui::EndDisabled();
    ShowItemTooltip("Remove the optional strobe definition from this page.");

    if (!page.strobe.has_value())
    {
        if (templateIds.empty())
        {
            ImGui::TextDisabled("No library reticle is available yet. Create one first.");
        }
        else
        {
            ImGui::TextDisabled("No strobe assigned to this page.");
        }
        return;
    }

    ImGui::TextDisabled("Strobe id: %s", page.strobe->reticle.id.c_str());
    if (!currentTemplateId.empty())
    {
        ImGui::TextDisabled("Source template: %s", currentTemplateId.c_str());
    }

    const char* captureShapeLabel =
        page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle ? "Circle" : "Rectangle";
    if (ImGui::BeginCombo("Capture shape", captureShapeLabel))
    {
        const bool circleSelected = page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle;
        if (ImGui::Selectable("Circle", circleSelected) && !circleSelected)
        {
            PushUndoSnapshot();
            page.strobe->capture.shape = mfd::StrobeCaptureShape::Circle;
        }
        if (circleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        const bool rectangleSelected = page.strobe->capture.shape == mfd::StrobeCaptureShape::Rectangle;
        if (ImGui::Selectable("Rectangle", rectangleSelected) && !rectangleSelected)
        {
            PushUndoSnapshot();
            page.strobe->capture.shape = mfd::StrobeCaptureShape::Rectangle;
        }
        if (rectangleSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the capture area shape used by the page strobe.");

    if (page.strobe->capture.shape == mfd::StrobeCaptureShape::Circle)
    {
        if (ImGui::DragFloat("Capture radius", &page.strobe->capture.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->capture.radius = std::max(0.001f, page.strobe->capture.radius);
        }
        ShowItemTooltip("Radius of the circular capture area around the strobe.");
    }
    else
    {
        if (ImGui::DragFloat2("Capture size", &page.strobe->capture.size.x, 0.002f, 0.001f, 2.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->capture.size.x = std::max(0.001f, page.strobe->capture.size.x);
            page.strobe->capture.size.y = std::max(0.001f, page.strobe->capture.size.y);
        }
        ShowItemTooltip("Width and height of the rectangular capture area around the strobe.");
    }

    bool magnetEnabled = page.strobe->magnet.enabled;
    if (ImGui::Checkbox("Magnet enabled", &magnetEnabled))
    {
        PushUndoSnapshot();
        page.strobe->magnet.enabled = magnetEnabled;
    }
    ShowItemTooltip("Enable attraction toward nearby dynamic reticles when the strobe moves.");

    if (page.strobe->magnet.enabled)
    {
        if (ImGui::DragFloat("Magnet radius", &page.strobe->magnet.radius, 0.002f, 0.001f, 1.0f, "%.4f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->magnet.radius = std::max(0.001f, page.strobe->magnet.radius);
        }
        ShowItemTooltip("Maximum distance used to snap the strobe toward nearby targets.");

        if (ImGui::DragFloat("Magnet strength", &page.strobe->magnet.strength, 0.01f, 0.0f, 1.0f, "%.2f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            page.strobe->magnet.strength = std::clamp(page.strobe->magnet.strength, 0.0f, 1.0f);
        }
        ShowItemTooltip("Blend factor applied when the strobe is attracted toward one target.");
    }
}

void EditorApplication::DrawPageLayerInspector(mfd::PageDefinition& page)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.82f, 0.73f, 0.94f, 1.0f), "Editor layers");
    ImGui::TextDisabled("Layers only affect the editor preview. Runtime rendering ignores them.");

    if (AccentButton("Add layer"))
    {
        const bool tutorialAddLayerMatched = tutorial_->MatchesTarget("inspector_add_layer");
        PushUndoSnapshot();
        page.editor.layers.push_back(mfd::EditorLayerDefinition {MakeUniqueLayerId(page, "layer"), true});
        if (tutorialAddLayerMatched)
        {
            tutorial_->SetFocusLayerId(page.editor.layers.back().id);
            tutorial_->AdvancePhase();
        }
        RebuildStatus("Editor layer added to page '" + page.name + "'.", false);
    }
    ShowItemTooltip("Create an editor-only visibility layer to group reticles while authoring.");
    tutorial_->DrawHalo(
        "inspector_add_layer",
        "Click Add layer",
        "Create one extra editor-only layer so the tutorial can show how authoring visibility works.");

    if (page.editor.layers.empty())
    {
        ImGui::TextDisabled("No editor layer yet. Reticles without layer always stay visible.");
        return;
    }

    for (std::size_t index = 0; index < page.editor.layers.size(); ++index)
    {
        mfd::EditorLayerDefinition& layer = page.editor.layers[index];
        const std::size_t assignedReticles = CountEditorLayerAssignments(page, layer.id);

        ImGui::PushID(static_cast<int>(index));
        ImGui::Spacing();
        ImGui::Separator();

        bool visible = layer.visible;
        if (ImGui::Checkbox("Visible", &visible))
        {
            PushUndoSnapshot();
            layer.visible = visible;
            if (tutorial_->MatchesTarget("inspector_layer_visibility") &&
                layer.id == tutorial_->FocusLayerId() &&
                !layer.visible)
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
                "Hide the layer you just created to confirm that editor layers only affect authoring visibility.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%zu reticle%s", assignedReticles, assignedReticles == 1U ? "" : "s");

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
            const bool duplicateId = std::any_of(
                page.editor.layers.begin(),
                page.editor.layers.end(),
                [&layer, &nextId](const mfd::EditorLayerDefinition& candidate)
                {
                    return &candidate != &layer && candidate.id == nextId;
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
                RenameEditorLayerReferences(page, previousId, layer.id);
            }
        }

        if (ImGui::Button("Remove layer"))
        {
            const std::string removedLayerId = layer.id;
            const std::size_t clearedReticles = CountEditorLayerAssignments(page, removedLayerId);

            PushUndoSnapshot();
            ClearEditorLayerReferences(page, removedLayerId);
            page.editor.layers.erase(page.editor.layers.begin() + static_cast<std::ptrdiff_t>(index));

            std::string status = "Editor layer '" + removedLayerId + "' removed from page '" + page.name + "'.";
            if (clearedReticles > 0U)
            {
                status += " Cleared " + std::to_string(clearedReticles) + " reticle assignment(s).";
            }
            RebuildStatus(status, false);

            ImGui::PopID();
            break;
        }
        ShowItemTooltip("Delete this editor-only layer and clear its assignments.");

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
        ImGui::BeginDisabled(pageReticleClipboard_.empty());
        if (ImGui::Button("Paste copies"))
        {
            PasteCopiedPageReticles();
            ImGui::EndDisabled();
            return;
        }
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        ImGui::EndDisabled();

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
    ImGui::Text("Reticle id: %s", reticle->id.c_str());
    if (!reticle->sourceTemplateId.empty())
    {
        ImGui::TextDisabled("Template: %s", reticle->sourceTemplateId.c_str());
    }
    ImGui::TextDisabled("Move inside the frame, rotate with the blue handle, scale with the corner handles.");
    ImGui::Separator();

    const int reticleIndex = selection_.pageReticleIndex;
    const int lastReticleIndex = static_cast<int>(page->staticReticles.size()) - 1;
    auto moveSelectedReticleTo = [&](int targetIndex, const char* action)
    {
        if (reticleIndex < 0 ||
            reticleIndex >= static_cast<int>(page->staticReticles.size()) ||
            targetIndex < 0 ||
            targetIndex >= static_cast<int>(page->staticReticles.size()) ||
            targetIndex == reticleIndex)
        {
            return;
        }

        PushUndoSnapshot();
        const std::string movedReticleId = reticle->id;

        auto movedReticle =
            std::move(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);

        const int insertionIndex =
            std::clamp(targetIndex, 0, static_cast<int>(page->staticReticles.size()));

        page->staticReticles.insert(page->staticReticles.begin() + insertionIndex, std::move(movedReticle));
        SelectPageReticle(selection_.pageIndex, insertionIndex);
        RebuildStatus("Reticle '" + movedReticleId + "' moved " + action + " on page '" + page->name + "'.", false);
    };

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
    ImGui::BeginDisabled(pageReticleClipboard_.empty());
    if (ImGui::Button("Paste copies"))
    {
        PasteCopiedPageReticles();
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Paste copied page reticles onto the active page.");
    ImGui::EndDisabled();

    if (!reticle->sourceTemplateId.empty() &&
        loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end() &&
        ImGui::Button("Edit source template"))
    {
        SelectLibraryReticle(reticle->sourceTemplateId);
        RebuildStatus("Editing template '" + reticle->sourceTemplateId + "' in the reticle studio.", false);
        return;
    }
    if (!reticle->sourceTemplateId.empty() && loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end())
    {
        ShowItemTooltip("Open the shared template that this page reticle instance was created from.");
    }

    if (!reticle->sourceTemplateId.empty() && loaded_.document.reticleLibrary.find(reticle->sourceTemplateId) != loaded_.document.reticleLibrary.end())
    {
        ImGui::SameLine();
    }

    ImGui::TextDisabled("Shortcut: Suppr");
    ImGui::TextDisabled("Cut / copy / paste: Ctrl+X / Ctrl+C / Ctrl+V");
    ImGui::TextDisabled("Esc clears the current page-reticle selection.");
    ImGui::TextDisabled("Draw order: %d / %d", reticleIndex + 1, std::max(1, static_cast<int>(page->staticReticles.size())));

    const std::string currentLayerLabel = reticle->editor.layerId.empty() ? std::string {"<none>"} : reticle->editor.layerId;
    if (ImGui::BeginCombo("Editor layer", currentLayerLabel.c_str()))
    {
        const bool noLayerSelected = reticle->editor.layerId.empty();
        if (ImGui::Selectable("<none>", noLayerSelected))
        {
            PushUndoSnapshot();
            reticle->editor.layerId.clear();
        }
        if (noLayerSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (const auto& layer : page->editor.layers)
        {
            const bool selected = reticle->editor.layerId == layer.id;
            const std::string label = layer.id + (layer.visible ? "" : " (hidden)");
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                reticle->editor.layerId = layer.id;
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
        SelectPage(selection_.pageIndex);
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
        moveSelectedReticleTo(0, "to the back");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle to the first draw-order slot on the page.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveBackward);
    if (ImGui::Button("Step back", ImVec2(110.0f, 0.0f)))
    {
        moveSelectedReticleTo(reticleIndex - 1, "backward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step earlier in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Step forward", ImVec2(130.0f, 0.0f)))
    {
        moveSelectedReticleTo(reticleIndex + 1, "forward");
        ImGui::EndDisabled();
        return;
    }
    ShowItemTooltip("Move this reticle one step later in the page draw order.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canMoveForward);
    if (ImGui::Button("Bring to front", ImVec2(130.0f, 0.0f)))
    {
        moveSelectedReticleTo(lastReticleIndex, "to the front");
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
                    ApplyPageReticleClipping(selection_.pageReticleIndex, reticle->clipping.mode, option.primitiveId);
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
                    ApplyPageReticleClipping(selection_.pageReticleIndex, mode, primitiveId);
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

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Library reticle");
    ImGui::Text("Template id: %s", reticle->id.c_str());
    if (const auto fileIt = files_.templateFiles.find(reticle->id); fileIt != files_.templateFiles.end())
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
        const mfd::Vec2 dropPosition = page == nullptr ? mfd::Vec2 {} : pagePreviewView_.center;
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
        "Instantiate the prepared tutorial circle on Page1 so clipping can be demonstrated next.");
    if (!canAddToPage)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete library reticle"))
    {
        DeleteSelectedLibraryReticle();
        return;
    }
    ShowItemTooltip("Delete this shared reticle template from the library.");

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

    ImGui::Separator();
    ImGui::TextDisabled("Primitives");
    ImGui::TextDisabled("Click a primitive below or directly in the studio preview to focus and edit it.");

    ImGui::BeginChild("PrimitiveCatalog", ImVec2(0.0f, 210.0f), true);
    for (int index = 0; index < static_cast<int>(reticle->primitives.size()); ++index)
    {
        const auto& primitive = reticle->primitives[static_cast<std::size_t>(index)];
        const bool selected = selection_.kind == SelectionKind::LibraryPrimitive &&
                              selection_.libraryReticleId == reticle->id &&
                              selection_.primitiveIndex == index;
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

    if (ImGui::BeginCombo("Add primitive", PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)]).c_str()))
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (ImGui::Selectable(PrimitiveTypeLabel(kPrimitiveTypes[static_cast<std::size_t>(index)]).c_str(),
                                  newLibraryReticleDraft_.primitiveTypeIndex == index))
            {
                newLibraryReticleDraft_.primitiveTypeIndex = index;
            }
        }
        ImGui::EndCombo();
    }
    ShowItemTooltip("Choose the primitive type that will be appended to this reticle.");

    if (AccentButton("Append primitive"))
    {
        const bool tutorialAppendMatched = tutorial_->MatchesTarget("library_append_primitive");
        const mfd::PrimitiveType primitiveType =
            kPrimitiveTypes[static_cast<std::size_t>(newLibraryReticleDraft_.primitiveTypeIndex)];
        if (tutorialAppendMatched)
        {
            if (reticle->id != kTutorialStrobeCursorTemplateId)
            {
                RebuildStatus("Tutorial: append the second line inside 'mfd_tutorial_strobe_cursor'.", true);
                return;
            }

            if (primitiveType != mfd::PrimitiveType::Line)
            {
                RebuildStatus("Tutorial: keep the Add primitive selector on 'Line' for the strobe cursor.", true);
                return;
            }
        }

        PushUndoSnapshot();
        mfd::ReticleGroup seed = MakePrimitiveReticle("seed", primitiveType);
        mfd::Primitive primitive = seed.primitives.front();
        primitive.id = "primitive_" + std::to_string(reticle->primitives.size() + 1);
        if (tutorialAppendMatched)
        {
            (void)ConfigureTutorialStrobeLinePrimitive(primitive, true);
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
        "Append the second line so the tutorial strobe cursor becomes a simple cross.");

    ImGui::SameLine();
    if (ImGui::Button("Remove selected primitive"))
    {
        mfd::Primitive* primitive = SelectedLibraryPrimitive();
        if (primitive != nullptr)
        {
            PushUndoSnapshot();
            reticle->primitives.erase(reticle->primitives.begin() + selection_.primitiveIndex);
            selection_.kind = SelectionKind::LibraryReticle;
            selection_.primitiveIndex = -1;
        }
    }
    ShowItemTooltip("Delete the currently selected primitive from this reticle.");

}

void EditorApplication::DrawLibraryPrimitiveInspector()
{
    mfd::Primitive* primitive = SelectedLibraryPrimitive();
    if (primitive == nullptr)
    {
        ImGui::TextDisabled("Select a primitive inside a library reticle.");
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Primitive");
    ImGui::TextDisabled("Green handle moves the primitive. Orange handles edit geometry directly in the studio.");
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

    ImVec4 fill = ToImGuiColor(primitive->style.fillColor);
    if (ImGui::ColorEdit4("Fill", &fill.x))
    {
        if (ImGui::IsItemActivated())
        {
            PushUndoSnapshot();
        }
        primitive->style.fillColor = ToColorRgba(fill);
    }
    ShowItemTooltip("Fill color used when this primitive supports filled rendering.");

    {
        bool filled = primitive->style.filled;
        if (ImGui::Checkbox("Filled", &filled))
        {
            PushUndoSnapshot();
            primitive->style.filled = filled;
        }
        ShowItemTooltip("Toggle filled rendering for primitives that support it.");
    }

    auto editPointArray = [&](const char* label, mfd::Vec2* value)
    {
        if (ImGui::DragFloat2(label, &value->x, 0.01f, -1.0f, 1.0f, "%.3f"))
        {
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
        }
    };

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
        editPointArray("Start", &line->start);
        editPointArray("End", &line->end);
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
        editPointArray("Point A", &triangle->points[0]);
        editPointArray("Point B", &triangle->points[1]);
        editPointArray("Point C", &triangle->points[2]);
        return;
    }

    if (auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive->geometry))
    {
        bool closed = polyline->closed;
        if (ImGui::Checkbox("Closed", &closed))
        {
            PushUndoSnapshot();
            polyline->closed = closed;
        }
        ShowItemTooltip("Close the polyline by linking the last point back to the first.");

        for (int index = 0; index < static_cast<int>(polyline->points.size()); ++index)
        {
            const std::string label = "Point " + std::to_string(index + 1);
            editPointArray(label.c_str(), &polyline->points[static_cast<std::size_t>(index)]);
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
            editPointArray(label.c_str(), &bezier->controlPoints[static_cast<std::size_t>(index)]);
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
    }
}
