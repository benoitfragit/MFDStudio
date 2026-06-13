/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal offscreen page renderer independent from the window host path.
 */

#include <filesystem>
#include <memory>

namespace mfd
{
class SceneRegistry;
}

namespace mfd::runtime_api::internal
{
/**
 * @brief Renders one runtime page directly into the currently bound offscreen target.
 *
 * @note This helper stays private to `mfd_runtime_api` so the public runtime
 * package keeps a small API surface while preserving the clipping and text
 * behaviour of the existing raylib render layer.
 */
class OffscreenPageRenderer
{
public:
    OffscreenPageRenderer();
    ~OffscreenPageRenderer();

    OffscreenPageRenderer(const OffscreenPageRenderer&) = delete;
    OffscreenPageRenderer& operator=(const OffscreenPageRenderer&) = delete;
    OffscreenPageRenderer(OffscreenPageRenderer&&) noexcept;
    OffscreenPageRenderer& operator=(OffscreenPageRenderer&&) noexcept;

    /**
     * @brief Sets the optional font file used to render text primitives.
     * @param fontFile Font path resolved by the runtime session.
     */
    void SetTextFontFile(std::filesystem::path fontFile);

    /**
     * @brief Releases every cached render resource while one valid GL context is bound.
     */
    void Release() noexcept;

    /**
     * @brief Draws the active page into the currently bound render target.
     * @param scene Runtime scene providing the active page state.
     * @param viewportWidth Offscreen viewport width in pixels.
     * @param viewportHeight Offscreen viewport height in pixels.
     * @param clippingAvailable `true` when the target owns a stencil attachment.
     */
    void DrawActivePage(const mfd::SceneRegistry& scene,
                        int viewportWidth,
                        int viewportHeight,
                        bool clippingAvailable);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::runtime_api::internal
