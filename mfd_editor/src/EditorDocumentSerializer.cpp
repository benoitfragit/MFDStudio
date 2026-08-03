/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorDocumentSerializer.h"

/**
 * @file
 * @brief JSON serializer used by the editor save workflow for windows, pages and reticle templates.
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "mfd/model/Reticle.h"
#include "mfd/model/Types.h"

namespace editor
{
namespace
{
using json = nlohmann::json;

std::optional<json> SerializeReticleClipping(const mfd::ReticleClipState& clipping);
std::optional<json> SerializeReticleClippingOverride(const mfd::ReticleClipState& clipping,
                                                     const mfd::ReticleClipState& inherited);

std::string Lowercase(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char ch : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lowered;
}

json ToVec2(const mfd::Vec2& value)
{
    return json::array({value.x, value.y});
}

bool IsZero(const float value) noexcept
{
    return std::abs(value) < 0.0001f;
}

bool IsDefaultTransform(const mfd::Transform2D& transform) noexcept
{
    return IsZero(transform.position.x) &&
           IsZero(transform.position.y) &&
           IsZero(transform.rotationDegrees) &&
           std::abs(transform.scale.x - 1.0f) < 0.0001f &&
           std::abs(transform.scale.y - 1.0f) < 0.0001f;
}

mfd::Transform2D ResolveAuthoredTemplateTransform(const mfd::ReticleGroup& instance,
                                                  const mfd::ReticleGroup& reticleTemplate) noexcept
{
    if (!instance.authoredTemplateTransform.has_value())
    {
        return instance.transform;
    }

    mfd::Transform2D authored = *instance.authoredTemplateTransform;
    const mfd::Vec2 translated = mfd::Rotate(instance.transform.position - reticleTemplate.transform.position,
                                             -reticleTemplate.transform.rotationDegrees);

    if (!IsZero(reticleTemplate.transform.scale.x))
    {
        authored.position.x = translated.x / reticleTemplate.transform.scale.x;
        authored.scale.x = instance.transform.scale.x / reticleTemplate.transform.scale.x;
    }
    if (!IsZero(reticleTemplate.transform.scale.y))
    {
        authored.position.y = translated.y / reticleTemplate.transform.scale.y;
        authored.scale.y = instance.transform.scale.y / reticleTemplate.transform.scale.y;
    }
    authored.rotationDegrees = instance.transform.rotationDegrees - reticleTemplate.transform.rotationDegrees;
    return authored;
}

std::string ToHexColor(const mfd::ColorRgba& color)
{
    std::ostringstream stream;
    stream << '#'
           << std::uppercase
           << std::hex
           << std::setw(2) << std::setfill('0') << static_cast<int>(color.r)
           << std::setw(2) << std::setfill('0') << static_cast<int>(color.g)
           << std::setw(2) << std::setfill('0') << static_cast<int>(color.b)
           << std::setw(2) << std::setfill('0') << static_cast<int>(color.a);
    return stream.str();
}

const char* ToLineStyleText(const mfd::LineStyle lineStyle) noexcept
{
    switch (lineStyle)
    {
    case mfd::LineStyle::Dotted:
        return "dotted";
    case mfd::LineStyle::Dashed:
        return "dashed";
    case mfd::LineStyle::Solid:
    default:
        return "solid";
    }
}

const char* ToAlignText(const mfd::Align align) noexcept
{
    switch (align)
    {
    case mfd::Align::Left:
        return "left";
    case mfd::Align::Right:
        return "right";
    case mfd::Align::Center:
    default:
        return "center";
    }
}

std::string SerializePathRelativeTo(const std::filesystem::path& path, const std::filesystem::path& baseFolder)
{
    if (path.empty())
    {
        return {};
    }

    if (baseFolder.empty())
    {
        return path.generic_string();
    }

    std::error_code relativeError;
    const std::filesystem::path relativePath = std::filesystem::relative(path, baseFolder, relativeError);
    if (relativeError || relativePath.empty())
    {
        return path.generic_string();
    }

    return relativePath.generic_string();
}

void WriteTransformFields(json& node, const mfd::Transform2D& transform);

bool IsDefaultPageTitleTransform(const mfd::Transform2D& transform) noexcept
{
    const mfd::Transform2D defaultTransform = mfd::PageTitleDisplayDefinition {}.transform;

    return std::abs(transform.position.x - defaultTransform.position.x) < 0.0001f &&
           std::abs(transform.position.y - defaultTransform.position.y) < 0.0001f &&
           std::abs(transform.rotationDegrees - defaultTransform.rotationDegrees) < 0.0001f &&
           std::abs(transform.scale.x - defaultTransform.scale.x) < 0.0001f &&
           std::abs(transform.scale.y - defaultTransform.scale.y) < 0.0001f;
}

bool IsDefaultPageTitleDisplay(const mfd::PageTitleDisplayDefinition& display) noexcept
{
    const mfd::PageTitleDisplayDefinition defaults {};

    return display.visible == defaults.visible &&
           IsDefaultPageTitleTransform(display.transform) &&
           display.color.r == defaults.color.r &&
           display.color.g == defaults.color.g &&
           display.color.b == defaults.color.b &&
           display.color.a == defaults.color.a &&
           std::abs(display.lineWidth - defaults.lineWidth) < 0.0001f &&
           display.lineStyle == defaults.lineStyle &&
           display.decoration == defaults.decoration;
}

const char* ToPageTitleDecorationText(const mfd::PageTitleDecoration decoration) noexcept
{
    switch (decoration)
    {
    case mfd::PageTitleDecoration::None:
        return "none";
    case mfd::PageTitleDecoration::Frame:
        return "frame";
    case mfd::PageTitleDecoration::Underline:
    default:
        return "underline";
    }
}

std::optional<json> SerializePageTitleDisplay(const mfd::PageTitleDisplayDefinition& display)
{
    if (IsDefaultPageTitleDisplay(display))
    {
        return std::nullopt;
    }

    json node = json::object();
    if (!display.visible)
    {
        node["visible"] = false;
    }

    const mfd::PageTitleDisplayDefinition defaults {};
    const mfd::Transform2D& defaultTransform = defaults.transform;

    if (std::abs(display.transform.position.x - defaultTransform.position.x) >= 0.0001f ||
        std::abs(display.transform.position.y - defaultTransform.position.y) >= 0.0001f)
    {
        node["at"] = ToVec2(display.transform.position);
    }

    if (std::abs(display.transform.rotationDegrees - defaultTransform.rotationDegrees) >= 0.0001f)
    {
        node["angle"] = display.transform.rotationDegrees;
    }

    if (std::abs(display.transform.scale.x - defaultTransform.scale.x) >= 0.0001f ||
        std::abs(display.transform.scale.y - defaultTransform.scale.y) >= 0.0001f)
    {
        if (std::abs(display.transform.scale.x - display.transform.scale.y) < 0.0001f)
        {
            node["scale"] = display.transform.scale.x;
        }
        else
        {
            node["scale"] = json::array({display.transform.scale.x, display.transform.scale.y});
        }
    }

    if (!(display.color.r == defaults.color.r &&
          display.color.g == defaults.color.g &&
          display.color.b == defaults.color.b &&
          display.color.a == defaults.color.a))
    {
        node["stroke"] = ToHexColor(display.color);
    }

    if (std::abs(display.lineWidth - defaults.lineWidth) >= 0.0001f)
    {
        node["lineWidth"] = display.lineWidth;
    }

    if (display.lineStyle != defaults.lineStyle)
    {
        node["lineStyle"] = ToLineStyleText(display.lineStyle);
    }

    if (display.decoration != defaults.decoration)
    {
        node["decoration"] = ToPageTitleDecorationText(display.decoration);
    }

    return node;
}

void WriteFeedbackIntervalField(json& feedbackNode,
                                const char* millisecondsKey,
                                const char* secondsKey,
                                const float seconds)
{
    const double milliseconds = static_cast<double>(seconds) * 1000.0;
    const double roundedMilliseconds = std::round(milliseconds);
    if (std::abs(milliseconds - roundedMilliseconds) < 0.001)
    {
        feedbackNode[millisecondsKey] = static_cast<int>(roundedMilliseconds);
        return;
    }

    feedbackNode[secondsKey] = seconds;
}

void WriteTransformFields(json& node, const mfd::Transform2D& transform)
{
    if (!IsZero(transform.position.x) || !IsZero(transform.position.y))
    {
        node["at"] = ToVec2(transform.position);
    }

    if (!IsZero(transform.rotationDegrees))
    {
        node["angle"] = transform.rotationDegrees;
    }

    if (std::abs(transform.scale.x - 1.0f) >= 0.0001f || std::abs(transform.scale.y - 1.0f) >= 0.0001f)
    {
        if (std::abs(transform.scale.x - transform.scale.y) < 0.0001f)
        {
            node["scale"] = transform.scale.x;
        }
        else
        {
            node["scale"] = json::array({transform.scale.x, transform.scale.y});
        }
    }
}

void WriteReticleOverrideFields(json& node, const mfd::ReticleStyleOverride& style)
{
    if (style.color.has_value())
    {
        node["stroke"] = ToHexColor(*style.color);
    }

    if (style.thickness.has_value())
    {
        node["lineWidth"] = *style.thickness;
    }

    if (style.fillColor.has_value())
    {
        node["fill"] = ToHexColor(*style.fillColor);
    }

    if (style.filled.has_value())
    {
        node["filled"] = *style.filled;
    }
}

void WritePrimitiveStyleFields(json& node, const mfd::Primitive& primitive)
{
    const mfd::PrimitiveStyle& style = primitive.style;
    const mfd::PrimitiveStyle defaults {};
    const bool supportsFilledRendering = mfd::SupportsFilledPrimitive(primitive);

    if (style.visible != defaults.visible)
    {
        node["visible"] = style.visible;
    }

    if (style.color.r != defaults.color.r ||
        style.color.g != defaults.color.g ||
        style.color.b != defaults.color.b ||
        style.color.a != defaults.color.a)
    {
        node["stroke"] = ToHexColor(style.color);
    }

    if (std::abs(style.thickness - defaults.thickness) >= 0.0001f)
    {
        node["lineWidth"] = style.thickness;
    }

    if (style.lineStyle != defaults.lineStyle)
    {
        node["lineStyle"] = ToLineStyleText(style.lineStyle);
    }

    if (supportsFilledRendering &&
        (style.fillColor.r != defaults.fillColor.r ||
         style.fillColor.g != defaults.fillColor.g ||
         style.fillColor.b != defaults.fillColor.b ||
         style.fillColor.a != defaults.fillColor.a))
    {
        node["fill"] = ToHexColor(style.fillColor);
    }

    if (supportsFilledRendering)
    {
        node["filled"] = style.filled;
    }
}

void WritePrimitiveGeometry(json& node, const mfd::Primitive& primitive, const std::filesystem::path& baseFolder)
{
    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        node["text"] = text->text;
        node["size"] = text->fontSize;
        if (std::abs(text->letterSpacing - mfd::kDefaultTextLetterSpacing) >= 0.0001f)
        {
            node["letterSpacing"] = text->letterSpacing;
        }
        if (text->align != mfd::Align::Center)
        {
            node["align"] = ToAlignText(text->align);
        }
        return;
    }

    if (const auto* time = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        node["format"] = time->format;
        node["size"] = time->fontSize;
        if (std::abs(time->letterSpacing - mfd::kDefaultTextLetterSpacing) >= 0.0001f)
        {
            node["letterSpacing"] = time->letterSpacing;
        }
        if (time->utc)
        {
            node["utc"] = true;
        }
        if (time->align != mfd::Align::Center)
        {
            node["align"] = ToAlignText(time->align);
        }
        return;
    }

    if (const auto* line = std::get_if<mfd::LineGeometry>(&primitive.geometry))
    {
        node["start"] = ToVec2(line->start);
        node["end"] = ToVec2(line->end);
        return;
    }

    if (const auto* circle = std::get_if<mfd::CircleGeometry>(&primitive.geometry))
    {
        node["radius"] = circle->radius;
        return;
    }

    if (const auto* ring = std::get_if<mfd::RingGeometry>(&primitive.geometry))
    {
        node["innerRadius"] = ring->innerRadius;
        node["outerRadius"] = ring->outerRadius;
        if (ring->segments != 64)
        {
            node["segments"] = ring->segments;
        }
        return;
    }

    if (const auto* rectangle = std::get_if<mfd::RectangleGeometry>(&primitive.geometry))
    {
        if (std::abs(rectangle->width - rectangle->height) < 0.0001f)
        {
            node["size"] = rectangle->width;
        }
        else
        {
            node["width"] = rectangle->width;
            node["height"] = rectangle->height;
        }
        return;
    }

    if (const auto* ellipse = std::get_if<mfd::EllipseGeometry>(&primitive.geometry))
    {
        if (std::abs(ellipse->width - ellipse->height) < 0.0001f)
        {
            node["size"] = ellipse->width;
        }
        else
        {
            node["width"] = ellipse->width;
            node["height"] = ellipse->height;
        }
        return;
    }

    if (const auto* square = std::get_if<mfd::SquareGeometry>(&primitive.geometry))
    {
        // A square is uniform, so a single side length round-trips the geometry.
        node["size"] = square->width;
        return;
    }

    if (const auto* diamond = std::get_if<mfd::DiamondGeometry>(&primitive.geometry))
    {
        if (std::abs(diamond->width - diamond->height) < 0.0001f)
        {
            node["size"] = diamond->width;
        }
        else
        {
            node["width"] = diamond->width;
            node["height"] = diamond->height;
        }
        return;
    }

    if (const auto* triangle = std::get_if<mfd::TriangleGeometry>(&primitive.geometry))
    {
        node["points"] = json::array({ToVec2(triangle->points[0]), ToVec2(triangle->points[1]), ToVec2(triangle->points[2])});
        return;
    }

    if (const auto* polyline = std::get_if<mfd::PolylineGeometry>(&primitive.geometry))
    {
        json points = json::array();
        for (const auto& point : polyline->points)
        {
            points.push_back(ToVec2(point));
        }
        node["points"] = std::move(points);
        if (polyline->closed)
        {
            node["closed"] = true;
        }
        return;
    }

    if (const auto* bezier = std::get_if<mfd::BezierGeometry>(&primitive.geometry))
    {
        json points = json::array();
        for (const auto& point : bezier->controlPoints)
        {
            points.push_back(ToVec2(point));
        }
        node["points"] = std::move(points);
        if (bezier->segments != 32)
        {
            node["segments"] = bezier->segments;
        }
        return;
    }

    if (const auto* arc = std::get_if<mfd::ArcGeometry>(&primitive.geometry))
    {
        node["radius"] = arc->radius;
        node["startAngleDegrees"] = arc->startAngleDegrees;
        node["endAngleDegrees"] = arc->endAngleDegrees;
        if (arc->segments != 48)
        {
            node["segments"] = arc->segments;
        }
        return;
    }

    if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
    {
        node["file"] = SerializePathRelativeTo(image->file, baseFolder);
        if (std::abs(image->width - image->height) < 0.0001f)
        {
            node["size"] = image->width;
        }
        else
        {
            node["width"] = image->width;
            node["height"] = image->height;
        }
    }
}

std::string PrimitiveTypeName(const mfd::PrimitiveType type)
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
        return "text";
    case mfd::PrimitiveType::Time:
        return "time";
    case mfd::PrimitiveType::Line:
        return "line";
    case mfd::PrimitiveType::Circle:
        return "circle";
    case mfd::PrimitiveType::Ring:
        return "ring";
    case mfd::PrimitiveType::Rectangle:
        return "rectangle";
    case mfd::PrimitiveType::Ellipse:
        return "ellipse";
    case mfd::PrimitiveType::Square:
        return "square";
    case mfd::PrimitiveType::Diamond:
        return "diamond";
    case mfd::PrimitiveType::Triangle:
        return "triangle";
    case mfd::PrimitiveType::Polyline:
        return "polyline";
    case mfd::PrimitiveType::Bezier:
        return "bezier";
    case mfd::PrimitiveType::Arc:
        return "arc";
    case mfd::PrimitiveType::Image:
        return "image";
    }

    return "line";
}

json SerializePrimitive(const mfd::Primitive& primitive, const std::filesystem::path& baseFolder)
{
    json node = json::object();
    if (!primitive.id.empty())
    {
        node["id"] = primitive.id;
    }
    node["type"] = PrimitiveTypeName(primitive.type);
    if (primitive.exposed)
    {
        node["exposed"] = true;
    }
    if (!primitive.reticleRotationSensitive)
    {
        node["reticleRotationSensitive"] = false;
    }
    if (!primitive.reticleScaleSensitive)
    {
        node["reticleScaleSensitive"] = false;
    }
    WriteTransformFields(node, primitive.transform);
    WritePrimitiveStyleFields(node, primitive);
    WritePrimitiveGeometry(node, primitive, baseFolder);
    return node;
}

void WriteReticleInfoFields(json& node, const mfd::ReticleInfo& info)
{
    if (!info.label.empty())
    {
        node["label"] = info.label;
    }

    if (!info.category.empty())
    {
        node["category"] = info.category;
    }

    if (!info.metadata.empty())
    {
        node["metadata"] = info.metadata;
    }
}

std::optional<json> SerializeBlinkBinding(const mfd::ReticleBlinkState& blink)
{
    if (!blink.enabled && blink.typeName.empty())
    {
        return std::nullopt;
    }

    if (blink.enabled && !blink.typeName.empty())
    {
        return json(blink.typeName);
    }

    if (blink.enabled)
    {
        return json(true);
    }

    json node = json::object();
    node["enabled"] = false;
    if (!blink.typeName.empty())
    {
        node["type"] = blink.typeName;
    }

    return node;
}

std::optional<json> SerializePageBlinkTypes(const mfd::PageDefinition& page)
{
    if (page.blinkTypes.empty())
    {
        return std::nullopt;
    }

    json blinkTypes = json::array();
    for (const auto& blinkType : page.blinkTypes)
    {
        blinkTypes.push_back(json {
            {"name", blinkType.name},
            {"durationMs", blinkType.durationMs}});
    }

    return blinkTypes;
}

struct SerializedPageLayerState
{
    json layers = json::array();
    std::unordered_set<std::string> normalizedLayerIds;
};

void ValidatePageNameCatalog(const mfd::MfdDocument& document)
{
    std::unordered_set<std::string> normalizedPageNames;
    normalizedPageNames.reserve(document.pages.size());
    for (const auto& page : document.pages)
    {
        const std::string normalizedPageName = mfd::NormalizePageName(page.name);
        if (normalizedPageName.empty())
        {
            throw std::runtime_error("Page names cannot be empty before saving.");
        }

        if (!normalizedPageNames.insert(normalizedPageName).second)
        {
            throw std::runtime_error("Duplicate page name '" + page.name + "' detected after normalization.");
        }
    }
}

void ValidatePageBlinkCatalog(const mfd::PageDefinition& page)
{
    std::unordered_set<std::string> normalizedBlinkNames;
    normalizedBlinkNames.reserve(page.blinkTypes.size());
    for (const auto& blinkType : page.blinkTypes)
    {
        const std::string normalizedBlinkName = mfd::NormalizePageName(blinkType.name);
        if (normalizedBlinkName.empty())
        {
            throw std::runtime_error("Page '" + page.name + "' contains a blink type with an empty name.");
        }

        if (!normalizedBlinkNames.insert(normalizedBlinkName).second)
        {
            throw std::runtime_error("Page '" + page.name + "' contains duplicate blink type '" + blinkType.name + "'.");
        }
    }

    if (!page.defaultBlinkTypeName.empty() &&
        mfd::FindPageBlinkDefinition(page, page.defaultBlinkTypeName) == nullptr)
    {
        throw std::runtime_error("Page '" + page.name + "' references unknown default blink type '" +
                                 page.defaultBlinkTypeName + "'.");
    }
}

void WriteReticleLayerField(json& node, const mfd::ReticleGroup& reticle, const bool required)
{
    if (mfd::NormalizePageName(reticle.layerId).empty())
    {
        if (required)
        {
            throw std::runtime_error("Reticle '" + reticle.id + "' must define layerId before serialization.");
        }

        return;
    }

    node["layerId"] = reticle.layerId;
}

SerializedPageLayerState SerializePageLayers(const mfd::PageDefinition& page)
{
    if (page.layers.empty())
    {
        throw std::runtime_error("Page '" + page.name + "' must define a non-empty layers array before saving.");
    }

    SerializedPageLayerState result;
    result.normalizedLayerIds.reserve(page.layers.size());
    for (const auto& layer : page.layers)
    {
        const std::string normalizedLayerId = mfd::NormalizePageName(layer.id);
        if (normalizedLayerId.empty())
        {
            throw std::runtime_error("Page '" + page.name + "' contains a runtime layer with an empty id.");
        }

        if (!result.normalizedLayerIds.insert(normalizedLayerId).second)
        {
            throw std::runtime_error("Page '" + page.name + "' contains duplicate runtime layer id '" + layer.id + "'.");
        }

        result.layers.push_back(json {{"id", layer.id}});
    }

    return result;
}

void ValidateStaticReticleLayers(const mfd::PageDefinition& page,
                                 const mfd::ReticleLibrary& library,
                                 const std::unordered_set<std::string>& normalizedLayerIds)
{
    std::unordered_set<std::string> normalizedReticleIds;
    normalizedReticleIds.reserve(page.staticReticles.size());
    for (const auto& reticle : page.staticReticles)
    {
        const std::string normalizedReticleId = mfd::NormalizePageName(reticle.id);
        if (normalizedReticleId.empty())
        {
            throw std::runtime_error("Static reticle ids cannot be empty on page '" + page.name + "'.");
        }

        if (!normalizedReticleIds.insert(normalizedReticleId).second)
        {
            throw std::runtime_error("Page '" + page.name + "' contains duplicate static reticle id '" + reticle.id + "'.");
        }

        const std::string normalizedLayerId = mfd::NormalizePageName(reticle.layerId);
        if (normalizedLayerId.empty())
        {
            throw std::runtime_error(
                "Static reticle '" + reticle.id + "' must define layerId on page '" + page.name + "' before saving.");
        }

        if (normalizedLayerIds.find(normalizedLayerId) == normalizedLayerIds.end())
        {
            throw std::runtime_error(
                "Static reticle '" + reticle.id + "' references unknown runtime layer '" + reticle.layerId +
                "' on page '" + page.name + "'.");
        }

        if (!reticle.sourceTemplateId.empty() && library.find(reticle.sourceTemplateId) == library.end())
        {
            throw std::runtime_error("Static reticle '" + reticle.id + "' references unknown template '" +
                                     reticle.sourceTemplateId + "' on page '" + page.name + "'.");
        }

        if (reticle.clipping.mode != mfd::ReticleClipMode::None && mfd::ResolveClipPrimitive(reticle) == nullptr)
        {
            throw std::runtime_error("Static reticle '" + reticle.id +
                                     "' references one invalid clipping primitive on page '" + page.name + "'.");
        }

        if (reticle.blink.enabled &&
            !reticle.blink.typeName.empty() &&
            mfd::FindPageBlinkDefinition(page, reticle.blink.typeName) == nullptr)
        {
            throw std::runtime_error("Static reticle '" + reticle.id + "' references unknown blink type '" +
                                     reticle.blink.typeName + "' on page '" + page.name + "'.");
        }
    }
}

std::optional<json> SerializeDynamicReticleBindings(const mfd::PageDefinition& page,
                                                    const mfd::ReticleLibrary& library,
                                                    const std::unordered_set<std::string>& normalizedLayerIds)
{
    if (page.dynamicReticleBindings.empty())
    {
        return std::nullopt;
    }

    json bindings = json::array();
    std::unordered_set<std::string> normalizedTemplateIds;
    std::unordered_map<std::string, std::unordered_set<int>> orderInLayerByLayer;
    for (const auto& binding : page.dynamicReticleBindings)
    {
        const std::string normalizedTemplateId = mfd::NormalizePageName(binding.templateId);
        if (normalizedTemplateId.empty())
        {
            throw std::runtime_error("Dynamic reticle bindings on page '" + page.name +
                                     "' must define a non-empty templateId before saving.");
        }

        if (!normalizedTemplateIds.insert(normalizedTemplateId).second)
        {
            throw std::runtime_error(
                "Page '" + page.name + "' contains duplicate dynamic reticle binding template '" + binding.templateId +
                "'.");
        }

        if (library.find(binding.templateId) == library.end())
        {
            throw std::runtime_error("Dynamic reticle binding '" + binding.templateId +
                                     "' references one unloaded template on page '" + page.name + "'.");
        }

        const std::string normalizedLayerId = mfd::NormalizePageName(binding.layerId);
        if (normalizedLayerId.empty())
        {
            throw std::runtime_error("Dynamic reticle binding '" + binding.templateId +
                                     "' on page '" + page.name +
                                     "' must define a non-empty layerId before saving.");
        }

        if (normalizedLayerIds.find(normalizedLayerId) == normalizedLayerIds.end())
        {
            throw std::runtime_error(
                "Dynamic reticle binding '" + binding.templateId + "' references unknown runtime layer '" +
                binding.layerId + "' on page '" + page.name + "'.");
        }

        if (!orderInLayerByLayer[normalizedLayerId].insert(binding.orderInLayer).second)
        {
            throw std::runtime_error(
                "Page '" + page.name + "' contains duplicate dynamic reticle binding orderInLayer " +
                std::to_string(binding.orderInLayer) + " on runtime layer '" + binding.layerId + "'.");
        }

        bindings.push_back(json {
            {"templateId", binding.templateId},
            {"layerId", binding.layerId},
            {"orderInLayer", binding.orderInLayer}});
    }

    if (bindings.empty())
    {
        return std::nullopt;
    }

    return bindings;
}

void ValidatePageStrobes(const mfd::PageDefinition& page, const mfd::ReticleLibrary& library)
{
    for (const auto& strobe : page.strobes)
    {
        if (strobe.reticle.sourceTemplateId.empty())
        {
            throw std::runtime_error("Page strobe '" + strobe.name + "' must reference one template before saving.");
        }

        if (library.find(strobe.reticle.sourceTemplateId) == library.end())
        {
            throw std::runtime_error("Page strobe '" + strobe.name + "' references unknown template '" +
                                     strobe.reticle.sourceTemplateId + "'.");
        }

        if (strobe.reticle.clipping.mode != mfd::ReticleClipMode::None &&
            mfd::ResolveClipPrimitive(strobe.reticle) == nullptr)
        {
            throw std::runtime_error("Page strobe '" + strobe.name + "' references one invalid clipping primitive.");
        }

        if (strobe.reticle.blink.enabled &&
            !strobe.reticle.blink.typeName.empty() &&
            mfd::FindPageBlinkDefinition(page, strobe.reticle.blink.typeName) == nullptr)
        {
            throw std::runtime_error("Page strobe '" + strobe.name + "' references unknown blink type '" +
                                     strobe.reticle.blink.typeName + "'.");
        }
    }
}

std::optional<json> SerializePageEditorState(const mfd::PageDefinition& page)
{
    const bool hasHiddenLayer = std::any_of(page.editor.layers.begin(),
                                            page.editor.layers.end(),
                                            [](const mfd::EditorLayerDefinition& layer)
                                            {
                                                return !layer.id.empty() && !layer.visible;
                                            });
    if (!hasHiddenLayer)
    {
        return std::nullopt;
    }

    json editorState = json::object();

    json layers = json::array();
    for (const auto& layer : page.editor.layers)
    {
        if (layer.id.empty())
        {
            continue;
        }

        json layerNode = json {
            {"id", layer.id}};
        if (!layer.visible)
        {
            layerNode["visible"] = false;
        }

        layers.push_back(std::move(layerNode));
    }

    if (!layers.empty())
    {
        editorState["layers"] = std::move(layers);
    }

    if (editorState.empty())
    {
        return std::nullopt;
    }

    return editorState;
}

std::optional<json> SerializeTextOverrides(const mfd::ReticleGroup& reticle)
{
    json texts = json::object();
    for (const auto& primitive : reticle.primitives)
    {
        const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        if (text == nullptr || primitive.id.empty())
        {
            continue;
        }

        texts[primitive.id] = text->text;
    }

    if (texts.empty())
    {
        return std::nullopt;
    }

    return texts;
}

std::optional<json> SerializeLetterSpacingOverrides(const mfd::ReticleGroup& reticle)
{
    json letterSpacings = json::object();
    for (const auto& primitive : reticle.primitives)
    {
        const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry);
        if (text == nullptr || primitive.id.empty())
        {
            continue;
        }

        letterSpacings[primitive.id] = text->letterSpacing;
    }

    if (letterSpacings.empty())
    {
        return std::nullopt;
    }

    return letterSpacings;
}

json SerializeInlineReticle(const mfd::ReticleGroup& reticle, const std::filesystem::path& baseFolder)
{
    json node = json::object();
    node["id"] = reticle.id;
    WriteReticleInfoFields(node, reticle.info);

    if (!reticle.visible)
    {
        node["visible"] = false;
    }

    if (reticle.drawOnTop)
    {
        node["drawOnTop"] = true;
    }

    WriteReticleLayerField(node, reticle, false);
    WriteTransformFields(node, reticle.transform);
    WriteReticleOverrideFields(node, reticle.overrides);
    if (const auto clipping = SerializeReticleClipping(reticle.clipping); clipping.has_value())
    {
        node["clipping"] = *clipping;
    }

    json elements = json::array();
    for (const auto& primitive : reticle.primitives)
    {
        elements.push_back(SerializePrimitive(primitive, baseFolder));
    }
    node["elements"] = std::move(elements);

    return node;
}

json SerializePageReticle(const mfd::ReticleGroup& reticle,
                          const mfd::ReticleLibrary& library,
                          const std::filesystem::path& baseFolder)
{
    if (!reticle.sourceTemplateId.empty() && library.find(reticle.sourceTemplateId) != library.end())
    {
        json node = json::object();
        node["id"] = reticle.id;
        node["template"] = reticle.sourceTemplateId;
        WriteReticleInfoFields(node, reticle.info);

        if (!reticle.visible)
        {
            node["visible"] = false;
        }

        if (reticle.drawOnTop != library.at(reticle.sourceTemplateId).drawOnTop)
        {
            node["drawOnTop"] = reticle.drawOnTop;
        }

        WriteTransformFields(node, ResolveAuthoredTemplateTransform(reticle, library.at(reticle.sourceTemplateId)));
        WriteReticleOverrideFields(node, reticle.overrides);
        WriteReticleLayerField(node, reticle, true);

        if (const auto texts = SerializeTextOverrides(reticle); texts.has_value())
        {
            node["texts"] = *texts;
        }

        if (const auto letterSpacings = SerializeLetterSpacingOverrides(reticle); letterSpacings.has_value())
        {
            node["letterSpacings"] = *letterSpacings;
        }

        if (const auto blink = SerializeBlinkBinding(reticle.blink); blink.has_value())
        {
            node["blink"] = *blink;
        }

        if (const auto clipping =
                SerializeReticleClippingOverride(reticle.clipping, library.at(reticle.sourceTemplateId).clipping);
            clipping.has_value())
        {
            node["clipping"] = *clipping;
        }

        return node;
    }

    json node = SerializeInlineReticle(reticle, baseFolder);
    if (const auto blink = SerializeBlinkBinding(reticle.blink); blink.has_value())
    {
        node["blink"] = *blink;
    }

    return node;
}

json SerializeStrobe(const mfd::PageStrobeDefinition& strobe,
                     const mfd::ReticleLibrary& library,
                     const std::filesystem::path& /*baseFolder*/)
{
    json node = json::object();
    node["name"] = strobe.name;
    node["id"] = strobe.reticle.id;

    if (!strobe.reticle.sourceTemplateId.empty())
    {
        node["template"] = strobe.reticle.sourceTemplateId;
    }

    const auto templateIterator = library.find(strobe.reticle.sourceTemplateId);
    const mfd::Transform2D authoredTransform = templateIterator == library.end()
                                                   ? strobe.reticle.transform
                                                   : ResolveAuthoredTemplateTransform(strobe.reticle,
                                                                                      templateIterator->second);
    WriteTransformFields(node, authoredTransform);
    WriteReticleOverrideFields(node, strobe.reticle.overrides);
    if (const auto blink = SerializeBlinkBinding(strobe.reticle.blink); blink.has_value())
    {
        node["blink"] = *blink;
    }
    if (const auto clipping = SerializeReticleClipping(strobe.reticle.clipping); clipping.has_value())
    {
        node["clipping"] = *clipping;
    }
    if (strobe.reticle.drawOnTop)
    {
        node["drawOnTop"] = true;
    }

    json capture = json::object();
    capture["shape"] = strobe.capture.shape == mfd::StrobeCaptureShape::Circle ? "circle" : "rectangle";
    if (strobe.capture.shape == mfd::StrobeCaptureShape::Circle)
    {
        capture["radius"] = strobe.capture.radius;
    }
    else
    {
        capture["size"] = ToVec2(strobe.capture.size);
    }

    node["capture"] = std::move(capture);

    if (strobe.magnet.enabled)
    {
        json magnet = json {
            {"enabled", strobe.magnet.enabled},
            {"radius", strobe.magnet.radius},
            {"strength", strobe.magnet.strength}};
        if (strobe.magnet.visualShapeEnabled)
        {
            magnet["visual"] = json {
                {"enabled", true},
                {"shape", strobe.magnet.visualShape == mfd::StrobeMagnetVisualShape::Circle ? "circle" : "square"},
                {"size", strobe.magnet.visualShapeSize}};
        }
        node["magnet"] = std::move(magnet);
    }

    return node;
}

json SerializePage(const mfd::PageDefinition& page,
                   const mfd::ReticleLibrary& library,
                   const std::filesystem::path& baseFolder)
{
    ValidatePageBlinkCatalog(page);
    const SerializedPageLayerState pageLayers = SerializePageLayers(page);
    ValidateStaticReticleLayers(page, library, pageLayers.normalizedLayerIds);
    ValidatePageStrobes(page, library);

    json node = json::object();
    node["name"] = page.name;
    node["title"] = page.title;
    if (const auto titleDisplay = SerializePageTitleDisplay(page.titleDisplay); titleDisplay.has_value())
    {
        node["titleDisplay"] = *titleDisplay;
    }
    node["bg"] = ToHexColor(page.backgroundColor);

    if (!IsZero(page.view.center.x) || !IsZero(page.view.center.y) || std::abs(page.view.zoom - 1.0f) >= 0.0001f)
    {
        node["view"] = json {
            {"center", ToVec2(page.view.center)},
            {"zoom", page.view.zoom}};
    }

    if (!page.strobes.empty())
    {
        json strobes = json::array();
        for (const auto& strobe : page.strobes)
        {
            strobes.push_back(SerializeStrobe(strobe, library, baseFolder));
        }

        node["strobes"] = std::move(strobes);
        node["activeStrobe"] = page.activeStrobeName.empty() ? page.strobes.front().name : page.activeStrobeName;
    }

    if (const auto blinkTypes = SerializePageBlinkTypes(page); blinkTypes.has_value())
    {
        node["blinkTypes"] = *blinkTypes;
    }

    if (!page.defaultBlinkTypeName.empty())
    {
        node["defaultBlink"] = page.defaultBlinkTypeName;
    }

    node["layers"] = pageLayers.layers;

    if (const auto bindings = SerializeDynamicReticleBindings(page, library, pageLayers.normalizedLayerIds); bindings.has_value())
    {
        node["dynamicReticleBindings"] = *bindings;
    }

    if (const auto editorState = SerializePageEditorState(page); editorState.has_value())
    {
        node["_editor"] = *editorState;
    }

    json reticles = json::array();
    for (const auto& reticle : page.staticReticles)
    {
        reticles.push_back(SerializePageReticle(reticle, library, baseFolder));
    }
    node["staticReticles"] = std::move(reticles);
    return node;
}

json SerializeWindow(const mfd::WindowAssetDefinition& window,
                     const mfd::MfdDocument& document,
                     const EditorFileLayout& layout)
{
    const std::filesystem::path baseFolder = window.sourceFile.parent_path();

    json node = json::object();
    node["title"] = window.title;
    node["size"] = json::array({window.width, window.height});
    node["position"] = json::array({window.positionX, window.positionY});
    node["targetFps"] = window.targetFps;
    if (!window.fontFile.empty())
    {
        node["fontFile"] = SerializePathRelativeTo(window.fontFile, baseFolder);
    }
    node["reticleLibraryFolder"] = SerializePathRelativeTo(window.reticleLibraryFolder, baseFolder);

    if (window.commandTransports.udp.has_value())
    {
        json commands = json::object();

        if (window.commandTransports.udp.has_value())
        {
            const auto& udp = *window.commandTransports.udp;
            commands["udp"] = json {
                {"enabled", udp.enabled},
                {"address", udp.address},
                {"port", udp.port},
                {"maxPacketSize", udp.maxPacketSize}};
        }

        node["commands"] = std::move(commands);
    }

    if (window.feedbackTransports.udp.has_value())
    {
        const auto& udp = *window.feedbackTransports.udp;
        json feedback = json {
            {"udp",
             {
                 {"enabled", udp.enabled},
                 {"address", udp.address},
                 {"port", udp.port},
                 {"maxPacketSize", udp.maxPacketSize},
             }}};
        WriteFeedbackIntervalField(
            feedback,
            "fastIntervalMs",
            "fastIntervalSeconds",
            window.feedbackFastIntervalSeconds);
        WriteFeedbackIntervalField(
            feedback,
            "heartbeatIntervalMs",
            "heartbeatIntervalSeconds",
            window.feedbackHeartbeatIntervalSeconds);
        node["feedback"] = std::move(feedback);
    }

    json pages = json::array();
    std::optional<std::string> defaultPageName;
    for (std::size_t index = 0; index < layout.pageFiles.size() && index < document.pages.size(); ++index)
    {
        const std::string relativePageFile = SerializePathRelativeTo(layout.pageFiles[index], baseFolder);
        pages.push_back(relativePageFile);

        if (document.pages[index].defaultPage && !document.pages[index].name.empty())
        {
            defaultPageName = document.pages[index].name;
        }
    }
    node["pages"] = std::move(pages);

    if (defaultPageName.has_value())
    {
        node["defaultPage"] = *defaultPageName;
    }

    return node;
}

std::optional<json> SerializeReticleClipping(const mfd::ReticleClipState& clipping)
{
    if (clipping.mode == mfd::ReticleClipMode::None)
    {
        return std::nullopt;
    }

    json node = json::object();
    node["mode"] = clipping.mode == mfd::ReticleClipMode::Inner ? "inner" : "outer";
    node["primitive"] = clipping.primitiveId;
    if (clipping.eraseLayerOnly)
    {
        node["eraseLayerOnly"] = true;
    }
    return node;
}

std::optional<json> SerializeReticleClippingOverride(const mfd::ReticleClipState& clipping,
                                                     const mfd::ReticleClipState& inherited)
{
    if (clipping.mode == inherited.mode && clipping.primitiveId == inherited.primitiveId &&
        clipping.eraseLayerOnly == inherited.eraseLayerOnly)
    {
        return std::nullopt;
    }

    if (clipping.mode == mfd::ReticleClipMode::None)
    {
        return json {
            {"mode", "none"}};
    }

    return SerializeReticleClipping(clipping);
}

void WriteJsonFile(const std::filesystem::path& path, const json& value)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open file for writing: " + path.string());
    }

    stream << value.dump(2) << '\n';
}

std::string JsonToString(const json& value)
{
    return value.dump(2) + '\n';
}

