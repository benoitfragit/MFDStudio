/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>

namespace mfd
{
/** @brief Loads a display font for both authoring and runtime. */
Font LoadDisplayFont(const char* path, int pixelSize);

/** @brief Caches the centerlines of the active font independently of character size and stroke width. */
class GlyphStrokeRenderer
{
public:
    GlyphStrokeRenderer() = default;
    ~GlyphStrokeRenderer();
    GlyphStrokeRenderer(const GlyphStrokeRenderer&) = delete;
    GlyphStrokeRenderer& operator=(const GlyphStrokeRenderer&) = delete;

    void Clear() noexcept;
    bool Draw(const std::string& text, const Font& font, float size, float spacing, Vector2 anchor,
              Vector2 origin, float rotation, Color color, float width);

private:
    bool Prepare(const Font& font);
    Texture2D Resolve(const Font& font, int index);
    const GlyphInfo* glyphs_ = nullptr;
    unsigned int fontTexture_ = 0;
    Shader shader_ {};
    int radiusLocation_ = -1;
    bool attempted_ = false;
    std::unordered_map<int, Texture2D> fields_;
};
}
