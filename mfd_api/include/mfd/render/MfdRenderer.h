/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Convenience renderer drawing the active page of a scene registry.
 */

#include <filesystem>
#include <memory>
#include <string_view>

#include "mfd/MfdExport.h"
#include "mfd/runtime/SceneRegistry.h"

namespace mfd
{

/**
 * @brief Offscreen render status codes returned by `RenderActivePageOffscreen()`.
 */
enum class OffscreenRenderStatus
{
    Success,
    InvalidDimensions,
    DisplayDisabled,
    NoActivePage,
    RenderTargetUnavailable
};

/**
 * @brief Non-owning backend-specific texture handle view.
 *
 * @note `backendTextureId` is currently a raylib/OpenGL texture object id.
 * Its lifetime is bound to the owning `MfdRenderer` and invalidated after
 * offscreen target resize or renderer destruction.
 */
struct OffscreenTextureHandleView
{
    unsigned int backendTextureId = 0;

    /**
     * @brief Returns true when the non-owning handle is usable.
     */
    bool IsValid() const noexcept
    {
        return backendTextureId != 0;
    }
};

/**
 * @brief Request parameters for active-page offscreen rendering.
 */
struct OffscreenRenderRequest
{
    int width = 0;
    int height = 0;

    /**
     * @brief Returns true when both dimensions are strictly positive.
     */
    bool HasValidDimensions() const noexcept
    {
        return width > 0 && height > 0;
    }
};

/**
 * @brief Result payload for active-page offscreen rendering.
 */
struct OffscreenRenderResult
{
    OffscreenRenderStatus status = OffscreenRenderStatus::RenderTargetUnavailable;
    std::string_view message = "render target unavailable";
    int requestedWidth = 0;
    int requestedHeight = 0;
    int actualWidth = 0;
    int actualHeight = 0;
    OffscreenTextureHandleView texture {};

    /**
     * @brief Returns true when rendering succeeded and a valid handle is available.
     */
    bool Succeeded() const noexcept
    {
        return status == OffscreenRenderStatus::Success && texture.IsValid();
    }
};

/**
 * @brief Convenience renderer drawing the active page of a scene registry.
 */
class MFD_API MfdRenderer
{
public:
    MfdRenderer();
    ~MfdRenderer();

    MfdRenderer(const MfdRenderer&) = delete;
    MfdRenderer& operator=(const MfdRenderer&) = delete;
    MfdRenderer(MfdRenderer&&) noexcept;
    MfdRenderer& operator=(MfdRenderer&&) noexcept;

    /**
     * @brief Sets the optional font file used to render text primitives.
     * @param fontFile Font path resolved by the host application from the window JSON.
     *
     * @note An empty path restores the default raylib font.
     */
    void SetTextFontFile(std::filesystem::path fontFile);

    /**
     * @brief Draws the currently active page.
     * @param scene Scene registry providing the active page and reticles.
     */
    void DrawActivePage(const SceneRegistry& scene);

    /**
     * @brief Draws the currently active page into a top-left anchored viewport.
     * @param scene Scene registry providing the active page and reticles.
     * @param viewportWidth Viewport width in pixels.
     * @param viewportHeight Viewport height in pixels.
     *
     * @note This overload is useful when the host window reserves one adjacent
     * side panel for tools while keeping the MFD rendering isolated from that
     * UI area.
     */
    void DrawActivePage(const SceneRegistry& scene, int viewportWidth, int viewportHeight);

    /**
     * @brief Renders the active page into an internal offscreen texture.
     *
     * @param scene Scene registry providing the active page and reticles.
     * @param request Requested output dimensions in pixels.
     * @return Rendering status, effective dimensions, optional short status message, and non-owning texture handle view.
     *
     * @note The returned texture handle is backend-specific and currently maps to
     * a raylib/OpenGL texture object id.
     * @note The returned handle is invalidated after offscreen resize/recreate and
     * when this renderer is destroyed.
     * @note This call must execute on the active graphics-context thread.
     */
    OffscreenRenderResult RenderActivePageOffscreen(const SceneRegistry& scene, const OffscreenRenderRequest& request);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd
