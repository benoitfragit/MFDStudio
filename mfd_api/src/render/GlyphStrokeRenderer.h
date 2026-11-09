/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal centerline-stroke renderer for text and time labels.
 */

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <raylib.h>

namespace mfd
{
/**
 * @brief Loads a display font for both authoring and runtime.
 * @param path TrueType or OpenType font path.
 * @param pixelSize Requested rasterization size.
 * @return Loaded raylib font, or an empty font when loading fails.
 */
Font LoadDisplayFont(const char* path, int pixelSize);

/**
 * @brief Caches and draws font centerlines independently of character size and stroke width.
 *
 * @note Common glyphs are packed into bounded atlas pages so one label normally
 * uses one texture. A per-glyph texture path remains available when atlas
 * padding or capacity cannot preserve the requested stroke.
 */
class GlyphStrokeRenderer
{
public:
    /** @brief Lightweight path and resource counters exposed to render tests. */
    struct Stats
    {
        std::size_t atlasDraws = 0U;
        std::size_t legacyDraws = 0U;
        std::size_t atlasPages = 0U;
        std::size_t atlasGlyphs = 0U;
        std::size_t legacyGlyphs = 0U;
    };

    GlyphStrokeRenderer() = default;
    ~GlyphStrokeRenderer();
    GlyphStrokeRenderer(const GlyphStrokeRenderer&) = delete;
    GlyphStrokeRenderer& operator=(const GlyphStrokeRenderer&) = delete;

    /** @brief Releases every shader, atlas page and fallback texture. */
    void Clear() noexcept;

    /**
     * @brief Draws one UTF-8 text run as constant-width font centerlines.
     * @param text UTF-8 label content.
     * @param font Font providing glyph bitmaps and metrics.
     * @param size Character size in screen pixels.
     * @param spacing Additional character spacing in screen pixels.
     * @param anchor Rotation anchor in screen coordinates.
     * @param origin Alignment origin relative to @p anchor.
     * @param rotation Clockwise rotation in degrees.
     * @param color Stroke color.
     * @param width Full stroke width in screen pixels.
     * @return `true` when the complete run was drawn by the centerline renderer.
     */
    bool Draw(const std::string& text,
              const Font& font,
              float size,
              float spacing,
              Vector2 anchor,
              Vector2 origin,
              float rotation,
              Color color,
              float width);

    /**
     * @brief Returns cumulative draw-path counters and current resource counts.
     * @return Current renderer statistics.
     */
    [[nodiscard]] Stats CacheStats() const noexcept;

private:
    struct AtlasGlyph
    {
        std::size_t pageIndex = 0U;
        Rectangle core {};
    };

    struct AtlasPage
    {
        Texture2D texture {};
        int nextX = 0;
        int nextY = 0;
        int rowHeight = 0;
    };

    struct GlyphRunItem
    {
        int index = 0;
        bool lineBreak = false;
        bool drawable = false;
        const AtlasGlyph* atlasGlyph = nullptr;
        Texture2D legacyField {};
    };

    bool Prepare(const Font& font);
    bool BuildGlyphRun(const std::string& text, const Font& font);
    bool AtlasSupportsRun(const Font& font, float scale, float radius) const noexcept;
    bool ResolveAtlasRun(const Font& font);
    bool ResolveLegacyRun(const Font& font);
    const AtlasGlyph* ResolveAtlas(const Font& font, int index);
    Texture2D ResolveLegacy(const Font& font, int index);
    bool TryPlaceAtlasRegion(int width,
                             int height,
                             std::size_t& pageIndex,
                             int& x,
                             int& y);
    bool CreateAtlasPage();
    void DrawAtlasRun(const Font& font,
                      float scale,
                      float spacing,
                      float lineStep,
                      float radius,
                      Color color) const;
    void DrawLegacyRun(const Font& font,
                       float scale,
                       float spacing,
                       float lineStep,
                       float radius,
                       Color color) const;

    const GlyphInfo* glyphs_ = nullptr;
    unsigned int fontTexture_ = 0U;
    Shader shader_ {};
    int radiusLocation_ = -1;
    bool attempted_ = false;
    std::unordered_map<int, Texture2D> legacyFields_ {};
    std::unordered_map<int, AtlasGlyph> atlasGlyphs_ {};
    std::vector<AtlasPage> atlasPages_ {};
    std::vector<GlyphRunItem> glyphRunScratch_ {};
    std::vector<int> uniqueGlyphScratch_ {};
    std::size_t atlasDrawCount_ = 0U;
    std::size_t legacyDrawCount_ = 0U;
};
} // namespace mfd
