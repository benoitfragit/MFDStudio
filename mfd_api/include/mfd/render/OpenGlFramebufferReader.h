/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief OpenGL framebuffer readback helpers returning RGBA8 pixels.
 */

#include <cstddef>
#include <cstdint>
#include "mfd/core/ArrayView.h"
#include <vector>

namespace mfd
{
/**
 * @brief One RGBA8 pixel returned by a framebuffer capture.
 */
struct Rgba8Pixel
{
    /** @brief Red channel. */
    std::uint8_t r = 0;
    /** @brief Green channel. */
    std::uint8_t g = 0;
    /** @brief Blue channel. */
    std::uint8_t b = 0;
    /** @brief Alpha channel. */
    std::uint8_t a = 255;
};

/**
 * @brief Declares the origin convention of the requested framebuffer capture.
 */
enum class FramebufferOrigin
{
    TopLeft,
    BottomLeft
};

/**
 * @brief Rectangle and orientation requested when capturing the OpenGL framebuffer.
 */
struct FramebufferCaptureRequest
{
    /** @brief Capture origin X in pixels. */
    int x = 0;
    /** @brief Capture origin Y in pixels. */
    int y = 0;
    /** @brief Capture width in pixels. A non-positive value means full framebuffer width. */
    int width = 0;
    /** @brief Capture height in pixels. A non-positive value means full framebuffer height. */
    int height = 0;
    /** @brief Origin convention expected by the caller. */
    FramebufferOrigin origin = FramebufferOrigin::TopLeft;
    /** @brief Additional vertical flip applied after readback. */
    bool flipVertically = false;
};

/**
 * @brief CPU-side RGBA32 framebuffer snapshot.
 */
struct Rgba32Framebuffer
{
    Rgba32Framebuffer();
    ~Rgba32Framebuffer();
    Rgba32Framebuffer(const Rgba32Framebuffer& other);
    Rgba32Framebuffer& operator=(const Rgba32Framebuffer& other);
    Rgba32Framebuffer(Rgba32Framebuffer&& other) noexcept;
    Rgba32Framebuffer& operator=(Rgba32Framebuffer&& other) noexcept;

    /** @brief Capture width in pixels. */
    int width = 0;
    /** @brief Capture height in pixels. */
    int height = 0;
    /** @brief Contiguous pixel buffer stored row by row. */
    std::vector<Rgba8Pixel> pixels;

    /** @brief Returns `true` when the framebuffer contains no pixel data. */
    bool Empty() const noexcept;
    /** @brief Returns the number of pixels stored in the buffer. */
    std::size_t PixelCount() const noexcept;
    /** @brief Returns the total size in bytes of the buffer. */
    std::size_t ByteSize() const noexcept;
    /** @brief Returns a read-only span over the pixel array. */
    ArrayView<const Rgba8Pixel> Pixels() const noexcept;
    /** @brief Returns a mutable span over the pixel array. */
    ArrayView<Rgba8Pixel> Pixels() noexcept;
    /**
     * @brief Returns a read-only raw byte view over the pixel buffer.
     *
     * @note This is the most direct API when the caller wants to forward the
     * framebuffer to a generic byte-oriented transport such as shared memory,
     * an IPC pipe or an external image-processing stage.
     */
    ByteView Bytes() const noexcept;
    /**
     * @brief Returns a mutable raw byte view over the pixel buffer.
     *
     * @note This is the mutable counterpart of the shared-memory-friendly byte
     * API exposed by the framebuffer snapshot.
     */
    ArrayView<std::byte> Bytes() noexcept;
};

/**
 * @brief Utility reading the current OpenGL framebuffer into RGBA8 memory.
 *
 * @note This helper belongs to the repository host-side raylib render layer.
 */
class OpenGlFramebufferReader
{
public:
    /**
     * @brief Captures the current OpenGL framebuffer as RGBA32 pixels.
     * @param request Capture rectangle and orientation.
     * @return Captured framebuffer.
     */
    static Rgba32Framebuffer ReadRgba32(const FramebufferCaptureRequest& request = {});
};
} // namespace mfd
