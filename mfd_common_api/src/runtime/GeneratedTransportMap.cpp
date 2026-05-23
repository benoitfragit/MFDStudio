/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for GeneratedTransportMap.
 */

#include "mfd/runtime/GeneratedTransportMap.h"

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

namespace mfd
{
namespace
{
using json = nlohmann::json;

std::string JoinPathContext(const std::string_view prefix, const std::string_view suffix)
{
    if (prefix.empty())
    {
        return std::string(suffix);
    }

    if (suffix.empty())
    {
        return std::string(prefix);
    }

    return std::string(prefix) + "." + std::string(suffix);
}

json LoadJsonFile(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open generated transport map: " + path.string());
    }

    try
    {
        json document;
        stream >> document;
        return document;
    }
    catch (const nlohmann::json::parse_error& exception)
    {
        throw std::runtime_error("Unable to parse generated transport map '" + path.string() +
                                 "' at byte " + std::to_string(exception.byte) +
                                 ": " + exception.what());
    }
    catch (const nlohmann::json::exception& exception)
    {
        throw std::runtime_error("Unable to read generated transport map '" + path.string() +
                                 "': " + exception.what());
    }
}

const json& RequireObjectField(const json& node,
                               const std::string_view context,
                               const std::string_view fieldName)
{
    if (!node.is_object())
    {
        throw std::runtime_error(std::string(context) + " must be a JSON object");
    }

    const auto iterator = node.find(std::string(fieldName));
    if (iterator == node.end())
    {
        throw std::runtime_error(std::string(context) + " must define '" + std::string(fieldName) + "'");
    }

    return *iterator;
}

const json& RequireArrayField(const json& node,
                              const std::string_view context,
                              const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_array())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be a JSON array");
    }

    return value;
}

std::string RequireStringField(const json& node,
                               const std::string_view context,
                               const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_string())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be a string");
    }

    return value.get<std::string>();
}

bool RequireBoolField(const json& node,
                      const std::string_view context,
                      const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_boolean())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be a boolean");
    }

    return value.get<bool>();
}

int RequireIntField(const json& node,
                    const std::string_view context,
                    const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_number_integer())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be an integer");
    }

    return value.get<int>();
}

std::uint32_t RequireUint32Field(const json& node,
                                 const std::string_view context,
                                 const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_number_unsigned() && !value.is_number_integer())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be an unsigned integer");
    }

    const auto rawValue = value.get<std::uint64_t>();
    if (rawValue > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " exceeds the supported uint32 range");
    }

    return static_cast<std::uint32_t>(rawValue);
}

TransportId RequireTransportIdField(const json& node,
                                    const std::string_view context,
                                    const std::string_view fieldName)
{
    const json& value = RequireObjectField(node, context, fieldName);
    if (!value.is_number_unsigned() && !value.is_number_integer())
    {
        throw std::runtime_error(JoinPathContext(context, fieldName) + " must be an unsigned integer");
    }

    return value.get<TransportId>();
}

TransportPrimitiveOwnerKind ParseOwnerKind(const std::string_view value)
{
    if (value == "reticle")
    {
        return TransportPrimitiveOwnerKind::Reticle;
    }

    if (value == "template")
    {
        return TransportPrimitiveOwnerKind::Template;
    }

    throw std::runtime_error("Unsupported primitive owner kind: " + std::string(value));
}

TransportMapWindowInfo ParseWindowInfo(const json& node)
{
    TransportMapWindowInfo window;
    window.name = RequireStringField(node, "window", "name");
    window.title = RequireStringField(node, "window", "title");
    window.source = RequireStringField(node, "window", "source");
    return window;
}

std::vector<TransportMapPageEntry> ParsePages(const json& node)
{
    std::vector<TransportMapPageEntry> pages;
    pages.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "pages[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapPageEntry page;
        page.id = RequireTransportIdField(entry, context, "id");
        page.name = RequireStringField(entry, context, "name");
        page.normalizedName = RequireStringField(entry, context, "normalizedName");
        page.hasStrobe = RequireBoolField(entry, context, "hasStrobe");
        page.defaultPage = RequireBoolField(entry, context, "defaultPage");
        pages.push_back(std::move(page));
    }

    return pages;
}

std::vector<TransportMapReticleEntry> ParseReticles(const json& node)
{
    std::vector<TransportMapReticleEntry> reticles;
    reticles.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "reticles[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapReticleEntry reticle;
        reticle.id = RequireTransportIdField(entry, context, "id");
        reticle.pageId = RequireTransportIdField(entry, context, "pageId");
        reticle.reticleId = RequireStringField(entry, context, "reticleId");
        reticle.normalizedReticleId = RequireStringField(entry, context, "normalizedReticleId");
        reticle.source = RequireStringField(entry, context, "source");
        reticles.push_back(std::move(reticle));
    }

    return reticles;
}

