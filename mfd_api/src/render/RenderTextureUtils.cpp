/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/render/RenderTextureUtils.h"

#include <raylib.h>

#include "OpenGlCompat.h"

namespace mfd
{
RenderTexture2D LoadRenderTextureWithStencil(const int width, const int height, bool* stencilAvailable)
{
    if (stencilAvailable != nullptr)
    {
        *stencilAvailable = false;
    }

    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id == 0 || target.depth.id == 0)
    {
        return target;
    }

    unsigned int depthStencilId = 0;
    if (!detail::OpenGlReplaceDepthRenderbufferWithDepthStencil(
            target.id,
            target.depth.id,
            width,
            height,
            &depthStencilId))
    {
        return target;
    }

    target.depth.id = depthStencilId;
    target.depth.width = width;
    target.depth.height = height;
    target.depth.mipmaps = 1;

    if (stencilAvailable != nullptr)
    {
        *stencilAvailable = true;
    }

    return target;
}
} // namespace mfd
