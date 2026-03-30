/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/render/OpenGlFramebufferReader.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <raylib.h>
#include <rlgl.h>

namespace mfd
{
namespace
{
static_assert(std::is_trivially_copyable_v<Rgba8Pixel>);
static_assert(sizeof(Rgba8Pixel) == 4);

int ResolveExtent(const int offset, const int requestedExtent, const int fullExtent, const char* axisName)
{
    if (offset < 0 || offset > fullExtent)
    {
        throw std::runtime_error(std::string("Framebuffer capture ") + axisName + " offset is out of range");
    }

    const int resolvedExtent = requestedExtent > 0 ? requestedExtent : fullExtent - offset;
    if (resolvedExtent <= 0 || offset + resolvedExtent > fullExtent)
    {
        throw std::runtime_error(std::string("Framebuffer capture ") + axisName + " extent is invalid");
    }

    return resolvedExtent;
}

void FlipRows(std::vector<Rgba8Pixel>& pixels, const int width, const int height)
{
    const std::size_t stride = static_cast<std::size_t>(width) * sizeof(Rgba8Pixel);
    std::vector<Rgba8Pixel> scratchRow(static_cast<std::size_t>(width));

    for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
    {
        Rgba8Pixel* topRow = pixels.data() + static_cast<std::size_t>(top) * static_cast<std::size_t>(width);
        Rgba8Pixel* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * static_cast<std::size_t>(width);

        std::memcpy(scratchRow.data(), topRow, stride);
        std::memcpy(topRow, bottomRow, stride);
        std::memcpy(bottomRow, scratchRow.data(), stride);
    }
}
} // namespace

bool Rgba32Framebuffer::Empty() const noexcept
{
    return pixels.empty();
}

std::size_t Rgba32Framebuffer::PixelCount() const noexcept
{
    return pixels.size();
}

std::size_t Rgba32Framebuffer::ByteSize() const noexcept
{
    return pixels.size() * sizeof(Rgba8Pixel);
}

std::span<const Rgba8Pixel> Rgba32Framebuffer::Pixels() const noexcept
{
    return pixels;
}

std::span<Rgba8Pixel> Rgba32Framebuffer::Pixels() noexcept
{
    return pixels;
}

std::span<const std::byte> Rgba32Framebuffer::Bytes() const noexcept
{
    return {reinterpret_cast<const std::byte*>(pixels.data()), ByteSize()};
}

std::span<std::byte> Rgba32Framebuffer::Bytes() noexcept
{
    return {reinterpret_cast<std::byte*>(pixels.data()), ByteSize()};
}

Rgba32Framebuffer OpenGlFramebufferReader::ReadRgba32(const FramebufferCaptureRequest& request)
{
    const int framebufferWidth = GetRenderWidth();
    const int framebufferHeight = GetRenderHeight();

    if (framebufferWidth <= 0 || framebufferHeight <= 0)
    {
        throw std::runtime_error("No active OpenGL framebuffer is available");
    }

    const int captureWidth = ResolveExtent(request.x, request.width, framebufferWidth, "x");
    const int captureHeight = ResolveExtent(request.y, request.height, framebufferHeight, "y");

    int glReadY = request.y;
    if (request.origin == FramebufferOrigin::TopLeft)
    {
        glReadY = framebufferHeight - (request.y + captureHeight);
    }

    if (glReadY < 0 || glReadY + captureHeight > framebufferHeight)
    {
        throw std::runtime_error("Framebuffer capture y extent is invalid after origin conversion");
    }

    rlDrawRenderBatchActive();

    unsigned char* fullFramebufferPixels = rlReadScreenPixels(framebufferWidth, framebufferHeight);
    if (fullFramebufferPixels == nullptr)
    {
        throw std::runtime_error("Unable to read the current OpenGL framebuffer");
    }

    Rgba32Framebuffer framebuffer;
    framebuffer.width = captureWidth;
    framebuffer.height = captureHeight;
    framebuffer.pixels.resize(static_cast<std::size_t>(captureWidth) *
                              static_cast<std::size_t>(captureHeight));

    const std::size_t fullStride = static_cast<std::size_t>(framebufferWidth) * sizeof(Rgba8Pixel);
    const std::size_t captureStride = static_cast<std::size_t>(captureWidth) * sizeof(Rgba8Pixel);
    const int topLeftY = request.origin == FramebufferOrigin::TopLeft
                             ? request.y
                             : framebufferHeight - (request.y + captureHeight);

    for (int row = 0; row < captureHeight; ++row)
    {
        const std::size_t sourceOffset =
            (static_cast<std::size_t>(topLeftY + row) * fullStride) +
            (static_cast<std::size_t>(request.x) * sizeof(Rgba8Pixel));
        const std::size_t destinationOffset = static_cast<std::size_t>(row) * captureStride;

        std::memcpy(reinterpret_cast<std::uint8_t*>(framebuffer.pixels.data()) + destinationOffset,
                    fullFramebufferPixels + sourceOffset,
                    captureStride);
    }

    MemFree(fullFramebufferPixels);

    if (request.flipVertically)
    {
        FlipRows(framebuffer.pixels, framebuffer.width, framebuffer.height);
    }

    return framebuffer;
}
} // namespace mfd