void DeleteFileIfPresent(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        return;
    }

    std::filesystem::remove(path, error);
}

bool IsPathWithinRoot(const std::filesystem::path& candidate, const std::filesystem::path& root)
{
    // Reject any bundle path that does not resolve to a descendant of the authored asset root. The
    // candidate is made absolute against the root when relative, then normalized, so "..", absolute
    // paths and drive changes that escape the root are refused before any write touches the disk.
    const std::filesystem::path normalizedRoot = root.lexically_normal();
    const std::filesystem::path absoluteCandidate =
        candidate.is_absolute() ? candidate.lexically_normal() : (normalizedRoot / candidate).lexically_normal();

    const std::filesystem::path relative = absoluteCandidate.lexically_relative(normalizedRoot);
    if (relative.empty())
    {
        return false;
    }

    return *relative.begin() != "..";
}

std::optional<std::string> TryReadTemplateId(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        return std::nullopt;
    }

    json node;
    stream >> node;
    if (!node.is_object() || !node.contains("id") || !node.at("id").is_string())
    {
        return std::nullopt;
    }

    return node.at("id").get<std::string>();
}

std::string MakeNormalizedGenericPathKey(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

// Serializes every authored file (window, pages, reticle templates) into (path, document) pairs
// without touching disk, so both the on-disk save and the in-memory recovery bundle share one source
// of truth for what a saved document looks like.
std::vector<std::pair<std::filesystem::path, json>> CollectEditorDocumentFiles(
    const mfd::LoadedWindowConfiguration& loaded,
    const EditorFileLayout& layout)
{
    if (layout.pageFiles.size() != loaded.document.pages.size())
    {
        throw std::runtime_error("Page file layout does not match the number of pages");
    }

    ValidatePageNameCatalog(loaded.document);

    std::vector<std::pair<std::filesystem::path, json>> files;
    files.reserve(1U + loaded.document.pages.size() + loaded.document.reticleLibrary.size());

    files.emplace_back(loaded.window.sourceFile, SerializeWindow(loaded.window, loaded.document, layout));

    for (std::size_t index = 0; index < loaded.document.pages.size(); ++index)
    {
        files.emplace_back(
            layout.pageFiles[index],
            SerializePage(loaded.document.pages[index],
                          loaded.document.reticleLibrary,
                          layout.pageFiles[index].parent_path()));
    }

    std::vector<std::string> templateIds;
    templateIds.reserve(loaded.document.reticleLibrary.size());
    for (const auto& entry : loaded.document.reticleLibrary)
    {
        templateIds.push_back(entry.first);
    }
    std::sort(templateIds.begin(), templateIds.end());

    for (const auto& templateId : templateIds)
    {
        const auto iterator = loaded.document.reticleLibrary.find(templateId);
        if (iterator == loaded.document.reticleLibrary.end())
        {
            continue;
        }

        const auto fileIterator = layout.templateFiles.find(templateId);
        const std::filesystem::path templatePath =
            fileIterator != layout.templateFiles.end()
                ? fileIterator->second
                : DefaultTemplateFilePath(loaded.window.reticleLibraryFolder, templateId);

        files.emplace_back(templatePath, SerializeInlineReticle(iterator->second, templatePath.parent_path()));
    }

    return files;
}
} // namespace

