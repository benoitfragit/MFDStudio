/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "GlyphStrokeRenderer.h"

#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace mfd
{
namespace
{
// The field stores the nearest centerline pixel (RG) and its 8-connected neighbours (B).
// Evaluate distance to segments in screen pixels so resizing a label never widens its stems.
constexpr char kStrokeShader[] = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float radius;
const vec2 step8[8] = vec2[8](vec2(0,-1),vec2(1,-1),vec2(1,0),vec2(1,1),
                               vec2(0,1),vec2(-1,1),vec2(-1,0),vec2(-1,-1));
vec2 subpixel(int packedOffset)
{
    return (vec2(packedOffset & 15, packedOffset >> 4) - 8.0) * 0.2;
}
void main()
{
    ivec2 extent = textureSize(texture0, 0);
    vec2 p = fragTexCoord * vec2(extent);
    ivec2 cell = clamp(ivec2(floor(p)), ivec2(0), extent - 1);
    ivec4 field = ivec4(round(texelFetch(texture0,cell,0)*255.0));
    vec2 pixelsPerCell = 1.0 / max(vec2(length(vec2(dFdx(p.x),dFdy(p.x))),
                                         length(vec2(dFdx(p.y),dFdy(p.y)))),vec2(1e-6));
    vec2 shift = subpixel(field.a);
    vec2 v = (p - vec2(field.rg) - 0.5 - shift) * pixelsPerCell;
    float best = dot(v,v);
    for (int i=0; i<8; ++i)
    {
        if ((field.b & (1<<i)) == 0) continue;
        ivec2 adjacent = field.rg + ivec2(step8[i]);
        int encoded = int(round(texelFetch(texture0,adjacent,0).a*255.0));
        vec2 end = (step8[i] + subpixel(encoded) - shift) * pixelsPerCell;
        vec2 delta = v - end * clamp(dot(v,end)/max(dot(end,end),1e-10),0.0,1.0);
        best = min(best,dot(delta,delta));
    }
    float coverage = clamp(radius + 0.5 - sqrt(best), 0.0, 1.0);
    finalColor = vec4(fragColor.rgb, fragColor.a * coverage);
}
)glsl";

constexpr int kMaxFieldSide = 128;
constexpr int kMaxCachedGlyphs = 512;
constexpr int kAtlasSide = 256;
constexpr int kAtlasPadding = 8;
constexpr std::size_t kMaxAtlasPages = 8U;
constexpr std::array<int, 8> kDx {0, 1, 1, 1, 0, -1, -1, -1};
constexpr std::array<int, 8> kDy {-1, -1, 0, 1, 1, 1, 0, -1};

struct Mask
{
    int width;
    int height;
    std::vector<unsigned char> data;

    bool At(const int x, const int y) const noexcept
    {
        return x >= 0 && y >= 0 && x < width && y < height && data[y * width + x] != 0;
    }

    int Neighbours(const int x, const int y) const noexcept
    {
        int bits = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (At(x + kDx[i], y + kDy[i])) bits |= 1 << i;
        }
        return bits;
    }
};

struct StrokeFieldPixels
{
    int width = 0;
    int height = 0;
    int coreWidth = 0;
    int coreHeight = 0;
    int padding = 0;
    std::vector<Color> pixels {};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return width > 0 && height > 0 && coreWidth > 0 && coreHeight > 0 &&
               pixels.size() == static_cast<std::size_t>(width * height);
    }
};

bool CanPeel(const int neighbours, const int pass) noexcept
{
    int count = 0;
    int starts = 0;
    for (int i = 0; i < 8; ++i)
    {
        if ((neighbours & (1 << i)) == 0) continue;
        ++count;
        if ((neighbours & (1 << ((i + 7) % 8))) == 0) ++starts;
    }
    // N, E, S and W occupy bits 0, 2, 4 and 6. Alternate directional deletion constraints.
    const int first = pass == 0 ? 0x15 : 0x45;
    const int second = pass == 0 ? 0x54 : 0x51;
    return count >= 2 && count <= 6 && starts == 1 &&
           (neighbours & first) != first && (neighbours & second) != second;
}

// Zhang-Suen thinning preserves endpoints and connected strokes. Work is bounded by the native grid.
void Thin(Mask& mask)
{
    std::vector<int> pending;
    for (int round = 0; round < std::max(mask.width, mask.height); ++round)
    {
        bool changed = false;
        for (int pass = 0; pass < 2; ++pass)
        {
            pending.clear();
            for (int y = 0; y < mask.height; ++y)
            {
                for (int x = 0; x < mask.width; ++x)
                {
                    if (mask.At(x, y) && CanPeel(mask.Neighbours(x, y), pass))
                        pending.push_back(y * mask.width + x);
                }
            }
            for (const int index : pending)
            {
                // Preserve the last pixel of a detached dot during simultaneous deletion.
                if (mask.Neighbours(index % mask.width, index / mask.width) == 0) continue;
                mask.data[index] = 0;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

Mask ReadGlyphCoverage(const Image& image)
{
    const float scale = std::min(1.0f, static_cast<float>(kMaxFieldSide) / std::max(image.width, image.height));
    const int width = std::max(1, static_cast<int>(std::round(image.width * scale)));
    const int height = std::max(1, static_cast<int>(std::round(image.height * scale)));
    Mask mask {width, height, std::vector<unsigned char>(width * height)};
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const Color pixel = GetImageColor(image, static_cast<int>((x + 0.5f) * image.width / width),
                                              static_cast<int>((y + 0.5f) * image.height / height));
            const auto coverage = image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE ? pixel.r : pixel.a;
            mask.data[y * width + x] = coverage >= 96;
        }
    }
    return mask;
}

Vector2 FieldCoreSize(const Image& image) noexcept
{
    if (!image.data || image.width <= 0 || image.height <= 0)
    {
        return {};
    }

    const float fieldScale =
        std::min(1.0f, static_cast<float>(kMaxFieldSide) / std::max(image.width, image.height));
    return Vector2 {
        static_cast<float>(std::max(1, static_cast<int>(std::round(image.width * fieldScale)))),
        static_cast<float>(std::max(1, static_cast<int>(std::round(image.height * fieldScale))))};
}

int DistanceSquared(const Color seed, const int x, const int y) noexcept
{
    if (!seed.a) return std::numeric_limits<int>::max();
    const int dx = x - seed.r;
    const int dy = y - seed.g;
    return dx * dx + dy * dy;
}

int InitialPropagationJump(const int width, const int height) noexcept
{
    const int maximumSide = std::max(width, height);
    int jump = 1;
    while (jump <= (maximumSide - 1) / 2)
    {
        jump *= 2;
    }
    return jump;
}

void PropagateNearest(std::vector<Color>& nearest, const int width, const int height)
{
    std::vector<Color> next(nearest.size());
    for (int jump = InitialPropagationJump(width, height); jump >= 1; jump /= 2)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                Color best = nearest[y * width + x];
                int bestDistance = DistanceSquared(best, x, y);
                for (int i = 0; i < 8; ++i)
                {
                    const int sx = x + jump * kDx[i];
                    const int sy = y + jump * kDy[i];
                    if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
                    const Color candidate = nearest[sy * width + sx];
                    const int distance = DistanceSquared(candidate, x, y);
                    if (distance < bestDistance)
                    {
                        best = candidate;
                        bestDistance = distance;
                    }
                }
                next[y * width + x] = best;
            }
        }
        nearest.swap(next);
    }
}

int StrokeLinks(const Mask& mask, const int x, const int y)
{
    int links = mask.Neighbours(x, y);
    for (int i = 1; i < 8; i += 2)
    {
        // At a square corner the diagonal is a shortcut, not a third branch.
        if (mask.At(x + kDx[i], y) || mask.At(x, y + kDy[i])) links &= ~(1 << i);
    }
    return links;
}

std::vector<Vector2> SmoothCenterlines(const Mask& mask, const std::vector<Color>& seeds)
{
    std::vector<Vector2> offsets(seeds.size());
    std::vector<Vector2> next(seeds.size());
    for (int pass = 0; pass < 8; ++pass)
    {
        for (std::size_t index = 0; index < seeds.size(); ++index)
        {
            const Color seed = seeds[index];
            if (!seed.a) continue;
            int count = 0;
            Vector2 sum {};
            for (int i = 0; i < 8; ++i)
            {
                if ((seed.b & (1 << i)) == 0) continue;
                const auto adjacent = static_cast<std::size_t>((seed.g + kDy[i]) * mask.width + seed.r + kDx[i]);
                sum.x += static_cast<float>(kDx[i]) + offsets[adjacent].x;
                sum.y += static_cast<float>(kDy[i]) + offsets[adjacent].y;
                ++count;
            }
            // Keep junctions and endpoints anchored and smooth only the interior of each stroke.
            if (count == 2)
            {
                next[index] = {0.5f * offsets[index].x + 0.25f * sum.x,
                               0.5f * offsets[index].y + 0.25f * sum.y};
            }
        }
        offsets.swap(next);
    }
    return offsets;
}

