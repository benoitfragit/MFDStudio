/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for the internal direct offscreen page renderer.
 */

#include "internal/OffscreenPageRenderer.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <raylib.h>

#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"
#include "render/BezierPolylineCache.h"
#include "render/Canvas2D.h"
#include "render/ImageTextureCache.h"
#include "render/TextLayoutCache.h"

namespace mfd::runtime_api::internal
{
namespace
{
Color ToRayColor(const ColorRgba& color) noexcept
{
    return Color {color.r, color.g, color.b, color.a};
}

Font ResolveTextFont(const Font* textFont) noexcept
{
    return textFont != nullptr ? *textFont : GetFontDefault();
}

void ApplyPointFilterToFont(const Font& font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    }
}

bool HasSamePageTitleDisplay(const PageTitleDisplayDefinition& lhs,
                             const PageTitleDisplayDefinition& rhs) noexcept
{
    return lhs.visible == rhs.visible &&
           lhs.transform.position.x == rhs.transform.position.x &&
           lhs.transform.position.y == rhs.transform.position.y &&
           lhs.transform.rotationDegrees == rhs.transform.rotationDegrees &&
           lhs.transform.scale.x == rhs.transform.scale.x &&
           lhs.transform.scale.y == rhs.transform.scale.y &&
           lhs.color.r == rhs.color.r &&
           lhs.color.g == rhs.color.g &&
           lhs.color.b == rhs.color.b &&
           lhs.color.a == rhs.color.a &&
           lhs.lineWidth == rhs.lineWidth &&
           lhs.lineStyle == rhs.lineStyle &&
           lhs.decoration == rhs.decoration;
}

bool ActiveReticleViewsUseClipping(const std::vector<ReticleRenderView>& activeReticles)
{
    for (const ReticleRenderView& reticle : activeReticles)
    {
        if (reticle.group != nullptr && reticle.visible && ResolveClipPrimitive(*reticle.group) != nullptr)
        {
            return true;
        }
    }

    return false;
}

void DrawActivePageContent(const SceneRegistry& scene,
                           const std::vector<ReticleRenderView>& activeReticles,
                           const ReticleGroup& titleReticle,
                           const int width,
                           const int height,
                           const Font* textFont,
                           const bool clippingEnabled,
                           BezierPolylineCache* bezierCache,
                           ImageTextureCache* imageCache,
                           TextLayoutCache* textLayoutCache)
{
    const Canvas2D canvas(
        width,
        height,
        scene.ActivePageView(),
        textFont,
        ToRayColor(scene.ActiveBackgroundColor()),
        clippingEnabled,
        bezierCache,
        imageCache,
        textLayoutCache);

    for (const ReticleRenderView& reticle : activeReticles)
    {
        if (reticle.group != nullptr)
        {
            canvas.DrawReticle(*reticle.group, reticle.visible);
        }
    }

    canvas.DrawReticle(titleReticle);
}
} // namespace

class OffscreenPageRenderer::Impl
{
    struct TitleReticleCache
    {
        std::string pageName {};
        std::string pageTitle {};
        PageTitleDisplayDefinition display {};
        ReticleGroup reticle {};
        bool valid = false;
    };

public:
    Impl() = default;

    ~Impl()
    {
        Release();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void Release() noexcept
    {
        bezierCache_.Clear();
        imageCache_.Clear();
        ResetTextFont();
    }

    void ResetTextFont() noexcept
    {
        textLayoutCache_.Clear();
        if (textFontReady_ && IsWindowReady())
        {
            UnloadFont(textFont_);
        }

        textFont_ = {};
        textFontReady_ = false;
        textFontLoadAttempted_ = false;
    }

    void SetTextFontFile(std::filesystem::path path)
    {
        if (!path.empty())
        {
            path = path.lexically_normal();
        }

        if (fontFile_ == path)
        {
            return;
        }

        ResetTextFont();
        fontFile_ = std::move(path);
    }

    bool EnsureTextFont()
    {
        if (fontFile_.empty())
        {
            return true;
        }

        if (textFontReady_)
        {
            return true;
        }

        if (textFontLoadAttempted_)
        {
            return false;
        }

        textFontLoadAttempted_ = true;
        const std::string path = fontFile_.string();
        const Font loadedFont = LoadFont(path.c_str());
        if (loadedFont.texture.id == 0)
        {
            return false;
        }

        ApplyPointFilterToFont(loadedFont);
        textFont_ = loadedFont;
        textFontReady_ = true;
        return true;
    }

    const Font* ActiveTextFont() const noexcept
    {
        return textFontReady_ ? &textFont_ : nullptr;
    }

    const ReticleGroup& ResolveTitleReticle(const PageSummary& activePage)
    {
        if (!titleReticleCache_.valid ||
            titleReticleCache_.pageName != activePage.name ||
            titleReticleCache_.pageTitle != activePage.title ||
            !HasSamePageTitleDisplay(titleReticleCache_.display, activePage.titleDisplay))
        {
            titleReticleCache_.pageName = activePage.name;
            titleReticleCache_.pageTitle = activePage.title;
            titleReticleCache_.display = activePage.titleDisplay;
            titleReticleCache_.reticle =
                BuildPageTitleDisplayReticle(activePage.name, activePage.title, activePage.titleDisplay);
            titleReticleCache_.valid = true;
        }

        return titleReticleCache_.reticle;
    }

    void DrawActivePage(const SceneRegistry& scene,
                        const int viewportWidth,
                        const int viewportHeight,
                        const bool clippingAvailable)
    {
        if (viewportWidth <= 0 || viewportHeight <= 0)
        {
            return;
        }

        const auto activePage = scene.ActivePageSummary();
        if (!activePage.has_value())
        {
            return;
        }

        EnsureTextFont();
        const ReticleGroup& titleReticle = ResolveTitleReticle(*activePage);
        const Font* activeTextFont = ActiveTextFont();
        ApplyPointFilterToFont(ResolveTextFont(activeTextFont));

        scene.CollectActiveReticleViews(activeReticlesScratch_);
        const bool clippingEnabled =
            clippingAvailable && ActiveReticleViewsUseClipping(activeReticlesScratch_);
        DrawActivePageContent(
            scene,
            activeReticlesScratch_,
            titleReticle,
            viewportWidth,
            viewportHeight,
            activeTextFont,
            clippingEnabled,
            &bezierCache_,
            &imageCache_,
            &textLayoutCache_);
    }

private:
    std::filesystem::path fontFile_ {};
    Font textFont_ {};
    bool textFontReady_ = false;
    bool textFontLoadAttempted_ = false;
    std::vector<ReticleRenderView> activeReticlesScratch_ {};
    BezierPolylineCache bezierCache_ {};
    ImageTextureCache imageCache_ {};
    TextLayoutCache textLayoutCache_ {};
    TitleReticleCache titleReticleCache_ {};
};

OffscreenPageRenderer::OffscreenPageRenderer()
    : impl_(std::make_unique<Impl>())
{
}

OffscreenPageRenderer::~OffscreenPageRenderer() = default;

OffscreenPageRenderer::OffscreenPageRenderer(OffscreenPageRenderer&&) noexcept = default;

OffscreenPageRenderer& OffscreenPageRenderer::operator=(OffscreenPageRenderer&&) noexcept = default;

void OffscreenPageRenderer::SetTextFontFile(std::filesystem::path fontFile)
{
    if (impl_ != nullptr)
    {
        impl_->SetTextFontFile(std::move(fontFile));
    }
}

void OffscreenPageRenderer::DrawActivePage(const SceneRegistry& scene,
                                           const int viewportWidth,
                                           const int viewportHeight,
                                           const bool clippingAvailable)
{
    if (impl_ != nullptr)
    {
        impl_->DrawActivePage(scene, viewportWidth, viewportHeight, clippingAvailable);
    }
}

void OffscreenPageRenderer::Release() noexcept
{
    if (impl_ != nullptr)
    {
        impl_->Release();
    }
}
} // namespace mfd::runtime_api::internal
