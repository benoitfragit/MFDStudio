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

#include "mfd/MfdExport.h"
#include "mfd/runtime/SceneRegistry.h"

namespace mfd
{
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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd
