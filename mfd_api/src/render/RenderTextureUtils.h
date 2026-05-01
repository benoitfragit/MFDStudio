/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Helpers for off-screen render textures used by the runtime and tools.
 */

#include <raylib.h>

#include "mfd/MfdExport.h"

namespace mfd
{
/**
 * @brief Creates a render texture with a combined depth/stencil attachment.
 *
 * @details The returned render texture keeps the same RGBA color attachment as
 * raylib's default `LoadRenderTexture()`, but upgrades the depth attachment to
 * a depth/stencil renderbuffer so stencil clipping can be used safely.
 *
 * @param width Render target width in pixels.
 * @param height Render target height in pixels.
 * @param stencilAvailable Optional output set to `true` only when the depth
 * attachment was successfully upgraded to depth/stencil.
 * @return A render texture compatible with `BeginTextureMode()` and
 * `UnloadRenderTexture()`.
 */
MFD_API RenderTexture2D LoadRenderTextureWithStencil(int width, int height, bool* stencilAvailable = nullptr);
} // namespace mfd
