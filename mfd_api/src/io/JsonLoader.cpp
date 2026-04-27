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
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace mfd
{
namespace
{
using json = nlohmann::json;

ReticleInfo ParseReticleInfo(const json& node);
ReticleBlinkState ParseReticleBlinkState(const json& node);
ReticleEditorState ParseReticleEditorState(const json& node);
ReticleClipState ParseReticleClipState(const json& node);
std::vector<PageBlinkDefinition> ParsePageBlinkDefinitions(const json& node);
PageEditorState ParsePageEditorState(const json& node);
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

std::string_view TrimAsciiWhitespace(std::string_view value) noexcept
{
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return value.substr(first, last - first);
}

std::string Lowercase(std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (const char ch : value)
    {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return result;
}

std::string CanonicalToken(std::string_view value)
{
    const std::string_view trimmed = TrimAsciiWhitespace(value);

    std::string result;
    result.reserve(trimmed.size());

    for (const char ch : trimmed)
    {
        if (ch == ' ' || ch == '-' || ch == '_')
        {
            continue;
        }

        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return result;
}

std::string JsonScalarToString(const json& value)
{
    if (value.is_string())
    {
        return value.get<std::string>();
    }

    if (value.is_boolean())
    {
        return value.get<bool>() ? "true" : "false";
    }

    if (value.is_number_integer() || value.is_number_unsigned())
    {
        return std::to_string(value.get<long long>());
    }

    if (value.is_number_float())
    {
        std::ostringstream stream;
        stream << value.get<double>();
        return stream.str();
    }

    return value.dump();
}

json LoadJsonFile(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open JSON file: " + path.string());
    }

    try
    {
        json document;
        stream >> document;
        return document;
    }
    catch (const nlohmann::json::parse_error& exception)
    {
        throw std::runtime_error("Unable to parse JSON file '" + path.string() +
                                 "' at byte " + std::to_string(exception.byte) +
                                 ": " + exception.what());
    }
    catch (const nlohmann::json::exception& exception)
    {
        throw std::runtime_error("Unable to read JSON file '" + path.string() +
                                 "': " + exception.what());
    }
}

std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    return path.is_absolute()
               ? path.lexically_normal()
               : std::filesystem::absolute(path).lexically_normal();
}

std::filesystem::path ResolvePath(const std::filesystem::path& baseFolder, const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (baseFolder / path).lexically_normal();
}

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

const json* FindField(const json& node, const std::initializer_list<const char*> fieldNames)
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

const json* FindEditorField(const json& node, const std::initializer_list<const char*> fieldNames)
{
    const json* editor = FindField(node, {"_editor", "editor"});
    return editor == nullptr ? nullptr : FindField(*editor, fieldNames);
}

bool IsPageFileList(const json& value)
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

        if (entry.is_object() && FindField(entry, {"file", "path", "json"}) != nullptr)
        {
            continue;
        }

        return false;
    }

    return true;
}

int ParsePixelNumber(const json& value, const char* fieldName)
{
    if (!value.is_number())
    {
        throw std::runtime_error(std::string(fieldName) + " must be numeric");
    }

    return static_cast<int>(std::lround(value.get<double>()));
}

void ParseWindowSize(const json& value, int& width, int& height)
{
    if (value.is_array())
    {
        if (value.size() < 2)
        {
            throw std::runtime_error("Window size array must contain width and height");
        }

        width = ParsePixelNumber(value.at(0), "window width");
        height = ParsePixelNumber(value.at(1), "window height");
        return;
    }

    if (value.is_object())
    {
        if (!value.contains("width") || !value.contains("height"))
        {
            throw std::runtime_error("Window size object must contain width and height");
        }

        width = ParsePixelNumber(value.at("width"), "window width");
        height = ParsePixelNumber(value.at("height"), "window height");
        return;
    }

    throw std::runtime_error("Unsupported window size format");
}

void ParseWindowPosition(const json& value, int& x, int& y)
{
    if (value.is_array())
    {
        if (value.size() < 2)
        {
            throw std::runtime_error("Window position array must contain x and y");
        }

        x = ParsePixelNumber(value.at(0), "window x");
        y = ParsePixelNumber(value.at(1), "window y");
        return;
    }

    if (value.is_object())
    {
        x = ParsePixelNumber(value.value("x", json(x)), "window x");
        y = ParsePixelNumber(value.value("y", json(y)), "window y");
        return;
    }

    throw std::runtime_error("Unsupported window position format");
}

std::uint16_t ParsePortNumber(const json& value, const char* fieldName)
{
    const int port = ParsePixelNumber(value, fieldName);
    if (port < 0 || port > 65535)
    {
        throw std::runtime_error(std::string(fieldName) + " must be in [0, 65535]");
    }

    return static_cast<std::uint16_t>(port);
}

