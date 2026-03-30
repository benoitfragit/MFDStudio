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

#include <vector>

#include <raylib.h>

#include "mfd/MfdExport.h"
#include "mfd/model/Reticle.h"

namespace mfd
{
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
     */
    Canvas2D(int width, int height, PageViewState view = {}, const Font* textFont = nullptr);

    /**
     * @brief Draws one full reticle group.
     * @param reticle Reticle to render.
     */
    void DrawReticle(const ReticleGroup& reticle) const;

private:
    Font TextFont() const noexcept;
    float LogicalScale() const noexcept;
    float ToPixels(float logicalValue) const noexcept;
    Vector2 ToScreen(const Vec2& logical) const noexcept;
    Vec2 TransformPoint(const Vec2& point, const Primitive& primitive, const ReticleGroup& group) const noexcept;
    std::vector<Vector2> BuildScreenPoints(const std::vector<Vec2>& points,
                                           const Primitive& primitive,
                                           const ReticleGroup& group) const;
    void DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const;

    int width_ = 0;
    int height_ = 0;
    PageViewState view_ {};
    const Font* textFont_ = nullptr;
};
} // namespace mfd
