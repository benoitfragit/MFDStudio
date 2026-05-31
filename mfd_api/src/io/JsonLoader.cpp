/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for JsonLoader.
 */

#include "mfd/io/JsonLoader.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "../runtime/RuntimeValidation.h"
#include "json/JsonNumberParsing.h"
#include "json/JsonValueHelpers.h"
#include "mfd/runtime/DocumentSemanticValidator.h"

namespace mfd
{
namespace
{
using json = nlohmann::json;

ReticleInfo ParseReticleInfo(const json& node);
ReticleBlinkState ParseReticleBlinkState(const json& node);
std::string ParseReticleLayerId(const json& node);
ReticleClipState ParseReticleClipState(const json& node);
std::vector<PageBlinkDefinition> ParsePageBlinkDefinitions(const json& node);
std::vector<PageLayerDefinition> ParsePageLayerDefinitions(const json& node);
PageEditorState ParsePageEditorState(const json& node);
std::vector<DynamicReticleLayerBinding> ParseDynamicReticleLayerBindings(const json& node);
StrobeCaptureConfig ParseStrobeCaptureConfig(const json& node);
StrobeMagnetConfig ParseStrobeMagnetConfig(const json& node);
PageStrobeDefinition ParsePageStrobe(const json& node,
                                     const ReticleLibrary& library,
                                     const std::filesystem::path& baseFolder);
PageViewState ParsePageViewState(const json& node);
WindowCommandTransportConfig ParseWindowCommandTransportConfig(const json& root);
WindowFeedbackTransportConfig ParseWindowFeedbackTransportConfig(const json& root);
void ApplyReticleTextOverrides(const json& node, ReticleGroup& group);
bool ResolveReticleBlinkState(const PageDefinition& page, ReticleGroup& reticle);
void ValidateGeneratedTransportMapAgainstDocument(const GeneratedTransportMap& map,
                                                  const std::filesystem::path& windowFile,
                                                  const WindowAssetDefinition& window,
                                                  const MfdDocument& document);

std::uint8_t ClampByte(const int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

int HexDigit(const char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f')
    {
        return 10 + (value - 'a');
    }

    if (value >= 'A' && value <= 'F')
    {
        return 10 + (value - 'A');
    }

    throw std::runtime_error("Invalid hexadecimal color value");
}

std::uint8_t ParseHexByte(const std::string_view value)
{
    return ClampByte(HexDigit(value[0]) * 16 + HexDigit(value[1]));
}

using json_loader_detail::CanonicalToken;
using json_loader_detail::FindEditorField;
using json_loader_detail::FindField;
using json_loader_detail::IsPageFileList;
using json_loader_detail::JsonScalarToString;
using json_loader_detail::LoadJsonFile;
using json_loader_detail::Lowercase;
using json_loader_detail::NormalizePath;
using json_loader_detail::ParseDurationMilliseconds;
using json_loader_detail::ParsePixelNumber;
using json_loader_detail::ParsePortNumber;
using json_loader_detail::ParsePositivePacketSize;
using json_loader_detail::ParseWindowPosition;
using json_loader_detail::ParseWindowSize;
using json_loader_detail::ResolvePath;
using json_loader_detail::TrimAsciiWhitespace;

using runtime_validation::kMaxAbsAngleDegrees;
using runtime_validation::kMaxAbsCoordinate;
using runtime_validation::kMaxAbsScale;
using runtime_validation::kMaxFilledPolygonPoints;
using runtime_validation::kMaxLogicalSize;
using runtime_validation::kMaxPrimitivePoints;
using runtime_validation::kMaxPrimitiveSegments;
using runtime_validation::kMaxTextBytes;
using runtime_validation::kMaxThickness;
using runtime_validation::kMaxZoom;

constexpr int kMaxWindowExtent = 16384;

float ParseFiniteFloat(const json& value, const char* fieldName)
{
    if (!value.is_number())
    {
        throw std::runtime_error(std::string(fieldName) + " must be numeric");
    }

    const float parsed = value.get<float>();
    if (!std::isfinite(parsed))
    {
        throw std::runtime_error(std::string(fieldName) + " must be finite");
    }

    return parsed;
}

void ValidateFiniteAbs(const float value, const char* fieldName, const float maxAbs)
{
    if (!std::isfinite(value))
    {
        throw std::runtime_error(std::string(fieldName) + " must be finite");
    }

    if (!runtime_validation::IsFiniteAbsWithin(value, maxAbs))
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidatePositiveFinite(const float value, const char* fieldName, const float maxValue)
{
    if (!std::isfinite(value) || value <= 0.0f)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive and finite");
    }

    if (!runtime_validation::IsPositiveFiniteWithin(value, maxValue))
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateVec2(const Vec2& value, const char* fieldName, const float maxAbs = kMaxAbsCoordinate)
{
    if (!runtime_validation::IsFiniteVec2(value))
    {
        throw std::runtime_error(std::string(fieldName) + " must contain finite coordinates");
    }

    if (!runtime_validation::IsValidVec2(value, maxAbs))
    {
        throw std::runtime_error(std::string(fieldName) + " exceeds runtime safety limits");
    }
}

void ValidateScale(const Vec2& value, const char* fieldName)
{
    ValidateVec2(value, fieldName, kMaxAbsScale);
}

void ValidateWindowExtent(const int value, const char* fieldName)
{
    if (value <= 0 || value > kMaxWindowExtent)
    {
        throw std::runtime_error(
            std::string(fieldName) + " must be in [1, " + std::to_string(kMaxWindowExtent) + "]");
    }
}

void ValidateSegmentCount(const int value, const char* fieldName)
{
    if (value < 2 || value > kMaxPrimitiveSegments)
    {
        throw std::runtime_error(std::string(fieldName) + " must stay in [2, " +
                                 std::to_string(kMaxPrimitiveSegments) + "]");
    }
}

void ValidatePointCount(const std::size_t value, const std::size_t minimum, const char* fieldName)
{
    if (value < minimum || value > kMaxPrimitivePoints)
    {
        throw std::runtime_error(std::string(fieldName) + " must stay in [" +
                                 std::to_string(minimum) + ", " +
                                 std::to_string(kMaxPrimitivePoints) + "]");
    }
}

void ValidatePrimitiveStyle(const PrimitiveStyle& style)
{
    if (!runtime_validation::IsPositiveFiniteWithin(style.thickness, kMaxThickness))
    {
        throw std::runtime_error("Primitive thickness exceeds runtime safety limits");
    }
}

void ValidateTextGeometry(const TextGeometry& geometry)
{
    ValidatePositiveFinite(geometry.fontSize, "Text font size", kMaxLogicalSize);
    ValidateFiniteAbs(geometry.letterSpacing, "Text letter spacing", kMaxLogicalSize);
    if (geometry.text.size() > kMaxTextBytes)
    {
        throw std::runtime_error("Text payload exceeds runtime safety limits");
    }
}

void ValidateTimeGeometry(const TimeGeometry& geometry)
{
    ValidatePositiveFinite(geometry.fontSize, "Time font size", kMaxLogicalSize);
    ValidateFiniteAbs(geometry.letterSpacing, "Time letter spacing", kMaxLogicalSize);
    if (geometry.format.size() > kMaxTextBytes)
    {
        throw std::runtime_error("Time format exceeds runtime safety limits");
    }
}

void ValidatePrimitiveForRuntime(const Primitive& primitive)
{
    ValidateVec2(primitive.transform.position, "Primitive position");
    ValidateFiniteAbs(primitive.transform.rotationDegrees, "Primitive rotation", kMaxAbsAngleDegrees);
    ValidateScale(primitive.transform.scale, "Primitive scale");
    ValidatePrimitiveStyle(primitive.style);

    std::visit(
        [](const auto& geometry)
        {
            using Geometry = std::decay_t<decltype(geometry)>;

            if constexpr (std::is_same_v<Geometry, TextGeometry>)
            {
                ValidateTextGeometry(geometry);
            }
            else if constexpr (std::is_same_v<Geometry, TimeGeometry>)
            {
                ValidateTimeGeometry(geometry);
            }
            else if constexpr (std::is_same_v<Geometry, LineGeometry>)
            {
                ValidateVec2(geometry.start, "Line start");
                ValidateVec2(geometry.end, "Line end");
            }
            else if constexpr (std::is_same_v<Geometry, CircleGeometry>)
            {
                ValidateFiniteAbs(geometry.radius, "Circle radius", kMaxLogicalSize);
            }
            else if constexpr (std::is_same_v<Geometry, RingGeometry>)
            {
                ValidateFiniteAbs(geometry.innerRadius, "Ring inner radius", kMaxLogicalSize);
                ValidateFiniteAbs(geometry.outerRadius, "Ring outer radius", kMaxLogicalSize);
                ValidateSegmentCount(geometry.segments, "Ring segments");
            }
            else if constexpr (std::is_same_v<Geometry, RectangleGeometry> ||
                               std::is_same_v<Geometry, EllipseGeometry> ||
                               std::is_same_v<Geometry, SquareGeometry> ||
                               std::is_same_v<Geometry, DiamondGeometry>)
            {
                ValidateFiniteAbs(geometry.width, "Primitive width", kMaxLogicalSize);
                ValidateFiniteAbs(geometry.height, "Primitive height", kMaxLogicalSize);
            }
            else if constexpr (std::is_same_v<Geometry, TriangleGeometry>)
            {
                for (const Vec2& point : geometry.points)
                {
                    ValidateVec2(point, "Triangle point");
                }
            }
            else if constexpr (std::is_same_v<Geometry, PolylineGeometry>)
            {
                ValidatePointCount(geometry.points.size(), 2U, "Polyline point count");
                if (geometry.closed && geometry.points.size() > kMaxFilledPolygonPoints)
                {
                    throw std::runtime_error("Closed polyline exceeds filled polygon runtime safety limits");
                }

                for (const Vec2& point : geometry.points)
                {
                    ValidateVec2(point, "Polyline point");
                }
            }
            else if constexpr (std::is_same_v<Geometry, BezierGeometry>)
            {
                ValidatePointCount(geometry.controlPoints.size(), 2U, "Bezier control point count");
                ValidateSegmentCount(geometry.segments, "Bezier segments");
                for (const Vec2& point : geometry.controlPoints)
                {
                    ValidateVec2(point, "Bezier control point");
                }
            }
            else if constexpr (std::is_same_v<Geometry, ArcGeometry>)
            {
                ValidateFiniteAbs(geometry.radius, "Arc radius", kMaxLogicalSize);
                ValidateFiniteAbs(geometry.startAngleDegrees, "Arc start angle", kMaxAbsAngleDegrees);
                ValidateFiniteAbs(geometry.endAngleDegrees, "Arc end angle", kMaxAbsAngleDegrees);
                ValidateSegmentCount(geometry.segments, "Arc segments");
            }
            else if constexpr (std::is_same_v<Geometry, ImageGeometry>)
            {
                ValidateFiniteAbs(geometry.width, "Image width", kMaxLogicalSize);
                ValidateFiniteAbs(geometry.height, "Image height", kMaxLogicalSize);
                if (geometry.file.string().size() > kMaxTextBytes)
                {
                    throw std::runtime_error("Image path exceeds runtime safety limits");
                }
            }
        },
        primitive.geometry);
}

void ValidateReticleGroupForRuntime(const ReticleGroup& group)
{
    ValidateVec2(group.transform.position, "Reticle position");
    ValidateFiniteAbs(group.transform.rotationDegrees, "Reticle rotation", kMaxAbsAngleDegrees);
    ValidateScale(group.transform.scale, "Reticle scale");

    if (group.overrides.thickness.has_value() &&
        (!std::isfinite(*group.overrides.thickness) ||
         *group.overrides.thickness <= 0.0f ||
         *group.overrides.thickness > kMaxThickness))
    {
        throw std::runtime_error("Reticle override thickness exceeds runtime safety limits");
    }

    if (!group.clipping.primitiveId.empty() && group.clipping.primitiveId.size() > kMaxTextBytes)
    {
        throw std::runtime_error("Reticle clipping primitive id exceeds runtime safety limits");
    }

    for (const Primitive& primitive : group.primitives)
    {
        ValidatePrimitiveForRuntime(primitive);
    }
}

void ValidatePageViewStateForRuntime(const PageViewState& view)
{
    ValidateVec2(view.center, "Page view center");
    ValidatePositiveFinite(view.zoom, "Page view zoom", kMaxZoom);
}

void ValidateStrobeCaptureConfigForRuntime(const StrobeCaptureConfig& config)
{
    ValidateFiniteAbs(config.radius, "Strobe capture radius", kMaxLogicalSize);
    ValidateVec2(config.size, "Strobe capture size", kMaxLogicalSize);
}

void ValidateStrobeMagnetConfigForRuntime(const StrobeMagnetConfig& config)
{
    ValidateFiniteAbs(config.radius, "Strobe magnet radius", kMaxLogicalSize);
    if (!std::isfinite(config.strength) || config.strength < 0.0f || config.strength > 1.0f)
    {
        throw std::runtime_error("Strobe magnet strength must stay in [0, 1]");
    }

    ValidatePositiveFinite(config.visualShapeSize, "Strobe visual shape size", kMaxLogicalSize);
}

void ValidatePageDefinitionForRuntime(const PageDefinition& page)
{
    ValidatePageViewStateForRuntime(page.view);

    for (const ReticleGroup& reticle : page.staticReticles)
    {
        ValidateReticleGroupForRuntime(reticle);
    }

    for (const PageStrobeDefinition& strobe : page.strobes)
    {
        ValidateReticleGroupForRuntime(strobe.reticle);
        ValidateStrobeCaptureConfigForRuntime(strobe.capture);
        ValidateStrobeMagnetConfigForRuntime(strobe.magnet);
    }
}

std::optional<std::string> ParseDefaultPageName(const json& root, const char* sourceLabel)
{
    const json* defaultPage = FindField(root, {"defaultPage"});
    if (defaultPage == nullptr || defaultPage->is_null())
    {
        return std::nullopt;
    }

    if (!defaultPage->is_string())
    {
        throw std::runtime_error(std::string(sourceLabel) + " defaultPage must be a string");
    }

    return defaultPage->get<std::string>();
}

std::vector<std::filesystem::path> ParsePageFileList(const json& root,
                                                     const std::filesystem::path& baseFolder)
{
    const json* pages = FindField(root, {"pageFiles", "pages", "pageJsons"});
    if (pages == nullptr || !pages->is_array())
    {
        throw std::runtime_error("Window JSON must contain a pages array of JSON file paths");
    }

    std::vector<std::filesystem::path> pageFiles;
    pageFiles.reserve(pages->size());

    for (const auto& entry : *pages)
    {
        if (entry.is_string())
        {
            pageFiles.push_back(ResolvePath(baseFolder, entry.get<std::string>()));
            continue;
        }

        if (entry.is_object())
        {
            const json* file = FindField(entry, {"file", "path", "json"});
            if (file != nullptr && file->is_string())
            {
                if (FindField(entry, {"default"}) != nullptr)
                {
                    throw std::runtime_error(
                        "pages[].default is no longer supported; use the root-level defaultPage field instead");
                }

                pageFiles.push_back(ResolvePath(baseFolder, file->get<std::string>()));
                continue;
            }
        }

        throw std::runtime_error(
            "Each page entry must be a JSON filename or an object containing file/path/json");
    }

    if (pageFiles.empty())
    {
        throw std::runtime_error("Window JSON must reference at least one page JSON file");
    }

    return pageFiles;
}

void ApplyDefaultPageName(MfdDocument& document,
                          const std::optional<std::string>& defaultPageName,
                          const std::string_view sourceLabel)
{
    for (auto& page : document.pages)
    {
        page.defaultPage = false;
    }

    if (!defaultPageName.has_value())
    {
        return;
    }

    const std::string normalizedDefaultPageName = NormalizePageName(*defaultPageName);
    for (auto& page : document.pages)
    {
        if (page.normalizedName == normalizedDefaultPageName)
        {
            page.defaultPage = true;
            return;
        }
    }

    throw std::runtime_error(
        "Unknown defaultPage '" + *defaultPageName + "' in " + std::string(sourceLabel));
}

std::uint8_t ParseChannelNumber(const json& value)
{
    if (!value.is_number())
    {
        throw std::runtime_error("Color channels must be numeric");
    }

    const double rawValue = value.get<double>();
    if (rawValue >= 0.0 && rawValue <= 1.0)
    {
        return ClampByte(static_cast<int>(std::lround(rawValue * 255.0)));
    }

    return ClampByte(static_cast<int>(std::lround(rawValue)));
}

std::uint8_t ParseOpacityNumber(const json& value)
{
    if (!value.is_number())
    {
        throw std::runtime_error("Opacity must be numeric");
    }

    const double rawValue = value.get<double>();
    if (rawValue >= 0.0 && rawValue <= 1.0)
    {
        return ClampByte(static_cast<int>(std::lround(rawValue * 255.0)));
    }

    if (rawValue >= 0.0 && rawValue <= 100.0)
    {
        return ClampByte(static_cast<int>(std::lround(rawValue * 255.0 / 100.0)));
    }

    return ClampByte(static_cast<int>(std::lround(rawValue)));
}

bool BeginsWith(const std::string_view value, const std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool LooksLikeHexColor(std::string_view value)
{
    value = TrimAsciiWhitespace(value);

    if (BeginsWith(value, "#"))
    {
        value.remove_prefix(1);
    }
    else if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
    {
        value.remove_prefix(2);
    }

    if (value.size() != 3 && value.size() != 4 && value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    return std::all_of(value.begin(),
                       value.end(),
                       [](const char character)
                       {
                           return std::isxdigit(static_cast<unsigned char>(character)) != 0;
                       });
}

std::optional<ColorRgba> TryParseNamedColor(const std::string_view value)
{
    static const std::array<std::pair<std::string_view, ColorRgba>, 21> palette {{
        {"transparent", {0, 0, 0, 0}},
        {"none", {0, 0, 0, 0}},
        {"black", {0, 0, 0, 255}},
        {"white", {255, 255, 255, 255}},
        {"green", {0, 255, 102, 255}},
        {"lime", {51, 255, 136, 255}},
        {"hud", {51, 255, 136, 255}},
        {"hudgreen", {51, 255, 136, 255}},
        {"mfdgreen", {51, 255, 136, 255}},
        {"cyan", {77, 224, 255, 255}},
        {"radar", {77, 224, 255, 255}},
        {"track", {77, 224, 255, 255}},
        {"amber", {255, 203, 107, 255}},
        {"yellow", {249, 214, 111, 255}},
        {"orange", {255, 165, 0, 255}},
        {"red", {255, 96, 96, 255}},
        {"warning", {255, 214, 102, 255}},
        {"danger", {255, 96, 96, 255}},
        {"friendly", {120, 255, 154, 255}},
        {"hostile", {255, 144, 112, 255}},
        {"ghost", {203, 255, 225, 255}},
    }};

    const std::string key = CanonicalToken(value);
    for (const auto& [name, color] : palette)
    {
        if (name == key)
        {
            return color;
        }
    }

    return std::nullopt;
}

ColorRgba ParseHexColor(std::string_view value)
{
    value = TrimAsciiWhitespace(value);

    if (BeginsWith(value, "#"))
    {
        value.remove_prefix(1);
    }
    else if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
    {
        value.remove_prefix(2);
    }

    std::string expandedValue;
    if (value.size() == 3 || value.size() == 4)
    {
        expandedValue.reserve(value.size() * 2);

        for (const char character : value)
        {
            expandedValue.push_back(character);
            expandedValue.push_back(character);
        }

        value = expandedValue;
    }

    if (value.size() != 6 && value.size() != 8)
    {
        throw std::runtime_error("Hex color must use RGB, RGBA, RRGGBB or RRGGBBAA format");
    }

    ColorRgba color;
    color.r = ParseHexByte(value.substr(0, 2));
    color.g = ParseHexByte(value.substr(2, 2));
    color.b = ParseHexByte(value.substr(4, 2));
    color.a = value.size() == 8 ? ParseHexByte(value.substr(6, 2)) : 255;
    return color;
}

std::vector<std::string_view> SplitArguments(const std::string_view value)
{
    std::vector<std::string_view> arguments;
    std::size_t start = 0;

    while (start <= value.size())
    {
        const std::size_t separator = value.find(',', start);
        const std::size_t end = separator == std::string_view::npos ? value.size() : separator;
        arguments.push_back(TrimAsciiWhitespace(value.substr(start, end - start)));

        if (separator == std::string_view::npos)
        {
            break;
        }

        start = separator + 1;
    }

    return arguments;
}

double ParseNumericToken(const std::string_view value)
{
    const std::string_view trimmed = TrimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        throw std::runtime_error("Numeric token cannot be empty");
    }

    std::size_t processedCharacters = 0;
    const double parsedValue = std::stod(std::string(trimmed), &processedCharacters);
    if (processedCharacters != trimmed.size())
    {
        throw std::runtime_error("Invalid numeric token");
    }

    return parsedValue;
}

std::uint8_t ParseChannelToken(std::string_view value)
{
    value = TrimAsciiWhitespace(value);

    if (!value.empty() && value.back() == '%')
    {
        const double percent = ParseNumericToken(value.substr(0, value.size() - 1));
        return ClampByte(static_cast<int>(std::lround(percent * 255.0 / 100.0)));
    }

    const double numericValue = ParseNumericToken(value);
    if (numericValue >= 0.0 && numericValue <= 1.0)
    {
        return ClampByte(static_cast<int>(std::lround(numericValue * 255.0)));
    }

    return ClampByte(static_cast<int>(std::lround(numericValue)));
}

std::optional<ColorRgba> TryParseRgbFunction(const std::string_view value)
{
    const std::string_view trimmed = TrimAsciiWhitespace(value);
    const std::string lowered = Lowercase(trimmed);

    const bool hasAlpha = BeginsWith(lowered, "rgba(");
    if (!hasAlpha && !BeginsWith(lowered, "rgb("))
    {
        return std::nullopt;
    }

    const std::size_t openParenthesis = trimmed.find('(');
    const std::size_t closeParenthesis = trimmed.rfind(')');
    if (openParenthesis == std::string_view::npos ||
        closeParenthesis == std::string_view::npos ||
        closeParenthesis <= openParenthesis)
    {
        throw std::runtime_error("Invalid rgb()/rgba() color format");
    }

    const auto arguments = SplitArguments(trimmed.substr(openParenthesis + 1,
                                                         closeParenthesis - openParenthesis - 1));
    const std::size_t expectedArgumentCount = hasAlpha ? 4U : 3U;
    if (arguments.size() != expectedArgumentCount)
    {
        throw std::runtime_error("rgb()/rgba() color format has an invalid number of channels");
    }

    ColorRgba color;
    color.r = ParseChannelToken(arguments[0]);
    color.g = ParseChannelToken(arguments[1]);
    color.b = ParseChannelToken(arguments[2]);
    color.a = hasAlpha ? ParseChannelToken(arguments[3]) : 255;
    return color;
}

ColorRgba ParseColor(const json& value)
{
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();

        if (const auto namedColor = TryParseNamedColor(text); namedColor.has_value())
        {
            return *namedColor;
        }

        if (const auto rgbColor = TryParseRgbFunction(text); rgbColor.has_value())
        {
            return *rgbColor;
        }

        if (LooksLikeHexColor(text))
        {
            return ParseHexColor(text);
        }

        throw std::runtime_error("Unknown color name or format: " + text);
    }

    if (value.is_array())
    {
        if (value.size() < 3 || value.size() > 4)
        {
            throw std::runtime_error("Color array must contain 3 or 4 channels");
        }

        ColorRgba color;
        color.r = ParseChannelNumber(value.at(0));
        color.g = ParseChannelNumber(value.at(1));
        color.b = ParseChannelNumber(value.at(2));
        color.a = value.size() == 4 ? ParseChannelNumber(value.at(3)) : 255;
        return color;
    }

    if (value.is_object())
    {
        if (const json* namedColor = FindField(value, {"name", "preset", "hex"}))
        {
            ColorRgba color = ParseColor(*namedColor);

            if (const json* alpha = FindField(value, {"a", "alpha"}))
            {
                color.a = ParseChannelNumber(*alpha);
            }
            else if (const json* opacity = FindField(value, {"opacity"}))
            {
                color.a = ParseOpacityNumber(*opacity);
            }

            return color;
        }

        if (const json* rgba = FindField(value, {"rgba"}))
        {
            ColorRgba color = ParseColor(*rgba);

            if (const json* opacity = FindField(value, {"opacity"}))
            {
                color.a = ParseOpacityNumber(*opacity);
            }

            return color;
        }

        if (const json* rgb = FindField(value, {"rgb"}))
        {
            ColorRgba color = ParseColor(*rgb);

            if (const json* alpha = FindField(value, {"a", "alpha"}))
            {
                color.a = ParseChannelNumber(*alpha);
            }
            else if (const json* opacity = FindField(value, {"opacity"}))
            {
                color.a = ParseOpacityNumber(*opacity);
            }

            return color;
        }

        ColorRgba color {
            ParseChannelNumber(value.value("r", json(0))),
            ParseChannelNumber(value.value("g", json(255))),
            ParseChannelNumber(value.value("b", json(0))),
            ParseChannelNumber(value.value("a", json(255)))};

        if (const json* opacity = FindField(value, {"opacity"}))
        {
            color.a = ParseOpacityNumber(*opacity);
        }

        return color;
    }

    throw std::runtime_error("Unsupported color format");
}

Vec2 ParseVec2(const json& value)
{
    if (value.is_array())
    {
        if (value.size() != 2)
        {
            throw std::runtime_error("Vec2 array must contain exactly 2 values");
        }

        return Vec2 {
            ParseFiniteFloat(value.at(0), "Vec2.x"),
            ParseFiniteFloat(value.at(1), "Vec2.y")};
    }

    if (value.is_object())
    {
        return Vec2 {
            ParseFiniteFloat(value.contains("x") ? value.at("x") : json(0.0f), "Vec2.x"),
            ParseFiniteFloat(value.contains("y") ? value.at("y") : json(0.0f), "Vec2.y")};
    }

    throw std::runtime_error("Unsupported Vec2 format");
}

Vec2 ParseScale(const json& value)
{
    if (value.is_number())
    {
        const float scalar = ParseFiniteFloat(value, "Scale");
        return Vec2 {scalar, scalar};
    }

    return ParseVec2(value);
}

std::optional<bool> ParseVisibleFlag(const json& node)
{
    if (const json* show = FindField(node, {"show", "visible"}))
    {
        return show->get<bool>();
    }

    if (const json* hidden = FindField(node, {"hidden"}))
    {
        return !hidden->get<bool>();
    }

    return std::nullopt;
}

std::optional<ColorRgba> ParseStrokeColorField(const json& node)
{
    if (const json* stroke = FindField(node, {"stroke", "color", "strokeColor"}))
    {
        return ParseColor(*stroke);
    }

    return std::nullopt;
}

std::optional<ColorRgba> ParseFillColorField(const json& node)
{
    if (const json* fill = FindField(node, {"fill", "fillColor"}))
    {
        if (fill->is_boolean())
        {
            return std::nullopt;
        }

        return ParseColor(*fill);
    }

    return std::nullopt;
}

std::optional<float> ParseThicknessField(const json& node)
{
    if (const json* width = FindField(node, {"lineWidth", "strokeWidth", "thickness", "strokeThickness"}))
    {
        return ParseFiniteFloat(*width, "Primitive thickness");
    }

    return std::nullopt;
}

LineStyle ParseLineStyle(const json& value)
{
    if (!value.is_string())
    {
        throw std::runtime_error("Line style must be a string");
    }

    const std::string token = CanonicalToken(value.get<std::string>());
    if (token == "solid" || token == "plain" || token == "full" || token == "plein")
    {
        return LineStyle::Solid;
    }

    if (token == "dotted" || token == "dot" || token == "pointille")
    {
        return LineStyle::Dotted;
    }

    if (token == "dashed" || token == "dash" || token == "tiret" || token == "tirets")
    {
        return LineStyle::Dashed;
    }

    throw std::runtime_error("Unsupported line style: " + value.get<std::string>());
}

std::optional<LineStyle> ParseLineStyleField(const json& node)
{
    if (const json* style = FindField(node, {"lineStyle", "strokeStyle", "strokePattern"}))
    {
        return ParseLineStyle(*style);
    }

    return std::nullopt;
}

Transform2D ParseTransform(const json& node);

PageTitleDecoration ParsePageTitleDecoration(const json& value)
{
    if (!value.is_string())
    {
        throw std::runtime_error("titleDisplay decoration must be a string");
    }

    const std::string token = CanonicalToken(value.get<std::string>());
    if (token == "none")
    {
        return PageTitleDecoration::None;
    }

    if (token == "underline" || token == "line")
    {
        return PageTitleDecoration::Underline;
    }

    if (token == "frame" || token == "box" || token == "border" || token == "encadre")
    {
        return PageTitleDecoration::Frame;
    }

    throw std::runtime_error("Unsupported titleDisplay decoration: " + value.get<std::string>());
}

PageTitleDisplayDefinition ParsePageTitleDisplay(const json& node)
{
    PageTitleDisplayDefinition display;

    if (const json* visible = FindField(node, {"visible", "enabled"}); visible != nullptr)
    {
        if (!visible->is_boolean())
        {
            throw std::runtime_error("titleDisplay visible must be a boolean");
        }
        display.visible = visible->get<bool>();
    }

    display.transform = ParseTransform(node);

    if (const auto stroke = ParseStrokeColorField(node); stroke.has_value())
    {
        display.color = *stroke;
    }

    if (const auto lineWidth = ParseThicknessField(node); lineWidth.has_value())
    {
        display.lineWidth = *lineWidth;
    }

    if (const auto lineStyle = ParseLineStyleField(node); lineStyle.has_value())
    {
        display.lineStyle = *lineStyle;
    }

    if (const json* decoration = FindField(node, {"decoration", "mode", "shape"}); decoration != nullptr)
    {
        display.decoration = ParsePageTitleDecoration(*decoration);
    }

    return display;
}

std::optional<float> ParseLetterSpacingField(const json& node)
{
    if (const json* spacing = FindField(node, {"letterSpacing", "spacing", "tracking"}))
    {
        return ParseFiniteFloat(*spacing, "Text letter spacing");
    }

    return std::nullopt;
}

std::optional<bool> ParseFilledFlag(const json& node)
{
    if (const json* filled = FindField(node, {"filled"}))
    {
        return filled->get<bool>();
    }

    if (const json* fill = FindField(node, {"fill"}))
    {
        if (fill->is_boolean())
        {
            return fill->get<bool>();
        }

        return true;
    }

    if (FindField(node, {"fillColor"}) != nullptr)
    {
        return true;
    }

    return std::nullopt;
}

bool ParsePrimitiveExposed(const json& node)
{
    if (const json* exposed = FindField(node, {"expose", "exposed", "public", "clientExpose", "runtimeControl"});
        exposed != nullptr && exposed->is_boolean())
    {
        return exposed->get<bool>();
    }

    if (const json* clientNode = FindField(node, {"client"}); clientNode != nullptr && clientNode->is_object())
    {
        if (const json* exposed = FindField(*clientNode, {"expose", "public"});
            exposed != nullptr && exposed->is_boolean())
        {
            return exposed->get<bool>();
        }
    }

    return false;
}

bool ParsePrimitiveReticleRotationSensitive(const json& node)
{
    if (const json* sensitive = FindField(
            node,
            {"reticleRotationSensitive", "followReticleRotation", "inheritReticleRotation"});
        sensitive != nullptr)
    {
        if (!sensitive->is_boolean())
        {
            throw std::runtime_error("reticleRotationSensitive must be a boolean");
        }

        return sensitive->get<bool>();
    }

    return true;
}

bool ParsePrimitiveReticleScaleSensitive(const json& node)
{
    if (const json* sensitive = FindField(
            node,
            {"reticleScaleSensitive", "followReticleScale", "inheritReticleScale"});
        sensitive != nullptr)
    {
        if (!sensitive->is_boolean())
        {
            throw std::runtime_error("reticleScaleSensitive must be a boolean");
        }

        return sensitive->get<bool>();
    }

    return true;
}

void ApplyTransformFields(const json& node, Transform2D& transform)
{
    if (const json* position = FindField(node, {"position", "at", "pos"}))
    {
        transform.position = ParseVec2(*position);
    }

    if (node.contains("x") || node.contains("y"))
    {
        transform.position = {
            ParseFiniteFloat(node.contains("x") ? node.at("x") : json(transform.position.x), "Transform x"),
            ParseFiniteFloat(node.contains("y") ? node.at("y") : json(transform.position.y), "Transform y")};
    }

    if (const json* angle = FindField(node, {"rotationDegrees", "angle", "rotation"}))
    {
        transform.rotationDegrees = ParseFiniteFloat(*angle, "Transform rotation");
    }

    if (const json* scale = FindField(node, {"scale", "zoom"}))
    {
        transform.scale = ParseScale(*scale);
    }

    if (node.contains("sx") || node.contains("sy") ||
        node.contains("scaleX") || node.contains("scaleY"))
    {
        transform.scale = {
            ParseFiniteFloat(
                node.contains("sx") ? node.at("sx") :
                (node.contains("scaleX") ? node.at("scaleX") : json(transform.scale.x)),
                "Transform scale x"),
            ParseFiniteFloat(
                node.contains("sy") ? node.at("sy") :
                (node.contains("scaleY") ? node.at("scaleY") : json(transform.scale.y)),
                "Transform scale y")};
    }
}

Transform2D ParseTransform(const json& node)
{
    Transform2D transform;

    if (node.contains("transform"))
    {
        ApplyTransformFields(node.at("transform"), transform);
    }

    ApplyTransformFields(node, transform);
    return transform;
}

void ApplyPageViewFields(const json& node, PageViewState& view)
{
    if (const json* center = FindField(node, {"center", "viewCenter", "zoomCenter"}))
    {
        view.center = ParseVec2(*center);
    }

    if (node.contains("x") || node.contains("y"))
    {
        view.center = {
            ParseFiniteFloat(node.contains("x") ? node.at("x") : json(view.center.x), "Page view x"),
            ParseFiniteFloat(node.contains("y") ? node.at("y") : json(view.center.y), "Page view y")};
    }

    if (const json* zoom = FindField(node, {"zoom", "zoomLevel"}))
    {
        view.zoom = ParseFiniteFloat(*zoom, "Page view zoom");
    }
}

PageViewState ParsePageViewState(const json& node)
{
    PageViewState view;

    if (node.contains("view"))
    {
        ApplyPageViewFields(node.at("view"), view);
    }

    ApplyPageViewFields(node, view);
    ValidatePageViewStateForRuntime(view);
    return view;
}

ReticleStyleOverride ParseOverrideFields(const json& node)
{
    ReticleStyleOverride overrides;

    if (const auto stroke = ParseStrokeColorField(node); stroke.has_value())
    {
        overrides.color = *stroke;
    }

    if (const auto thickness = ParseThicknessField(node); thickness.has_value())
    {
        overrides.thickness = *thickness;
    }

    if (const auto fillColor = ParseFillColorField(node); fillColor.has_value())
    {
        overrides.fillColor = *fillColor;
    }

    if (const auto filled = ParseFilledFlag(node); filled.has_value())
    {
        overrides.filled = *filled;
    }

    return overrides;
}

ReticleStyleOverride ParseReticleOverrides(const json& node)
{
    ReticleStyleOverride overrides;

    if (node.contains("style"))
    {
        overrides = MergeOverrides(overrides, ParseOverrideFields(node.at("style")));
    }

    overrides = MergeOverrides(overrides, ParseOverrideFields(node));

    if (node.contains("overrides"))
    {
        overrides = MergeOverrides(overrides, ParseOverrideFields(node.at("overrides")));
    }

    return overrides;
}

PrimitiveStyle ParsePrimitiveStyle(const json& node)
{
    PrimitiveStyle style;

    if (node.contains("style"))
    {
        const PrimitiveStyle styleOverrides = ParsePrimitiveStyle(node.at("style"));
        style.visible = styleOverrides.visible;
        style.color = styleOverrides.color;
        style.thickness = styleOverrides.thickness;
        style.lineStyle = styleOverrides.lineStyle;
        style.fillColor = styleOverrides.fillColor;
        style.filled = styleOverrides.filled;
    }

    if (const auto visible = ParseVisibleFlag(node); visible.has_value())
    {
        style.visible = *visible;
    }

    if (const auto stroke = ParseStrokeColorField(node); stroke.has_value())
    {
        style.color = *stroke;
    }

    if (const auto thickness = ParseThicknessField(node); thickness.has_value())
    {
        style.thickness = *thickness;
    }

    if (const auto lineStyle = ParseLineStyleField(node); lineStyle.has_value())
    {
        style.lineStyle = *lineStyle;
    }

    if (const auto fillColor = ParseFillColorField(node); fillColor.has_value())
    {
        style.fillColor = *fillColor;
    }

    if (const auto filled = ParseFilledFlag(node); filled.has_value())
    {
        style.filled = *filled;
    }

    return style;
}

std::vector<Vec2> ParsePointList(const json& value, const std::size_t minimumCount)
{
    if (!value.is_array())
    {
        throw std::runtime_error("Point list must be a JSON array");
    }

    ValidatePointCount(value.size(), minimumCount, "Point list size");

    std::vector<Vec2> points;
    points.reserve(value.size());

    for (const auto& point : value)
    {
        points.push_back(ParseVec2(point));
    }

    if (points.size() < minimumCount)
    {
        throw std::runtime_error("Point list does not contain enough points");
    }

    return points;
}

PrimitiveType ParsePrimitiveType(const std::string_view value)
{
    const auto lowered = CanonicalToken(value);

    if (lowered == "text")
    {
        return PrimitiveType::Text;
    }

    if (lowered == "time" || lowered == "clock" || lowered == "heure")
    {
        return PrimitiveType::Time;
    }

    if (lowered == "line")
    {
        return PrimitiveType::Line;
    }

    if (lowered == "circle")
    {
        return PrimitiveType::Circle;
    }

    if (lowered == "ring" || lowered == "annulus" || lowered == "donut")
    {
        return PrimitiveType::Ring;
    }

    if (lowered == "rectangle" || lowered == "rect" || lowered == "box")
    {
        return PrimitiveType::Rectangle;
    }

    if (lowered == "ellipse" || lowered == "oval")
    {
        return PrimitiveType::Ellipse;
    }

    if (lowered == "square")
    {
        return PrimitiveType::Square;
    }

    if (lowered == "diamond")
    {
        return PrimitiveType::Diamond;
    }

    if (lowered == "triangle")
    {
        return PrimitiveType::Triangle;
    }

    if (lowered == "polyline")
    {
        return PrimitiveType::Polyline;
    }

    if (lowered == "bezier")
    {
        return PrimitiveType::Bezier;
    }

    if (lowered == "arc")
    {
        return PrimitiveType::Arc;
    }

    if (lowered == "image" || lowered == "picture" || lowered == "sprite")
    {
        return PrimitiveType::Image;
    }

    throw std::runtime_error("Unknown primitive type: " + std::string(value));
}

Primitive ParsePrimitive(const json& node, const std::filesystem::path& baseFolder)
{
    if (!node.contains("type"))
    {
        throw std::runtime_error("Primitive must define a type");
    }

    Primitive primitive;
    primitive.id = node.value("id", "");
    primitive.type = ParsePrimitiveType(node.at("type").get<std::string>());
    primitive.transform = ParseTransform(node);
    primitive.style = ParsePrimitiveStyle(node);
    primitive.exposed = ParsePrimitiveExposed(node);
    primitive.reticleRotationSensitive = ParsePrimitiveReticleRotationSensitive(node);
    primitive.reticleScaleSensitive = ParsePrimitiveReticleScaleSensitive(node);

    switch (primitive.type)
    {
    case PrimitiveType::Text:
    {
        TextGeometry geometry;
        geometry.text = node.value("text", "");
        geometry.fontSize = node.value("fontSize", node.value("size", kDefaultTextFontSize));
        if (const auto letterSpacing = ParseLetterSpacingField(node); letterSpacing.has_value())
        {
            geometry.letterSpacing = *letterSpacing;
        }
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Time:
    {
        TimeGeometry geometry;
        geometry.format = node.value("format", node.value("pattern", geometry.format));
        geometry.utc = node.value("utc", node.value("useUtc", geometry.utc));
        geometry.fontSize = node.value("fontSize", node.value("size", kDefaultTextFontSize));
        if (const auto letterSpacing = ParseLetterSpacingField(node); letterSpacing.has_value())
        {
            geometry.letterSpacing = *letterSpacing;
        }
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Line:
    {
        LineGeometry geometry;
        if (node.contains("start"))
        {
            geometry.start = ParseVec2(node.at("start"));
        }
        else if (node.contains("from"))
        {
            geometry.start = ParseVec2(node.at("from"));
        }

        if (node.contains("end"))
        {
            geometry.end = ParseVec2(node.at("end"));
        }
        else if (node.contains("to"))
        {
            geometry.end = ParseVec2(node.at("to"));
        }

        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Circle:
    {
        CircleGeometry geometry;
        geometry.radius = node.value("radius", 10.0f);
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Ring:
    {
        RingGeometry geometry;
        const float outerRadius = node.value("outerRadius", node.value("radius", geometry.outerRadius));
        const float defaultBandWidth = std::max(0.0f, geometry.outerRadius - geometry.innerRadius);
        const float bandWidth = node.value("bandWidth", node.value("ringWidth", defaultBandWidth));
        geometry.outerRadius = std::max(0.0f, outerRadius);
        geometry.innerRadius = std::max(0.0f,
                                        node.value("innerRadius",
                                                   node.value("holeRadius",
                                                              geometry.outerRadius - std::max(0.0f, bandWidth))));
        geometry.innerRadius = std::min(geometry.innerRadius, geometry.outerRadius);
        geometry.segments = std::max(12, node.value("segments", geometry.segments));
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Rectangle:
    {
        RectangleGeometry geometry;
        const float size = node.value("size", 0.10f);
        geometry.width = node.value("width", node.value("diameterX", size));
        geometry.height = node.value("height", node.value("diameterY", size));
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Ellipse:
    {
        EllipseGeometry geometry;
        const float size = node.value("size", 0.10f);
        const float radius = node.value("radius", size * 0.5f);
        geometry.width = node.value("width",
                                    node.value("diameterX",
                                               node.value("radiusX", node.value("rx", radius)) * 2.0f));
        geometry.height = node.value("height",
                                     node.value("diameterY",
                                                node.value("radiusY", node.value("ry", radius)) * 2.0f));
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Square:
    {
        SquareGeometry geometry;
        const float size = node.value("size", 10.0f);
        geometry.width = node.value("width", size);
        geometry.height = node.value("height", size);
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Diamond:
    {
        DiamondGeometry geometry;
        const float size = node.value("size", 10.0f);
        geometry.width = node.value("width", size);
        geometry.height = node.value("height", size);
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Triangle:
    {
        TriangleGeometry geometry;
        if (!node.contains("points") || !node.at("points").is_array() || node.at("points").size() != 3)
        {
            throw std::runtime_error("triangle.points must contain exactly three points");
        }

        const auto points = ParsePointList(node.at("points"), 3);
        geometry.points = {points.at(0), points.at(1), points.at(2)};
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Polyline:
    {
        PolylineGeometry geometry;
        geometry.points = ParsePointList(node.at("points"), 2);
        geometry.closed = node.value("closed", false);
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Bezier:
    {
        BezierGeometry geometry;
        const bool hasControlPoints = node.contains("controlPoints");
        const bool hasPoints = node.contains("points");
        if (hasControlPoints && hasPoints)
        {
            throw std::runtime_error("bezier cannot define both controlPoints and points");
        }

        if (hasControlPoints)
        {
            geometry.controlPoints = ParsePointList(node.at("controlPoints"), 2);
        }
        else if (hasPoints)
        {
            geometry.controlPoints = ParsePointList(node.at("points"), 2);
        }
        else
        {
            throw std::runtime_error("bezier requires controlPoints or points");
        }

        geometry.segments = node.value("segments", 32);
        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Arc:
    {
        ArcGeometry geometry;
        geometry.radius = node.value("radius", geometry.radius);
        geometry.segments = node.value("segments", geometry.segments);

        if (const json* startAngle = FindField(node, {"startAngleDegrees", "startAngle", "fromDegrees", "angleStart"}))
        {
            geometry.startAngleDegrees = ParseFiniteFloat(*startAngle, "Arc start angle");
        }

        if (const json* endAngle = FindField(node, {"endAngleDegrees", "endAngle", "toDegrees", "angleEnd"}))
        {
            geometry.endAngleDegrees = ParseFiniteFloat(*endAngle, "Arc end angle");
        }

        primitive.geometry = std::move(geometry);
        break;
    }
    case PrimitiveType::Image:
    {
        ImageGeometry geometry;
        const json* imageFile = FindField(node, {"file", "image", "source", "path"});
        if (imageFile == nullptr || !imageFile->is_string())
        {
            throw std::runtime_error("image primitive requires a string file path");
        }

        geometry.file = ResolvePath(baseFolder, imageFile->get<std::string>());

        if (const json* size = FindField(node, {"size"}); size != nullptr)
        {
            if (size->is_number())
            {
                const float scalar = ParseFiniteFloat(*size, "Image size");
                geometry.width = scalar;
                geometry.height = scalar;
            }
            else
            {
                const Vec2 dimensions = ParseVec2(*size);
                geometry.width = dimensions.x;
                geometry.height = dimensions.y;
            }
        }

        if (const json* width = FindField(node, {"width"}))
        {
            geometry.width = ParseFiniteFloat(*width, "Image width");
        }

        if (const json* height = FindField(node, {"height"}))
        {
            geometry.height = ParseFiniteFloat(*height, "Image height");
        }

        geometry.width = std::max(0.001f, geometry.width);
        geometry.height = std::max(0.001f, geometry.height);
        primitive.geometry = std::move(geometry);
        break;
    }
    }

    return primitive;
}

ReticleGroup ParseInlineReticle(const json& node, const std::filesystem::path& baseFolder)
{
    if (!node.contains("elements"))
    {
        throw std::runtime_error("Inline reticle must declare an elements array");
    }

    ReticleGroup group;
    group.id = node.value("id", "");
    group.sourceTemplateId = node.value("sourceTemplateId", "");
    group.info = ParseReticleInfo(node);
    group.visible = ParseVisibleFlag(node).value_or(true);
    group.drawOnTop = node.value("drawOnTop", node.value("onTop", false));
    group.transform = ParseTransform(node);
    group.overrides = ParseReticleOverrides(node);
    group.layerId = ParseReticleLayerId(node);
    group.clipping = ParseReticleClipState(node);

    for (const auto& element : node.at("elements"))
    {
        group.primitives.push_back(ParsePrimitive(element, baseFolder));
    }

    ApplyReticleTextOverrides(node, group);
    ValidateReticleGroupForRuntime(group);

    return group;
}

ReticleGroup ParseReticle(const json& node,
                          const ReticleLibrary& library,
                          const std::filesystem::path& baseFolder)
{
    if (node.contains("template"))
    {
        const std::string templateId = node.at("template").get<std::string>();
        const auto iterator = library.find(templateId);
        if (iterator == library.end())
        {
            throw std::runtime_error("Unknown reticle template: " + templateId);
        }

        ReticleGroup group = InstantiateReticle(iterator->second,
                                                node.value("id", templateId),
                                                ParseTransform(node),
                                                ParseReticleOverrides(node));
        const ReticleInfo infoOverrides = ParseReticleInfo(node);
        if (!infoOverrides.label.empty())
        {
            group.info.label = infoOverrides.label;
        }

        if (!infoOverrides.category.empty())
        {
            group.info.category = infoOverrides.category;
        }

        group.info.metadata.insert(infoOverrides.metadata.begin(), infoOverrides.metadata.end());
        group.blink = ParseReticleBlinkState(node);
        group.visible = ParseVisibleFlag(node).value_or(group.visible);
        group.drawOnTop = node.value("drawOnTop", node.value("onTop", group.drawOnTop));
        group.layerId = ParseReticleLayerId(node);
        if (FindField(node, {"clipping", "clip"}) != nullptr)
        {
            group.clipping = ParseReticleClipState(node);
        }
        ApplyReticleTextOverrides(node, group);
        ValidateReticleGroupForRuntime(group);
        return group;
    }

    ReticleGroup group = ParseInlineReticle(node, baseFolder);
    group.blink = ParseReticleBlinkState(node);
    ValidateReticleGroupForRuntime(group);
    return group;
}

void ApplyReticleTextOverrides(const json& node, ReticleGroup& group)
{
    if (const json* text = FindField(node, {"text"}); text != nullptr && text->is_string())
    {
        for (auto& primitive : group.primitives)
        {
            if (auto* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->text = text->get<std::string>();
                break;
            }
        }
    }

    if (const json* texts = FindField(node, {"texts"}); texts != nullptr && texts->is_object())
    {
        for (const auto& [primitiveId, primitiveText] : texts->items())
        {
            if (!primitiveText.is_string())
            {
                throw std::runtime_error("Reticle texts overrides must be string values");
            }

            SetTextPrimitive(group, primitiveId, primitiveText.get<std::string>());
        }
    }

    if (const auto letterSpacing = ParseLetterSpacingField(node); letterSpacing.has_value())
    {
        for (auto& primitive : group.primitives)
        {
            if (auto* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->letterSpacing = *letterSpacing;
                break;
            }
            if (auto* geometry = std::get_if<TimeGeometry>(&primitive.geometry))
            {
                geometry->letterSpacing = *letterSpacing;
                break;
            }
        }
    }

    if (const json* letterSpacings = FindField(node, {"letterSpacings", "spacings"});
        letterSpacings != nullptr && letterSpacings->is_object())
    {
        for (const auto& [primitiveId, primitiveSpacing] : letterSpacings->items())
        {
            SetTextPrimitiveLetterSpacing(
                group,
                primitiveId,
                ParseFiniteFloat(primitiveSpacing, "Reticle letter spacing override"));
        }
    }
}

ReticleLibrary LoadReticleLibrary(const std::filesystem::path& folder)
{
    if (!std::filesystem::exists(folder))
    {
        throw std::runtime_error("Reticle library folder does not exist: " + folder.string());
    }

    std::vector<std::filesystem::path> jsonFiles;

    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        jsonFiles.push_back(entry.path());
    }

    std::sort(jsonFiles.begin(), jsonFiles.end());

    ReticleLibrary library;

    for (const auto& file : jsonFiles)
    {
        try
        {
            const json templateJson = LoadJsonFile(file);
            if (templateJson.contains("template"))
            {
                throw std::runtime_error("Template chaining is not supported in reticle library files");
            }

            ReticleGroup group = ParseInlineReticle(templateJson, file.parent_path());
            if (group.id.empty())
            {
                group.id = file.stem().string();
            }

            if (group.sourceTemplateId.empty())
            {
                group.sourceTemplateId = group.id;
            }

            library[group.id] = std::move(group);
        }
        catch (const std::exception& exception)
        {
            throw std::runtime_error(
                "Unable to load reticle library asset '" + file.string() + "': " + exception.what());
        }
    }

    return library;
}

ColorRgba ParseBackgroundColor(const json& node)
{
    if (const json* backgroundColor = FindField(node, {"backgroundColor", "background", "bgColor", "bg"}))
    {
        return ParseColor(*backgroundColor);
    }

    return ColorRgba {6, 14, 20, 255};
}

ReticleInfo ParseReticleInfo(const json& node)
{
    ReticleInfo info;

    if (const json* label = FindField(node, {"label", "name"}))
    {
        if (label->is_string())
        {
            info.label = label->get<std::string>();
        }
    }

    if (const json* category = FindField(node, {"category", "kind"}))
    {
        if (category->is_string())
        {
            info.category = category->get<std::string>();
        }
    }

    if (const json* metadata = FindField(node, {"metadata", "data"}))
    {
        if (!metadata->is_object())
        {
            throw std::runtime_error("Reticle metadata must be a JSON object");
        }

        for (auto iterator = metadata->begin(); iterator != metadata->end(); ++iterator)
        {
            info.metadata.emplace(iterator.key(), JsonScalarToString(iterator.value()));
        }
    }

    return info;
}

ReticleBlinkState ParseReticleBlinkState(const json& node)
{
    ReticleBlinkState blink;

    const json* blinkNode = FindField(node, {"blink", "blinking"});
    if (blinkNode == nullptr || blinkNode->is_null())
    {
        return blink;
    }

    if (blinkNode->is_boolean())
    {
        blink.enabled = blinkNode->get<bool>();
        return blink;
    }

    if (blinkNode->is_string())
    {
        blink.enabled = true;
        blink.typeName = blinkNode->get<std::string>();
        blink.normalizedTypeName = NormalizePageName(blink.typeName);
        return blink;
    }

    if (!blinkNode->is_object())
    {
        throw std::runtime_error("blink must be a boolean, a string, or an object");
    }

    blink.enabled = blinkNode->value("enabled", true);

    if (const json* blinkType = FindField(*blinkNode, {"type", "name", "blinkType"}))
    {
        if (!blinkType->is_string())
        {
            throw std::runtime_error("blink.type must be a string");
        }

        blink.typeName = blinkType->get<std::string>();
        blink.normalizedTypeName = NormalizePageName(blink.typeName);
    }

    return blink;
}

std::string ParseReticleLayerId(const json& node)
{
    if (const json* layer = FindField(node, {"layerId"}); layer != nullptr && !layer->is_null())
    {
        if (!layer->is_string())
        {
            throw std::runtime_error("Reticle layerId must be a string");
        }

        return layer->get<std::string>();
    }

    return {};
}

ReticleClipState ParseReticleClipState(const json& node)
{
    ReticleClipState clipping;

    const json* clippingNode = FindField(node, {"clipping", "clip"});
    if (clippingNode == nullptr || clippingNode->is_null())
    {
        return clipping;
    }

    if (!clippingNode->is_object())
    {
        throw std::runtime_error("clipping must be a JSON object");
    }

    const json* modeNode = FindField(*clippingNode, {"mode", "type"});
    if (modeNode == nullptr || !modeNode->is_string())
    {
        throw std::runtime_error("clipping.mode must be a string");
    }

    const std::string modeToken = CanonicalToken(modeNode->get<std::string>());
    if (modeToken.empty() || modeToken == "none" || modeToken == "disabled" || modeToken == "off")
    {
        clipping.mode = ReticleClipMode::None;
    }
    else if (modeToken == "inner" || modeToken == "clipinner" || modeToken == "inside" || modeToken == "clipinside")
    {
        clipping.mode = ReticleClipMode::Inner;
    }
    else if (modeToken == "outer" || modeToken == "clipouter" || modeToken == "outside" || modeToken == "clipoutside")
    {
        clipping.mode = ReticleClipMode::Outer;
    }
    else
    {
        throw std::runtime_error("Unsupported clipping mode: " + modeNode->get<std::string>());
    }

    if (const json* primitiveNode = FindField(*clippingNode, {"primitive", "primitiveId", "mask"});
        primitiveNode != nullptr && !primitiveNode->is_null())
    {
        if (!primitiveNode->is_string())
        {
            throw std::runtime_error("clipping.primitive must be a string");
        }

        clipping.primitiveId = primitiveNode->get<std::string>();
    }

    return clipping;
}

std::vector<PageBlinkDefinition> ParsePageBlinkDefinitions(const json& node)
{
    const json* blinkTypesNode = FindField(node, {"blinkTypes", "blinks"});
    if (blinkTypesNode == nullptr || blinkTypesNode->is_null())
    {
        return {};
    }

    if (!blinkTypesNode->is_array())
    {
        throw std::runtime_error("blinkTypes must be a JSON array");
    }

    std::vector<PageBlinkDefinition> blinkTypes;
    blinkTypes.reserve(blinkTypesNode->size());
    std::unordered_set<std::string> normalizedNames;

    for (const auto& entry : *blinkTypesNode)
    {
        if (!entry.is_object())
        {
            throw std::runtime_error("Each blink type must be a JSON object");
        }

        PageBlinkDefinition blinkType;

        if (const json* name = FindField(entry, {"name", "id", "type"}); name != nullptr && name->is_string())
        {
            blinkType.name = name->get<std::string>();
        }

        blinkType.normalizedName = NormalizePageName(blinkType.name);
        if (blinkType.normalizedName.empty())
        {
            throw std::runtime_error("Each blink type must define a non-empty name");
        }

        if (const json* durationMs = FindField(entry, {"durationMs", "periodMs", "duration", "period"}))
        {
            blinkType.durationMs = ParseDurationMilliseconds(*durationMs, "blink duration");
        }
        else
        {
            throw std::runtime_error("Each blink type must define durationMs");
        }

        if (!normalizedNames.insert(blinkType.normalizedName).second)
        {
            throw std::runtime_error("Duplicate blink type name: " + blinkType.name);
        }

        blinkTypes.push_back(std::move(blinkType));
    }

    return blinkTypes;
}

std::vector<PageLayerDefinition> ParsePageLayerDefinitions(const json& node)
{
    const json* layersNode = FindField(node, {"layers"});
    if (layersNode == nullptr || layersNode->is_null())
    {
        return {};
    }

    if (!layersNode->is_array())
    {
        throw std::runtime_error("Page layers must be an array");
    }

    std::vector<PageLayerDefinition> layers;
    layers.reserve(layersNode->size());
    std::unordered_set<std::string> normalizedLayerIds;

    for (const auto& entry : *layersNode)
    {
        if (!entry.is_object())
        {
            throw std::runtime_error("Each page layer must be an object");
        }

        if (!entry.contains("id") || !entry.at("id").is_string())
        {
            throw std::runtime_error("Each page layer must define a string id");
        }

        PageLayerDefinition layer;
        layer.id = entry.at("id").get<std::string>();
        const std::string normalizedLayerId = NormalizePageName(layer.id);
        if (normalizedLayerId.empty())
        {
            throw std::runtime_error("Page layer id cannot be empty");
        }

        if (!normalizedLayerIds.insert(normalizedLayerId).second)
        {
            throw std::runtime_error("Duplicate page layer id: " + layer.id);
        }

        layers.push_back(std::move(layer));
    }

    return layers;
}

PageEditorState ParsePageEditorState(const json& node)
{
    PageEditorState editorState;
    const json* layers = FindEditorField(node, {"layers"});
    if (layers != nullptr && !layers->is_null())
    {
        if (!layers->is_array())
        {
            throw std::runtime_error("Page editor layers must be an array");
        }

        std::unordered_set<std::string> layerIds;
        for (const auto& entry : *layers)
        {
            if (!entry.is_object())
            {
                throw std::runtime_error("Each page editor layer must be an object");
            }

            if (!entry.contains("id") || !entry.at("id").is_string())
            {
                throw std::runtime_error("Each page editor layer must define a string id");
            }

            EditorLayerDefinition layer;
            layer.id = entry.at("id").get<std::string>();
            const std::string normalizedLayerId = NormalizePageName(layer.id);
            if (normalizedLayerId.empty())
            {
                throw std::runtime_error("Page editor layer id cannot be empty");
            }

            if (!layerIds.insert(normalizedLayerId).second)
            {
                throw std::runtime_error("Duplicate page editor layer id: " + layer.id);
            }

            if (const json* visible = FindField(entry, {"visible"}); visible != nullptr && !visible->is_null())
            {
                if (!visible->is_boolean())
                {
                    throw std::runtime_error("Page editor layer visibility must be a boolean");
                }

                layer.visible = visible->get<bool>();
            }

            editorState.layers.push_back(std::move(layer));
        }
    }

    return editorState;
}

std::vector<DynamicReticleLayerBinding> ParseDynamicReticleLayerBindings(const json& node)
{
    const json* bindingsNode = FindField(node, {"dynamicReticleBindings"});
    if (bindingsNode == nullptr || bindingsNode->is_null())
    {
        return {};
    }

    if (!bindingsNode->is_array())
    {
        throw std::runtime_error("dynamicReticleBindings must be an array");
    }

    std::vector<DynamicReticleLayerBinding> bindings;
    bindings.reserve(bindingsNode->size());
    for (const auto& entry : *bindingsNode)
    {
        if (!entry.is_object())
        {
            throw std::runtime_error("Each dynamic reticle binding must be an object");
        }

        if (!entry.contains("templateId") || !entry.at("templateId").is_string())
        {
            throw std::runtime_error("Each dynamic reticle binding must define a string templateId");
        }

        if (!entry.contains("layerId") || !entry.at("layerId").is_string())
        {
            throw std::runtime_error("Each dynamic reticle binding must define a string layerId");
        }

        if (!entry.contains("orderInLayer") || !entry.at("orderInLayer").is_number_integer())
        {
            throw std::runtime_error("Each dynamic reticle binding must define an integer orderInLayer");
        }

        DynamicReticleLayerBinding binding;
        binding.templateId = entry.at("templateId").get<std::string>();
        binding.layerId = entry.at("layerId").get<std::string>();
        binding.orderInLayer = entry.at("orderInLayer").get<int>();
        bindings.push_back(std::move(binding));
    }

    return bindings;
}

bool ResolveReticleBlinkState(const PageDefinition& page, ReticleGroup& reticle)
{
    if (!reticle.blink.enabled && reticle.blink.normalizedTypeName.empty())
    {
        reticle.blink.durationMs = 0;
        return true;
    }

    if (page.blinkTypes.empty())
    {
        return false;
    }

    if (!reticle.blink.normalizedTypeName.empty())
    {
        if (const PageBlinkDefinition* blinkType = FindPageBlinkDefinition(page, reticle.blink.normalizedTypeName);
            blinkType != nullptr)
        {
            reticle.blink.durationMs = blinkType->durationMs;
            return true;
        }

        return false;
    }

    const PageBlinkDefinition* defaultBlinkType = nullptr;
    if (!page.normalizedDefaultBlinkTypeName.empty())
    {
        defaultBlinkType = FindPageBlinkDefinition(page, page.normalizedDefaultBlinkTypeName);
    }

    if (defaultBlinkType == nullptr && !page.blinkTypes.empty())
    {
        defaultBlinkType = &page.blinkTypes.front();
    }

    if (defaultBlinkType == nullptr)
    {
        return false;
    }

    reticle.blink.durationMs = defaultBlinkType->durationMs;
    return true;
}

StrobeCaptureShape ParseStrobeCaptureShape(const std::string_view value)
{
    const std::string lowered = CanonicalToken(value);

    if (lowered == "circle" || lowered == "radius")
    {
        return StrobeCaptureShape::Circle;
    }

    if (lowered == "rectangle" || lowered == "rect" || lowered == "box" || lowered == "square")
    {
        return StrobeCaptureShape::Rectangle;
    }

    throw std::runtime_error("Unknown strobe capture shape: " + std::string(value));
}

StrobeMagnetVisualShape ParseStrobeMagnetVisualShape(const std::string_view value)
{
    const std::string lowered = CanonicalToken(value);

    if (lowered == "circle" || lowered == "round")
    {
        return StrobeMagnetVisualShape::Circle;
    }

    if (lowered == "square" || lowered == "box")
    {
        return StrobeMagnetVisualShape::Square;
    }

    throw std::runtime_error("Unknown strobe magnet visual shape: " + std::string(value));
}

StrobeCaptureConfig ParseStrobeCaptureConfig(const json& node)
{
    StrobeCaptureConfig config;

    if (const json* captureNode = FindField(node, {"capture"}))
    {
        return ParseStrobeCaptureConfig(*captureNode);
    }

    if (const json* shape = FindField(node, {"shape"}))
    {
        config.shape = ParseStrobeCaptureShape(shape->get<std::string>());
    }

    if (const json* radius = FindField(node, {"radius", "captureRadius"}))
    {
        config.radius = ParseFiniteFloat(*radius, "Strobe capture radius");
    }

    if (const json* size = FindField(node, {"size", "captureSize"}))
    {
        if (size->is_number())
        {
            const float scalar = ParseFiniteFloat(*size, "Strobe capture size");
            config.size = {scalar, scalar};
        }
        else
        {
            config.size = ParseVec2(*size);
        }
    }

    if (node.contains("width") || node.contains("height"))
    {
        config.size = {
            ParseFiniteFloat(node.contains("width") ? node.at("width") : json(config.size.x), "Strobe capture width"),
            ParseFiniteFloat(node.contains("height") ? node.at("height") : json(config.size.y), "Strobe capture height")};
    }

    return config;
}

StrobeMagnetConfig ParseStrobeMagnetConfig(const json& node)
{
    StrobeMagnetConfig config;

    const json* magnetNode = FindField(node, {"magnet", "magnetization", "aimantation", "snap"});
    if (magnetNode == nullptr)
    {
        return config;
    }

    if (magnetNode->is_boolean())
    {
        config.enabled = magnetNode->get<bool>();
        return config;
    }

    if (!magnetNode->is_object())
    {
        throw std::runtime_error("strobe.magnet must be a boolean or a JSON object");
    }

    config.enabled = magnetNode->value("enabled", true);

    if (const json* radius = FindField(*magnetNode, {"radius", "magnetRadius", "snapRadius", "distance"}))
    {
        config.radius = ParseFiniteFloat(*radius, "Strobe magnet radius");
    }

    if (const json* strength = FindField(*magnetNode, {"strength", "magnetStrength", "snapStrength", "blend"}))
    {
        config.strength = std::clamp(ParseFiniteFloat(*strength, "Strobe magnet strength"), 0.0f, 1.0f);
    }

    const json* visualNode =
        FindField(*magnetNode, {"visual", "visualCue", "visualStyle", "magnetizedVisual", "magnetizedShape"});
    if (visualNode != nullptr)
    {
        if (visualNode->is_boolean())
        {
            config.visualShapeEnabled = visualNode->get<bool>();
        }
        else if (visualNode->is_string())
        {
            config.visualShapeEnabled = true;
            config.visualShape = ParseStrobeMagnetVisualShape(visualNode->get<std::string>());
        }
        else if (visualNode->is_object())
        {
            config.visualShapeEnabled = visualNode->value("enabled", true);
            if (const json* shape = FindField(*visualNode, {"shape", "type"}))
            {
                config.visualShape = ParseStrobeMagnetVisualShape(shape->get<std::string>());
            }
            if (const json* size = FindField(*visualNode, {"size", "radius", "width"}))
            {
                config.visualShapeSize = std::max(0.001f, ParseFiniteFloat(*size, "Strobe visual shape size"));
            }
        }
        else
        {
            throw std::runtime_error("strobe.magnet.visual must be a boolean, string or JSON object");
        }
    }

    if (const json* shape = FindField(*magnetNode, {"visualShape", "magnetVisualShape"}))
    {
        config.visualShapeEnabled = true;
        config.visualShape = ParseStrobeMagnetVisualShape(shape->get<std::string>());
    }

    if (const json* visualEnabled = FindField(*magnetNode, {"visualShapeEnabled", "showVisualShape"}))
    {
        config.visualShapeEnabled = visualEnabled->get<bool>();
    }

    if (const json* visualSize = FindField(*magnetNode, {"visualShapeSize", "visualSize"}))
    {
        config.visualShapeSize = std::max(0.001f, ParseFiniteFloat(*visualSize, "Strobe visual shape size"));
    }

    return config;
}

PageStrobeDefinition ParsePageStrobe(const json& node,
                                     const ReticleLibrary& library,
                                     const std::filesystem::path& baseFolder,
                                     std::string defaultName)
{
    PageStrobeDefinition strobe;
    if (const json* name = FindField(node, {"name", "strobeName"}); name != nullptr && !name->is_null())
    {
        if (!name->is_string())
        {
            throw std::runtime_error("strobe.name must be a string");
        }

        defaultName = name->get<std::string>();
    }

    strobe.name = std::move(defaultName);
    strobe.normalizedName = NormalizePageName(strobe.name);
    if (strobe.normalizedName.empty())
    {
        throw std::runtime_error("Strobe name cannot be empty");
    }

    strobe.reticle = ParseReticle(node, library, baseFolder);
    strobe.reticle.layerId.clear();
    strobe.capture = ParseStrobeCaptureConfig(node);
    strobe.magnet = ParseStrobeMagnetConfig(node);
    return strobe;
}

WindowUdpCommandTransport ParseWindowUdpCommandTransport(const json& node)
{
    if (!node.is_object())
    {
        throw std::runtime_error("commands.udp must be a JSON object");
    }

    WindowUdpCommandTransport config;
    config.enabled = node.value("enabled", true);
    config.address = node.value("address", node.value("bindAddress", node.value("host", config.address)));

    if (const json* port = FindField(node, {"port", "listenPort"}))
    {
        config.port = ParsePortNumber(*port, "commands.udp.port");
    }

    if (const json* maxPacketSize = FindField(node, {"maxPacketSize", "packetSize", "bufferSize"}))
    {
        config.maxPacketSize = ParsePositivePacketSize(*maxPacketSize, "commands.udp.maxPacketSize");
    }

    return config;
}

WindowCommandTransportConfig ParseWindowCommandTransportConfig(const json& root)
{
    WindowCommandTransportConfig config;
    const json* commands = FindField(root, {"commands", "commandTransport", "commandTransports"});
    if (commands == nullptr)
    {
        return config;
    }

    if (!commands->is_object())
    {
        throw std::runtime_error("commands must be a JSON object");
    }

    if (const json* udp = FindField(*commands, {"udp"}))
    {
        config.udp = ParseWindowUdpCommandTransport(*udp);
    }

    return config;
}

WindowUdpFeedbackTransport ParseWindowUdpFeedbackTransport(const json& node)
{
    if (!node.is_object())
    {
        throw std::runtime_error("feedback.udp must be a JSON object");
    }

    WindowUdpFeedbackTransport config;
    config.enabled = node.value("enabled", true);
    config.address = node.value("address", node.value("bindAddress", node.value("host", config.address)));

    if (const json* port = FindField(node, {"port", "listenPort"}))
    {
        config.port = ParsePortNumber(*port, "feedback.udp.port");
    }

    if (const json* maxPacketSize = FindField(node, {"maxPacketSize", "packetSize", "bufferSize"}))
    {
        config.maxPacketSize = ParsePositivePacketSize(*maxPacketSize, "feedback.udp.maxPacketSize");
    }

    return config;
}

WindowFeedbackTransportConfig ParseWindowFeedbackTransportConfig(const json& root)
{
    WindowFeedbackTransportConfig config;
    const json* feedback = FindField(root,
                                     {"feedback", "feedbackTransport", "feedbackTransports", "strobeFeedback", "events"});
    if (feedback == nullptr)
    {
        return config;
    }

    if (!feedback->is_object())
    {
        throw std::runtime_error("feedback must be a JSON object");
    }

    if (const json* udp = FindField(*feedback, {"udp"}))
    {
        config.udp = ParseWindowUdpFeedbackTransport(*udp);
    }

    return config;
}

float ParsePositiveIntervalSeconds(const json& node, const char* fieldName)
{
    const float value = ParseFiniteFloat(node, fieldName);
    if (value <= 0.0f)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive");
    }

    return value;
}

float ParsePositiveIntervalMilliseconds(const json& node, const char* fieldName)
{
    return ParsePositiveIntervalSeconds(node, fieldName) / 1000.0f;
}

void ParseFeedbackCadence(const json& root, WindowAssetDefinition& window)
{
    const json* feedback = FindField(root,
                                     {"feedback", "feedbackTransport", "feedbackTransports", "strobeFeedback", "events"});
    if (feedback == nullptr || !feedback->is_object())
    {
        return;
    }

    if (const json* fastSeconds = FindField(*feedback, {"fastIntervalSeconds", "changedIntervalSeconds"}))
    {
        window.feedbackFastIntervalSeconds =
            ParsePositiveIntervalSeconds(*fastSeconds, "feedback.fastIntervalSeconds");
    }
    else if (const json* fastMilliseconds = FindField(*feedback, {"fastIntervalMs", "changedIntervalMs"}))
    {
        window.feedbackFastIntervalSeconds =
            ParsePositiveIntervalMilliseconds(*fastMilliseconds, "feedback.fastIntervalMs");
    }

    if (const json* heartbeatSeconds = FindField(*feedback, {"heartbeatIntervalSeconds", "unchangedIntervalSeconds"}))
    {
        window.feedbackHeartbeatIntervalSeconds =
            ParsePositiveIntervalSeconds(*heartbeatSeconds, "feedback.heartbeatIntervalSeconds");
    }
    else if (const json* heartbeatMilliseconds = FindField(*feedback, {"heartbeatIntervalMs", "unchangedIntervalMs"}))
    {
        window.feedbackHeartbeatIntervalSeconds =
            ParsePositiveIntervalMilliseconds(*heartbeatMilliseconds, "feedback.heartbeatIntervalMs");
    }

    if (window.feedbackHeartbeatIntervalSeconds < window.feedbackFastIntervalSeconds)
    {
        throw std::runtime_error("feedback.heartbeatIntervalSeconds must be greater than or equal to feedback.fastIntervalSeconds");
    }
}

const json* FindPageNode(const json& root) noexcept
{
    if (const json* page = FindField(root, {"page"}))
    {
        return page;
    }

    return &root;
}

WindowAssetDefinition ParseWindowAssetDefinition(const json& root,
                                                 const std::filesystem::path& windowFile)
{
    WindowAssetDefinition window;
    window.sourceFile = windowFile;
    window.title = root.value("title", window.title);

    if (const json* size = FindField(root, {"size", "dimensions", "windowSize"}))
    {
        ParseWindowSize(*size, window.width, window.height);
    }

    if (root.contains("width") || root.contains("height"))
    {
        window.width = ParsePixelNumber(root.value("width", json(window.width)), "window width");
        window.height = ParsePixelNumber(root.value("height", json(window.height)), "window height");
    }

    ValidateWindowExtent(window.width, "window width");
    ValidateWindowExtent(window.height, "window height");

    if (const json* position = FindField(root, {"position", "windowPosition", "screenPosition"}))
    {
        ParseWindowPosition(*position, window.positionX, window.positionY);
    }

    if (root.contains("x") || root.contains("y"))
    {
        window.positionX = ParsePixelNumber(root.value("x", json(window.positionX)), "window x");
        window.positionY = ParsePixelNumber(root.value("y", json(window.positionY)), "window y");
    }

    if (const json* targetFps = FindField(root, {"targetFps", "fps"}))
    {
        window.targetFps = ParsePixelNumber(*targetFps, "target fps");
    }

    if (const json* fontFile = FindField(root, {"fontFile", "font", "fontPath"}))
    {
        if (!fontFile->is_string())
        {
            throw std::runtime_error("fontFile must be a string path");
        }

        window.fontFile = ResolvePath(windowFile.parent_path(), fontFile->get<std::string>());
    }

    if (const json* iconFile = FindField(root, {"iconFile", "icon", "windowIcon", "windowIconFile", "iconPath"}))
    {
        if (!iconFile->is_string())
        {
            throw std::runtime_error("iconFile must be a string path");
        }

        window.iconFile = ResolvePath(windowFile.parent_path(), iconFile->get<std::string>());
    }

    window.reticleLibraryFolder = windowFile.parent_path();
    if (const json* folder = FindField(root, {"reticleLibraryFolder", "reticles", "reticleFolder"}))
    {
        if (!folder->is_string())
        {
            throw std::runtime_error("reticleLibraryFolder must be a string path");
        }

        window.reticleLibraryFolder = ResolvePath(windowFile.parent_path(), folder->get<std::string>());
    }

    window.pageFiles = ParsePageFileList(root, windowFile.parent_path());
    window.commandTransports = ParseWindowCommandTransportConfig(root);
    window.feedbackTransports = ParseWindowFeedbackTransportConfig(root);
    ParseFeedbackCadence(root, window);
    return window;
}

PageDefinition ParsePage(const json& node,
                         const ReticleLibrary& library,
                         const std::filesystem::path& baseFolder)
{
    std::string pageName;
    if (node.contains("name"))
    {
        pageName = node.at("name").get<std::string>();
    }
    else if (node.contains("id"))
    {
        pageName = node.at("id").get<std::string>();
    }
    else
    {
        throw std::runtime_error("Page must define a name or id");
    }

    const std::string normalizedPageName = NormalizePageName(pageName);
    if (normalizedPageName.empty())
    {
        throw std::runtime_error("Page name cannot be empty");
    }

    PageDefinition page;
    page.name = std::move(pageName);
    page.normalizedName = normalizedPageName;
    page.title = node.value("title", page.name);
    if (const json* titleDisplayNode = FindField(node, {"titleDisplay", "titleChrome"});
        titleDisplayNode != nullptr && !titleDisplayNode->is_null())
    {
        if (!titleDisplayNode->is_object())
        {
            throw std::runtime_error("titleDisplay must be an object");
        }

        page.titleDisplay = ParsePageTitleDisplay(*titleDisplayNode);
    }
    page.backgroundColor = ParseBackgroundColor(node);
    page.view = ParsePageViewState(node);
    page.blinkTypes = ParsePageBlinkDefinitions(node);
    page.layers = ParsePageLayerDefinitions(node);
    page.editor = ParsePageEditorState(node);
    page.dynamicReticleBindings = ParseDynamicReticleLayerBindings(node);

    if (page.layers.empty())
    {
        throw std::runtime_error("Page must define a non-empty layers array: " + page.name);
    }

    if (const json* defaultBlink = FindField(node, {"defaultBlink", "defaultBlinkType"});
        defaultBlink != nullptr && !defaultBlink->is_null())
    {
        if (!defaultBlink->is_string())
        {
            throw std::runtime_error("defaultBlink must be a string");
        }

        page.defaultBlinkTypeName = defaultBlink->get<std::string>();
        page.normalizedDefaultBlinkTypeName = NormalizePageName(page.defaultBlinkTypeName);
    }

    if (!page.normalizedDefaultBlinkTypeName.empty() &&
        FindPageBlinkDefinition(page, page.normalizedDefaultBlinkTypeName) == nullptr)
    {
        throw std::runtime_error(
            "Unknown default blink type '" + page.defaultBlinkTypeName + "' on page: " + page.name);
    }

    if (node.contains("staticReticles"))
    {
        for (const auto& reticle : node.at("staticReticles"))
        {
            page.staticReticles.push_back(ParseReticle(reticle, library, baseFolder));
        }
    }

    if (const json* activeStrobe = FindField(node, {"activeStrobe", "defaultStrobe"});
        activeStrobe != nullptr && !activeStrobe->is_null())
    {
        if (!activeStrobe->is_string())
        {
            throw std::runtime_error("activeStrobe must be a string");
        }

        page.activeStrobeName = activeStrobe->get<std::string>();
        page.normalizedActiveStrobeName = NormalizePageName(page.activeStrobeName);
        if (page.normalizedActiveStrobeName.empty())
        {
            throw std::runtime_error("activeStrobe cannot be empty");
        }
    }

    if (node.contains("strobes"))
    {
        const json& strobes = node.at("strobes");
        if (!strobes.is_array())
        {
            throw std::runtime_error("strobes must be a JSON array");
        }

        page.strobes.reserve(strobes.size());
        for (std::size_t index = 0; index < strobes.size(); ++index)
        {
            page.strobes.push_back(ParsePageStrobe(
                strobes.at(index),
                library,
                baseFolder,
                index == 0U ? std::string {"Default"} : ("Strobe" + std::to_string(index))));
        }
    }
    else if (node.contains("strobe"))
    {
        page.strobes.push_back(ParsePageStrobe(node.at("strobe"), library, baseFolder, "Default"));
    }

    if (!page.strobes.empty() && page.normalizedActiveStrobeName.empty())
    {
        page.activeStrobeName = page.strobes.front().name;
        page.normalizedActiveStrobeName = page.strobes.front().normalizedName;
    }

    for (auto& reticle : page.staticReticles)
    {
        if (!ResolveReticleBlinkState(page, reticle))
        {
            const std::string blinkType =
                reticle.blink.typeName.empty() ? std::string {"<default>"} : reticle.blink.typeName;
            throw std::runtime_error(
                "Unknown blink type '" + blinkType + "' for reticle '" + reticle.id + "' on page: " + page.name);
        }
    }

    for (auto& strobe : page.strobes)
    {
        if (ResolveReticleBlinkState(page, strobe.reticle))
        {
            continue;
        }

        const std::string blinkType =
            strobe.reticle.blink.typeName.empty() ? std::string {"<default>"} : strobe.reticle.blink.typeName;
        throw std::runtime_error(
            "Unknown blink type '" + blinkType + "' for strobe '" + strobe.reticle.id + "' on page: " + page.name);
    }

    std::unordered_set<std::string> reticleIds;
    std::unordered_set<std::string> pageLayerIds;
    for (const auto& layer : page.layers)
    {
        pageLayerIds.insert(NormalizePageName(layer.id));
    }

    for (const auto& editorLayer : page.editor.layers)
    {
        if (pageLayerIds.find(NormalizePageName(editorLayer.id)) == pageLayerIds.end())
        {
            throw std::runtime_error(
                "Unknown runtime layer '" + editorLayer.id + "' referenced by page editor state on page: " + page.name);
        }
    }

    std::unordered_set<std::string> normalizedDynamicTemplateIds;
    std::unordered_map<std::string, std::unordered_set<int>> bindingOrdersByLayer;
    for (const auto& binding : page.dynamicReticleBindings)
    {
        const std::string normalizedTemplateId = NormalizePageName(binding.templateId);
        if (normalizedTemplateId.empty())
        {
            throw std::runtime_error("Dynamic reticle bindings must define a non-empty templateId on page: " + page.name);
        }

        if (library.find(binding.templateId) == library.end())
        {
            throw std::runtime_error(
                "Unknown dynamic reticle template '" + binding.templateId + "' on page: " + page.name);
        }

        if (!normalizedDynamicTemplateIds.insert(normalizedTemplateId).second)
        {
            throw std::runtime_error(
                "Duplicate dynamic reticle binding template '" + binding.templateId + "' on page: " + page.name);
        }

        const std::string normalizedLayerId = NormalizePageName(binding.layerId);
        if (normalizedLayerId.empty())
        {
            throw std::runtime_error("Dynamic reticle bindings must define a non-empty layerId on page: " + page.name);
        }

        if (pageLayerIds.find(normalizedLayerId) == pageLayerIds.end())
        {
            throw std::runtime_error(
                "Unknown runtime layer '" + binding.layerId + "' for dynamic reticle binding '" + binding.templateId +
                "' on page: " + page.name);
        }

        if (!bindingOrdersByLayer[normalizedLayerId].insert(binding.orderInLayer).second)
        {
            throw std::runtime_error(
                "Duplicate dynamic reticle binding orderInLayer " + std::to_string(binding.orderInLayer) +
                " on layer '" + binding.layerId + "' for page: " + page.name);
        }
    }

    for (const auto& reticle : page.staticReticles)
    {
        const std::string normalizedReticleId = NormalizePageName(reticle.id);
        if (normalizedReticleId.empty())
        {
            throw std::runtime_error("Static reticles must define a non-empty id on page: " + page.name);
        }

        if (reticle.layerId.empty())
        {
            throw std::runtime_error(
                "Static reticle '" + reticle.id + "' must define layerId on page: " + page.name);
        }

        if (pageLayerIds.find(NormalizePageName(reticle.layerId)) == pageLayerIds.end())
        {
            throw std::runtime_error(
                "Unknown runtime layer '" + reticle.layerId + "' for reticle '" + reticle.id +
                "' on page: " + page.name);
        }

        if (!reticleIds.insert(normalizedReticleId).second)
        {
            throw std::runtime_error("Duplicate reticle id '" + reticle.id + "' on page: " + page.name);
        }
    }

    std::unordered_set<std::string> normalizedStrobeNames;
    for (const auto& strobe : page.strobes)
    {
        if (!normalizedStrobeNames.insert(strobe.normalizedName).second)
        {
            throw std::runtime_error("Duplicate strobe name '" + strobe.name + "' on page: " + page.name);
        }

        const std::string normalizedReticleId = NormalizePageName(strobe.reticle.id);
        if (normalizedReticleId.empty())
        {
            throw std::runtime_error("Strobe reticle must define a non-empty id on page: " + page.name);
        }

        if (!reticleIds.insert(normalizedReticleId).second)
        {
            throw std::runtime_error("Duplicate reticle id '" + strobe.reticle.id + "' on page: " + page.name);
        }
    }

    if (!page.normalizedActiveStrobeName.empty() &&
        FindPageStrobeDefinition(page, page.normalizedActiveStrobeName) == nullptr)
    {
        throw std::runtime_error(
            "Unknown active strobe '" + page.activeStrobeName + "' on page: " + page.name);
    }

    ValidatePageDefinitionForRuntime(page);
    return page;
}

const ReticleGroup* FindStaticReticle(const PageDefinition& page, const std::string_view reticleId) noexcept
{
    const std::string normalizedReticleId = NormalizePageName(reticleId);

    for (const ReticleGroup& reticle : page.staticReticles)
    {
        if (NormalizePageName(reticle.id) == normalizedReticleId)
        {
            return &reticle;
        }
    }

    return nullptr;
}

const ReticleGroup* FindStrobeReticle(const PageDefinition& page, const std::string_view reticleId) noexcept
{
    const std::string normalizedReticleId = NormalizePageName(reticleId);

    for (const PageStrobeDefinition& strobe : page.strobes)
    {
        if (NormalizePageName(strobe.reticle.id) == normalizedReticleId)
        {
            return &strobe.reticle;
        }
    }

    return nullptr;
}

const ReticleGroup* FindPageOwnedReticle(const PageDefinition& page,
                                         const TransportMapReticleEntry& reticleEntry) noexcept
{
    const std::string normalizedSource = CanonicalToken(reticleEntry.source);
    if (normalizedSource == "strobe")
    {
        return FindStrobeReticle(page, reticleEntry.reticleId);
    }

    if (normalizedSource == "static")
    {
        return FindStaticReticle(page, reticleEntry.reticleId);
    }

    return nullptr;
}

std::string PrimitiveTypeToGeneratedName(const PrimitiveType type)
{
    switch (type)
    {
    case PrimitiveType::Text:
        return "text";
    case PrimitiveType::Time:
        return "time";
    case PrimitiveType::Line:
        return "line";
    case PrimitiveType::Circle:
        return "circle";
    case PrimitiveType::Ring:
        return "ring";
    case PrimitiveType::Rectangle:
        return "rectangle";
    case PrimitiveType::Ellipse:
        return "ellipse";
    case PrimitiveType::Square:
        return "square";
    case PrimitiveType::Diamond:
        return "diamond";
    case PrimitiveType::Triangle:
        return "triangle";
    case PrimitiveType::Polyline:
        return "polyline";
    case PrimitiveType::Bezier:
        return "bezier";
    case PrimitiveType::Arc:
        return "arc";
    case PrimitiveType::Image:
        return "image";
    }

    return {};
}

void ValidateGeneratedTransportMapAgainstDocument(const GeneratedTransportMap& map,
                                                  const std::filesystem::path& windowFile,
                                                  const WindowAssetDefinition& window,
                                                  const MfdDocument& document)
{
    const std::filesystem::path expectedSourceFile = windowFile.filename();
    if (map.window.source != expectedSourceFile.string())
    {
        throw std::runtime_error(
            "Generated transport map source mismatch for '" + windowFile.string() +
            "': expected '" + expectedSourceFile.string() + "' but found '" + map.window.source + "'");
    }

    if (map.window.name != windowFile.stem().string())
    {
        throw std::runtime_error(
            "Generated transport map window name mismatch for '" + windowFile.string() +
            "': expected '" + windowFile.stem().string() + "' but found '" + map.window.name + "'");
    }

    if (map.window.title != window.title)
    {
        throw std::runtime_error(
            "Generated transport map title mismatch for '" + windowFile.string() +
            "': expected '" + window.title + "' but found '" + map.window.title + "'");
    }

    std::unordered_map<TransportId, const TransportMapPageEntry*> pagesById;
    for (const auto& pageEntry : map.pages)
    {
        const PageDefinition* page = FindPageDefinition(document, pageEntry.name);
        if (page == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown page '" + pageEntry.name + "'");
        }

        if (page->normalizedName != pageEntry.normalizedName)
        {
            throw std::runtime_error(
                "Generated transport map page normalization mismatch for '" + pageEntry.name + "'");
        }

        if (!page->strobes.empty() != pageEntry.hasStrobe)
        {
            throw std::runtime_error(
                "Generated transport map strobe flag mismatch for page '" + pageEntry.name + "'");
        }

        if (page->defaultPage != pageEntry.defaultPage)
        {
            throw std::runtime_error(
                "Generated transport map defaultPage mismatch for page '" + pageEntry.name + "'");
        }

        pagesById.emplace(pageEntry.id, &pageEntry);
    }

    std::unordered_map<TransportId, const TransportMapReticleEntry*> reticlesById;
    for (const auto& reticleEntry : map.reticles)
    {
        const auto pageIt = pagesById.find(reticleEntry.pageId);
        if (pageIt == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map reticle '" + reticleEntry.reticleId +
                "' references unknown page id " + std::to_string(reticleEntry.pageId));
        }

        const PageDefinition* page = FindPageDefinition(document, pageIt->second->name);
        const ReticleGroup* reticle = page == nullptr ? nullptr : FindPageOwnedReticle(*page, reticleEntry);
        if (reticle == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown page reticle '" + reticleEntry.reticleId +
                "' on page '" + pageIt->second->name + "'");
        }

        if (NormalizePageName(reticle->id) != reticleEntry.normalizedReticleId)
        {
            throw std::runtime_error(
                "Generated transport map reticle normalization mismatch for '" + reticleEntry.reticleId +
                "' on page '" + pageIt->second->name + "'");
        }

        reticlesById.emplace(reticleEntry.id, &reticleEntry);
    }

    std::unordered_map<TransportId, const TransportMapTemplateEntry*> templatesById;
    for (const auto& templateEntry : map.templates)
    {
        const auto iterator = document.reticleLibrary.find(templateEntry.templateId);
        if (iterator == document.reticleLibrary.end())
        {
            throw std::runtime_error(
                "Generated transport map references unknown template '" + templateEntry.templateId + "'");
        }

        if (NormalizePageName(templateEntry.templateId) != templateEntry.normalizedTemplateId)
        {
            throw std::runtime_error(
                "Generated transport map template normalization mismatch for '" + templateEntry.templateId + "'");
        }

        templatesById.emplace(templateEntry.id, &templateEntry);
    }

    for (const auto& primitiveEntry : map.primitives)
    {
        const ReticleGroup* owner = nullptr;

        switch (primitiveEntry.ownerKind)
        {
        case TransportPrimitiveOwnerKind::Reticle:
        {
            const auto reticleIt = reticlesById.find(primitiveEntry.ownerId);
            if (reticleIt == reticlesById.end())
            {
                throw std::runtime_error(
                    "Generated transport map primitive '" + primitiveEntry.primitiveId +
                    "' references unknown reticle owner id " + std::to_string(primitiveEntry.ownerId));
            }

            const auto pageIt = pagesById.find(reticleIt->second->pageId);
            const PageDefinition* page = pageIt == pagesById.end() ? nullptr : FindPageDefinition(document, pageIt->second->name);
            owner = page == nullptr ? nullptr : FindPageOwnedReticle(*page, *reticleIt->second);
            break;
        }

        case TransportPrimitiveOwnerKind::Template:
        {
            const auto templateIt = templatesById.find(primitiveEntry.ownerId);
            if (templateIt == templatesById.end())
            {
                throw std::runtime_error(
                    "Generated transport map primitive '" + primitiveEntry.primitiveId +
                    "' references unknown template owner id " + std::to_string(primitiveEntry.ownerId));
            }

            const auto libraryIt = document.reticleLibrary.find(templateIt->second->templateId);
            if (libraryIt != document.reticleLibrary.end())
            {
                owner = &libraryIt->second;
            }
            break;
        }
        }

        if (owner == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map primitive '" + primitiveEntry.primitiveId + "' owner could not be resolved");
        }

        const Primitive* primitive = FindPrimitive(*owner, primitiveEntry.primitiveId);
        if (primitive == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown primitive '" + primitiveEntry.primitiveId + "'");
        }

        if (NormalizePageName(primitive->id) != primitiveEntry.normalizedPrimitiveId)
        {
            throw std::runtime_error(
                "Generated transport map primitive normalization mismatch for '" + primitiveEntry.primitiveId + "'");
        }

        if (PrimitiveTypeToGeneratedName(primitive->type) != primitiveEntry.primitiveType)
        {
            throw std::runtime_error(
                "Generated transport map primitive type mismatch for '" + primitiveEntry.primitiveId + "'");
        }
    }

    for (const auto& blinkTypeEntry : map.blinkTypes)
    {
        const auto pageIt = pagesById.find(blinkTypeEntry.pageId);
        if (pageIt == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map blink type '" + blinkTypeEntry.blinkType +
                "' references unknown page id " + std::to_string(blinkTypeEntry.pageId));
        }

        const PageDefinition* page = FindPageDefinition(document, pageIt->second->name);
        const PageBlinkDefinition* blinkType =
            page == nullptr ? nullptr : FindPageBlinkDefinition(*page, blinkTypeEntry.blinkType);
        if (blinkType == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown blink type '" + blinkTypeEntry.blinkType +
                "' on page '" + pageIt->second->name + "'");
        }

        if (blinkType->normalizedName != blinkTypeEntry.normalizedBlinkType)
        {
            throw std::runtime_error(
                "Generated transport map blink normalization mismatch for '" + blinkTypeEntry.blinkType +
                "' on page '" + pageIt->second->name + "'");
        }

        if (blinkType->durationMs != blinkTypeEntry.durationMs)
        {
            throw std::runtime_error(
                "Generated transport map blink duration mismatch for '" + blinkTypeEntry.blinkType +
                "' on page '" + pageIt->second->name + "'");
        }
    }

    for (const auto& strobeEntry : map.strobes)
    {
        const auto pageIt = pagesById.find(strobeEntry.pageId);
        if (pageIt == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map strobe '" + strobeEntry.strobeName +
                "' references unknown page id " + std::to_string(strobeEntry.pageId));
        }

        const PageDefinition* page = FindPageDefinition(document, pageIt->second->name);
        const PageStrobeDefinition* strobe =
            page == nullptr ? nullptr : FindPageStrobeDefinition(*page, strobeEntry.strobeName);
        if (strobe == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown strobe '" + strobeEntry.strobeName +
                "' on page '" + pageIt->second->name + "'");
        }

        if (strobe->normalizedName != strobeEntry.normalizedStrobeName)
        {
            throw std::runtime_error(
                "Generated transport map strobe normalization mismatch for '" + strobeEntry.strobeName +
                "' on page '" + pageIt->second->name + "'");
        }

        if (strobe->reticle.id != strobeEntry.reticleId)
        {
            throw std::runtime_error(
                "Generated transport map strobe reticle mismatch for '" + strobeEntry.strobeName +
                "' on page '" + pageIt->second->name + "'");
        }

        const bool defaultActive =
            !page->normalizedActiveStrobeName.empty() &&
            page->normalizedActiveStrobeName == strobeEntry.normalizedStrobeName;
        if (defaultActive != strobeEntry.defaultActive)
        {
            throw std::runtime_error(
                "Generated transport map active strobe mismatch for '" + strobeEntry.strobeName +
                "' on page '" + pageIt->second->name + "'");
        }
    }
}
} // namespace

LoadedWindowConfiguration JsonLoader::LoadWindowConfiguration(const std::filesystem::path& windowFile) const
{
    const std::filesystem::path resolvedWindowFile = NormalizePath(windowFile);
    const json root = LoadJsonFile(resolvedWindowFile);

    LoadedWindowConfiguration loaded;
    loaded.window = ParseWindowAssetDefinition(root, resolvedWindowFile);
    loaded.document.sourceFile = resolvedWindowFile;
    loaded.document.reticleLibraryFolder = loaded.window.reticleLibraryFolder;
    loaded.document.reticleLibrary = LoadReticleLibrary(loaded.window.reticleLibraryFolder);

    std::unordered_set<std::string> pageNames;

    for (const auto& pageFile : loaded.window.pageFiles)
    {
        PageDefinition page;

        try
        {
            const json pageRoot = LoadJsonFile(pageFile);
            page = ParsePage(*FindPageNode(pageRoot), loaded.document.reticleLibrary, pageFile.parent_path());
        }
        catch (const std::exception& exception)
        {
            throw std::runtime_error("Unable to load page asset '" + pageFile.string() + "': " + exception.what());
        }

        if (!pageNames.insert(page.normalizedName).second)
        {
            throw std::runtime_error("Duplicate page name in window JSON: " + page.name);
        }

        loaded.document.pages.push_back(std::move(page));
    }

    ApplyDefaultPageName(loaded.document, ParseDefaultPageName(root, "Window JSON"), "window JSON");

    std::string generatedMapError;
    loaded.generatedTransportMap = TryLoadGeneratedTransportMap(resolvedWindowFile, &generatedMapError);
    if (!generatedMapError.empty())
    {
        throw std::runtime_error(generatedMapError);
    }

    if (loaded.generatedTransportMap.has_value())
    {
        ValidateGeneratedTransportMapAgainstDocument(*loaded.generatedTransportMap,
                                                    resolvedWindowFile,
                                                    loaded.window,
                                                    loaded.document);
    }

    ThrowIfDocumentSemanticsInvalid(loaded.document);
    return loaded;
}

MfdDocument JsonLoader::LoadDocument(const std::filesystem::path& pagesFile) const
{
    const std::filesystem::path resolvedPagesFile = NormalizePath(pagesFile);
    const json root = LoadJsonFile(resolvedPagesFile);

    if (const json* pages = FindField(root, {"pages"}); pages != nullptr && IsPageFileList(*pages))
    {
        return LoadWindowConfiguration(resolvedPagesFile).document;
    }

    if (!root.contains("pages") || !root.at("pages").is_array())
    {
        throw std::runtime_error("Pages JSON must contain a pages array");
    }

    std::filesystem::path libraryFolder = resolvedPagesFile.parent_path();
    if (root.contains("reticleLibraryFolder"))
    {
        libraryFolder = ResolvePath(resolvedPagesFile.parent_path(),
                                    root.at("reticleLibraryFolder").get<std::string>());
    }

    MfdDocument document;
    document.sourceFile = resolvedPagesFile;
    document.reticleLibraryFolder = libraryFolder;
    document.reticleLibrary = LoadReticleLibrary(libraryFolder);
    const auto defaultPageName = ParseDefaultPageName(root, "Pages JSON");
    std::unordered_set<std::string> pageNames;

    for (const auto& pageNode : root.at("pages"))
    {
        PageDefinition page = ParsePage(pageNode, document.reticleLibrary, pagesFile.parent_path());
        if (!pageNames.insert(page.normalizedName).second)
        {
            throw std::runtime_error("Duplicate page name in pages JSON: " + page.name);
        }

        document.pages.push_back(std::move(page));
    }

    ApplyDefaultPageName(document, defaultPageName, "pages JSON");
    ThrowIfDocumentSemanticsInvalid(document);
    return document;
}
} // namespace mfd
