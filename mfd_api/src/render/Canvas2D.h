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
#include <functional>
#include <vector>

#include <raylib.h>

#include "mfd/model/Reticle.h"
#include "RenderWorkBudget.h"

namespace mfd
{
class BezierPolylineCache;
class ImageTextureCache;
class TextLayoutCache;

/**
 * @brief Lightweight 2D renderer turning reticle data into raylib draw calls.
 */
class Canvas2D
{
public:
    /**
     * @brief Repaints the page background inside the currently active clip region.
     *
     * Invoked while a clipping stencil is active, so the callback must redraw the
     * background exactly as for a normal frame and must not touch the stencil state.
     */
    using BackgroundRestoreCallback = std::function<void()>;

    /**
     * @brief Creates a canvas for a given viewport size and page view.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view Page view center and zoom.
     * @param textFont Optional font override used for text-like primitives.
     * @param backgroundColor Color restored when one clipping primitive erases part of the page.
     * @param clippingEnabled Enables clipping-mask evaluation for clipping primitives.
     * @param bezierCache Optional persistent cache used to flatten Bézier primitives once.
     * @param imageCache Optional texture cache used by image primitives.
     * @param textLayoutCache Optional text-layout cache reused across frames.
     * @param backgroundRestore Optional callback repainting the clipped background; when empty the
     *        runtime behaviour of filling the erased region with @p backgroundColor is kept.
     */
    Canvas2D(int width,
             int height,
             PageViewState view = {},
             const Font* textFont = nullptr,
             Color backgroundColor = BLACK,
             bool clippingEnabled = false,
             BezierPolylineCache* bezierCache = nullptr,
             ImageTextureCache* imageCache = nullptr,
             TextLayoutCache* textLayoutCache = nullptr,
             BackgroundRestoreCallback backgroundRestore = {});

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

    /**
     * @brief Draws one reticle's visible primitives without applying its clipping mask.
     * @param reticle Reticle to render.
     * @param visible Blink-resolved visibility to apply for this draw call.
     *
     * @note Used by layer-local clipping restore callbacks to repaint lower-layer reticles while a
     * stencil is active. It never evaluates `ApplyClipMask` and never touches the stencil state, so
     * it is safe to call from within a `BackgroundRestoreCallback`.
     */
    void DrawReticleWithoutClipping(const ReticleGroup& reticle, bool visible) const;

private:
    /** @brief Identifies the already validated representation used to draw one clipping mask. */
    enum class PreparedClipMaskType
    {
        None,
        Circle,
        Polygon
    };

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
    /**
     * @brief Transforms every logical point and rejects the complete collection on the first invalid value.
     * @return `true` only when exactly @p pointCount finite screen points were produced.
     */
    bool BuildScreenPointsExactInto(const Vec2* points,
                                    std::size_t pointCount,
                                    const Primitive& primitive,
                                    const ReticleGroup& group,
                                    std::vector<Vector2>& destination) const;
    void DrawReticlePrimitives(const ReticleGroup& reticle) const;
    void RestoreClippedBackground() const;
    void ApplyClipMask(const Primitive& primitive, const ReticleGroup& group) const;
    /** @brief Prepares one polygonal clip mask without mutating OpenGL state. */
    bool PrepareClipMaskPolygon(const Vec2* points,
                                std::size_t pointCount,
                                const Primitive& primitive,
                                const ReticleGroup& group) const;
    /** @brief Validates and prepares one clip mask without mutating OpenGL state. */
    PreparedClipMaskType PrepareClipMaskPrimitive(const Primitive& primitive,
                                                  const ReticleGroup& group,
                                                  Vector2& circleCenter,
                                                  float& circleRadius) const;
    /** @brief Draws one previously validated mask into the active stencil pass. */
    void DrawPreparedClipMask(PreparedClipMaskType maskType,
                              const Vector2& circleCenter,
                              float circleRadius) const;
    void DrawPrimitive(const Primitive& primitive, const ReticleGroup& group) const;

    int width_ = 0;
    int height_ = 0;
    PageViewState view_ {};
    const Font* textFont_ = nullptr;
    Color backgroundColor_ = BLACK;
    bool clippingEnabled_ = false;
    BackgroundRestoreCallback backgroundRestore_ {};
    BezierPolylineCache* bezierCache_ = nullptr;
    ImageTextureCache* imageCache_ = nullptr;
    TextLayoutCache* textLayoutCache_ = nullptr;
    mutable std::vector<Vec2> logicalScratchA_ {};
    mutable std::vector<Vec2> logicalScratchB_ {};
    mutable std::vector<Vector2> screenScratchA_ {};
    mutable std::vector<Vector2> screenScratchB_ {};
    mutable std::vector<std::size_t> triangleIndexScratch_ {};
    mutable detail::RenderWorkBudget drawBudget_ {detail::kMaxCanvasDrawOperations};
};
} // namespace mfd
