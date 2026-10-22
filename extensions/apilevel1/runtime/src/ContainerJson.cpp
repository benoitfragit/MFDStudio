/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerJson.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "mfd/apilevel1/ContainerLayoutEngine.h"

namespace mfd::api1
{
namespace
{
using json = nlohmann::json;

float ParseFiniteFloat(const json& value, const std::string& field)
{
    if (!value.is_number())
    {
        throw std::runtime_error(field + " must be numeric.");
    }
    const float result = value.get<float>();
    if (!std::isfinite(result))
    {
        throw std::runtime_error(field + " must be finite.");
    }
    return result;
}

Vec2 ParseVec2(const json& value, const std::string& field)
{
    if (!value.is_array() || value.size() != 2U)
    {
        throw std::runtime_error(field + " must contain exactly two numbers.");
    }
    return {ParseFiniteFloat(value.at(0U), field), ParseFiniteFloat(value.at(1U), field)};
}

ContainerInsets ParseInsets(const json& value, const std::string& field)
{
    if (value.is_number())
    {
        const float uniform = ParseFiniteFloat(value, field);
        return {uniform, uniform, uniform, uniform};
    }
    if (!value.is_array() || value.size() != 4U)
    {
        throw std::runtime_error(field + " must be one number or [left, top, right, bottom].");
    }
    return {ParseFiniteFloat(value.at(0U), field),
            ParseFiniteFloat(value.at(1U), field),
            ParseFiniteFloat(value.at(2U), field),
            ParseFiniteFloat(value.at(3U), field)};
}

ColorRgba ParseColor(const json& value, const std::string& field)
{
    if (!value.is_array() || (value.size() != 3U && value.size() != 4U))
    {
        throw std::runtime_error(field + " must be [r, g, b] or [r, g, b, a].");
    }
    std::array<std::uint8_t, 4U> channels {0U, 0U, 0U, 255U};
    for (std::size_t index = 0U; index < value.size(); ++index)
    {
        if (!value.at(index).is_number_integer() && !value.at(index).is_number_unsigned())
        {
            throw std::runtime_error(field + " channels must be integers in [0, 255].");
        }
        const json& channel = value.at(index);
        // Compare in JSON's original signed/unsigned domain before narrowing.
        if (channel.is_number_unsigned() ? channel.get<std::uint64_t>() > 255U
                                         : (channel.get<std::int64_t>() < 0 ||
                                            channel.get<std::int64_t>() > 255))
        {
            throw std::runtime_error(field + " channels must be integers in [0, 255].");
        }
        channels[index] = channel.get<std::uint8_t>();
    }
    return {channels[0U], channels[1U], channels[2U], channels[3U]};
}

std::size_t ParseEnumIndex(const json& value,
                           const std::initializer_list<std::string_view> labels,
                           const std::string& field)
{
    if (!value.is_string())
    {
        throw std::runtime_error(field + " must be a string.");
    }
    const std::string text = value.get<std::string>();
    std::size_t index = 0U;
    for (const std::string_view label : labels)
    {
        if (text == label)
        {
            return index;
        }
        ++index;
    }
    throw std::runtime_error(field + " contains unsupported value '" + text + "'.");
}

std::uint32_t ParsePositiveIndex(const json& node,
                                 const char* field,
                                 const std::uint32_t fallback,
                                 const bool allowZero)
{
    if (!node.contains(field))
    {
        return fallback;
    }
    const json& value = node.at(field);
    if (!value.is_number_unsigned() && !value.is_number_integer())
    {
        throw std::runtime_error(std::string(field) + " must be an integer.");
    }
    std::uint64_t parsed = 0U;
    if (value.is_number_unsigned())
    {
        parsed = value.get<std::uint64_t>();
    }
    else
    {
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue < (allowZero ? 0 : 1))
        {
            throw std::runtime_error(std::string(field) + " is outside the supported range.");
        }
        parsed = static_cast<std::uint64_t>(signedValue);
    }
    if ((!allowZero && parsed == 0U) || parsed > kMaximumContainerCount)
    {
        throw std::runtime_error(std::string(field) + " is outside the supported range.");
    }
    return static_cast<std::uint32_t>(parsed);
}

ContainerChildPlacement ParseChild(const json& node)
{
    if (!node.is_object() || !node.contains("id") || !node.at("id").is_string())
    {
        throw std::runtime_error("Each container child must define a string id.");
    }
    ContainerChildPlacement result;
    result.childId = node.at("id").get<std::string>();
    if (node.contains("resizeMode"))
    {
        result.resizeMode = static_cast<ChildResizeMode>(ParseEnumIndex(
            node.at("resizeMode"), {"fixed", "proportional", "stretch"}, "resizeMode"));
    }
    if (node.contains("horizontalAlignment"))
    {
        result.horizontalAlignment = static_cast<ContainerAlignment>(ParseEnumIndex(
            node.at("horizontalAlignment"),
            {"start", "center", "end", "stretch"},
            "horizontalAlignment"));
    }
    if (node.contains("verticalAlignment"))
    {
        result.verticalAlignment = static_cast<ContainerAlignment>(ParseEnumIndex(
            node.at("verticalAlignment"),
            {"start", "center", "end", "stretch"},
            "verticalAlignment"));
    }
    result.row = ParsePositiveIndex(node, "row", 0U, true);
    result.column = ParsePositiveIndex(node, "column", 0U, true);
    result.rowSpan = ParsePositiveIndex(node, "rowSpan", 1U, false);
    result.columnSpan = ParsePositiveIndex(node, "columnSpan", 1U, false);
    if (node.contains("renderOrder"))
    {
        if (!node.at("renderOrder").is_number_integer())
        {
            throw std::runtime_error("renderOrder must be an integer.");
        }
        result.renderOrder = node.at("renderOrder").get<int>();
    }
    return result;
}

void ParseAppearance(const json& node, ContainerDefinition& result)
{
    if (!node.contains("appearance"))
    {
        return;
    }
    const json& appearance = node.at("appearance");
    if (!appearance.is_object())
    {
        throw std::runtime_error("Container appearance must be an object.");
    }
    result.appearance.backgroundVisible = appearance.value("backgroundVisible", false);
    result.appearance.borderVisible = appearance.value("borderVisible", false);
    if (appearance.contains("backgroundColor"))
    {
        result.appearance.backgroundColor = ParseColor(appearance.at("backgroundColor"), "backgroundColor");
    }
    if (appearance.contains("borderColor"))
    {
        result.appearance.borderColor = ParseColor(appearance.at("borderColor"), "borderColor");
    }
    if (appearance.contains("borderThickness"))
    {
        result.appearance.borderThickness = ParseFiniteFloat(appearance.at("borderThickness"), "borderThickness");
    }
    if (appearance.contains("cornerRadius"))
    {
        result.appearance.cornerRadius = ParseFiniteFloat(appearance.at("cornerRadius"), "cornerRadius");
    }
}

void ParseScrolling(const json& node, ContainerDefinition& result)
{
    if (!node.contains("scrolling"))
    {
        return;
    }
    const json& scrolling = node.at("scrolling");
    if (!scrolling.is_object())
    {
        throw std::runtime_error("Container scrolling must be an object.");
    }
    if (scrolling.contains("horizontal"))
    {
        result.scrolling.horizontal = static_cast<ScrollMode>(
            ParseEnumIndex(scrolling.at("horizontal"), {"disabled", "automatic", "always"}, "horizontal"));
    }
    if (scrolling.contains("vertical"))
    {
        result.scrolling.vertical = static_cast<ScrollMode>(
            ParseEnumIndex(scrolling.at("vertical"), {"disabled", "automatic", "always"}, "vertical"));
    }
    if (scrolling.contains("offset"))
    {
        result.scrolling.offset = ParseVec2(scrolling.at("offset"), "scroll offset");
    }
    result.scrolling.clipContent = scrolling.value("clipContent", true);
    if (!scrolling.contains("appearance"))
    {
        return;
    }
    const json& appearance = scrolling.at("appearance");
    if (!appearance.is_object())
    {
        throw std::runtime_error("Scrollbar appearance must be an object.");
    }
    ScrollbarAppearance& target = result.scrolling.appearance;
    if (appearance.contains("trackColor")) target.trackColor = ParseColor(appearance.at("trackColor"), "trackColor");
    if (appearance.contains("thumbColor")) target.thumbColor = ParseColor(appearance.at("thumbColor"), "thumbColor");
    if (appearance.contains("hoverColor")) target.thumbHoverColor = ParseColor(appearance.at("hoverColor"), "hoverColor");
    if (appearance.contains("activeColor")) target.thumbActiveColor = ParseColor(appearance.at("activeColor"), "activeColor");
    if (appearance.contains("thickness")) target.thickness = ParseFiniteFloat(appearance.at("thickness"), "thickness");
    if (appearance.contains("cornerRadius")) target.cornerRadius = ParseFiniteFloat(appearance.at("cornerRadius"), "cornerRadius");
    if (appearance.contains("minimumThumbSize")) target.minimumThumbSize = ParseFiniteFloat(appearance.at("minimumThumbSize"), "minimumThumbSize");
    if (appearance.contains("margins")) target.margins = ParseInsets(appearance.at("margins"), "scrollbar margins");
}

ContainerDefinition ParseContainer(const json& node)
{
    if (!node.is_object() || !node.contains("id") || !node.at("id").is_string())
    {
        throw std::runtime_error("Each container must define a string id.");
    }
    ContainerDefinition result;
    result.id = node.at("id").get<std::string>();
    result.name = node.value("name", result.id);
    if (node.contains("type"))
    {
        result.type = static_cast<ContainerType>(ParseEnumIndex(
            node.at("type"),
            {"canvas", "linear", "grid", "overlay"},
            "container type"));
    }
    if (node.contains("sizeMode"))
    {
        result.sizeMode = static_cast<ContainerSizeMode>(ParseEnumIndex(
            node.at("sizeMode"),
            {"fixed", "fitContent", "fitHorizontal", "fitVertical"},
            "container sizeMode"));
    }
    if (node.contains("position")) result.geometry.center = ParseVec2(node.at("position"), "position");
    if (node.contains("size")) result.geometry.size = ParseVec2(node.at("size"), "size");
    if (node.contains("minimumSize")) result.minimumSize = ParseVec2(node.at("minimumSize"), "minimumSize");
    if (node.contains("layoutReferenceSize")) result.layoutReferenceSize = ParseVec2(node.at("layoutReferenceSize"), "layoutReferenceSize");
    else result.layoutReferenceSize = result.geometry.size;
    result.visible = node.value("visible", true);
    result.enabled = node.value("enabled", true);
    result.exposed = node.value("exposed", false);
    if (node.contains("padding")) result.padding = ParseInsets(node.at("padding"), "padding");
    if (node.contains("spacing")) result.spacing = ParseFiniteFloat(node.at("spacing"), "spacing");
    if (node.contains("orientation"))
    {
        result.orientation = static_cast<LinearOrientation>(
            ParseEnumIndex(node.at("orientation"), {"horizontal", "vertical"}, "orientation"));
    }
    if (node.contains("alignment"))
    {
        result.alignment = static_cast<ContainerAlignment>(
            ParseEnumIndex(node.at("alignment"), {"start", "center", "end", "stretch"}, "alignment"));
    }
    if (node.contains("grid"))
    {
        const json& grid = node.at("grid");
        if (!grid.is_object()) throw std::runtime_error("Container grid must be an object.");
        result.rows = ParsePositiveIndex(grid, "rows", 1U, false);
        result.columns = ParsePositiveIndex(grid, "columns", 1U, false);
    }
    ParseAppearance(node, result);
    ParseScrolling(node, result);
    if (node.contains("children"))
    {
        const json& children = node.at("children");
        if (!children.is_array()) throw std::runtime_error("Container children must be an array.");
        result.children.reserve(children.size());
        for (const json& child : children) result.children.push_back(ParseChild(child));
    }
    return result;
}

json ToVec2(const Vec2 value)
{
    return json::array({value.x, value.y});
}

json ToInsets(const ContainerInsets& value)
{
    return json::array({value.left, value.top, value.right, value.bottom});
}

json ToColor(const ColorRgba value)
{
    return json::array({value.r, value.g, value.b, value.a});
}

const char* ContainerTypeName(const ContainerType value) noexcept
{
    switch (value)
    {
    case ContainerType::Canvas: return "canvas";
    case ContainerType::Linear: return "linear";
    case ContainerType::Grid: return "grid";
    case ContainerType::Overlay: return "overlay";
    }
    return "canvas";
}

const char* ContainerSizeModeName(const ContainerSizeMode value) noexcept
{
    switch (value)
    {
    case ContainerSizeMode::Fixed: return "fixed";
    case ContainerSizeMode::FitContent: return "fitContent";
    case ContainerSizeMode::FitHorizontal: return "fitHorizontal";
    case ContainerSizeMode::FitVertical: return "fitVertical";
    }
    return "fixed";
}

const char* AlignmentName(const ContainerAlignment value) noexcept
{
    switch (value)
    {
    case ContainerAlignment::Start: return "start";
    case ContainerAlignment::Center: return "center";
    case ContainerAlignment::End: return "end";
    case ContainerAlignment::Stretch: return "stretch";
    }
    return "start";
}

const char* ResizeModeName(const ChildResizeMode value) noexcept
{
    switch (value)
    {
    case ChildResizeMode::Fixed: return "fixed";
    case ChildResizeMode::Proportional: return "proportional";
    case ChildResizeMode::Stretch: return "stretch";
    }
    return "fixed";
}

const char* ScrollModeName(const ScrollMode value) noexcept
{
    switch (value)
    {
    case ScrollMode::Disabled: return "disabled";
    case ScrollMode::Automatic: return "automatic";
    case ScrollMode::AlwaysVisible: return "always";
    }
    return "disabled";
}
} // namespace

std::vector<ContainerDefinition> ParsePageContainers(const nlohmann::json& pageNode)
{
    if (!pageNode.contains("containers"))
    {
        return {};
    }
    const json& containersNode = pageNode.at("containers");
    if (!containersNode.is_array())
    {
        throw std::runtime_error("Page containers must be a JSON array.");
    }
    if (containersNode.size() > kMaximumContainerCount)
    {
        throw std::runtime_error("Page container count exceeds the runtime safety limit.");
    }
    std::vector<ContainerDefinition> result;
    result.reserve(containersNode.size());
    for (const json& node : containersNode)
    {
        result.push_back(ParseContainer(node));
    }
    std::string error;
    if (!ContainerLayoutEngine {}.Validate(result, error))
    {
        throw std::runtime_error(error);
    }
    return result;
}

nlohmann::json SerializePageContainers(const std::vector<ContainerDefinition>& containers)
{
    std::string error;
    if (!ContainerLayoutEngine {}.Validate(containers, error))
    {
        throw std::runtime_error(error);
    }
    json output = json::array();
    for (const ContainerDefinition& container : containers)
    {
        json node {
            {"id", container.id},
            {"name", container.name.empty() ? container.id : container.name},
            {"type", ContainerTypeName(container.type)},
            {"sizeMode", ContainerSizeModeName(container.sizeMode)},
            {"position", ToVec2(container.geometry.center)},
            {"size", ToVec2(container.geometry.size)},
            {"minimumSize", ToVec2(container.minimumSize)},
            {"layoutReferenceSize", ToVec2(container.layoutReferenceSize)},
            {"visible", container.visible},
            {"enabled", container.enabled},
            {"exposed", container.exposed},
            {"padding", ToInsets(container.padding)},
            {"spacing", container.spacing},
            {"orientation", container.orientation == LinearOrientation::Horizontal ? "horizontal" : "vertical"},
            {"alignment", AlignmentName(container.alignment)},
            {"grid", {{"rows", container.rows}, {"columns", container.columns}}}};
        node["appearance"] = {
            {"backgroundVisible", container.appearance.backgroundVisible},
            {"backgroundColor", ToColor(container.appearance.backgroundColor)},
            {"borderVisible", container.appearance.borderVisible},
            {"borderColor", ToColor(container.appearance.borderColor)},
            {"borderThickness", container.appearance.borderThickness},
            {"cornerRadius", container.appearance.cornerRadius}};
        node["scrolling"] = {
            {"horizontal", ScrollModeName(container.scrolling.horizontal)},
            {"vertical", ScrollModeName(container.scrolling.vertical)},
            {"offset", ToVec2(container.scrolling.offset)},
            {"clipContent", container.scrolling.clipContent},
            {"appearance",
             {{"trackColor", ToColor(container.scrolling.appearance.trackColor)},
              {"thumbColor", ToColor(container.scrolling.appearance.thumbColor)},
              {"hoverColor", ToColor(container.scrolling.appearance.thumbHoverColor)},
              {"activeColor", ToColor(container.scrolling.appearance.thumbActiveColor)},
              {"thickness", container.scrolling.appearance.thickness},
              {"cornerRadius", container.scrolling.appearance.cornerRadius},
              {"minimumThumbSize", container.scrolling.appearance.minimumThumbSize},
              {"margins", ToInsets(container.scrolling.appearance.margins)}}}};
        node["children"] = json::array();
        for (const ContainerChildPlacement& child : container.children)
        {
            node["children"].push_back({
                {"id", child.childId},
                {"resizeMode", ResizeModeName(child.resizeMode)},
                {"horizontalAlignment", AlignmentName(child.horizontalAlignment)},
                {"verticalAlignment", AlignmentName(child.verticalAlignment)},
                {"row", child.row},
                {"column", child.column},
                {"rowSpan", child.rowSpan},
                {"columnSpan", child.columnSpan},
                {"renderOrder", child.renderOrder}});
        }
        output.push_back(std::move(node));
    }
    return output;
}
} // namespace mfd::api1
