/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private numeric JSON parsing helpers shared by JsonLoader implementation files.
 */

#include <cstddef>
#include <cstdint>

#include <nlohmann/json.hpp>

namespace mfd::json_loader_detail
{
int ParsePixelNumber(const nlohmann::json& value, const char* fieldName);
void ParseWindowSize(const nlohmann::json& value, int& width, int& height);
void ParseWindowPosition(const nlohmann::json& value, int& x, int& y);
std::uint16_t ParsePortNumber(const nlohmann::json& value, const char* fieldName);
std::size_t ParsePositivePacketSize(const nlohmann::json& value, const char* fieldName);
std::uint32_t ParseDurationMilliseconds(const nlohmann::json& value, const char* fieldName);
} // namespace mfd::json_loader_detail
