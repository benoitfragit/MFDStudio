/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Implementation for OpenGlCompat.
 */

#include <cstdint>

namespace mfd::detail
{
enum class GlStencilCompare
{
    Always,
    Equal,
    NotEqual
};

enum class GlStencilOperation
{
    Keep,
    Replace
};

bool OpenGlStencilApiAvailable() noexcept;
void OpenGlSetStencilEnabled(bool enabled) noexcept;
void OpenGlSetStencilMask(std::uint32_t mask) noexcept;
void OpenGlClearStencilValue(int value) noexcept;
void OpenGlClearStencilBuffer() noexcept;
void OpenGlSetColorWriteMask(bool red, bool green, bool blue, bool alpha) noexcept;
void OpenGlSetStencilFunction(GlStencilCompare compare, int reference, std::uint32_t mask) noexcept;
void OpenGlSetStencilOperation(GlStencilOperation stencilFail,
                               GlStencilOperation depthFail,
                               GlStencilOperation depthPass) noexcept;
bool OpenGlReplaceDepthRenderbufferWithDepthStencil(unsigned int framebufferId,
                                                    unsigned int previousDepthId,
                                                    int width,
                                                    int height,
                                                    unsigned int* replacementRenderbufferId) noexcept;
} // namespace mfd::detail