int EncodeOffset(const float offset) noexcept
{
    return std::clamp(static_cast<int>(std::lround(5.0f * offset)) + 8, 0, 15);
}

StrokeFieldPixels BuildFieldPixels(const Image& image, const int padding)
{
    if (!image.data || image.width <= 0 || image.height <= 0 || padding < 0)
    {
        return {};
    }

    Mask mask = ReadGlyphCoverage(image);
    Thin(mask);
    std::vector<Color> coreSeeds(mask.data.size(), Color {0, 0, 0, 0});
    bool hasInk = false;
    for (int y = 0; y < mask.height; ++y)
    {
        for (int x = 0; x < mask.width; ++x)
        {
            if (!mask.At(x, y)) continue;
            hasInk = true;
            coreSeeds[y * mask.width + x] = {
                static_cast<unsigned char>(x),
                static_cast<unsigned char>(y),
                static_cast<unsigned char>(StrokeLinks(mask, x, y)),
                255};
        }
    }
    if (!hasInk)
    {
        return {};
    }

    const std::vector<Vector2> offsets = SmoothCenterlines(mask, coreSeeds);
    StrokeFieldPixels field;
    field.width = mask.width + 2 * padding;
    field.height = mask.height + 2 * padding;
    field.coreWidth = mask.width;
    field.coreHeight = mask.height;
    field.padding = padding;
    field.pixels.assign(static_cast<std::size_t>(field.width * field.height), Color {0, 0, 0, 0});
    for (int y = 0; y < mask.height; ++y)
    {
        for (int x = 0; x < mask.width; ++x)
        {
            const Color& seed = coreSeeds[static_cast<std::size_t>(y * mask.width + x)];
            if (!seed.a)
            {
                continue;
            }

            field.pixels[static_cast<std::size_t>((y + padding) * field.width + x + padding)] = {
                static_cast<unsigned char>(x + padding),
                static_cast<unsigned char>(y + padding),
                seed.b,
                seed.a};
        }
    }

    PropagateNearest(field.pixels, field.width, field.height);
    for (Color& pixel : field.pixels)
    {
        const int seedX = static_cast<int>(pixel.r) - padding;
        const int seedY = static_cast<int>(pixel.g) - padding;
        if (seedX < 0 || seedY < 0 || seedX >= mask.width || seedY >= mask.height)
        {
            return {};
        }

        const Vector2& offset = offsets[static_cast<std::size_t>(seedY * mask.width + seedX)];
        pixel.a = static_cast<unsigned char>(EncodeOffset(offset.x) | (EncodeOffset(offset.y) << 4));
    }
    return field;
}