std::filesystem::path DefaultPageFilePath(const std::filesystem::path& windowFile, const std::string_view pageName)
{
    return windowFile.parent_path() / (mfd::NormalizePageName(pageName) + ".json");
}

std::filesystem::path DefaultTemplateFilePath(const std::filesystem::path& libraryFolder, const std::string_view templateId)
{
    return libraryFolder / (mfd::NormalizePageName(templateId) + ".json");
}

bool DiscoverReticleTemplateFiles(const std::filesystem::path& libraryFolder,
                                  EditorFileLayout& layout,
                                  std::string* error)
{
    try
    {
        layout.templateFiles.clear();

        if (!std::filesystem::exists(libraryFolder))
        {
            throw std::runtime_error("Reticle library folder does not exist: " + libraryFolder.string());
        }

        for (const auto& entry : std::filesystem::directory_iterator(libraryFolder))
        {
            if (!entry.is_regular_file() || Lowercase(entry.path().extension().string()) != ".json")
            {
                continue;
            }

            if (const auto templateId = TryReadTemplateId(entry.path()); templateId.has_value())
            {
                layout.templateFiles.emplace(*templateId, entry.path().lexically_normal());
            }
        }

        if (error != nullptr)
        {
            error->clear();
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }
        return false;
    }
}

std::string SerializeReticleTemplateToJsonString(const mfd::ReticleGroup& reticle,
                                                 const std::filesystem::path& baseFolder)
{
    return JsonToString(SerializeInlineReticle(reticle, baseFolder));
}

