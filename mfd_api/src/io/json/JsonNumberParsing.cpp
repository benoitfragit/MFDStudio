/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Private numeric JSON parsing helpers shared by JsonLoader implementation files.
 */

#include "JsonNumberParsing.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "mfd/ipc/UdpLimits.h"

namespace mfd::json_loader_detail
{
namespace
{
double ParseFiniteNumber(const nlohmann::json& value, const char* fieldName)
{
    if (!value.is_number())
    {
        throw std::runtime_error(std::string(fieldName) + " must be numeric");
    }

    const double raw = value.get<double>();
    if (!std::isfinite(raw))
    {
        throw std::runtime_error(std::string(fieldName) + " must be finite");
    }

    return raw;
}

int ParseStrictInteger(const nlohmann::json& value, const char* fieldName)
{
    if (!value.is_number_integer())
    {
        throw std::runtime_error(std::string(fieldName) + " must be an integer");
    }

    return ParsePixelNumber(value, fieldName);
}
} // namespace

int ParsePixelNumber(const nlohmann::json& value, const char* fieldName)
{
    const double raw = ParseFiniteNumber(value, fieldName);
    const double rounded = std::round(raw);
    const double minValue = static_cast<double>(std::numeric_limits<int>::min());
    const double maxValue = static_cast<double>(std::numeric_limits<int>::max());

    if (rounded < minValue || rounded > maxValue)
    {
        throw std::runtime_error(std::string(fieldName) + " must fit in a 32-bit signed integer");
    }

    return static_cast<int>(rounded);
}

void ParseWindowSize(const nlohmann::json& value, int& width, int& height)
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

void ParseWindowPosition(const nlohmann::json& value, int& x, int& y)
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
        x = ParsePixelNumber(value.value("x", nlohmann::json(x)), "window x");
        y = ParsePixelNumber(value.value("y", nlohmann::json(y)), "window y");
        return;
    }

    throw std::runtime_error("Unsupported window position format");
}

std::uint16_t ParsePortNumber(const nlohmann::json& value, const char* fieldName)
{
    const int port = ParseStrictInteger(value, fieldName);
    if (port < 0 || port > 65535)
    {
        throw std::runtime_error(std::string(fieldName) + " must be in [0, 65535]");
    }

    return static_cast<std::uint16_t>(port);
}

std::size_t ParsePositivePacketSize(const nlohmann::json& value, const char* fieldName)
{
    const int packetSize = ParseStrictInteger(value, fieldName);
    if (packetSize < static_cast<int>(kUdpMinPayloadBytes) ||
        packetSize > static_cast<int>(kUdpMaxPayloadBytes))
    {
        throw std::runtime_error(std::string(fieldName) + " must be in [" +
                                 std::to_string(kUdpMinPayloadBytes) + ", " +
                                 std::to_string(kUdpMaxPayloadBytes) + "]");
    }

    return static_cast<std::size_t>(packetSize);
}

std::uint32_t ParseDurationMilliseconds(const nlohmann::json& value, const char* fieldName)
{
    const int durationMs = ParseStrictInteger(value, fieldName);
    if (durationMs <= 0)
    {
        throw std::runtime_error(std::string(fieldName) + " must be strictly positive");
    }

    return static_cast<std::uint32_t>(durationMs);
}
} // namespace mfd::json_loader_detail