Texture2D LoadFieldTexture(const Image& image)
{
    StrokeFieldPixels field = BuildFieldPixels(image, 0);
    if (!field.IsValid())
    {
        return {};
    }

    return LoadTextureFromImage(
        Image {field.pixels.data(), field.width, field.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
}

void DrawGlyph(const Texture2D field, const Rectangle metrics, const GlyphInfo& glyph, const Vector2 pen,
               const float scale, const float radius, const Color color)
{
    const float width = metrics.width * scale;
    const float height = metrics.height * scale;
    if (width <= 0.0f || height <= 0.0f) return;
    const float pad = radius + 1.0f;
    const float u = pad * field.width / width;
    const float v = pad * field.height / height;
    const Rectangle src {-u, -v, field.width + 2.0f * u, field.height + 2.0f * v};
    const Rectangle dst {pen.x + glyph.offsetX * scale - pad, pen.y + glyph.offsetY * scale - pad,
                         width + 2.0f * pad, height + 2.0f * pad};
    DrawTexturePro(field, src, dst, {}, 0.0f, color);
}

bool AtlasPaddingSupportsGlyph(const Image& image,
                               const Rectangle metrics,
                               const float scale,
                               const float radius) noexcept
{
    const Vector2 fieldSize = FieldCoreSize(image);
    const float screenWidth = metrics.width * scale;
    const float screenHeight = metrics.height * scale;
    if (fieldSize.x <= 0.0f || fieldSize.y <= 0.0f ||
        screenWidth <= 0.0f || screenHeight <= 0.0f)
    {
        return false;
    }

    const float screenPadding = radius + 1.0f;
    const float horizontalFieldPadding = screenPadding * fieldSize.x / screenWidth;
    const float verticalFieldPadding = screenPadding * fieldSize.y / screenHeight;
    return std::isfinite(horizontalFieldPadding) && std::isfinite(verticalFieldPadding) &&
           horizontalFieldPadding <= static_cast<float>(kAtlasPadding) &&
           verticalFieldPadding <= static_cast<float>(kAtlasPadding);
}

void DrawAtlasGlyph(const Texture2D atlas,
                    const Rectangle core,
                    const Rectangle metrics,
                    const GlyphInfo& glyph,
                    const Vector2 pen,
                    const float scale,
                    const float radius,
                    const Color color)
{
    const float width = metrics.width * scale;
    const float height = metrics.height * scale;
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    const float screenPadding = radius + 1.0f;
    const float horizontalFieldPadding = screenPadding * core.width / width;
    const float verticalFieldPadding = screenPadding * core.height / height;
    const Rectangle source {
        core.x - horizontalFieldPadding,
        core.y - verticalFieldPadding,
        core.width + 2.0f * horizontalFieldPadding,
        core.height + 2.0f * verticalFieldPadding};
    const Rectangle destination {
        pen.x + glyph.offsetX * scale - screenPadding,
        pen.y + glyph.offsetY * scale - screenPadding,
        width + 2.0f * screenPadding,
        height + 2.0f * screenPadding};
    DrawTexturePro(atlas, source, destination, {}, 0.0f, color);
}

// Restores drawing state even if building a previously unseen glyph fails to allocate.
class StrokeDrawScope
{
public:
    StrokeDrawScope(const Shader shader, const int radiusLocation, const float radius,
                    const Vector2 anchor, const Vector2 origin, const float rotation)
    {
        BeginShaderMode(shader);
        SetShaderValue(shader, radiusLocation, &radius, SHADER_UNIFORM_FLOAT);
        rlPushMatrix();
        rlTranslatef(anchor.x, anchor.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
        rlTranslatef(-origin.x, -origin.y, 0.0f);
    }
    ~StrokeDrawScope()
    {
        rlPopMatrix();
        EndShaderMode();
    }
    StrokeDrawScope(const StrokeDrawScope&) = delete;
    StrokeDrawScope& operator=(const StrokeDrawScope&) = delete;
};

} // namespace

Font LoadDisplayFont(const char* path, const int pixelSize)
{

    // Rasterize outlines at display resolution instead of enlarging raylib's 32px point-filtered atlas.
    std::array<int, 191> codepoints {};
    for (int i = 0; i < 95; ++i) codepoints[i] = 32 + i;
    for (int i = 95; i < 191; ++i) codepoints[i] = 160 + i - 95;
    Font font = LoadFontEx(path, pixelSize, codepoints.data(), static_cast<int>(codepoints.size()));
    if (font.texture.id == GetFontDefault().texture.id) return {};
    if (font.texture.id)
    {
        for (int i = 0; i < font.glyphCount; ++i)
        {
            if (font.recs[i].width <= 0 && font.glyphs[i].advanceX == 0) font.glyphs[i].value = -1;
        }
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    }
    return font;
}

GlyphStrokeRenderer::~GlyphStrokeRenderer() { Clear(); }

void GlyphStrokeRenderer::Clear() noexcept
{
    if ((shader_.id || !legacyFields_.empty() || !atlasPages_.empty()) && IsWindowReady())
    {
        if (!legacyFields_.empty() || !atlasPages_.empty())
        {
            rlDrawRenderBatchActive();
        }
        for (const auto& entry : legacyFields_)
        {
            if (entry.second.id) UnloadTexture(entry.second);
        }
        for (const AtlasPage& page : atlasPages_)
        {
            if (page.texture.id) UnloadTexture(page.texture);
        }
        if (shader_.id && shader_.id != rlGetShaderIdDefault()) UnloadShader(shader_);
    }
    legacyFields_.clear();
    atlasGlyphs_.clear();
    atlasPages_.clear();
    glyphRunScratch_.clear();
    uniqueGlyphScratch_.clear();
    shader_ = {};
    glyphs_ = nullptr;
    fontTexture_ = 0U;
    radiusLocation_ = -1;
    attempted_ = false;
    atlasDrawCount_ = 0U;
    legacyDrawCount_ = 0U;
}

bool GlyphStrokeRenderer::Prepare(const Font& font)
{
    if (font.glyphs != glyphs_ || font.texture.id != fontTexture_)
    {
        Clear();
        glyphs_ = font.glyphs;
        fontTexture_ = font.texture.id;
    }
    if (!attempted_)
    {
        attempted_ = true;
        shader_ = LoadShaderFromMemory(nullptr, kStrokeShader);
        if (shader_.id == rlGetShaderIdDefault())
        {
            if (shader_.locs && shader_.locs != rlGetShaderLocsDefault()) MemFree(shader_.locs);
            shader_ = {};
        }
        radiusLocation_ = shader_.id ? GetShaderLocation(shader_, "radius") : -1;
    }
    return shader_.id && radiusLocation_ >= 0;
}

bool GlyphStrokeRenderer::BuildGlyphRun(const std::string& text, const Font& font)
{
    glyphRunScratch_.clear();
    glyphRunScratch_.reserve(text.size());
    for (std::size_t offset = 0U; offset < text.size() && text[offset];)
    {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text.c_str() + offset, &byteCount);
        offset += static_cast<std::size_t>(std::max(1, byteCount));
        if (codepoint == '\n')
        {
            glyphRunScratch_.push_back(GlyphRunItem {0, true, false, nullptr, {}});
            continue;
        }

        const int index = GetGlyphIndex(font, codepoint);
        if (index < 0 || index >= font.glyphCount)
        {
            glyphRunScratch_.clear();
            return false;
        }

        glyphRunScratch_.push_back(
            GlyphRunItem {index, false, codepoint != ' ' && codepoint != '\t', nullptr, {}});
    }

    return true;
}

bool GlyphStrokeRenderer::AtlasSupportsRun(const Font& font,
                                           const float scale,
                                           const float radius) const noexcept
{
    for (const GlyphRunItem& item : glyphRunScratch_)
    {
        if (!item.drawable)
        {
            continue;
        }

        if (!AtlasPaddingSupportsGlyph(
                font.glyphs[item.index].image,
                font.recs[item.index],
                scale,
                radius))
        {
            return false;
        }
    }
    return true;
}

bool GlyphStrokeRenderer::ResolveAtlasRun(const Font& font)
{
    for (GlyphRunItem& item : glyphRunScratch_)
    {
        if (!item.drawable)
        {
            continue;
        }

        item.atlasGlyph = ResolveAtlas(font, item.index);
        if (item.atlasGlyph == nullptr)
        {
            return false;
        }
    }
    return true;
}

bool GlyphStrokeRenderer::ResolveLegacyRun(const Font& font)
{
    uniqueGlyphScratch_.clear();
    uniqueGlyphScratch_.reserve(glyphRunScratch_.size());
    for (const GlyphRunItem& item : glyphRunScratch_)
    {
        if (item.drawable)
        {
            uniqueGlyphScratch_.push_back(item.index);
        }
    }
    std::sort(uniqueGlyphScratch_.begin(), uniqueGlyphScratch_.end());
    uniqueGlyphScratch_.erase(
        std::unique(uniqueGlyphScratch_.begin(), uniqueGlyphScratch_.end()),
        uniqueGlyphScratch_.end());
    if (uniqueGlyphScratch_.size() > kMaxCachedGlyphs)
    {
        return false;
    }

    std::size_t missingGlyphCount = 0U;
    for (const int index : uniqueGlyphScratch_)
    {
        if (legacyFields_.find(index) == legacyFields_.end())
        {
            ++missingGlyphCount;
        }
    }
    if (missingGlyphCount > kMaxCachedGlyphs - legacyFields_.size())
    {
        rlDrawRenderBatchActive();
        for (const auto& entry : legacyFields_)
        {
            if (entry.second.id)
            {
                UnloadTexture(entry.second);
            }
        }
        legacyFields_.clear();
    }

    for (const int index : uniqueGlyphScratch_)
    {
        if (!ResolveLegacy(font, index).id)
        {
            return false;
        }
    }
    for (GlyphRunItem& item : glyphRunScratch_)
    {
        if (item.drawable)
        {
            item.legacyField = legacyFields_.find(item.index)->second;
        }
    }
    return true;
}

const GlyphStrokeRenderer::AtlasGlyph* GlyphStrokeRenderer::ResolveAtlas(const Font& font,
                                                                         const int index)
{
    const auto existing = atlasGlyphs_.find(index);
    if (existing != atlasGlyphs_.end())
    {
        return &existing->second;
    }
    if (atlasGlyphs_.size() >= kMaxCachedGlyphs)
    {
        return nullptr;
    }

    StrokeFieldPixels field = BuildFieldPixels(font.glyphs[index].image, kAtlasPadding);
    if (!field.IsValid() || field.width > kAtlasSide || field.height > kAtlasSide)
    {
        return nullptr;
    }

    std::size_t pageIndex = 0U;
    int atlasX = 0;
    int atlasY = 0;
    if (!TryPlaceAtlasRegion(field.width, field.height, pageIndex, atlasX, atlasY))
    {
        return nullptr;
    }

    for (Color& pixel : field.pixels)
    {
        const int nearestX = static_cast<int>(pixel.r) + atlasX;
        const int nearestY = static_cast<int>(pixel.g) + atlasY;
        if (nearestX >= kAtlasSide || nearestY >= kAtlasSide)
        {
            return nullptr;
        }
        pixel.r = static_cast<unsigned char>(nearestX);
        pixel.g = static_cast<unsigned char>(nearestY);
    }

    AtlasPage& page = atlasPages_[pageIndex];
    UpdateTextureRec(
        page.texture,
        Rectangle {
            static_cast<float>(atlasX),
            static_cast<float>(atlasY),
            static_cast<float>(field.width),
            static_cast<float>(field.height)},
        field.pixels.data());

    AtlasGlyph glyph;
    glyph.pageIndex = pageIndex;
    glyph.core = Rectangle {
        static_cast<float>(atlasX + field.padding),
        static_cast<float>(atlasY + field.padding),
        static_cast<float>(field.coreWidth),
        static_cast<float>(field.coreHeight)};
    return &atlasGlyphs_.emplace(index, glyph).first->second;
}

Texture2D GlyphStrokeRenderer::ResolveLegacy(const Font& font, const int index)
{
    const auto existing = legacyFields_.find(index);
    if (existing != legacyFields_.end())
    {
        return existing->second;
    }
    if (legacyFields_.size() >= kMaxCachedGlyphs)
    {
        return {};
    }

    const Texture2D field = LoadFieldTexture(font.glyphs[index].image);
    if (!field.id)
    {
        return {};
    }

    try
    {
        return legacyFields_.emplace(index, field).first->second;
    }
    catch (...)
    {
        UnloadTexture(field);
        throw;
    }
}

bool GlyphStrokeRenderer::TryPlaceAtlasRegion(const int width,
                                              const int height,
                                              std::size_t& pageIndex,
                                              int& x,
                                              int& y)
{
    if (width <= 0 || height <= 0 || width > kAtlasSide || height > kAtlasSide)
    {
        return false;
    }

    for (std::size_t index = 0U; index < atlasPages_.size(); ++index)
    {
        AtlasPage& page = atlasPages_[index];
        int candidateX = page.nextX;
        int candidateY = page.nextY;
        int candidateRowHeight = page.rowHeight;
        if (candidateX + width > kAtlasSide)
        {
            candidateX = 0;
            candidateY += candidateRowHeight;
            candidateRowHeight = 0;
        }
        if (candidateY + height > kAtlasSide)
        {
            continue;
        }

        x = candidateX;
        y = candidateY;
        pageIndex = index;
        page.nextX = candidateX + width;
        page.nextY = candidateY;
        page.rowHeight = std::max(candidateRowHeight, height);
        return true;
    }

    if (!CreateAtlasPage())
    {
        return false;
    }

    AtlasPage& page = atlasPages_.back();
    pageIndex = atlasPages_.size() - 1U;
    x = 0;
    y = 0;
    page.nextX = width;
    page.rowHeight = height;
    return true;
}

bool GlyphStrokeRenderer::CreateAtlasPage()
{
    if (atlasPages_.size() >= kMaxAtlasPages)
    {
        return false;
    }

    Image emptyAtlas = GenImageColor(kAtlasSide, kAtlasSide, Color {0, 0, 0, 0});
    if (!emptyAtlas.data)
    {
        return false;
    }
    const Texture2D texture = LoadTextureFromImage(emptyAtlas);
    UnloadImage(emptyAtlas);
    if (!texture.id)
    {
        return false;
    }

    try
    {
        if (atlasPages_.empty())
        {
            atlasPages_.reserve(kMaxAtlasPages);
            atlasGlyphs_.reserve(kMaxCachedGlyphs);
            legacyFields_.reserve(kMaxCachedGlyphs);
        }
        atlasPages_.push_back(AtlasPage {texture, 0, 0, 0});
    }
    catch (...)
    {
        UnloadTexture(texture);
        throw;
    }
    return true;
}

void GlyphStrokeRenderer::DrawAtlasRun(const Font& font,
                                       const float scale,
                                       const float spacing,
                                       const float lineStep,
                                       const float radius,
                                       const Color color) const
{
    Vector2 pen {};
    for (const GlyphRunItem& item : glyphRunScratch_)
    {
        if (item.lineBreak)
        {
            pen.x = 0.0f;
            pen.y += lineStep;
            continue;
        }

        const GlyphInfo& glyph = font.glyphs[item.index];
        const Rectangle metrics = font.recs[item.index];
        if (item.drawable && item.atlasGlyph != nullptr &&
            item.atlasGlyph->pageIndex < atlasPages_.size())
        {
            DrawAtlasGlyph(
                atlasPages_[item.atlasGlyph->pageIndex].texture,
                item.atlasGlyph->core,
                metrics,
                glyph,
                pen,
                scale,
                radius,
                color);
        }
        const float advance = glyph.advanceX ? static_cast<float>(glyph.advanceX) : metrics.width;
        pen.x += advance * scale + spacing;
    }
}

void GlyphStrokeRenderer::DrawLegacyRun(const Font& font,
                                        const float scale,
                                        const float spacing,
                                        const float lineStep,
                                        const float radius,
                                        const Color color) const
{
    Vector2 pen {};
    for (const GlyphRunItem& item : glyphRunScratch_)
    {
        if (item.lineBreak)
        {
            pen.x = 0.0f;
            pen.y += lineStep;
            continue;
        }

        const GlyphInfo& glyph = font.glyphs[item.index];
        const Rectangle metrics = font.recs[item.index];
        if (item.drawable && item.legacyField.id)
        {
            DrawGlyph(item.legacyField, metrics, glyph, pen, scale, radius, color);
        }
        const float advance = glyph.advanceX ? static_cast<float>(glyph.advanceX) : metrics.width;
        pen.x += advance * scale + spacing;
    }
}

bool GlyphStrokeRenderer::Draw(const std::string& text,
                               const Font& font,
                               const float size,
                               const float spacing,
                               const Vector2 anchor,
                               const Vector2 origin,
                               const float rotation,
                               const Color color,
                               const float width)
{
    if (!std::isfinite(width) || !std::isfinite(size) || size <= 0.0f)
    {
        return false;
    }
    if (!font.glyphs || !font.recs || font.glyphCount <= 0 || font.baseSize <= 0 || !Prepare(font) ||
        !BuildGlyphRun(text, font))
    {
        return false;
    }

    const float radius = 0.5f * width;
    const float scale = size / static_cast<float>(font.baseSize);
    float lineStep = 0.0f;
    for (const GlyphRunItem& item : glyphRunScratch_)
    {
        if (item.lineBreak)
        {
            lineStep = MeasureTextEx(font, "M\nM", size, spacing).y -
                       MeasureTextEx(font, "M", size, spacing).y;
            break;
        }
    }

    if (AtlasSupportsRun(font, scale, radius) && ResolveAtlasRun(font))
    {
        const StrokeDrawScope drawScope(shader_, radiusLocation_, radius, anchor, origin, rotation);
        DrawAtlasRun(font, scale, spacing, lineStep, radius, color);
        ++atlasDrawCount_;
        return true;
    }

    if (!ResolveLegacyRun(font))
    {
        return false;
    }
    const StrokeDrawScope drawScope(shader_, radiusLocation_, radius, anchor, origin, rotation);
    DrawLegacyRun(font, scale, spacing, lineStep, radius, color);
    ++legacyDrawCount_;
    return true;
}

GlyphStrokeRenderer::Stats GlyphStrokeRenderer::CacheStats() const noexcept
{
    return Stats {
        atlasDrawCount_,
        legacyDrawCount_,
        atlasPages_.size(),
        atlasGlyphs_.size(),
        legacyFields_.size()};
}
} // namespace mfd