std::vector<TransportMapPrimitiveEntry> ParsePrimitives(const json& node)
{
    std::vector<TransportMapPrimitiveEntry> primitives;
    primitives.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "primitives[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapPrimitiveEntry primitive;
        primitive.id = RequireTransportIdField(entry, context, "id");
        primitive.ownerKind = ParseOwnerKind(RequireStringField(entry, context, "ownerKind"));
        primitive.ownerId = RequireTransportIdField(entry, context, "ownerId");
        primitive.primitiveId = RequireStringField(entry, context, "primitiveId");
        primitive.normalizedPrimitiveId = RequireStringField(entry, context, "normalizedPrimitiveId");
        primitive.primitiveType = RequireStringField(entry, context, "primitiveType");
        primitive.exposed = RequireBoolField(entry, context, "exposed");
        primitives.push_back(std::move(primitive));
    }

    return primitives;
}

std::vector<TransportMapTemplateEntry> ParseTemplates(const json& node)
{
    std::vector<TransportMapTemplateEntry> templates;
    templates.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "templates[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapTemplateEntry templ;
        templ.id = RequireTransportIdField(entry, context, "id");
        templ.templateId = RequireStringField(entry, context, "templateId");
        templ.normalizedTemplateId = RequireStringField(entry, context, "normalizedTemplateId");
        templates.push_back(std::move(templ));
    }

    return templates;
}

std::vector<TransportMapBlinkTypeEntry> ParseBlinkTypes(const json& node)
{
    std::vector<TransportMapBlinkTypeEntry> blinkTypes;
    blinkTypes.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "blinkTypes[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapBlinkTypeEntry blinkType;
        blinkType.id = RequireTransportIdField(entry, context, "id");
        blinkType.pageId = RequireTransportIdField(entry, context, "pageId");
        blinkType.blinkType = RequireStringField(entry, context, "blinkType");
        blinkType.normalizedBlinkType = RequireStringField(entry, context, "normalizedBlinkType");
        blinkType.durationMs = RequireUint32Field(entry, context, "durationMs");
        blinkTypes.push_back(std::move(blinkType));
    }

    return blinkTypes;
}

std::vector<TransportMapStrobeEntry> ParseStrobes(const json& node)
{
    std::vector<TransportMapStrobeEntry> strobes;
    strobes.reserve(node.size());

    for (std::size_t index = 0; index < node.size(); ++index)
    {
        const json& entry = node.at(index);
        const std::string context = "strobes[" + std::to_string(index) + "]";
        if (!entry.is_object())
        {
            throw std::runtime_error(context + " must be a JSON object");
        }

        TransportMapStrobeEntry strobe;
        strobe.id = RequireTransportIdField(entry, context, "id");
        strobe.pageId = RequireTransportIdField(entry, context, "pageId");
        strobe.strobeName = RequireStringField(entry, context, "strobeName");
        strobe.normalizedStrobeName = RequireStringField(entry, context, "normalizedStrobeName");
        strobe.reticleId = RequireStringField(entry, context, "reticleId");
        strobe.defaultActive = RequireBoolField(entry, context, "defaultActive");
        strobes.push_back(std::move(strobe));
    }

    return strobes;
}

template <typename Entry>
void EnsureUniqueIds(const std::vector<Entry>& entries,
                     const std::string_view tableName,
                     std::unordered_map<TransportId, std::string>& idOwners)
{
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const auto [iterator, inserted] =
            idOwners.emplace(entries[index].id, std::string(tableName) + "[" + std::to_string(index) + "]");
        if (!inserted)
        {
            throw std::runtime_error(
                "Duplicate generated transport id " + std::to_string(entries[index].id) +
                " shared by " + iterator->second + " and " +
                std::string(tableName) + "[" + std::to_string(index) + "]");
        }
    }
}

void ValidateReferences(const GeneratedTransportMap& map)
{
    std::unordered_map<TransportId, const TransportMapPageEntry*> pagesById;
    for (const auto& page : map.pages)
    {
        pagesById.emplace(page.id, &page);
    }

    std::unordered_map<TransportId, const TransportMapReticleEntry*> reticlesById;
    for (const auto& reticle : map.reticles)
    {
        if (pagesById.find(reticle.pageId) == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map reticle '" + reticle.reticleId +
                "' references unknown page id " + std::to_string(reticle.pageId));
        }

        reticlesById.emplace(reticle.id, &reticle);
    }

    std::unordered_map<TransportId, const TransportMapTemplateEntry*> templatesById;
    for (const auto& templ : map.templates)
    {
        templatesById.emplace(templ.id, &templ);
    }

    for (const auto& primitive : map.primitives)
    {
        switch (primitive.ownerKind)
        {
        case TransportPrimitiveOwnerKind::Reticle:
            if (reticlesById.find(primitive.ownerId) == reticlesById.end())
            {
                throw std::runtime_error(
                    "Generated transport map primitive '" + primitive.primitiveId +
                    "' references unknown reticle owner id " + std::to_string(primitive.ownerId));
            }
            break;

        case TransportPrimitiveOwnerKind::Template:
            if (templatesById.find(primitive.ownerId) == templatesById.end())
            {
                throw std::runtime_error(
                    "Generated transport map primitive '" + primitive.primitiveId +
                    "' references unknown template owner id " + std::to_string(primitive.ownerId));
            }
            break;
        }
    }

    for (const auto& blinkType : map.blinkTypes)
    {
        if (pagesById.find(blinkType.pageId) == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map blink type '" + blinkType.blinkType +
                "' references unknown page id " + std::to_string(blinkType.pageId));
        }
    }

    for (const auto& strobe : map.strobes)
    {
        if (pagesById.find(strobe.pageId) == pagesById.end())
        {
            throw std::runtime_error(
                "Generated transport map strobe '" + strobe.strobeName +
                "' references unknown page id " + std::to_string(strobe.pageId));
        }
    }
}

GeneratedTransportMap ParseGeneratedTransportMap(const std::filesystem::path& sourceFile, const json& root)
{
    if (!root.is_object())
    {
        throw std::runtime_error("Generated transport map root must be a JSON object");
    }

    GeneratedTransportMap map;
    map.sourceFile = sourceFile;
    map.schemaVersion = RequireIntField(root, "generatedTransportMap", "schemaVersion");
    if (map.schemaVersion != 1)
    {
        throw std::runtime_error(
            "Unsupported generated transport map schemaVersion: " + std::to_string(map.schemaVersion));
    }

    map.mappingHash = RequireStringField(root, "generatedTransportMap", "mappingHash");
    if (map.mappingHash.empty())
    {
        throw std::runtime_error("generatedTransportMap.mappingHash cannot be empty");
    }

    map.window = ParseWindowInfo(RequireObjectField(root, "generatedTransportMap", "window"));
    map.pages = ParsePages(RequireArrayField(root, "generatedTransportMap", "pages"));
    map.reticles = ParseReticles(RequireArrayField(root, "generatedTransportMap", "reticles"));
    map.primitives = ParsePrimitives(RequireArrayField(root, "generatedTransportMap", "primitives"));
    map.templates = ParseTemplates(RequireArrayField(root, "generatedTransportMap", "templates"));
    map.blinkTypes = ParseBlinkTypes(RequireArrayField(root, "generatedTransportMap", "blinkTypes"));
    map.strobes = ParseStrobes(RequireArrayField(root, "generatedTransportMap", "strobes"));

    std::unordered_map<TransportId, std::string> idOwners;
    EnsureUniqueIds(map.pages, "pages", idOwners);
    EnsureUniqueIds(map.reticles, "reticles", idOwners);
    EnsureUniqueIds(map.primitives, "primitives", idOwners);
    EnsureUniqueIds(map.templates, "templates", idOwners);
    EnsureUniqueIds(map.blinkTypes, "blinkTypes", idOwners);
    EnsureUniqueIds(map.strobes, "strobes", idOwners);
    ValidateReferences(map);
    return map;
}
} // namespace

std::filesystem::path CompanionGeneratedTransportMapPath(const std::filesystem::path& windowFile)
{
    const std::filesystem::path resolvedWindowFile =
        windowFile.is_absolute() ? windowFile.lexically_normal() : std::filesystem::absolute(windowFile).lexically_normal();
    return resolvedWindowFile.parent_path() /
           std::filesystem::path(resolvedWindowFile.stem().string() + ".generated.map");
}

std::optional<GeneratedTransportMap> TryLoadGeneratedTransportMap(const std::filesystem::path& windowFile,
                                                                  std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }

    try
    {
        const std::filesystem::path mapPath = CompanionGeneratedTransportMapPath(windowFile);
        if (!std::filesystem::exists(mapPath))
        {
            return std::nullopt;
        }

        return ParseGeneratedTransportMap(mapPath, LoadJsonFile(mapPath));
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }

        return std::nullopt;
    }
}
} // namespace mfd
