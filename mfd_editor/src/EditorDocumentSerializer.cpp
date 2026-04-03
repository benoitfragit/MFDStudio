/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorDocumentSerializer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
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

void WritePrimitiveStyleFields(json& node, const mfd::PrimitiveStyle& style)
{
    const mfd::PrimitiveStyle defaults {};

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

    if (style.fillColor.r != defaults.fillColor.r ||
        style.fillColor.g != defaults.fillColor.g ||
        style.fillColor.b != defaults.fillColor.b ||
        style.fillColor.a != defaults.fillColor.a)
    {
        node["fill"] = ToHexColor(style.fillColor);
    }

    if (style.filled != defaults.filled)
    {
        node["filled"] = style.filled;
    }
}

void WritePrimitiveGeometry(json& node, const mfd::Primitive& primitive)
{
    if (const auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        node["text"] = text->text;
        node["size"] = text->fontSize;
        if (std::abs(text->letterSpacing - mfd::kDefaultTextLetterSpacing) >= 0.0001f)
        {
            node["letterSpacing"] = text->letterSpacing;
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
        if (std::abs(square->width - square->height) < 0.0001f)
        {
            node["size"] = square->width;
        }
        else
        {
            node["width"] = square->width;
            node["height"] = square->height;
        }
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
    }

    return "line";
}

json SerializePrimitive(const mfd::Primitive& primitive)
{
    json node = json::object();
    if (!primitive.id.empty())
    {
        node["id"] = primitive.id;
    }
    node["type"] = PrimitiveTypeName(primitive.type);
    WriteTransformFields(node, primitive.transform);
    WritePrimitiveStyleFields(node, primitive.style);
    WritePrimitiveGeometry(node, primitive);
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

void WriteReticleEditorFields(json& node, const mfd::ReticleGroup& reticle)
{
    if (reticle.editor.layerId.empty())
    {
        return;
    }

    node["_editor"] = json {
        {"layer", reticle.editor.layerId}};
}

std::optional<json> SerializePageEditorState(const mfd::PageDefinition& page)
{
    if (page.editor.layers.empty())
    {
        return std::nullopt;
    }

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

    if (layers.empty())
    {
        return std::nullopt;
    }

    return json {
        {"layers", std::move(layers)}};
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

json SerializeInlineReticle(const mfd::ReticleGroup& reticle)
{
    json node = json::object();
    node["id"] = reticle.id;
    WriteReticleInfoFields(node, reticle.info);

    if (!reticle.visible)
    {
        node["visible"] = false;
    }

    WriteTransformFields(node, reticle.transform);
    WriteReticleOverrideFields(node, reticle.overrides);

    json elements = json::array();
    for (const auto& primitive : reticle.primitives)
    {
        elements.push_back(SerializePrimitive(primitive));
    }
    node["elements"] = std::move(elements);

    return node;
}

json SerializePageReticle(const mfd::ReticleGroup& reticle, const mfd::ReticleLibrary& library)
{
    if (!reticle.sourceTemplateId.empty() && library.contains(reticle.sourceTemplateId))
    {
        json node = json::object();
        node["id"] = reticle.id;
        node["template"] = reticle.sourceTemplateId;
        WriteReticleInfoFields(node, reticle.info);

        if (!reticle.visible)
        {
            node["visible"] = false;
        }

        WriteTransformFields(node, reticle.transform);
        WriteReticleOverrideFields(node, reticle.overrides);
        WriteReticleEditorFields(node, reticle);

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

        return node;
    }

    json node = SerializeInlineReticle(reticle);
    WriteReticleEditorFields(node, reticle);
    if (const auto blink = SerializeBlinkBinding(reticle.blink); blink.has_value())
    {
        node["blink"] = *blink;
    }

    return node;
}

json SerializeStrobe(const mfd::PageStrobeDefinition& strobe)
{
    json node = json::object();
    node["id"] = strobe.reticle.id;

    if (!strobe.reticle.sourceTemplateId.empty())
    {
        node["template"] = strobe.reticle.sourceTemplateId;
    }

    WriteTransformFields(node, strobe.reticle.transform);
    WriteReticleOverrideFields(node, strobe.reticle.overrides);
    if (const auto blink = SerializeBlinkBinding(strobe.reticle.blink); blink.has_value())
    {
        node["blink"] = *blink;
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
        node["magnet"] = json {
            {"enabled", strobe.magnet.enabled},
            {"radius", strobe.magnet.radius},
            {"strength", strobe.magnet.strength}};
    }

    return node;
}

json SerializePage(const mfd::PageDefinition& page, const mfd::ReticleLibrary& library)
{
    json node = json::object();
    node["name"] = page.name;
    node["title"] = page.title;
    node["bg"] = ToHexColor(page.backgroundColor);

    if (!IsZero(page.view.center.x) || !IsZero(page.view.center.y) || std::abs(page.view.zoom - 1.0f) >= 0.0001f)
    {
        node["view"] = json {
            {"center", ToVec2(page.view.center)},
            {"zoom", page.view.zoom}};
    }

    if (page.strobe.has_value())
    {
        node["strobe"] = SerializeStrobe(*page.strobe);
    }

    if (const auto blinkTypes = SerializePageBlinkTypes(page); blinkTypes.has_value())
    {
        node["blinkTypes"] = *blinkTypes;
    }

    if (!page.defaultBlinkTypeName.empty())
    {
        node["defaultBlink"] = page.defaultBlinkTypeName;
    }

    if (const auto editorState = SerializePageEditorState(page); editorState.has_value())
    {
        node["_editor"] = *editorState;
    }

    json reticles = json::array();
    for (const auto& reticle : page.staticReticles)
    {
        reticles.push_back(SerializePageReticle(reticle, library));
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
        node["fontFile"] = std::filesystem::relative(window.fontFile, baseFolder).generic_string();
    }
    node["reticleLibraryFolder"] =
        std::filesystem::relative(window.reticleLibraryFolder, baseFolder).generic_string();

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
        node["feedback"] = json {
            {"udp",
             {
                 {"enabled", udp.enabled},
                 {"address", udp.address},
                 {"port", udp.port},
                 {"maxPacketSize", udp.maxPacketSize},
             }}};
    }

    json pages = json::array();
    std::optional<std::string> defaultPageName;
    for (std::size_t index = 0; index < layout.pageFiles.size() && index < document.pages.size(); ++index)
    {
        const std::string relativePageFile =
            std::filesystem::relative(layout.pageFiles[index], baseFolder).generic_string();
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

std::string SerializeReticleTemplateToJsonString(const mfd::ReticleGroup& reticle)
{
    return JsonToString(SerializeInlineReticle(reticle));
}

std::string SerializePageReticleToJsonString(const mfd::ReticleGroup& reticle,
                                             const mfd::ReticleLibrary& library)
{
    return JsonToString(SerializePageReticle(reticle, library));
}

bool SaveEditorDocument(const mfd::LoadedWindowConfiguration& loaded,
                        const EditorFileLayout& layout,
                        std::string* error)
{
    try
    {
        if (layout.pageFiles.size() != loaded.document.pages.size())
        {
            throw std::runtime_error("Page file layout does not match the number of pages");
        }

        WriteJsonFile(loaded.window.sourceFile, SerializeWindow(loaded.window, loaded.document, layout));

        for (std::size_t index = 0; index < loaded.document.pages.size(); ++index)
        {
            WriteJsonFile(layout.pageFiles[index], SerializePage(loaded.document.pages[index], loaded.document.reticleLibrary));
        }

        std::vector<std::string> templateIds;
        templateIds.reserve(loaded.document.reticleLibrary.size());
        for (const auto& [templateId, group] : loaded.document.reticleLibrary)
        {
            (void)group;
            templateIds.push_back(templateId);
        }
        std::sort(templateIds.begin(), templateIds.end());

        for (const auto& templateId : templateIds)
        {
            const auto iterator = loaded.document.reticleLibrary.find(templateId);
            if (iterator == loaded.document.reticleLibrary.end())
            {
                continue;
            }

            auto fileIterator = layout.templateFiles.find(templateId);
            const std::filesystem::path templatePath =
                fileIterator != layout.templateFiles.end()
                    ? fileIterator->second
                    : DefaultTemplateFilePath(loaded.window.reticleLibraryFolder, templateId);

            WriteJsonFile(templatePath, SerializeInlineReticle(iterator->second));
        }

        const std::unordered_set<std::filesystem::path> currentPageFiles(layout.pageFiles.begin(), layout.pageFiles.end());
        for (const auto& removedPageFile : layout.removedPageFiles)
        {
            if (!currentPageFiles.contains(removedPageFile))
            {
                DeleteFileIfPresent(removedPageFile);
            }
        }

        std::unordered_set<std::filesystem::path> currentTemplateFiles;
        currentTemplateFiles.reserve(layout.templateFiles.size());
        for (const auto& [templateId, templateFile] : layout.templateFiles)
        {
            (void)templateId;
            currentTemplateFiles.insert(templateFile);
        }

        for (const auto& removedTemplateFile : layout.removedTemplateFiles)
        {
            if (!currentTemplateFiles.contains(removedTemplateFile))
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
} // namespace editor