std::size_t ParsePositivePacketSize(const json& value, const char* fieldName)
{
    const int packetSize = ParsePixelNumber(value, fieldName);
    if (packetSize <= 0)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive");
    }

    return static_cast<std::size_t>(packetSize);
}

std::uint32_t ParseDurationMilliseconds(const json& value, const char* fieldName)
{
    const int durationMs = ParsePixelNumber(value, fieldName);
    if (durationMs <= 0)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive");
    }

    return static_cast<std::uint32_t>(durationMs);
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
        if (value.size() < 2)
        {
            throw std::runtime_error("Vec2 array must contain at least 2 values");
        }

        return Vec2 {value.at(0).get<float>(), value.at(1).get<float>()};
    }

    if (value.is_object())
    {
        return Vec2 {value.value("x", 0.0f), value.value("y", 0.0f)};
    }

    throw std::runtime_error("Unsupported Vec2 format");
}

Vec2 ParseScale(const json& value)
{
    if (value.is_number())
    {
        const float scalar = value.get<float>();
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
        return width->get<float>();
    }

    return std::nullopt;
}

std::optional<float> ParseLetterSpacingField(const json& node)
{
    if (const json* spacing = FindField(node, {"letterSpacing", "spacing", "tracking"}))
    {
        if (!spacing->is_number())
        {
            throw std::runtime_error("Text letter spacing must be numeric");
        }

        return spacing->get<float>();
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

void ApplyTransformFields(const json& node, Transform2D& transform)
{
    if (const json* position = FindField(node, {"position", "at", "pos"}))
    {
        transform.position = ParseVec2(*position);
    }

    if (node.contains("x") || node.contains("y"))
    {
        transform.position = {
            node.value("x", transform.position.x),
            node.value("y", transform.position.y)};
    }

    if (const json* angle = FindField(node, {"rotationDegrees", "angle", "rotation"}))
    {
        transform.rotationDegrees = angle->get<float>();
    }

    if (const json* scale = FindField(node, {"scale", "zoom"}))
    {
        transform.scale = ParseScale(*scale);
    }

    if (node.contains("sx") || node.contains("sy") ||
        node.contains("scaleX") || node.contains("scaleY"))
    {
        transform.scale = {
            node.value("sx", node.value("scaleX", transform.scale.x)),
            node.value("sy", node.value("scaleY", transform.scale.y))};
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
            node.value("x", view.center.x),
            node.value("y", view.center.y)};
    }

    if (const json* zoom = FindField(node, {"zoom", "zoomLevel"}))
    {
        view.zoom = zoom->get<float>();
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
    view.zoom = SanitizeZoom(view.zoom);
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
            geometry.startAngleDegrees = startAngle->get<float>();
        }

        if (const json* endAngle = FindField(node, {"endAngleDegrees", "endAngle", "toDegrees", "angleEnd"}))
        {
            geometry.endAngleDegrees = endAngle->get<float>();
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
                const float scalar = size->get<float>();
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
            geometry.width = width->get<float>();
        }

        if (const json* height = FindField(node, {"height"}))
        {
            geometry.height = height->get<float>();
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
    group.editor = ParseReticleEditorState(node);
    group.clipping = ParseReticleClipState(node);

    for (const auto& element : node.at("elements"))
    {
        group.primitives.push_back(ParsePrimitive(element, baseFolder));
    }

    ApplyReticleTextOverrides(node, group);

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
        group.editor = ParseReticleEditorState(node);
        if (FindField(node, {"clipping", "clip"}) != nullptr)
        {
            group.clipping = ParseReticleClipState(node);
        }
        ApplyReticleTextOverrides(node, group);
        return group;
    }

    ReticleGroup group = ParseInlineReticle(node, baseFolder);
    group.blink = ParseReticleBlinkState(node);
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
            if (!primitiveSpacing.is_number())
            {
                throw std::runtime_error("Reticle letter spacing overrides must be numeric values");
            }

            SetTextPrimitiveLetterSpacing(group, primitiveId, primitiveSpacing.get<float>());
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

ReticleEditorState ParseReticleEditorState(const json& node)
{
    ReticleEditorState editorState;

    if (const json* layer = FindEditorField(node, {"layer", "layerId"});
        layer != nullptr && !layer->is_null())
    {
        if (!layer->is_string())
        {
            throw std::runtime_error("Reticle editor layer must be a string");
        }

        editorState.layerId = layer->get<std::string>();
    }

    return editorState;
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

    if (clipping.mode != ReticleClipMode::None && clipping.primitiveId.empty())
    {
        throw std::runtime_error("clipping requires a primitive id when enabled");
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

PageEditorState ParsePageEditorState(const json& node)
{
    PageEditorState editorState;
    const json* layers = FindEditorField(node, {"layers"});
    if (layers == nullptr || layers->is_null())
    {
        return editorState;
    }

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
        if (layer.id.empty())
        {
            throw std::runtime_error("Page editor layer id cannot be empty");
        }

        if (!layerIds.insert(layer.id).second)
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

    return editorState;
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
        config.radius = radius->get<float>();
    }

    if (const json* size = FindField(node, {"size", "captureSize"}))
    {
        if (size->is_number())
        {
            const float scalar = size->get<float>();
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
            node.value("width", config.size.x),
            node.value("height", config.size.y)};
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
        config.radius = radius->get<float>();
    }

    if (const json* strength = FindField(*magnetNode, {"strength", "magnetStrength", "snapStrength", "blend"}))
    {
        config.strength = std::clamp(strength->get<float>(), 0.0f, 1.0f);
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
                config.visualShapeSize = std::max(0.001f, size->get<float>());
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
        config.visualShapeSize = std::max(0.001f, visualSize->get<float>());
    }

    return config;
}

PageStrobeDefinition ParsePageStrobe(const json& node,
                                     const ReticleLibrary& library,
                                     const std::filesystem::path& baseFolder)
{
    PageStrobeDefinition strobe;
    strobe.reticle = ParseReticle(node, library, baseFolder);
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

const json& ExtractPageNode(const json& root)
{
    if (const json* page = FindField(root, {"page"}))
    {
        return *page;
    }

    return root;
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
    page.backgroundColor = ParseBackgroundColor(node);
    page.view = ParsePageViewState(node);
    page.blinkTypes = ParsePageBlinkDefinitions(node);
    page.editor = ParsePageEditorState(node);

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

    if (node.contains("strobe"))
    {
        page.strobe = ParsePageStrobe(node.at("strobe"), library, baseFolder);
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

    if (page.strobe.has_value() && !ResolveReticleBlinkState(page, page.strobe->reticle))
    {
        const std::string blinkType =
            page.strobe->reticle.blink.typeName.empty() ? std::string {"<default>"} : page.strobe->reticle.blink.typeName;
        throw std::runtime_error(
            "Unknown blink type '" + blinkType + "' for strobe '" + page.strobe->reticle.id + "' on page: " + page.name);
    }

    std::unordered_set<std::string> reticleIds;
    std::unordered_set<std::string> pageLayerIds;
    for (const auto& layer : page.editor.layers)
    {
        pageLayerIds.insert(layer.id);
    }

    for (const auto& reticle : page.staticReticles)
    {
        const std::string normalizedReticleId = NormalizePageName(reticle.id);
        if (normalizedReticleId.empty())
        {
            throw std::runtime_error("Static reticles must define a non-empty id on page: " + page.name);
        }

        if (!reticle.editor.layerId.empty() && pageLayerIds.find(reticle.editor.layerId) == pageLayerIds.end())
        {
            throw std::runtime_error(
                "Unknown editor layer '" + reticle.editor.layerId + "' for reticle '" + reticle.id +
                "' on page: " + page.name);
        }

        if (!reticleIds.insert(normalizedReticleId).second)
        {
            throw std::runtime_error("Duplicate reticle id '" + reticle.id + "' on page: " + page.name);
        }
    }

    if (page.strobe.has_value())
    {
        const std::string normalizedReticleId = NormalizePageName(page.strobe->reticle.id);
        if (normalizedReticleId.empty())
        {
            throw std::runtime_error("Strobe reticle must define a non-empty id on page: " + page.name);
        }

        if (!reticleIds.insert(normalizedReticleId).second)
        {
            throw std::runtime_error("Duplicate reticle id '" + page.strobe->reticle.id + "' on page: " + page.name);
        }
    }

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

        if (page->strobe.has_value() != pageEntry.hasStrobe)
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
        const ReticleGroup* reticle = page == nullptr ? nullptr : FindStaticReticle(*page, reticleEntry.reticleId);
        if (reticle == nullptr)
        {
            throw std::runtime_error(
                "Generated transport map references unknown static reticle '" + reticleEntry.reticleId +
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
            owner = page == nullptr ? nullptr : FindStaticReticle(*page, reticleIt->second->reticleId);
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
            page = ParsePage(ExtractPageNode(pageRoot), loaded.document.reticleLibrary, pageFile.parent_path());
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
    return document;
}
} // namespace mfd
