#include "mfd/apilevel1/widgets/WidgetJson.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace mfd::api1
{
namespace
{
using json = nlohmann::json;
constexpr std::array<std::string_view, 11> kTypes {"button", "toggle", "checkbox", "radio", "slider", "knob", "gauge", "progressBar", "indicator", "numericDisplay", "label"};
constexpr std::array<std::string_view, 7> kStates {"normal", "hover", "pressed", "checked", "disabled", "warning", "pending"};
constexpr std::array<std::string_view, 7> kTargets {"text", "rotation", "scaleX", "scaleY", "positionX", "positionY", "visible"};
void Fields(const json& node, const std::initializer_list<std::string_view> allowed)
{
    if (!node.is_object()) throw std::runtime_error("Widget field must be an object.");
    for (auto it = node.begin(); it != node.end(); ++it)
    {
        if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end())
            throw std::runtime_error("Unsupported widget field: " + it.key());
    }
}
std::uint32_t Enum(const json& node, const std::string_view* names, const std::size_t count)
{
    if (!node.is_string()) throw std::runtime_error("Widget enum must be a string.");
    for (std::size_t i = 0; i < count; ++i)
        if (node.get_ref<const std::string&>() == names[i]) return static_cast<std::uint32_t>(i);
    throw std::runtime_error("Unknown widget enum: " + node.dump());
}
double Number(const json& node)
{
    if (!node.is_number()) throw std::runtime_error("Widget numeric value required.");
    const double value = node.get<double>();
    if (!std::isfinite(value)) throw std::runtime_error("Widget numeric value must be finite.");
    return value;
}
Vec2 Vector(const json& node)
{
    if (!node.is_array() || node.size() != 2U) throw std::runtime_error("Widget vector needs two coordinates.");
    const Vec2 value {static_cast<float>(Number(node[0])), static_cast<float>(Number(node[1]))};
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) throw std::runtime_error("Widget coordinate overflow.");
    return value;
}
WidgetOverrides Overrides(const json& node)
{
    Fields(node, {"checked", "warning", "value", "text"});
    WidgetOverrides value;
    if (node.contains("checked")) value.checked = node.at("checked").get<bool>();
    if (node.contains("warning")) value.warning = node.at("warning").get<bool>();
    if (node.contains("value")) value.value = Number(node.at("value"));
    if (node.contains("text")) value.text = node.at("text").get<std::string>();
    return value;
}
json ToVector(const Vec2 value) { return json::array({value.x, value.y}); }
void ValidateResources(const ReticleGroup& visual, const std::filesystem::path& assetRoot)
{
    for (const Primitive& primitive : visual.primitives)
    {
        if (const auto* image = std::get_if<ImageGeometry>(&primitive.geometry))
        {
            const auto relative = image->file.lexically_relative(assetRoot);
            const auto resolved = ResolveWidgetAsset(assetRoot, assetRoot, relative.generic_string());
            if (!std::filesystem::is_regular_file(resolved)) throw std::runtime_error("Missing widget image: " + resolved.string());
        }
    }
}
} // namespace

