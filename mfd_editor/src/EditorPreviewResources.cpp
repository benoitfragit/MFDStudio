/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorPreviewResources.h"

#include "RenderTextureUtils.h"

namespace editor
{
void ApplyPointFilterToFont(const Font font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    }
}

void PreviewResources::EnsureTexture(const int width, const int height)
{
    if (previewTextureReady_ &&
        previewTexture_.texture.width == width &&
        previewTexture_.texture.height == height)
    {
        return;
    }

    ReleaseTexture();
    previewTextureStencilReady_ = false;
    previewTexture_ = mfd::LoadRenderTextureWithStencil(width, height, &previewTextureStencilReady_);
    previewTextureReady_ = previewTexture_.texture.id != 0;
}

void PreviewResources::ReleaseTexture()
{
    const bool windowReady = IsWindowReady();
    if (previewTextureReady_)
    {
        if (windowReady)
        {
            UnloadRenderTexture(previewTexture_);
        }
        previewTexture_ = {};
    }

    previewTextureReady_ = false;
    previewTextureStencilReady_ = false;
}

bool PreviewResources::TextureReady() const noexcept
{
    return previewTextureReady_;
}

const RenderTexture2D& PreviewResources::Texture() const noexcept
{
    return previewTexture_;
}

bool PreviewResources::TextureStencilReady() const noexcept
{
    return previewTextureStencilReady_;
}

void PreviewResources::EnsureTooltipTexture(const int width, const int height)
{
    if (tooltipPreviewTextureReady_ &&
        tooltipPreviewTexture_.texture.width == width &&
        tooltipPreviewTexture_.texture.height == height)
    {
        return;
    }

    ReleaseTooltipTexture();
    tooltipPreviewTextureStencilReady_ = false;
    tooltipPreviewTexture_ =
        mfd::LoadRenderTextureWithStencil(width, height, &tooltipPreviewTextureStencilReady_);
    tooltipPreviewTextureReady_ = tooltipPreviewTexture_.texture.id != 0;
}

void PreviewResources::ReleaseTooltipTexture()
{
    const bool windowReady = IsWindowReady();
    if (tooltipPreviewTextureReady_)
    {
        if (windowReady)
        {
            UnloadRenderTexture(tooltipPreviewTexture_);
        }
        tooltipPreviewTexture_ = {};
    }

    tooltipPreviewTextureReady_ = false;
    tooltipPreviewTextureStencilReady_ = false;
}

bool PreviewResources::TooltipTextureReady() const noexcept
{
    return tooltipPreviewTextureReady_;
}

const RenderTexture2D& PreviewResources::TooltipTexture() const noexcept
{
    return tooltipPreviewTexture_;
}

bool PreviewResources::TooltipTextureStencilReady() const noexcept
{
    return tooltipPreviewTextureStencilReady_;
}

void PreviewResources::ReleaseLayerTextures() noexcept
{
    const bool windowReady = IsWindowReady();
    for (app::LayerPreviewTextureSlot& slot : layerPreviewTextures_)
    {
        if (slot.ready)
        {
            if (windowReady)
            {
                UnloadRenderTexture(slot.texture);
            }
            slot.texture = {};
        }

        slot.ready = false;
        slot.stencilReady = false;
        slot.width = 0;
        slot.height = 0;
    }

    layerPreviewTextures_.clear();
}

app::LayerPreviewTextureSlot& PreviewResources::ResolveLayerSlot(const std::size_t index, const int width, const int height)
{
    if (index >= layerPreviewTextures_.size())
    {
        layerPreviewTextures_.resize(index + 1U);
    }

    app::LayerPreviewTextureSlot& slot = layerPreviewTextures_[index];
    if (!slot.ready || slot.width != width || slot.height != height)
    {
        if (slot.ready)
        {
            UnloadRenderTexture(slot.texture);
            slot.texture = {};
            slot.ready = false;
        }

        slot.stencilReady = false;
        slot.texture = mfd::LoadRenderTextureWithStencil(width, height, &slot.stencilReady);
        slot.ready = slot.texture.texture.id != 0;
        slot.width = width;
        slot.height = height;
    }

    return slot;
}

void PreviewResources::TrimLayerTextures(const std::size_t keepCount)
{
    if (layerPreviewTextures_.size() <= keepCount)
    {
        return;
    }

    for (std::size_t slotIndex = keepCount; slotIndex < layerPreviewTextures_.size(); ++slotIndex)
    {
        if (layerPreviewTextures_[slotIndex].ready)
        {
            UnloadRenderTexture(layerPreviewTextures_[slotIndex].texture);
        }
    }
    layerPreviewTextures_.resize(keepCount);
}

void PreviewResources::ReleaseGpuResources() noexcept
{
    previewBezierCache_.Clear();
    previewImageCache_.Clear();
    previewTextLayoutCache_.Clear();
    ReleaseLayerTextures();
    ReleaseTooltipTexture();
    ReleaseTexture();
    ReleaseFont();
}

void PreviewResources::ApplyFontFile(std::filesystem::path fontFile)
{
    if (!fontFile.empty())
    {
        fontFile = fontFile.lexically_normal();
    }

    if (previewFontFile_ == fontFile)
    {
        return;
    }

    previewTextLayoutCache_.Clear();
    ReleaseFont();
    previewFontFile_ = std::move(fontFile);
    previewFontLoadAttempted_ = false;
}

void PreviewResources::EnsureFont()
{
    if (previewFontReady_ || previewFontLoadAttempted_ || previewFontFile_.empty() || !IsWindowReady())
    {
        return;
    }

    previewFontLoadAttempted_ = true;
    const std::string path = previewFontFile_.string();
    Font loadedFont = LoadFont(path.c_str());
    if (loadedFont.texture.id == 0)
    {
        return;
    }

    ApplyPointFilterToFont(loadedFont);
    previewFont_ = loadedFont;
    previewFontReady_ = true;
}

void PreviewResources::ReleaseFont() noexcept
{
    const bool windowReady = IsWindowReady();
    if (previewFontReady_)
    {
        if (windowReady)
        {
            UnloadFont(previewFont_);
        }
        previewFont_ = {};
        previewFontReady_ = false;
    }
}

const Font* PreviewResources::TextFont() const noexcept
{
    return previewFontReady_ ? &previewFont_ : nullptr;
}

mfd::BezierPolylineCache& PreviewResources::BezierCache() noexcept
{
    return previewBezierCache_;
}

mfd::ImageTextureCache& PreviewResources::ImageCache() noexcept
{
    return previewImageCache_;
}

mfd::TextLayoutCache& PreviewResources::TextLayoutCache() noexcept
{
    return previewTextLayoutCache_;
}

std::vector<int>& PreviewResources::ScratchStaticReticleOrder() noexcept
{
    return orderedStaticReticleIndices_;
}
} // namespace editor
