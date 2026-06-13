/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Public resizable offscreen rendering surface independent from backend headers.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "mfd/runtime_api/RuntimeApiExport.h"

namespace mfd::runtime_api
{
class RuntimeSession;

/**
 * @brief Pixel formats exposed by public offscreen frames.
 */
enum class FramePixelFormat
{
    /** @brief One interleaved 8-bit RGBA buffer. */
    Rgba8
};

/**
 * @brief Read-only CPU image view produced by one offscreen surface.
 */
struct OffscreenFrameView
{
    /** @brief Borrowed pixel buffer in row-major top-left order. */
    const std::uint8_t* pixels = nullptr;
    /** @brief Total readable byte count in `pixels`. */
    std::size_t byteSize = 0U;
    /** @brief Distance in bytes between two consecutive rows. */
    std::size_t rowStrideBytes = 0U;
    /** @brief Frame width in pixels. */
    int width = 0;
    /** @brief Frame height in pixels. */
    int height = 0;
    /** @brief Public pixel format. */
    FramePixelFormat pixelFormat = FramePixelFormat::Rgba8;

    /**
     * @brief Returns whether the frame view references one ready image.
     * @return `true` when `pixels` may be sampled safely.
     */
    [[nodiscard]] bool Ready() const noexcept
    {
        return pixels != nullptr && byteSize > 0U && rowStrideBytes > 0U && width > 0 && height > 0;
    }
};

/**
 * @brief Resizable offscreen surface rendering one runtime page into a private frame.
 *
 * @details Each instance owns one independent render target and one independent
 * CPU readback buffer so several surfaces may be rendered during the same frame
 * without texture or pixel collisions.
 */
class MFD_RUNTIME_API OffscreenSurface
{
public:
    OffscreenSurface();
    OffscreenSurface(int width, int height);
    ~OffscreenSurface();

    OffscreenSurface(const OffscreenSurface&) = delete;
    OffscreenSurface& operator=(const OffscreenSurface&) = delete;
    OffscreenSurface(OffscreenSurface&&) noexcept;
    OffscreenSurface& operator=(OffscreenSurface&&) noexcept;

    /**
     * @brief Resizes the owned offscreen render target.
     * @param width Requested offscreen width in pixels.
     * @param height Requested offscreen height in pixels.
     * @return `true` when the requested size now exists, otherwise `false`.
     *
     * @note Non-positive sizes release the current render target and return `false`.
     */
    bool Resize(int width, int height);

    /**
     * @brief Releases the current render target and the last CPU frame explicitly.
     */
    void Release() noexcept;

    /**
     * @brief Renders the active page of one runtime session offscreen.
     * @param session Runtime session providing the live page state to draw.
     * @return `true` when one offscreen frame was produced, otherwise `false`.
     */
    bool Render(const RuntimeSession& session);

    /**
     * @brief Returns whether the owned render target is ready.
     * @return `true` when the offscreen target exists and may render frames.
     */
    [[nodiscard]] bool Ready() const noexcept;

    /**
     * @brief Returns the current offscreen width in pixels.
     * @return Current render-target width, or `0` when no target is allocated.
     */
    [[nodiscard]] int Width() const noexcept;

    /**
     * @brief Returns the current offscreen height in pixels.
     * @return Current render-target height, or `0` when no target is allocated.
     */
    [[nodiscard]] int Height() const noexcept;

    /**
     * @brief Returns the latest rendered CPU frame.
     * @return Borrowed frame view ready to be uploaded by the host application.
     */
    [[nodiscard]] OffscreenFrameView FrameView() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::runtime_api
