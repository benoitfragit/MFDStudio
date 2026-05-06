/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Low-level 2D canvas used to render reticles with raylib.
 */

#include <cstddef>
#include <vector>

#include <raylib.h>

#include "mfd/MfdExport.h"
#include "mfd/model/Reticle.h"

namespace mfd
{
class ImageTextureCache;

/**
 * @brief Lightweight 2D renderer turning reticle data into raylib draw calls.
 */
class MFD_API Canvas2D
{
public:
    /**
     * @brief Creates a canvas for a given viewport size and page view.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view Page view center and zoom.
     * @param textFont Optional font override used for text-like primitives.
     * @param backgroundColor Color restored when one clipping primitive erases part of the page.
     * @param clippingEnabled Enables clipping-mask evaluation for clipping primitives.
     * @param imageCache Optional texture cache used by image primitives.
     */
    Canvas2D(int width,
             int height,
             PageViewState view = {},
             const Font* textFont = nullptr,
             Color backgroundColor = BLACK,
             bool clippingEnabled = false,
             ImageTextureCache* imageCache = nullptr);

    /**
     * @brief Draws one full reticle group.
     * @param reticle Reticle to render.
     */
    void DrawReticle(const ReticleGroup& reticle) const;
    /**
     * @brief Draws one full reticle group with externally resolved visibility.
     * @param reticle Reticle to render.
     * @param visible Blink-resolved visibility to apply for this draw call.
     */
    void DrawReticle(const ReticleGroup& reticle, bool visible) const;

private:
    Font TextFont() const noexcept;
    float LogicalScale() const noexcept;
    float ToPixels(float logicalValue) const noexcept;
    float ViewZoom() const noexcept;
    float ToViewPixels(float logicalValue) const noexcept;
    Vector2 ToScreen(const Vec2& logical) const noexcept;
    Vec2 TransformPoint(const Vec2& point, const Primitive& primitive, const ReticleGroup& group) const noexcept;
    void BuildScreenPointsInto(const Vec2* points,
                               std::size_t pointCount,
                               const Primitive& primitive,
                               const ReticleGroup& group,
                               std::vector<Vector2>& destination) const;
    void DrawReticlePrimitives(const ReticleGroup& reticle) const;
    void ApplyClipMask(const Primitive& primitive, const ReticleGroup& group) const;
    bool DrawClipMaskPrimitive(const Primitive& primitive, const ReticleGroup& group) const;
    void DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const;

    int width_ = 0;
    int height_ = 0;
    PageViewState view_ {};
    const Font* textFont_ = nullptr;
    Color backgroundColor_ = BLACK;
    bool clippingEnabled_ = false;
    ImageTextureCache* imageCache_ = nullptr;
    mutable std::vector<Vec2> logicalScratchA_ {};
    mutable std::vector<Vec2> logicalScratchB_ {};
    mutable std::vector<Vector2> screenScratchA_ {};
    mutable std::vector<Vector2> screenScratchB_ {};
    mutable std::vector<std::size_t> triangleIndexScratch_ {};
};
} // namespace mfd