std::string SerializePageReticleToJsonString(const mfd::ReticleGroup& reticle,
                                             const mfd::ReticleLibrary& library,
                                             const std::filesystem::path& baseFolder)
{
    return JsonToString(SerializePageReticle(reticle, library, baseFolder));
}

std::string SerializePageToJsonString(const mfd::PageDefinition& page,
                                      const mfd::ReticleLibrary& library,
                                      const EditorFileLayout& layout,
                                      const std::size_t pageIndex)
{
    const std::filesystem::path baseFolder =
        pageIndex < layout.pageFiles.size() ? layout.pageFiles[pageIndex].parent_path() : std::filesystem::path {};
    return JsonToString(SerializePage(page, library, baseFolder));
}

std::string SerializeWindowToJsonString(const mfd::WindowAssetDefinition& window,
                                        const mfd::MfdDocument& document,
                                        const EditorFileLayout& layout)
{
    return JsonToString(SerializeWindow(window, document, layout));
}

bool SaveEditorDocument(const mfd::LoadedWindowConfiguration& loaded,
                        const EditorFileLayout& layout,
                        std::string* error)
{
    try
    {
        const std::vector<std::pair<std::filesystem::path, json>> files =
            CollectEditorDocumentFiles(loaded, layout);
        for (const auto& [path, document] : files)
        {
            WriteJsonFile(path, document);
        }

        std::unordered_set<std::string> currentPageFiles;
        currentPageFiles.reserve(layout.pageFiles.size());
        for (const auto& pageFile : layout.pageFiles)
        {
            currentPageFiles.insert(MakeNormalizedGenericPathKey(pageFile));
        }
        for (const auto& removedPageFile : layout.removedPageFiles)
        {
            if (currentPageFiles.find(MakeNormalizedGenericPathKey(removedPageFile)) == currentPageFiles.end())
            {
                DeleteFileIfPresent(removedPageFile);
            }
        }

        std::unordered_set<std::string> currentTemplateFiles;
        currentTemplateFiles.reserve(layout.templateFiles.size());
        for (const auto& entry : layout.templateFiles)
        {
            currentTemplateFiles.insert(MakeNormalizedGenericPathKey(entry.second));
        }

        for (const auto& removedTemplateFile : layout.removedTemplateFiles)
        {
            if (currentTemplateFiles.find(MakeNormalizedGenericPathKey(removedTemplateFile)) == currentTemplateFiles.end())
            {
                DeleteFileIfPresent(removedTemplateFile);
            }
        }

        if (error != nullptr)
        {
            error->clear();
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }
        return false;
    }
}