std::filesystem::path ResolveWidgetAsset(const std::filesystem::path& baseFolder,
    const std::filesystem::path& assetRoot, const std::string& reference)
{
    const std::filesystem::path path(reference);
    if (reference.empty() || reference.size() > 4096U || reference.find('\0') != std::string::npos ||
        path.is_absolute() || path.has_root_name()) throw std::runtime_error("Widget references must be relative asset paths.");
    const auto root = std::filesystem::weakly_canonical(assetRoot);
    const auto resolved = std::filesystem::weakly_canonical(baseFolder / path);
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || *relative.begin() == "..") throw std::runtime_error("Widget dependency escapes asset root: " + reference);
    return resolved;
}
std::shared_ptr<const WidgetDefinition> LoadWidgetDefinition(const std::filesystem::path& file,
    const std::filesystem::path& assetRoot, const WidgetVisualParser& parseVisual)
{
    if (file.extension() != ".json" || !std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > 1048576U)
        throw std::runtime_error("Missing, oversized or invalid widget JSON asset: " + file.string());
    std::ifstream input(file);
    const json node = json::parse(input);
    Fields(node, {"schemaVersion", "id", "type", "preferredSize", "hitRegion", "range", "defaults", "visuals"});
    auto definition = std::make_shared<WidgetDefinition>();
    definition->sourceFile = file;
    if (!node.contains("schemaVersion") || !node.at("schemaVersion").is_number_integer() || node.at("schemaVersion") != 1)
        throw std::runtime_error("Unsupported widget schemaVersion in " + file.string());
    definition->id = node.at("id").get<std::string>();
    definition->type = static_cast<WidgetType>(Enum(node.at("type"), kTypes.data(), kTypes.size()));
    if (node.contains("preferredSize")) definition->preferredSize = Vector(node.at("preferredSize"));
    definition->hitRegion.size = definition->preferredSize;
    if (node.contains("hitRegion"))
    {
        const auto& hit = node.at("hitRegion");
        Fields(hit, {"position", "size"});
        if (hit.contains("position")) definition->hitRegion.center = Vector(hit.at("position"));
        if (hit.contains("size")) definition->hitRegion.size = Vector(hit.at("size"));
    }
    if (node.contains("range"))
    {
        const auto& range = node.at("range");
        Fields(range, {"min", "max", "step", "decimals"});
        if (range.contains("min")) definition->range.minimum = Number(range.at("min"));
        if (range.contains("max")) definition->range.maximum = Number(range.at("max"));
        if (range.contains("step")) definition->range.step = Number(range.at("step"));
        if (range.contains("decimals"))
        {
            if (!range.at("decimals").is_number_integer() || range.at("decimals") < 0 || range.at("decimals") > 9)
                throw std::runtime_error("Widget decimals must be an integer in [0,9].");
            definition->range.decimals = range.at("decimals").get<std::uint32_t>();
        }
    }
    definition->defaults.value = definition->range.minimum;
    if (node.contains("defaults"))
    {
        WidgetInstance initial;
        initial.definition = definition;
        initial.overrides = Overrides(node.at("defaults"));
        definition->defaults = InitialWidgetState(initial).confirmed;
    }
    const auto& visuals = node.at("visuals");
    if (!visuals.is_array() || visuals.size() > 7U) throw std::runtime_error("Widget visuals must be an array of at most seven states.");
    for (const auto& item : visuals)
    {
        Fields(item, {"state", "reticle", "bindings"});
        WidgetVisual visual;
        visual.state = static_cast<WidgetVisualState>(Enum(item.at("state"), kStates.data(), kStates.size()));
        visual.composition = parseVisual(item.at("reticle"), file.parent_path());
        ValidateResources(visual.composition, assetRoot);
        if (item.contains("bindings"))
        {
            const auto& bindings = item.at("bindings");
            if (!bindings.is_array() || bindings.size() > 64U) throw std::runtime_error("Widget bindings exceed the limit.");
            for (const auto& value : bindings)
            {
                Fields(value, {"primitive", "target", "from", "to"});
                WidgetBinding binding;
                binding.primitiveId = value.at("primitive").get<std::string>();
                binding.target = static_cast<WidgetBindingTarget>(Enum(value.at("target"), kTargets.data(), kTargets.size()));
                if (value.contains("from")) binding.from = Number(value.at("from"));
                if (value.contains("to")) binding.to = Number(value.at("to"));
                visual.bindings.push_back(std::move(binding));
            }
        }
        definition->visuals.push_back(std::move(visual));
    }
    std::string error;
    if (!ValidateWidgetDefinition(*definition, error)) throw std::runtime_error(file.string() + ": " + error);
    return definition;
}
std::vector<WidgetInstance> ParsePageWidgets(const json& page, const std::filesystem::path& baseFolder,
    const std::filesystem::path& assetRoot, const WidgetVisualParser& parseVisual)
{
    if (!page.contains("widgets")) return {};
    const auto& nodes = page.at("widgets");
    if (!nodes.is_array() || nodes.size() > kMaximumWidgetCount) throw std::runtime_error("Page widgets must be a bounded array.");
    std::unordered_map<std::string, std::shared_ptr<const WidgetDefinition>> definitions;
    std::vector<WidgetInstance> widgets;
    widgets.reserve(nodes.size());
    for (const auto& node : nodes)
    {
        Fields(node, {"id", "definition", "position", "size", "visible", "enabled", "exposed", "overrides"});
        WidgetInstance widget;
        widget.id = node.at("id").get<std::string>();
        widget.definitionFile = node.at("definition").get<std::string>();
        const auto file = ResolveWidgetAsset(baseFolder, assetRoot, widget.definitionFile);
        auto it = definitions.find(file.generic_string());
        if (it == definitions.end()) it = definitions.emplace(file.generic_string(), LoadWidgetDefinition(file, assetRoot, parseVisual)).first;
        widget.definition = it->second;
        if (node.contains("position")) widget.position = Vector(node.at("position"));
        if (node.contains("size")) widget.size = Vector(node.at("size"));
        widget.visible = node.value("visible", true);
        widget.enabled = node.value("enabled", true);
        widget.exposed = node.value("exposed", false);
        if (node.contains("overrides")) widget.overrides = Overrides(node.at("overrides"));
        widgets.push_back(std::move(widget));
    }
    return widgets;
}
json SerializePageWidgets(const std::vector<WidgetInstance>& widgets)
{
    json output = json::array();
    for (const WidgetInstance& widget : widgets)
    {
        json node {{"id", widget.id}, {"definition", widget.definitionFile}, {"position", ToVector(widget.position)},
                   {"visible", widget.visible}, {"enabled", widget.enabled}, {"exposed", widget.exposed}};
        if (widget.size) node["size"] = ToVector(*widget.size);
        json overrides = json::object();
        if (widget.overrides.checked) overrides["checked"] = *widget.overrides.checked;
        if (widget.overrides.warning) overrides["warning"] = *widget.overrides.warning;
        if (widget.overrides.value) overrides["value"] = *widget.overrides.value;
        if (widget.overrides.text) overrides["text"] = *widget.overrides.text;
        if (!overrides.empty()) node["overrides"] = std::move(overrides);
        output.push_back(std::move(node));
    }
    return output;
}
json SerializeWidgetDefinition(const WidgetDefinition& definition, const WidgetVisualSerializer& serializeVisual)
{
    std::string error;
    if (!ValidateWidgetDefinition(definition, error)) throw std::runtime_error(error);
    json node {{"schemaVersion", 1}, {"id", definition.id}, {"type", kTypes[static_cast<std::size_t>(definition.type)]},
               {"preferredSize", ToVector(definition.preferredSize)},
               {"hitRegion", {{"position", ToVector(definition.hitRegion.center)}, {"size", ToVector(definition.hitRegion.size)}}},
               {"range", {{"min", definition.range.minimum}, {"max", definition.range.maximum}, {"step", definition.range.step}, {"decimals", definition.range.decimals}}},
               {"defaults", {{"checked", definition.defaults.checked}, {"warning", definition.defaults.warning}, {"value", definition.defaults.value}, {"text", definition.defaults.text}}}};
    node["visuals"] = json::array();
    for (const WidgetVisual& visual : definition.visuals)
    {
        json item {{"state", kStates[static_cast<std::size_t>(visual.state)]}, {"reticle", serializeVisual(visual.composition, definition.sourceFile.parent_path())}};
        item["bindings"] = json::array();
        for (const WidgetBinding& binding : visual.bindings)
            item["bindings"].push_back({{"primitive", binding.primitiveId}, {"target", kTargets[static_cast<std::size_t>(binding.target)]}, {"from", binding.from}, {"to", binding.to}});
        node["visuals"].push_back(std::move(item));
    }
    return node;
}
} // namespace mfd::api1
