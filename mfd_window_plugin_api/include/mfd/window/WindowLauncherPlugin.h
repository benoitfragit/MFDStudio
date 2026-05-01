/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Public ABI helpers for framebuffer-processing plugins loaded by `mfd_window`.
 */

#include <cstddef>

#include "mfd/core/ArrayView.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(MFD_WINDOW_PLUGIN_API_EXPORTS)
#        define MFD_WINDOW_PLUGIN_API __declspec(dllexport)
#    elif defined(MFD_WINDOW_PLUGIN_API_DLL)
#        define MFD_WINDOW_PLUGIN_API __declspec(dllimport)
#    else
#        define MFD_WINDOW_PLUGIN_API
#    endif
#else
#    define MFD_WINDOW_PLUGIN_API
#endif

namespace mfd::window
{
/**
 * @brief Exported callback signature accepted from one framebuffer plugin DLL.
 *
 * The host passes the final frame as a transient `RGBA32` byte span. Plugin
 * implementations must copy the bytes if they need to retain them after the
 * callback returns.
 */
using LauncherFramebufferPluginEntryPoint = void(*)(int width, int height, mfd::ByteView rgba32Bytes);

/** @brief Exported symbol name searched inside a framebuffer plugin DLL. */
constexpr const char* kLauncherFramebufferPluginEntryPointName = "MfdWindowFramebufferCallback";

/**
 * @brief Computes the expected `RGBA32` byte count for one framebuffer.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @return Expected byte count, or `0` when the dimensions are invalid or overflow.
 */
MFD_WINDOW_PLUGIN_API std::size_t ComputeFramebufferRgba32ByteCount(int width, int height) noexcept;

/**
 * @brief Validates that one byte span matches the expected `RGBA32` frame layout.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param rgba32Bytes Temporary byte span received from the host.
 * @return `true` when the span matches one complete `RGBA32` frame.
 */
MFD_WINDOW_PLUGIN_API bool ValidateFramebufferRgba32Layout(int width, int height, mfd::ByteView rgba32Bytes) noexcept;
} // namespace mfd::window
