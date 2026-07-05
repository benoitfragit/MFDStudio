/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Hosts one MFD runtime offscreen and exposes its CPU RGBA8 frame.
 *
 * @note This translation unit deliberately depends only on `mfd_runtime_api`
 * and the raylib backend. It never includes Direct3D or `windows.h`, because
 * raylib and `windows.h` define clashing symbols. The DX11 upload of the frame
 * produced here lives in the separate @ref Dx11TextureUploader unit.
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "mfd/runtime_api/OffscreenSurface.h"
#include "mfd/runtime_api/RuntimeSession.h"

namespace lhld
{
/**
 * @brief Borrowed view over the latest offscreen MFD frame in CPU memory.
 */
struct OffscreenPixels
{
    /** @brief Borrowed RGBA8 pixel buffer, row-major top-left. */
    const std::uint8_t* pixels = nullptr;
    /** @brief Frame width in pixels. */
    int width = 0;
    /** @brief Frame height in pixels. */
    int height = 0;
    /** @brief Distance in bytes between two consecutive rows. */
    std::size_t rowStrideBytes = 0U;
    /** @brief True when @ref pixels may be sampled safely. */
    bool ready = false;
};

/**
 * @brief Runs one authored MFD window offscreen through the public runtime API.
 *
 * The client hosts the runtime in-process: the generated UI still publishes
 * command batches over loopback UDP, and this view receives them by advancing
 * the session, renders the active page into a private surface, and exposes the
 * result as CPU pixels for the DX11 bezel.
 */
class OffscreenMfdView
{
public:
    OffscreenMfdView();
    ~OffscreenMfdView();

    OffscreenMfdView(const OffscreenMfdView&) = delete;
    OffscreenMfdView& operator=(const OffscreenMfdView&) = delete;

    /**
     * @brief Creates the hidden render context and loads the window JSON.
     * @param windowFile Path to the authored root window JSON.
     * @param error Human-readable failure reason populated on error.
     * @return `true` when the session is ready to render.
     */
    bool Initialize(const std::filesystem::path& windowFile, std::string& error);

    /**
     * @brief Activates one page by name in the hosted runtime.
     * @param pageName Authored page name.
     * @return `true` when the page exists and is now active.
     */
    bool SetActivePage(std::string_view pageName);

    /**
     * @brief Advances the runtime and renders the active page into the surface.
     * @param deltaSeconds Frame delta in seconds.
     * @param width Requested surface width in pixels.
     * @param height Requested surface height in pixels.
     */
    void Update(float deltaSeconds, int width, int height);

    /**
     * @brief Returns the latest rendered CPU frame.
     */
    OffscreenPixels Pixels() const noexcept;

    /**
     * @brief Returns the active page name for the status panel.
     */
    std::string ActivePageName() const;

    /**
     * @brief Returns the latest command-side runtime status string.
     */
    std::string LastCommandStatus() const;

    /**
     * @brief Releases the surface and hidden render context.
     */
    void Shutdown() noexcept;

private:
    /** @brief True once the hidden raylib render context has been created. */
    bool renderContextReady_ = false;
    /** @brief In-process runtime hosting the authored MFD window. */
    mfd::runtime_api::RuntimeSession session_ {};
    /** @brief Private offscreen render target for the active page. */
    mfd::runtime_api::OffscreenSurface surface_ {};
};
} // namespace lhld