std::string SerializeRecoveryBundleToJsonString(const mfd::LoadedWindowConfiguration& loaded,
                                                const EditorFileLayout& layout)
{
    const std::vector<std::pair<std::filesystem::path, json>> files = CollectEditorDocumentFiles(loaded, layout);

    json bundle = json::object();
    bundle["schemaVersion"] = 1;
    bundle["windowFile"] = loaded.window.sourceFile.generic_string();

    json fileArray = json::array();
    for (const auto& [path, document] : files)
    {
        json entry = json::object();
        entry["path"] = path.generic_string();
        entry["document"] = document;
        fileArray.push_back(std::move(entry));
    }
    bundle["files"] = std::move(fileArray);

    return bundle.dump(2);
}

bool RestoreRecoveryBundle(const std::string& bundleJson,
                           const std::filesystem::path& assetRoot,
                           std::filesystem::path& windowFile,
                           std::string* error)
{
    std::vector<std::filesystem::path> stagedTempFiles;
    try
    {
        const json bundle = json::parse(bundleJson);
        if (!bundle.is_object() ||
            !bundle.contains("windowFile") || !bundle.at("windowFile").is_string() ||
            !bundle.contains("files") || !bundle.at("files").is_array())
        {
            throw std::runtime_error("Malformed recovery bundle");
        }

        const std::filesystem::path recoveredWindowFile(bundle.at("windowFile").get<std::string>());
        if (!IsPathWithinRoot(recoveredWindowFile, assetRoot))
        {
            throw std::runtime_error("Recovery bundle window path escapes the asset root: " +
                                     recoveredWindowFile.string());
        }

        // Phase 1: validate every entry and its target path before writing anything to disk.
        std::vector<std::pair<std::filesystem::path, const json*>> targets;
        targets.reserve(bundle.at("files").size());
        for (const auto& entry : bundle.at("files"))
        {
            if (!entry.is_object() || !entry.contains("path") || !entry.at("path").is_string() ||
                !entry.contains("document"))
            {
                throw std::runtime_error("Malformed recovery bundle entry");
            }

            std::filesystem::path target(entry.at("path").get<std::string>());
            if (!IsPathWithinRoot(target, assetRoot))
            {
                throw std::runtime_error("Recovery bundle path escapes the asset root: " + target.string());
            }

            targets.emplace_back(std::move(target), &entry.at("document"));
        }

        // Phase 2: stage every document into a temporary sibling file. A failure here leaves the
        // authored files untouched; the catch removes any temporary already written.
        for (const auto& [target, document] : targets)
        {
            std::filesystem::path tempFile = target;
            tempFile += ".recovery_tmp";
            WriteJsonFile(tempFile, *document);
            stagedTempFiles.push_back(std::move(tempFile));
        }

        // Phase 3: promote each staged file into place. Same-directory renames are atomic.
        for (std::size_t index = 0; index < targets.size(); ++index)
        {
            std::filesystem::rename(stagedTempFiles[index], targets[index].first);
        }

        windowFile = recoveredWindowFile;
        if (error != nullptr)
        {
            error->clear();
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        for (const std::filesystem::path& tempFile : stagedTempFiles)
        {
            std::error_code removeError;
            std::filesystem::remove(tempFile, removeError);
        }

        if (error != nullptr)
        {
            *error = exception.what();
        }
        return false;
    }
}
} // namespace editor
