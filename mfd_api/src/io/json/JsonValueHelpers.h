/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private JSON utility helpers shared by JsonLoader implementation files.
 */

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace mfd::json_loader_detail
{
std::string_view TrimAsciiWhitespace(std::string_view value) noexcept;
std::string Lowercase(std::string_view value);
std::string CanonicalToken(std::string_view value);
std::string JsonScalarToString(const nlohmann::json& value);
nlohmann::json LoadJsonFile(const std::filesystem::path& path);
std::filesystem::path NormalizePath(const std::filesystem::path& path);
std::filesystem::path ResolvePath(const std::filesystem::path& baseFolder, const std::filesystem::path& path);
const nlohmann::json* FindField(const nlohmann::json& node, std::initializer_list<const char*> fieldNames);
const nlohmann::json* FindEditorField(const nlohmann::json& node,
                                      std::initializer_list<const char*> fieldNames);
bool IsPageFileList(const nlohmann::json& value);
} // namespace mfd::json_loader_detail
