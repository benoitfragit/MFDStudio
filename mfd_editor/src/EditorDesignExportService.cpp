/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorDesignExportService.h"

/**
 * @file
 * @brief Implementation of the editor design-document export service.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>
#include <raylib.h>

#include "BezierPolylineCache.h"
#include "Canvas2D.h"
#include "ImageTextureCache.h"
#include "RenderTextureUtils.h"
#include "TextLayoutCache.h"
#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
using json = nlohmann::json;

constexpr int kExplodedCanvasSize = 960;
constexpr int kExplodedMinimumHeight = 960;
constexpr int kExplodedCanvasMargin = 48;
constexpr int kExplodedLabelGutter = 72;
constexpr float kExplodedTitleFontSize = 24.0f;
constexpr float kExplodedLabelFontSize = 20.0f;
constexpr float kExplodedDetailFontSize = 16.0f;
constexpr float kExplodedLabelSpacing = 52.0f;

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

std::filesystem::path NormalizePath(std::filesystem::path path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code error;
    if (!path.is_absolute())
    {
        path = std::filesystem::absolute(path, error);
        if (error)
        {
            error.clear();
        }
    }

    return path.lexically_normal();
}

bool PathExists(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    std::error_code error;
    return std::filesystem::exists(path, error);
}

bool DirectoryNotEmpty(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::is_directory(path, error))
    {
        return false;
    }

    return std::filesystem::directory_iterator(path, error) != std::filesystem::directory_iterator();
}

std::tm LocalTime(const std::time_t timestamp)
{
    std::tm localTime {};
#if defined(_WIN32)
    localtime_s(&localTime, &timestamp);
#else
    localtime_r(&timestamp, &localTime);
#endif
    return localTime;
}

std::string FormatTimestamp(const std::chrono::system_clock::time_point timePoint, const char* format)
{
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(timePoint);
    const std::tm localTime = LocalTime(timestamp);
    std::ostringstream stream;
    stream << std::put_time(&localTime, format);
    return stream.str();
}

std::string FormatDisplayTimestamp(const std::chrono::system_clock::time_point timePoint)
{
    return FormatTimestamp(timePoint, "%Y-%m-%d %H:%M:%S");
}

std::string FormatFolderTimestamp(const std::chrono::system_clock::time_point timePoint)
{
    return FormatTimestamp(timePoint, "%Y%m%d_%H%M%S");
}

std::string FormatFloat(const float value, const int precision = 3)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string FormatBool(const bool value)
{
    return value ? "true" : "false";
}

std::string FormatYesNo(const bool value)
{
    return value ? "yes" : "no";
}

std::string SafeString(const std::string_view value)
{
    return value.empty() ? std::string {"not available"} : std::string {value};
}

std::string MarkdownPath(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string RelativeMarkdownPath(const std::filesystem::path& fromFile, const std::filesystem::path& toFile)
{
    if (fromFile.empty() || toFile.empty())
    {
        return "not available";
    }

    const std::filesystem::path relative = toFile.lexically_relative(fromFile.parent_path());
    return MarkdownPath(relative.empty() ? toFile.filename() : relative);
}

std::string EscapeMarkdownCell(std::string value)
{
    std::replace(value.begin(), value.end(), '\n', ' ');

    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        if (character == '|')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }

    return escaped;
}

std::string SanitizeFileStem(const std::string_view value)
{
    std::string sanitized;
    sanitized.reserve(value.size());

    for (const char character : value)
    {
        const unsigned char unsignedCharacter = static_cast<unsigned char>(character);
        if (std::isalnum(unsignedCharacter))
        {
            sanitized.push_back(character);
        }
        else if (character == '-' || character == '_')
        {
            sanitized.push_back(character);
        }
        else if (character == ' ' || character == '.' || character == '/')
        {
            sanitized.push_back('_');
        }
        else
        {
            sanitized.push_back('_');
        }
    }

    while (!sanitized.empty() && (sanitized.front() == '_' || sanitized.front() == '.'))
    {
        sanitized.erase(sanitized.begin());
    }
    while (!sanitized.empty() && (sanitized.back() == '_' || sanitized.back() == '.' || sanitized.back() == ' '))
    {
        sanitized.pop_back();
    }

    if (sanitized.empty())
    {
        sanitized = "page";
    }

    return sanitized;
}

bool PathsEqualCaseInsensitive(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return Lowercase(lhs.generic_string()) == Lowercase(rhs.generic_string());
}

bool IsPathWithinRoot(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedRoot = NormalizePath(root);
    const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
    if (normalizedRoot.empty() || normalizedCandidate.empty())
    {
        return false;
    }

    auto rootIterator = normalizedRoot.begin();
    auto candidateIterator = normalizedCandidate.begin();
    for (; rootIterator != normalizedRoot.end() && candidateIterator != normalizedCandidate.end();
         ++rootIterator, ++candidateIterator)
    {
        if (Lowercase(rootIterator->string()) != Lowercase(candidateIterator->string()))
        {
            return false;
        }
    }

    return rootIterator == normalizedRoot.end();
}

void EnsureParentFolder(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        throw std::runtime_error("Unable to create export folder: " + path.parent_path().string());
    }
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    EnsureParentFolder(path);
    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open export file for writing: " + path.string());
    }

    stream << content;
}

std::filesystem::path ResolveOutputFolder(const std::filesystem::path& requestedFolder, std::vector<std::string>& warnings)
{
    std::filesystem::path normalized = NormalizePath(requestedFolder);
    if (normalized.empty())
    {
        return {};
    }

    if (!PathExists(normalized) || !DirectoryNotEmpty(normalized))
    {
        return normalized;
    }

    const std::string timestamp = FormatFolderTimestamp(std::chrono::system_clock::now());
    std::filesystem::path candidate = normalized / std::filesystem::path("MFDStudioDesignExport_" + timestamp);
    int collisionIndex = 2;
    while (PathExists(candidate))
    {
        candidate = normalized / std::filesystem::path("MFDStudioDesignExport_" + timestamp + "_" +
                                                       std::to_string(collisionIndex));
        ++collisionIndex;
    }

    warnings.push_back("The selected output folder was not empty. Exporting into '" + candidate.filename().string() +
                       "' instead.");
    return candidate;
}

const mfd::GeneratedTransportMap* ResolveGeneratedMap(const DesignExportPlan& plan) noexcept
{
    return plan.loaded.generatedTransportMap.has_value() ? &(*plan.loaded.generatedTransportMap) : nullptr;
}

const mfd::TransportMapPageEntry* FindPageEntry(const mfd::GeneratedTransportMap* map,
                                                const std::string_view pageName) noexcept
{
    if (map == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedPage = mfd::NormalizePageName(pageName);
    const auto iterator = std::find_if(
        map->pages.begin(),
        map->pages.end(),
        [&normalizedPage](const mfd::TransportMapPageEntry& entry)
        {
            return entry.normalizedName == normalizedPage;
        });
    return iterator == map->pages.end() ? nullptr : &(*iterator);
}

const mfd::TransportMapReticleEntry* FindReticleEntry(const mfd::GeneratedTransportMap* map,
                                                      const mfd::TransportId pageId,
                                                      const std::string_view reticleId) noexcept
{
    if (map == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedReticle = mfd::NormalizePageName(reticleId);
    const auto iterator = std::find_if(
        map->reticles.begin(),
        map->reticles.end(),
        [pageId, &normalizedReticle](const mfd::TransportMapReticleEntry& entry)
        {
            return entry.pageId == pageId && entry.normalizedReticleId == normalizedReticle;
        });
    return iterator == map->reticles.end() ? nullptr : &(*iterator);
}

const mfd::TransportMapTemplateEntry* FindTemplateEntry(const mfd::GeneratedTransportMap* map,
                                                        const std::string_view templateId) noexcept
{
    if (map == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedTemplate = mfd::NormalizePageName(templateId);
    const auto iterator = std::find_if(
        map->templates.begin(),
        map->templates.end(),
        [&normalizedTemplate](const mfd::TransportMapTemplateEntry& entry)
        {
            return entry.normalizedTemplateId == normalizedTemplate;
        });
    return iterator == map->templates.end() ? nullptr : &(*iterator);
}

const mfd::TransportMapBlinkTypeEntry* FindBlinkEntry(const mfd::GeneratedTransportMap* map,
                                                      const mfd::TransportId pageId,
                                                      const std::string_view blinkTypeName) noexcept
{
    if (map == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedBlinkType = mfd::NormalizePageName(blinkTypeName);
    const auto iterator = std::find_if(
        map->blinkTypes.begin(),
        map->blinkTypes.end(),
        [pageId, &normalizedBlinkType](const mfd::TransportMapBlinkTypeEntry& entry)
        {
            return entry.pageId == pageId && entry.normalizedBlinkType == normalizedBlinkType;
        });
    return iterator == map->blinkTypes.end() ? nullptr : &(*iterator);
}

const mfd::TransportMapPrimitiveEntry* FindPrimitiveEntry(const mfd::GeneratedTransportMap* map,
                                                          const mfd::TransportPrimitiveOwnerKind ownerKind,
                                                          const mfd::TransportId ownerId,
                                                          const std::string_view primitiveId) noexcept
{
    if (map == nullptr || ownerId == 0)
    {
        return nullptr;
    }

    const std::string normalizedPrimitive = mfd::NormalizePageName(primitiveId);
    const auto iterator = std::find_if(
        map->primitives.begin(),
        map->primitives.end(),
        [ownerKind, ownerId, &normalizedPrimitive](const mfd::TransportMapPrimitiveEntry& entry)
        {
            return entry.ownerKind == ownerKind && entry.ownerId == ownerId && entry.normalizedPrimitiveId == normalizedPrimitive;
        });
    return iterator == map->primitives.end() ? nullptr : &(*iterator);
}

std::string TransportIdOrNotAvailable(const std::uint64_t transportId)
{
    return transportId == 0U ? std::string {"not available"} : std::to_string(transportId);
}

std::string SourcePathOrNotAvailable(const std::filesystem::path& path)
{
    return path.empty() ? std::string {"not available"} : MarkdownPath(path);
}

std::string ReticleDisplayId(const mfd::ReticleGroup& reticle, const std::size_t index)
{
    if (!reticle.id.empty())
    {
        return reticle.id;
    }

    if (!reticle.sourceTemplateId.empty())
    {
        return reticle.sourceTemplateId + "_" + std::to_string(index + 1U);
    }

    return "reticle_" + std::to_string(index + 1U);
}

std::string AccessorNameOrNotAvailable()
{
    return "not available";
}

std::string PrimitiveOwnerTransportId(const mfd::GeneratedTransportMap* map,
                                      const mfd::ReticleGroup& reticle,
                                      const mfd::TransportMapReticleEntry* reticleEntry,
                                      const mfd::Primitive& primitive)
{
    const mfd::TransportId reticleTransportId = reticleEntry == nullptr ? 0U : reticleEntry->id;
    if (const auto* primitiveEntry =
            FindPrimitiveEntry(map, mfd::TransportPrimitiveOwnerKind::Reticle, reticleTransportId, primitive.id);
        primitiveEntry != nullptr)
    {
        return TransportIdOrNotAvailable(primitiveEntry->id);
    }

    if (const auto* templateEntry = FindTemplateEntry(map, reticle.sourceTemplateId); templateEntry != nullptr)
    {
        if (const auto* primitiveEntry =
                FindPrimitiveEntry(map, mfd::TransportPrimitiveOwnerKind::Template, templateEntry->id, primitive.id);
            primitiveEntry != nullptr)
        {
            return TransportIdOrNotAvailable(primitiveEntry->id);
        }
    }

    return "not available";
}

Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

struct LogicalBounds
{
    mfd::Vec2 min {};
    mfd::Vec2 max {};
    mfd::Vec2 center {};
    bool valid = false;
};

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

mfd::Vec2 EvaluateBezierPoint(const std::vector<mfd::Vec2>& controlPoints, const float t)
{
    if (controlPoints.empty())
    {
        return {};
    }

    std::vector<mfd::Vec2> scratch = controlPoints;
    for (std::size_t remaining = scratch.size(); remaining > 1U; --remaining)
    {
        for (std::size_t index = 0; index + 1U < remaining; ++index)
        {
            scratch[index] = scratch[index] * (1.0f - t) + scratch[index + 1U] * t;
        }
    }

    return scratch.front();
}

std::vector<mfd::Vec2> ApproximateBezierPoints(const std::vector<mfd::Vec2>& controlPoints, const int segments = 32)
{
    if (controlPoints.size() < 2U)
    {
        return controlPoints;
    }

    const int segmentCount = std::max(4, segments);
    std::vector<mfd::Vec2> points;
    points.reserve(static_cast<std::size_t>(segmentCount) + 1U);
    for (int index = 0; index <= segmentCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(segmentCount);
        points.push_back(EvaluateBezierPoint(controlPoints, t));
    }

    return points;
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

    bounds.center = {(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f};
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

void IncludePrimitiveTransformedPoint(LogicalBounds& bounds, const mfd::Primitive& primitive, const mfd::Vec2 localPoint)
{
    IncludeLogicalPoint(bounds, mfd::ApplyTransform(localPoint, primitive.transform));
}

LogicalBounds ComputePrimitiveLocalBounds(const mfd::Primitive& primitive)
{
    LogicalBounds bounds;
    if (!primitive.style.visible)
    {
        return bounds;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        IncludePrimitiveTransformedPoint(bounds, primitive, {-halfWidth, -halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {halfWidth, -halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {halfWidth, halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-halfWidth, halfHeight});
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        IncludePrimitiveTransformedPoint(bounds, primitive, {-halfWidth, -halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {halfWidth, -halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {halfWidth, halfHeight});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-halfWidth, halfHeight});
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, line->start);
        IncludePrimitiveTransformedPoint(bounds, primitive, line->end);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-circle->radius, -circle->radius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {circle->radius, -circle->radius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {circle->radius, circle->radius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-circle->radius, circle->radius});
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-ring->outerRadius, -ring->outerRadius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {ring->outerRadius, -ring->outerRadius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {ring->outerRadius, ring->outerRadius});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-ring->outerRadius, ring->outerRadius});
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-rectangle->width * 0.5f, -rectangle->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {rectangle->width * 0.5f, -rectangle->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {rectangle->width * 0.5f, rectangle->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-rectangle->width * 0.5f, rectangle->height * 0.5f});
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-ellipse->width * 0.5f, -ellipse->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {ellipse->width * 0.5f, -ellipse->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {ellipse->width * 0.5f, ellipse->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-ellipse->width * 0.5f, ellipse->height * 0.5f});
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-square->width * 0.5f, -square->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {square->width * 0.5f, -square->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {square->width * 0.5f, square->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-square->width * 0.5f, square->height * 0.5f});
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {0.0f, diamond->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {diamond->width * 0.5f, 0.0f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {0.0f, -diamond->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-diamond->width * 0.5f, 0.0f});
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, triangle->points[0]);
        IncludePrimitiveTransformedPoint(bounds, primitive, triangle->points[1]);
        IncludePrimitiveTransformedPoint(bounds, primitive, triangle->points[2]);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        for (const auto& point : polyline->points)
        {
            IncludePrimitiveTransformedPoint(bounds, primitive, point);
        }
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        for (const auto& point : bezier->controlPoints)
        {
            IncludePrimitiveTransformedPoint(bounds, primitive, point);
        }
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        for (const auto& point :
             ApproximateArcPoints(arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments))
        {
            IncludePrimitiveTransformedPoint(bounds, primitive, point);
        }

        if (primitive.style.filled)
        {
            IncludePrimitiveTransformedPoint(bounds, primitive, {});
        }
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        IncludePrimitiveTransformedPoint(bounds, primitive, {-image->width * 0.5f, -image->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {image->width * 0.5f, -image->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {image->width * 0.5f, image->height * 0.5f});
        IncludePrimitiveTransformedPoint(bounds, primitive, {-image->width * 0.5f, image->height * 0.5f});
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
}

LogicalBounds ComputeReticleLocalBounds(const mfd::ReticleGroup& reticle)
{
    LogicalBounds bounds;
    for (const auto& primitive : reticle.primitives)
    {
        IncludeLogicalBounds(bounds, ComputePrimitiveLocalBounds(primitive));
    }

    FinalizeLogicalBounds(bounds);
    return bounds;
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

mfd::PageViewState MakeViewFittingBounds(const LogicalBounds& bounds, const int width, const int height) noexcept
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
    const float fitZoom = std::min(visibleHalfWidth / halfExtentX, visibleHalfHeight / halfExtentY) * 0.82f;

    view.center = bounds.center;
    view.zoom = std::clamp(fitZoom, 0.1f, 20.0f);
    return view;
}

struct ExportViewport
{
    int width = 0;
    int height = 0;
    mfd::PageViewState view {};

    [[nodiscard]] float LogicalScale() const noexcept
    {
        return static_cast<float>(std::min(width, height)) * 0.5f;
    }

    [[nodiscard]] Vector2 ToScreen(const mfd::Vec2 logical) const noexcept
    {
        const mfd::Vec2 viewPoint = mfd::ApplyPageView(logical, view);
        const float scale = LogicalScale();
        return Vector2 {
            static_cast<float>(width) * 0.5f + viewPoint.x * scale,
            static_cast<float>(height) * 0.5f - viewPoint.y * scale};
    }
};

struct ScreenBounds
{
    Vector2 min {};
    Vector2 max {};
    Vector2 center {};
    bool valid = false;
};

struct ScreenSegment
{
    Vector2 start {};
    Vector2 end {};
};

void IncludeScreenPoint(ScreenBounds& bounds, const Vector2 point)
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

void FinalizeScreenBounds(ScreenBounds& bounds)
{
    if (!bounds.valid)
    {
        return;
    }

    bounds.center = {(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f};
}

Vector2 ToScreenPrimitivePoint(const ExportViewport& viewport,
                              const mfd::ReticleGroup& reticle,
                              const mfd::Primitive& primitive,
                              const mfd::Vec2 localPoint)
{
    const mfd::Vec2 primitiveLocal = mfd::ApplyTransform(localPoint, primitive.transform);
    const mfd::Vec2 worldPoint = mfd::ApplyTransform(primitiveLocal, reticle.transform);
    return viewport.ToScreen(worldPoint);
}

void AppendScreenPolylineSegments(const ExportViewport& viewport,
                                  const mfd::ReticleGroup& reticle,
                                  const mfd::Primitive& primitive,
                                  const std::vector<mfd::Vec2>& localPoints,
                                  const bool closed,
                                  std::vector<ScreenSegment>& segments)
{
    if (localPoints.size() < 2U)
    {
        return;
    }

    Vector2 previousPoint = ToScreenPrimitivePoint(viewport, reticle, primitive, localPoints.front());
    for (std::size_t index = 1; index < localPoints.size(); ++index)
    {
        const Vector2 currentPoint = ToScreenPrimitivePoint(viewport, reticle, primitive, localPoints[index]);
        segments.push_back(ScreenSegment {previousPoint, currentPoint});
        previousPoint = currentPoint;
    }

    if (closed)
    {
        segments.push_back(
            ScreenSegment {previousPoint, ToScreenPrimitivePoint(viewport, reticle, primitive, localPoints.front())});
    }
}

void AppendPrimitiveScreenSegments(const ExportViewport& viewport,
                                   const mfd::ReticleGroup& reticle,
                                   const mfd::Primitive& primitive,
                                   std::vector<ScreenSegment>& segments)
{
    if (!primitive.style.visible)
    {
        return;
    }

    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*text);
        const float halfHeight = EstimatedTextHalfHeight(*text);
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{-halfWidth, -halfHeight},
                                      {halfWidth, -halfHeight},
                                      {halfWidth, halfHeight},
                                      {-halfWidth, halfHeight}},
                                     true,
                                     segments);
    }
    else if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        const float halfWidth = EstimatedTextHalfWidth(*time);
        const float halfHeight = EstimatedTextHalfHeight(*time);
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{-halfWidth, -halfHeight},
                                      {halfWidth, -halfHeight},
                                      {halfWidth, halfHeight},
                                      {-halfWidth, halfHeight}},
                                     true,
                                     segments);
    }
    else if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport, reticle, primitive, {line->start, line->end}, false, segments);
    }
    else if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     ApproximateEllipsePoints(circle->radius * 2.0f, circle->radius * 2.0f, 64),
                                     true,
                                     segments);
    }
    else if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     ApproximateEllipsePoints(ring->outerRadius * 2.0f, ring->outerRadius * 2.0f, 64),
                                     true,
                                     segments);
    }
    else if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        const float halfWidth = rectangle->width * 0.5f;
        const float halfHeight = rectangle->height * 0.5f;
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{-halfWidth, -halfHeight},
                                      {halfWidth, -halfHeight},
                                      {halfWidth, halfHeight},
                                      {-halfWidth, halfHeight}},
                                     true,
                                     segments);
    }
    else if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(
            viewport, reticle, primitive, ApproximateEllipsePoints(ellipse->width, ellipse->height, 64), true, segments);
    }
    else if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        const float halfWidth = square->width * 0.5f;
        const float halfHeight = square->height * 0.5f;
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{-halfWidth, -halfHeight},
                                      {halfWidth, -halfHeight},
                                      {halfWidth, halfHeight},
                                      {-halfWidth, halfHeight}},
                                     true,
                                     segments);
    }
    else if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{0.0f, diamond->height * 0.5f},
                                      {diamond->width * 0.5f, 0.0f},
                                      {0.0f, -diamond->height * 0.5f},
                                      {-diamond->width * 0.5f, 0.0f}},
                                     true,
                                     segments);
    }
    else if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {triangle->points[0], triangle->points[1], triangle->points[2]},
                                     true,
                                     segments);
    }
    else if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport, reticle, primitive, polyline->points, polyline->closed, segments);
    }
    else if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(
            viewport, reticle, primitive, ApproximateBezierPoints(bezier->controlPoints, bezier->segments), false, segments);
    }
    else if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     ApproximateArcPoints(
                                         arc->radius, arc->startAngleDegrees, arc->endAngleDegrees, arc->segments),
                                     false,
                                     segments);
    }
    else if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        const float halfWidth = image->width * 0.5f;
        const float halfHeight = image->height * 0.5f;
        AppendScreenPolylineSegments(viewport,
                                     reticle,
                                     primitive,
                                     {{-halfWidth, -halfHeight},
                                      {halfWidth, -halfHeight},
                                      {halfWidth, halfHeight},
                                      {-halfWidth, halfHeight}},
                                     true,
                                     segments);
    }
}

std::vector<ScreenSegment> BuildReticleScreenSegments(const mfd::ReticleGroup& reticle, const ExportViewport& viewport)
{
    std::vector<ScreenSegment> segments;
    for (const auto& primitive : reticle.primitives)
    {
        AppendPrimitiveScreenSegments(viewport, reticle, primitive, segments);
    }
    return segments;
}

ScreenBounds ComputeScreenBounds(const std::vector<ScreenSegment>& segments)
{
    ScreenBounds bounds;
    for (const auto& segment : segments)
    {
        IncludeScreenPoint(bounds, segment.start);
        IncludeScreenPoint(bounds, segment.end);
    }
    FinalizeScreenBounds(bounds);
    return bounds;
}

std::optional<Vector2> SegmentIntersection(const Vector2 aStart,
                                           const Vector2 aEnd,
                                           const Vector2 bStart,
                                           const Vector2 bEnd)
{
    const float ax = aEnd.x - aStart.x;
    const float ay = aEnd.y - aStart.y;
    const float bx = bEnd.x - bStart.x;
    const float by = bEnd.y - bStart.y;
    const float denominator = ax * by - ay * bx;
    if (std::abs(denominator) <= 0.0001f)
    {
        return std::nullopt;
    }

    const float dx = bStart.x - aStart.x;
    const float dy = bStart.y - aStart.y;
    const float t = (dx * by - dy * bx) / denominator;
    const float u = (dx * ay - dy * ax) / denominator;
    if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f)
    {
        return std::nullopt;
    }

    return Vector2 {aStart.x + ax * t, aStart.y + ay * t};
}

Vector2 ClosestPointOnSegment(const Vector2 point, const ScreenSegment& segment)
{
    const float dx = segment.end.x - segment.start.x;
    const float dy = segment.end.y - segment.start.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0001f)
    {
        return segment.start;
    }

    const float t =
        std::clamp(((point.x - segment.start.x) * dx + (point.y - segment.start.y) * dy) / lengthSquared, 0.0f, 1.0f);
    return Vector2 {segment.start.x + dx * t, segment.start.y + dy * t};
}

float DistanceSquared(const Vector2 lhs, const Vector2 rhs)
{
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return dx * dx + dy * dy;
}

Vector2 ResolveBoundsBoundaryPoint(const Vector2 labelPoint, const ScreenBounds& bounds)
{
    if (!bounds.valid)
    {
        return labelPoint;
    }

    const Vector2 center = bounds.center;
    const std::array<ScreenSegment, 4> edges {{
        {Vector2 {bounds.min.x, bounds.min.y}, Vector2 {bounds.max.x, bounds.min.y}},
        {Vector2 {bounds.max.x, bounds.min.y}, Vector2 {bounds.max.x, bounds.max.y}},
        {Vector2 {bounds.max.x, bounds.max.y}, Vector2 {bounds.min.x, bounds.max.y}},
        {Vector2 {bounds.min.x, bounds.max.y}, Vector2 {bounds.min.x, bounds.min.y}},
    }};

    std::optional<Vector2> bestIntersection;
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto& edge : edges)
    {
        const std::optional<Vector2> intersection = SegmentIntersection(labelPoint, center, edge.start, edge.end);
        if (!intersection.has_value())
        {
            continue;
        }

        const float distance = DistanceSquared(labelPoint, *intersection);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIntersection = intersection;
        }
    }

    return bestIntersection.value_or(center);
}

struct ExplodedLabel
{
    std::string title;
    std::string detail;
    Vector2 center {};
    ScreenBounds bounds {};
    std::vector<ScreenSegment> outlineSegments {};
    bool visible = true;
};

std::vector<ExplodedLabel> BuildExplodedLabels(const mfd::PageDefinition& page,
                                               const ExportViewport& viewport,
                                               const bool includeCanvasCoordinates)
{
    std::vector<ExplodedLabel> labels;
    labels.reserve(page.staticReticles.size() + (page.strobe.has_value() ? 1U : 0U));

    for (std::size_t index = 0; index < page.staticReticles.size(); ++index)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[index];
        const std::vector<ScreenSegment> segments = BuildReticleScreenSegments(reticle, viewport);
        const ScreenBounds bounds = ComputeScreenBounds(segments);
        const Vector2 center = bounds.valid ? bounds.center : viewport.ToScreen(reticle.transform.position);

        std::ostringstream detail;
        if (includeCanvasCoordinates)
        {
            detail << "x=" << FormatFloat(reticle.transform.position.x) << " y=" << FormatFloat(reticle.transform.position.y);
        }
        else
        {
            detail << "template=" << SafeString(reticle.sourceTemplateId);
        }

        labels.push_back(ExplodedLabel {
            ReticleDisplayId(reticle, index),
            detail.str(),
            center,
            bounds,
            segments,
            reticle.visible});
    }

    if (page.strobe.has_value())
    {
        const mfd::ReticleGroup& reticle = page.strobe->reticle;
        const std::vector<ScreenSegment> segments = BuildReticleScreenSegments(reticle, viewport);
        const ScreenBounds bounds = ComputeScreenBounds(segments);
        const Vector2 center = bounds.valid ? bounds.center : viewport.ToScreen(reticle.transform.position);
        std::ostringstream detail;
        if (includeCanvasCoordinates)
        {
            detail << "x=" << FormatFloat(reticle.transform.position.x) << " y=" << FormatFloat(reticle.transform.position.y);
        }
        else
        {
            detail << "template=" << SafeString(reticle.sourceTemplateId);
        }

        labels.push_back(ExplodedLabel {"strobe", detail.str(), center, bounds, segments, reticle.visible});
    }

    std::sort(
        labels.begin(),
        labels.end(),
        [](const ExplodedLabel& lhs, const ExplodedLabel& rhs)
        {
            return lhs.center.y < rhs.center.y;
        });
    return labels;
}

Vector2 ResolveExplodedLabelTarget(const ExplodedLabel& label, const Vector2 labelPoint)
{
    std::optional<Vector2> bestIntersection;
    float bestIntersectionDistance = std::numeric_limits<float>::max();
    for (const auto& segment : label.outlineSegments)
    {
        const std::optional<Vector2> intersection =
            SegmentIntersection(labelPoint, label.center, segment.start, segment.end);
        if (!intersection.has_value())
        {
            continue;
        }

        const float distance = DistanceSquared(labelPoint, *intersection);
        if (distance < bestIntersectionDistance)
        {
            bestIntersectionDistance = distance;
            bestIntersection = intersection;
        }
    }

    if (bestIntersection.has_value())
    {
        return *bestIntersection;
    }

    if (!label.outlineSegments.empty())
    {
        Vector2 closestPoint = label.center;
        float bestDistance = std::numeric_limits<float>::max();
        for (const auto& segment : label.outlineSegments)
        {
            const Vector2 candidate = ClosestPointOnSegment(labelPoint, segment);
            const float distance = DistanceSquared(labelPoint, candidate);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                closestPoint = candidate;
            }
        }
        return closestPoint;
    }

    if (label.bounds.valid)
    {
        return ResolveBoundsBoundaryPoint(labelPoint, label.bounds);
    }

    return label.center;
}

std::string BuildReticleSnippet(const std::string_view pageName, const mfd::ReticleGroup& reticle, const std::size_t index)
{
    if (reticle.id.empty())
    {
        return "Generic authored-name commands are not available because this reticle has no public id.";
    }

    const std::string reticleId = ReticleDisplayId(reticle, index);
    std::ostringstream stream;
    stream << "client.SetReticleVisible(\"" << pageName << "\", \"" << reticleId << "\", "
           << (reticle.visible ? "true" : "false") << ");\n";
    stream << "client.SetReticlePosition(\"" << pageName << "\", \"" << reticleId << "\", {"
           << FormatFloat(reticle.transform.position.x) << "f, " << FormatFloat(reticle.transform.position.y) << "f});\n";
    stream << "client.SetReticleRotation(\"" << pageName << "\", \"" << reticleId << "\", "
           << FormatFloat(reticle.transform.rotationDegrees) << "f);";
    return stream.str();
}

std::string BuildBlinkSnippet(const std::string_view pageName,
                              const std::string_view reticleId,
                              const std::string_view blinkType)
{
    std::ostringstream stream;
    stream << "client.SetReticleBlinkType(\"" << pageName << "\", \"" << reticleId << "\", \"" << blinkType << "\");\n";
    stream << "client.ClearReticleBlinkType(\"" << pageName << "\", \"" << reticleId << "\");";
    return stream.str();
}

std::string BuildReadmeMarkdown(const DesignExportPlan& plan,
                                const std::string& generatedAt,
                                const std::vector<bool>& imageWritten)
{
    std::ostringstream stream;
    stream << "# MFDStudio Design Export\n\n";
    stream << "Window: " << SafeString(plan.loaded.window.title) << "\n";
    stream << "Source: " << SourcePathOrNotAvailable(plan.windowFile) << "\n";
    stream << "Generated: " << generatedAt << "\n\n";
    stream << "## Pages\n\n";

    for (std::size_t index = 0; index < plan.pages.size(); ++index)
    {
        stream << "- [" << plan.pages[index].pageName << "](" << RelativeMarkdownPath(plan.readmeFile, plan.pages[index].markdownFile)
               << ")\n";
    }

    stream << "\n## Images\n\n";
    if (!plan.exportExplodedViews)
    {
        stream << "- not exported\n";
    }
    else
    {
        for (std::size_t index = 0; index < plan.pages.size(); ++index)
        {
            stream << "- ";
            if (imageWritten[index])
            {
                stream << "`" << RelativeMarkdownPath(plan.readmeFile, plan.pages[index].imageFile) << "`\n";
            }
            else
            {
                stream << plan.pages[index].pageName << ": not available\n";
            }
        }
    }

    stream << "\n## ICD\n\n";
    stream << "See [window_icd.md](" << RelativeMarkdownPath(plan.readmeFile, plan.windowIcdFile) << ").\n";
    return stream.str();
}

std::string BuildWindowIcdMarkdown(const DesignExportPlan& plan)
{
    std::ostringstream stream;
    const mfd::GeneratedTransportMap* map = ResolveGeneratedMap(plan);

    stream << "# Window ICD\n\n";
    stream << "## Window\n\n";
    stream << "| Field | Value |\n";
    stream << "|---|---|\n";
    stream << "| Title | " << EscapeMarkdownCell(SafeString(plan.loaded.window.title)) << " |\n";
    stream << "| Source file | " << EscapeMarkdownCell(SourcePathOrNotAvailable(plan.windowFile)) << " |\n";
    stream << "| Mapping hash | "
           << EscapeMarkdownCell(plan.includeMappingHash && map != nullptr ? SafeString(map->mappingHash) : "not available")
           << " |\n\n";

    stream << "## Pages\n\n";
    stream << "| Page | Generated C++ access | Reticles | Strobe | Blink types |\n";
    stream << "|---|---|---:|---|---:|\n";
    for (const auto& artifact : plan.pages)
    {
        const mfd::PageDefinition& page = plan.loaded.document.pages[artifact.pageIndex];
        stream << "| [" << EscapeMarkdownCell(page.name) << "]("
               << RelativeMarkdownPath(plan.windowIcdFile, artifact.markdownFile) << ") | "
               << EscapeMarkdownCell(AccessorNameOrNotAvailable()) << " | " << page.staticReticles.size() << " | "
               << FormatYesNo(page.strobe.has_value()) << " | " << page.blinkTypes.size() << " |\n";
    }

    stream << "\n## Page Activation\n\n";
    if (!plan.pages.empty())
    {
        stream << "```cpp\n";
        stream << "client.ActivatePage(\"" << plan.pages.front().pageName << "\");\n";
        stream << "```\n\n";
    }
    else
    {
        stream << "No authored pages are available in the current window.\n\n";
    }

    stream << "## Notes\n\n";
    stream << "This document describes authored reticles, optional generated transport ids, and generic name-based C++ commands.\n";
    if (map == nullptr)
    {
        stream << "Generated accessor names are not available because no companion generated transport map is loaded.\n";
    }

    return stream.str();
}

std::string BuildPageMarkdown(const DesignExportPlan& plan,
                              const DesignExportPageArtifact& artifact,
                              const bool imageAvailable)
{
    const mfd::PageDefinition& page = plan.loaded.document.pages[artifact.pageIndex];
    const mfd::GeneratedTransportMap* map = ResolveGeneratedMap(plan);
    const mfd::TransportMapPageEntry* pageEntry = FindPageEntry(map, page.name);

    std::ostringstream stream;
    stream << "# Page ICD - " << page.name << "\n\n";

    if (plan.exportExplodedViews)
    {
        if (imageAvailable)
        {
            stream << "![Exploded design view](" << RelativeMarkdownPath(artifact.markdownFile, artifact.imageFile) << ")\n\n";
        }
        else
        {
            stream << "Exploded design view: not available.\n\n";
        }
    }

    stream << "## Page Summary\n\n";
    stream << "| Field | Value |\n";
    stream << "|---|---|\n";
    stream << "| Page name | " << EscapeMarkdownCell(page.name) << " |\n";
    stream << "| Source file | " << EscapeMarkdownCell(SourcePathOrNotAvailable(artifact.sourcePageFile)) << " |\n";
    stream << "| Canvas width | 2.000 |\n";
    stream << "| Canvas height | 2.000 |\n";
    stream << "| Reticle count | " << page.staticReticles.size() << " |\n";
    stream << "| Has strobe | " << FormatYesNo(page.strobe.has_value()) << " |\n";
    if (page.blinkTypes.empty())
    {
        stream << "| Blink types | none |\n";
    }
    else
    {
        std::ostringstream blinkList;
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            if (index > 0U)
            {
                blinkList << ", ";
            }
            blinkList << page.blinkTypes[index].name;
        }
        stream << "| Blink types | " << EscapeMarkdownCell(blinkList.str()) << " |\n";
    }
    stream << "| Generated page transport id | "
           << EscapeMarkdownCell(pageEntry == nullptr ? "not available" : TransportIdOrNotAvailable(pageEntry->id)) << " |\n";
    stream << "| Generated C++ access | " << EscapeMarkdownCell(AccessorNameOrNotAvailable()) << " |\n\n";

    stream << "## Reticles\n\n";
    stream << "| Reticle | Template | X | Y | Rotation | Scale | Visible | Layer | Generated C++ | Transport id |\n";
    stream << "|---|---|---:|---:|---:|---:|---|---|---|---|\n";
    for (std::size_t index = 0; index < page.staticReticles.size(); ++index)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[index];
        const mfd::TransportMapReticleEntry* reticleEntry =
            pageEntry == nullptr ? nullptr : FindReticleEntry(map, pageEntry->id, reticle.id);
        const float scale = (std::abs(reticle.transform.scale.x) + std::abs(reticle.transform.scale.y)) * 0.5f;
        stream << "| " << EscapeMarkdownCell(ReticleDisplayId(reticle, index)) << " | "
               << EscapeMarkdownCell(SafeString(reticle.sourceTemplateId)) << " | "
               << EscapeMarkdownCell(plan.includeCanvasCoordinates ? FormatFloat(reticle.transform.position.x) : "not exported")
               << " | "
               << EscapeMarkdownCell(plan.includeCanvasCoordinates ? FormatFloat(reticle.transform.position.y) : "not exported")
               << " | " << EscapeMarkdownCell(FormatFloat(reticle.transform.rotationDegrees)) << " | "
               << EscapeMarkdownCell(FormatFloat(scale)) << " | " << FormatBool(reticle.visible) << " | "
               << EscapeMarkdownCell(SafeString(reticle.layerId)) << " | "
               << EscapeMarkdownCell(AccessorNameOrNotAvailable()) << " | "
               << EscapeMarkdownCell(reticleEntry == nullptr ? "not available" : TransportIdOrNotAvailable(reticleEntry->id))
               << " |\n";
    }

    stream << "\n## Reticle Details\n\n";
    for (std::size_t index = 0; index < page.staticReticles.size(); ++index)
    {
        const mfd::ReticleGroup& reticle = page.staticReticles[index];
        const mfd::TransportMapReticleEntry* reticleEntry =
            pageEntry == nullptr ? nullptr : FindReticleEntry(map, pageEntry->id, reticle.id);
        const float scale = (std::abs(reticle.transform.scale.x) + std::abs(reticle.transform.scale.y)) * 0.5f;
        stream << "### " << ReticleDisplayId(reticle, index) << "\n\n";
        stream << "Canvas:\n";
        stream << "- x: `" << (plan.includeCanvasCoordinates ? FormatFloat(reticle.transform.position.x) : "not exported") << "`\n";
        stream << "- y: `" << (plan.includeCanvasCoordinates ? FormatFloat(reticle.transform.position.y) : "not exported") << "`\n";
        stream << "- rotation: `" << FormatFloat(reticle.transform.rotationDegrees) << "`\n";
        stream << "- scale: `" << FormatFloat(scale) << "`\n";
        stream << "- visible: `" << FormatBool(reticle.visible) << "`\n";
        stream << "- layer: `" << SafeString(reticle.layerId) << "`\n";
        stream << "- template: `" << SafeString(reticle.sourceTemplateId) << "`\n";
        stream << "- generated accessor: `" << AccessorNameOrNotAvailable() << "`\n";
        stream << "- transport id: `"
               << (reticleEntry == nullptr ? "not available" : TransportIdOrNotAvailable(reticleEntry->id)) << "`\n\n";

        if (plan.includePrimitiveIds)
        {
            stream << "Primitive ids:\n";
            if (reticle.primitives.empty())
            {
                stream << "- `not available`\n\n";
            }
            else
            {
                for (const auto& primitive : reticle.primitives)
                {
                    stream << "- `" << SafeString(primitive.id) << "`";
                    if (primitive.exposed)
                    {
                        stream << " (transport: `"
                               << PrimitiveOwnerTransportId(map, reticle, reticleEntry, primitive) << "`)";
                    }
                    stream << "\n";
                }
                stream << "\n";
            }
        }

        if (plan.includeCppSnippets)
        {
            stream << "C++:\n```cpp\n" << BuildReticleSnippet(page.name, reticle, index) << "\n```\n\n";
            if (!page.blinkTypes.empty() && !reticle.id.empty())
            {
                stream << "Blink example:\n```cpp\n"
                       << BuildBlinkSnippet(page.name, ReticleDisplayId(reticle, index), page.blinkTypes.front().name)
                       << "\n```\n\n";
            }
        }
    }

    if (plan.includeStrobe && page.strobe.has_value())
    {
        const mfd::ReticleGroup& strobeReticle = page.strobe->reticle;
        stream << "## Strobe ICD\n\n";
        stream << "| Field | Value |\n";
        stream << "|---|---|\n";
        stream << "| Available | yes |\n";
        stream << "| Reticle | " << EscapeMarkdownCell(ReticleDisplayId(strobeReticle, 0U)) << " |\n";
        stream << "| Template | " << EscapeMarkdownCell(SafeString(strobeReticle.sourceTemplateId)) << " |\n\n";

        if (plan.includeCppSnippets)
        {
            stream << "C++:\n```cpp\n";
            stream << "client.SetStrobeActive(\"" << page.name << "\", true);\n";
            stream << "client.SetStrobePosition(\"" << page.name << "\", {0.000f, 0.000f});\n";
            stream << "```\n\n";
        }
    }

    if (plan.includeBlink && !page.blinkTypes.empty())
    {
        stream << "## Blink ICD\n\n";
        stream << "| Blink type | Generated C++ | Duration ms | Transport id |\n";
        stream << "|---|---|---:|---|\n";
        for (const auto& blinkType : page.blinkTypes)
        {
            const mfd::TransportMapBlinkTypeEntry* blinkEntry =
                pageEntry == nullptr ? nullptr : FindBlinkEntry(map, pageEntry->id, blinkType.name);
            stream << "| " << EscapeMarkdownCell(blinkType.name) << " | " << EscapeMarkdownCell(AccessorNameOrNotAvailable())
                   << " | " << blinkType.durationMs << " | "
                   << EscapeMarkdownCell(blinkEntry == nullptr ? "not available" : TransportIdOrNotAvailable(blinkEntry->id))
                   << " |\n";
        }

        if (plan.includeCppSnippets && !page.staticReticles.empty() && !page.staticReticles.front().id.empty())
        {
            stream << "\nC++:\n```cpp\n"
                   << BuildBlinkSnippet(page.name, ReticleDisplayId(page.staticReticles.front(), 0U), page.blinkTypes.front().name)
                   << "\n```\n";
        }
    }

    return stream.str();
}

json BuildManifestJson(const DesignExportPlan& plan,
                       const std::string& generatedAt,
                       const std::vector<bool>& imageWritten,
                       const std::vector<std::string>& warnings)
{
    json pages = json::array();
    for (std::size_t index = 0; index < plan.pages.size(); ++index)
    {
        const DesignExportPageArtifact& artifact = plan.pages[index];
        const mfd::PageDefinition& page = plan.loaded.document.pages[artifact.pageIndex];

        pages.push_back(json {
            {"name", page.name},
            {"sourceFile", artifact.sourcePageFile.empty() ? json(nullptr) : json(MarkdownPath(artifact.sourcePageFile))},
            {"markdownFile", MarkdownPath(artifact.markdownFile.lexically_relative(plan.outputFolder))},
            {"imageFile",
             artifact.imageFile.empty() ? json(nullptr)
                                        : json(MarkdownPath(artifact.imageFile.lexically_relative(plan.outputFolder)))},
            {"imageWritten", imageWritten[index]},
            {"reticleCount", page.staticReticles.size()},
            {"hasStrobe", page.strobe.has_value()},
            {"blinkTypeCount", page.blinkTypes.size()}});
    }

    return json {
        {"generatedAt", generatedAt},
        {"window",
         {
             {"title", plan.loaded.window.title},
             {"sourceFile", plan.windowFile.empty() ? json(nullptr) : json(MarkdownPath(plan.windowFile))},
             {"mappingHash",
              plan.includeMappingHash && plan.loaded.generatedTransportMap.has_value()
                  ? json(plan.loaded.generatedTransportMap->mappingHash)
                  : json(nullptr)},
         }},
        {"options",
         {
             {"exportMarkdownIcd", plan.exportMarkdownIcd},
             {"exportExplodedViews", plan.exportExplodedViews},
             {"includeCanvasCoordinates", plan.includeCanvasCoordinates},
             {"includeCppSnippets", plan.includeCppSnippets},
             {"includeStrobe", plan.includeStrobe},
             {"includeBlink", plan.includeBlink},
             {"includePrimitiveIds", plan.includePrimitiveIds},
             {"includeMappingHash", plan.includeMappingHash},
         }},
        {"pages", std::move(pages)},
        {"warnings", warnings}};
}
} // namespace

EditorDesignExportService::EditorDesignExportService(ExplodedViewRenderer explodedViewRenderer)
    : explodedViewRenderer_(std::move(explodedViewRenderer))
{
}

DesignExportPlan EditorDesignExportService::BuildPlan(const DesignExportRequest& request) const
{
    DesignExportPlan plan;
    if (request.outputFolder.empty())
    {
        plan.error = "Select one output folder before exporting the design documentation.";
        return plan;
    }

    if (request.loaded == nullptr)
    {
        plan.error = "No loaded window is available for design export.";
        return plan;
    }

    if (request.files == nullptr)
    {
        plan.error = "No authored file layout is available for design export.";
        return plan;
    }

    plan.outputFolder = ResolveOutputFolder(request.outputFolder, plan.warnings);
    if (plan.outputFolder.empty())
    {
        plan.error = "Unable to resolve the export output folder.";
        return plan;
    }

    plan.loaded = *request.loaded;
    plan.files = *request.files;
    plan.windowFile = request.windowFile.empty() ? request.loaded->window.sourceFile : request.windowFile;
    plan.exportMarkdownIcd = request.exportMarkdownIcd;
    plan.exportExplodedViews = request.exportExplodedViews;
    plan.includeCanvasCoordinates = request.includeCanvasCoordinates;
    plan.includeCppSnippets = request.includeCppSnippets;
    plan.includeStrobe = request.includeStrobe;
    plan.includeBlink = request.includeBlink;
    plan.includePrimitiveIds = request.includePrimitiveIds;
    plan.includeMappingHash = request.includeMappingHash;

    plan.readmeFile = plan.outputFolder / "README.md";
    plan.windowIcdFile = plan.outputFolder / "window_icd.md";
    plan.manifestFile = plan.outputFolder / "data" / "design_manifest.json";

    std::unordered_set<std::string> usedStems;
    for (std::size_t pageIndex = 0; pageIndex < plan.loaded.document.pages.size(); ++pageIndex)
    {
        const mfd::PageDefinition& page = plan.loaded.document.pages[pageIndex];
        std::string safeStem = SanitizeFileStem(page.name);
        if (!usedStems.insert(Lowercase(safeStem)).second)
        {
            int duplicateIndex = 2;
            std::string candidate;
            do
            {
                candidate = safeStem + "_" + std::to_string(duplicateIndex);
                ++duplicateIndex;
            } while (!usedStems.insert(Lowercase(candidate)).second);
            safeStem = std::move(candidate);
        }

        DesignExportPageArtifact artifact;
        artifact.pageIndex = pageIndex;
        artifact.pageName = page.name;
        artifact.safeFileStem = safeStem;
        artifact.markdownFile = plan.outputFolder / "pages" / std::filesystem::path(safeStem + ".md");
        artifact.imageFile = plan.exportExplodedViews
                                 ? plan.outputFolder / "images" / std::filesystem::path(safeStem + "_design_exploded.png")
                                 : std::filesystem::path {};
        artifact.sourcePageFile =
            pageIndex < plan.files.pageFiles.size() ? plan.files.pageFiles[pageIndex] : std::filesystem::path {};
        plan.pages.push_back(std::move(artifact));
    }

    if (plan.exportMarkdownIcd)
    {
        plan.filesToWrite.push_back(plan.readmeFile);
        plan.filesToWrite.push_back(plan.windowIcdFile);
        for (const auto& artifact : plan.pages)
        {
            plan.filesToWrite.push_back(artifact.markdownFile);
        }
    }

    if (plan.exportExplodedViews)
    {
        for (const auto& artifact : plan.pages)
        {
            if (!artifact.imageFile.empty())
            {
                plan.filesToWrite.push_back(artifact.imageFile);
            }
        }
    }

    plan.filesToWrite.push_back(plan.manifestFile);

    for (const auto& file : plan.filesToWrite)
    {
        if (!IsPathWithinRoot(plan.outputFolder, file))
        {
            plan.error = "Export plan tried to write outside the selected output folder.";
            plan.canExecute = false;
            return plan;
        }
    }

    if (ResolveGeneratedMap(plan) == nullptr && plan.includeMappingHash)
    {
        plan.warnings.push_back("No companion generated transport map is loaded. Generated mapping hash and transport ids may be unavailable.");
    }

    plan.canExecute = true;
    return plan;
}

DesignExportResult EditorDesignExportService::Execute(const DesignExportPlan& plan) const
{
    DesignExportResult result;
    result.outputFolder = plan.outputFolder;
    if (!plan.canExecute)
    {
        result.warnings.push_back(plan.error.empty() ? "Design export cannot execute." : plan.error);
        return result;
    }

    try
    {
        std::error_code directoryError;
        std::filesystem::create_directories(plan.outputFolder, directoryError);
        if (directoryError)
        {
            throw std::runtime_error("Unable to create export output folder: " + plan.outputFolder.string());
        }
    }
    catch (const std::exception& exception)
    {
        result.warnings.push_back(exception.what());
        return result;
    }

    result.warnings = plan.warnings;
    const std::string generatedAt = FormatDisplayTimestamp(std::chrono::system_clock::now());
    std::vector<bool> imageWritten(plan.pages.size(), false);

    for (std::size_t index = 0; index < plan.pages.size(); ++index)
    {
        const DesignExportPageArtifact& artifact = plan.pages[index];

        if (plan.exportExplodedViews && !artifact.imageFile.empty())
        {
            if (!IsPathWithinRoot(plan.outputFolder, artifact.imageFile))
            {
                result.warnings.push_back("Skipped one exploded-view export outside the output folder: " +
                                          artifact.imageFile.string());
            }
            else
            {
                std::string renderError;
                const bool rendered = explodedViewRenderer_ != nullptr
                                          ? explodedViewRenderer_(plan, artifact, &renderError)
                                          : RenderExplodedView(plan, artifact, &renderError);
                if (rendered)
                {
                    imageWritten[index] = true;
                    result.writtenFiles.push_back(artifact.imageFile);
                }
                else
                {
                    result.warnings.push_back("Exploded view for page '" + artifact.pageName + "' was not generated: " +
                                              (renderError.empty() ? std::string {"unknown error"} : renderError));
                }
            }
        }
    }

    try
    {
        if (plan.exportMarkdownIcd)
        {
            WriteTextFile(plan.readmeFile, BuildReadmeMarkdown(plan, generatedAt, imageWritten));
            result.writtenFiles.push_back(plan.readmeFile);

            WriteTextFile(plan.windowIcdFile, BuildWindowIcdMarkdown(plan));
            result.writtenFiles.push_back(plan.windowIcdFile);

            for (std::size_t index = 0; index < plan.pages.size(); ++index)
            {
                const DesignExportPageArtifact& artifact = plan.pages[index];
                WriteTextFile(artifact.markdownFile, BuildPageMarkdown(plan, artifact, imageWritten[index]));
                result.writtenFiles.push_back(artifact.markdownFile);
            }
        }

        WriteTextFile(plan.manifestFile,
                      BuildManifestJson(plan, generatedAt, imageWritten, result.warnings).dump(2));
        result.writtenFiles.push_back(plan.manifestFile);
    }
    catch (const std::exception& exception)
    {
        result.warnings.push_back(exception.what());
    }

    return result;
}

bool EditorDesignExportService::RenderExplodedView(const DesignExportPlan& plan,
                                                   const DesignExportPageArtifact& artifact,
                                                   std::string* error) const
{
    if (artifact.pageIndex >= plan.loaded.document.pages.size())
    {
        if (error != nullptr)
        {
            *error = "Invalid page index.";
        }
        return false;
    }

    if (!IsWindowReady())
    {
        if (error != nullptr)
        {
            *error = "raylib is not ready, so the exploded designer view could not be rendered.";
        }
        return false;
    }

    const mfd::PageDefinition& page = plan.loaded.document.pages[artifact.pageIndex];
    LogicalBounds pageBounds;
    for (const auto& reticle : page.staticReticles)
    {
        IncludeLogicalBounds(pageBounds, ComputeReticleWorldBounds(reticle));
    }
    if (page.strobe.has_value())
    {
        IncludeLogicalBounds(pageBounds, ComputeReticleWorldBounds(page.strobe->reticle));
    }

    if (!pageBounds.valid)
    {
        pageBounds.valid = true;
        pageBounds.min = {-1.0f, -1.0f};
        pageBounds.max = {1.0f, 1.0f};
        pageBounds.center = {0.0f, 0.0f};
    }

    ExportViewport viewport;
    viewport.width = kExplodedCanvasSize;
    viewport.height = kExplodedCanvasSize;
    viewport.view = MakeViewFittingBounds(pageBounds, viewport.width, viewport.height);

    const std::vector<ExplodedLabel> labels = BuildExplodedLabels(page, viewport, plan.includeCanvasCoordinates);

    const Font font = GetFontDefault();
    float maxLabelWidth = 220.0f;
    for (const auto& label : labels)
    {
        maxLabelWidth =
            std::max(maxLabelWidth, MeasureTextEx(font, label.title.c_str(), kExplodedLabelFontSize, 1.0f).x);
        maxLabelWidth =
            std::max(maxLabelWidth, MeasureTextEx(font, label.detail.c_str(), kExplodedDetailFontSize, 0.5f).x);
    }

    const int finalHeight = std::max(kExplodedMinimumHeight,
                                     static_cast<int>(std::ceil(140.0f + labels.size() * kExplodedLabelSpacing)));
    const int finalWidth = kExplodedCanvasMargin * 2 + kExplodedCanvasSize + kExplodedLabelGutter +
                           static_cast<int>(std::ceil(maxLabelWidth)) + 120;

    bool previewTextureStencilReady = false;
    RenderTexture2D previewTexture =
        mfd::LoadRenderTextureWithStencil(kExplodedCanvasSize, kExplodedCanvasSize, &previewTextureStencilReady);
    if (previewTexture.texture.id == 0U)
    {
        if (error != nullptr)
        {
            *error = "Unable to allocate the preview render texture.";
        }
        return false;
    }

    RenderTexture2D finalTexture = LoadRenderTexture(finalWidth, finalHeight);
    if (finalTexture.texture.id == 0U)
    {
        UnloadRenderTexture(previewTexture);
        if (error != nullptr)
        {
            *error = "Unable to allocate the final export texture.";
        }
        return false;
    }

    bool success = false;
    mfd::BezierPolylineCache bezierCache;
    mfd::ImageTextureCache imageCache;
    mfd::TextLayoutCache textLayoutCache;
    BeginTextureMode(previewTexture);
    ClearBackground(ToRayColor(page.backgroundColor));
    {
        mfd::Canvas2D canvas(previewTexture.texture.width,
                             previewTexture.texture.height,
                             viewport.view,
                             nullptr,
                             ToRayColor(page.backgroundColor),
                             previewTextureStencilReady,
                             &bezierCache,
                             &imageCache,
                             &textLayoutCache);
        for (const auto& reticle : page.staticReticles)
        {
            canvas.DrawReticle(reticle);
        }
        if (page.strobe.has_value())
        {
            canvas.DrawReticle(page.strobe->reticle);
        }
    }
    EndTextureMode();

    BeginTextureMode(finalTexture);
    ClearBackground(Color {247, 245, 239, 255});
    const Rectangle source {0.0f, 0.0f, static_cast<float>(previewTexture.texture.width),
                            -static_cast<float>(previewTexture.texture.height)};
    const Rectangle destination {static_cast<float>(kExplodedCanvasMargin),
                                 static_cast<float>(kExplodedCanvasMargin + 28),
                                 static_cast<float>(kExplodedCanvasSize),
                                 static_cast<float>(kExplodedCanvasSize)};
    DrawRectangleRounded(Rectangle {destination.x - 10.0f, destination.y - 10.0f, destination.width + 20.0f,
                                    destination.height + 20.0f},
                         0.02f,
                         8,
                         Color {232, 228, 220, 255});
    DrawTexturePro(previewTexture.texture, source, destination, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
    DrawRectangleLinesEx(destination, 2.0f, Color {82, 92, 106, 255});
    DrawTextEx(font,
               page.name.c_str(),
               Vector2 {static_cast<float>(kExplodedCanvasMargin), 12.0f},
               kExplodedTitleFontSize,
               1.0f,
               Color {33, 41, 52, 255});
    DrawTextEx(font,
               "Exploded designer view",
               Vector2 {static_cast<float>(kExplodedCanvasMargin), static_cast<float>(kExplodedCanvasMargin + kExplodedCanvasSize + 40)},
               kExplodedDetailFontSize,
               0.5f,
               Color {94, 104, 118, 255});

    const float labelX = destination.x + destination.width + static_cast<float>(kExplodedLabelGutter);
    float labelY = 88.0f;
    for (const auto& label : labels)
    {
        const Vector2 lineEnd {labelX - 12.0f, labelY + 13.0f};
        const Vector2 labelPoint {lineEnd.x, lineEnd.y};
        const Vector2 lineStart = ResolveExplodedLabelTarget(
            label, Vector2 {labelPoint.x - destination.x, labelPoint.y - destination.y});
        const Vector2 finalLineStart {destination.x + lineStart.x, destination.y + lineStart.y};
        const Color labelColor = label.visible ? Color {33, 41, 52, 255} : Color {125, 129, 136, 255};
        const Color detailColor = label.visible ? Color {95, 104, 118, 255} : Color {150, 154, 160, 255};
        const Color lineColor = label.visible ? Color {73, 126, 107, 255} : Color {166, 170, 174, 255};

        DrawLineEx(finalLineStart, lineEnd, 1.5f, lineColor);
        DrawCircleV(finalLineStart, 3.0f, lineColor);
        DrawTextEx(font, label.title.c_str(), Vector2 {labelX, labelY}, kExplodedLabelFontSize, 1.0f, labelColor);
        DrawTextEx(font,
                   label.detail.c_str(),
                   Vector2 {labelX, labelY + 22.0f},
                   kExplodedDetailFontSize,
                   0.5f,
                   detailColor);
        labelY += kExplodedLabelSpacing;
    }
    EndTextureMode();

    Image finalImage = LoadImageFromTexture(finalTexture.texture);
    if (finalImage.data == nullptr)
    {
        if (error != nullptr)
        {
            *error = "Unable to read back the exploded-view texture.";
        }
    }
    else
    {
        ImageFlipVertical(&finalImage);
        EnsureParentFolder(artifact.imageFile);
        success = ExportImage(finalImage, artifact.imageFile.string().c_str());
        if (!success && error != nullptr)
        {
            *error = "ExportImage failed for '" + artifact.imageFile.string() + "'.";
        }
        UnloadImage(finalImage);
    }

    UnloadRenderTexture(finalTexture);
    UnloadRenderTexture(previewTexture);

    return success;
}
} // namespace editor
