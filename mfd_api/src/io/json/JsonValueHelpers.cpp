/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Private JSON utility helpers shared by JsonLoader implementation files.
 */

#include "JsonValueHelpers.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mfd::json_loader_detail
{
std::string_view TrimAsciiWhitespace(const std::string_view value) noexcept
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

std::string Lowercase(const std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (const char ch : value)
    {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return result;
}

std::string CanonicalToken(const std::string_view value)
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

std::string JsonScalarToString(const nlohmann::json& value)
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

nlohmann::json LoadJsonFile(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open JSON file: " + path.string());
    }

    try
    {
        nlohmann::json document;
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

const nlohmann::json* FindField(const nlohmann::json& node, const std::initializer_list<const char*> fieldNames)
{
    if (!node.is_object())
    {
        return nullptr;
    }

    const nlohmann::json* matchedField = nullptr;
    const char* matchedName = nullptr;
    for (const char* fieldName : fieldNames)
    {
        const auto iterator = node.find(fieldName);
        if (iterator != node.end())
        {
            if (matchedField != nullptr)
            {
                throw std::runtime_error(std::string("Conflicting JSON aliases: '") +
                                         matchedName +
                                         "' and '" +
                                         fieldName +
                                         "' cannot be used together");
            }

            matchedField = &(*iterator);
            matchedName = fieldName;
        }
    }

    return matchedField;
}

const nlohmann::json* FindEditorField(const nlohmann::json& node,
                                      const std::initializer_list<const char*> fieldNames)
{
    const nlohmann::json* editor = FindField(node, {"_editor", "editor"});
    return editor == nullptr ? nullptr : FindField(*editor, fieldNames);
}

bool IsPageFileList(const nlohmann::json& value)
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
} // namespace mfd::json_loader_detail
