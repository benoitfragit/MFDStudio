/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief GPU-backed preview render targets, font and render caches owned outside `EditorApplication`.
 */

#include <cstddef>
#include <filesystem>
#include <vector>

#include <raylib.h>

#include "internal/application/EditorApplicationState.h"
#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "TextLayoutCache.h"

namespace editor
{
/** @brief Applies point texture filtering to one preview font, matching the runtime's crisp glyph rendering. */
void ApplyPointFilterToFont(Font font) noexcept;

/**
 * @brief Owns the off-screen render targets, preview font and render caches shared by every editor preview viewport.
 */
class PreviewResources
{
public:
    /** @brief Ensures the main preview render target matches the requested size. */
    void EnsureTexture(int width, int height);
    /** @brief Releases the main preview render target. */
    void ReleaseTexture();
    /** @brief Returns whether the main preview render target is currently loaded. */
    [[nodiscard]] bool TextureReady() const noexcept;
    /** @brief Returns the main preview render target. */
    [[nodiscard]] const RenderTexture2D& Texture() const noexcept;
    /** @brief Returns whether the main preview render target has a usable stencil buffer. */
    [[nodiscard]] bool TextureStencilReady() const noexcept;

    /** @brief Ensures the hover-tooltip preview render target matches the requested size. */
    void EnsureTooltipTexture(int width, int height);
    /** @brief Releases the hover-tooltip preview render target. */
    void ReleaseTooltipTexture();
    /** @brief Returns whether the hover-tooltip preview render target is currently loaded. */
    [[nodiscard]] bool TooltipTextureReady() const noexcept;
    /** @brief Returns the hover-tooltip preview render target. */
    [[nodiscard]] const RenderTexture2D& TooltipTexture() const noexcept;
    /** @brief Returns whether the hover-tooltip preview render target has a usable stencil buffer. */
    [[nodiscard]] bool TooltipTextureStencilReady() const noexcept;

    /** @brief Releases every cached layer-preview thumbnail render target. */
    void ReleaseLayerTextures() noexcept;
    /** @brief Returns the layer-preview thumbnail slot for `index`, resizing or recreating it for `width`/`height`. */
    [[nodiscard]] app::LayerPreviewTextureSlot& ResolveLayerSlot(std::size_t index, int width, int height);
    /** @brief Releases and removes every layer-preview thumbnail slot beyond `keepCount`. */
    void TrimLayerTextures(std::size_t keepCount);

    /** @brief Releases every preview-side GPU resource before shutdown, including the font and render caches. */
    void ReleaseGpuResources() noexcept;

    /** @brief Applies the font file declared by the loaded window configuration. */
    void ApplyFontFile(std::filesystem::path fontFile);
    /** @brief Lazily loads the configured preview font once raylib is ready. */
    void EnsureFont();
    /** @brief Releases the preview font override. */
    void ReleaseFont() noexcept;
    /** @brief Returns the preview font override when one is currently loaded. */
    [[nodiscard]] const Font* TextFont() const noexcept;

    /** @brief Returns the bezier-polyline render cache shared by every preview viewport. */
    [[nodiscard]] mfd::BezierPolylineCache& BezierCache() noexcept;
    /** @brief Returns the image-texture render cache shared by every preview viewport. */
    [[nodiscard]] mfd::ImageTextureCache& ImageCache() noexcept;
    /** @brief Returns the text-layout render cache shared by every preview viewport. */
    [[nodiscard]] mfd::TextLayoutCache& TextLayoutCache() noexcept;
    /** @brief Returns the scratch buffer reused to order static reticles for one page-preview draw. */
    [[nodiscard]] std::vector<int>& ScratchStaticReticleOrder() noexcept;

private:
    RenderTexture2D previewTexture_ {};
    bool previewTextureReady_ = false;
    bool previewTextureStencilReady_ = false;
    RenderTexture2D tooltipPreviewTexture_ {};
    bool tooltipPreviewTextureReady_ = false;
    bool tooltipPreviewTextureStencilReady_ = false;
    std::vector<app::LayerPreviewTextureSlot> layerPreviewTextures_ {};
    std::filesystem::path previewFontFile_ {};
    Font previewFont_ {};
    bool previewFontReady_ = false;
    bool previewFontLoadAttempted_ = false;
    mfd::BezierPolylineCache previewBezierCache_ {};
    mfd::ImageTextureCache previewImageCache_ {};
    mfd::TextLayoutCache previewTextLayoutCache_ {};
    std::vector<int> orderedStaticReticleIndices_ {};
};
} // namespace editor
