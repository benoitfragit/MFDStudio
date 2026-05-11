/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for MfdRenderer.
 */

#include "mfd/render/MfdRenderer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <raylib.h>

#include "Canvas2D.h"
#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "RenderTextureUtils.h"

namespace mfd
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

void ApplyBilinearFilterToFont(const Font font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    }
}

constexpr const char* kWindowDisplayFragmentShader = R"glsl(
    #version 330

    in vec2 fragTexCoord;
    in vec4 fragColor;

    out vec4 finalColor;

    uniform sampler2D texture0;
    uniform vec4 colDiffuse;
    uniform float brightness;
    uniform float invertColors;

    void main()
    {
        vec4 color = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
        color.rgb = mix(color.rgb, vec3(1.0) - color.rgb, clamp(invertColors, 0.0, 1.0));
        color.rgb *= clamp(brightness, 0.0, 1.0);
        finalColor = color;
    }
)glsl";

bool NeedsWindowPostProcess(const WindowDisplayState& display) noexcept
{
    return display.invertColors || display.brightness < 0.9995f;
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
                           const int width,
                           const int height,
                           const Font* textFont,
                           const bool clippingEnabled,
                           BezierPolylineCache* bezierCache,
                           ImageTextureCache* imageCache)
{
    const auto activePage = scene.ActivePageSummary();
    if (!activePage.has_value())
    {
        return;
    }

    Canvas2D canvas(
        width,
        height,
        scene.ActivePageView(),
        textFont,
        ToRayColor(scene.ActiveBackgroundColor()),
        clippingEnabled,
        bezierCache,
        imageCache);

    for (const ReticleRenderView& reticle : activeReticles)
    {
        if (reticle.group != nullptr)
        {
            canvas.DrawReticle(*reticle.group, reticle.visible);
        }
    }

    const std::string title =
        activePage->title == activePage->name
            ? activePage->title
            : activePage->title + " [" + activePage->name + "]";
    DrawTextEx(ResolveTextFont(textFont),
               title.c_str(),
               Vector2 {18.0f, 18.0f},
               22.0f,
               1.0f,
               Color {220, 236, 220, 255});
}
} // namespace

struct MfdRenderer::Impl
{
    RenderTexture2D renderTarget {};
    Shader shader {};
    bool renderTargetReady = false;
    bool renderTargetStencilReady = false;
    bool shaderReady = false;
    int brightnessLocation = -1;
    int invertColorsLocation = -1;
    std::filesystem::path fontFile {};
    Font textFont {};
    bool textFontReady = false;
    bool textFontLoadAttempted = false;
    std::vector<ReticleRenderView> activeReticlesScratch {};
    BezierPolylineCache bezierCache {};
    ImageTextureCache imageCache {};

    ~Impl()
    {
        Release();
    }

    void ResetTextFont() noexcept
    {
        const bool windowReady = IsWindowReady();
        if (textFontReady)
        {
            if (windowReady)
            {
                UnloadFont(textFont);
            }
            textFont = {};
            textFontReady = false;
        }

        textFontLoadAttempted = false;
    }

    void Release() noexcept
    {
        bezierCache.Clear();
        imageCache.Clear();
        ResetTextFont();
        const bool windowReady = IsWindowReady();

        if (renderTargetReady)
        {
            if (windowReady)
            {
                UnloadRenderTexture(renderTarget);
            }
            renderTarget = {};
            renderTargetReady = false;
            renderTargetStencilReady = false;
        }

        if (shaderReady)
        {
            if (windowReady)
            {
                UnloadShader(shader);
            }
            shader = {};
            shaderReady = false;
            brightnessLocation = -1;
            invertColorsLocation = -1;
        }
    }

    bool EnsureRenderTarget(const int width, const int height)
    {
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        if (renderTargetReady &&
            renderTarget.texture.width == width &&
            renderTarget.texture.height == height)
        {
            return true;
        }

        if (renderTargetReady)
        {
            UnloadRenderTexture(renderTarget);
            renderTarget = {};
            renderTargetReady = false;
            renderTargetStencilReady = false;
        }

        renderTargetStencilReady = false;
        renderTarget = LoadRenderTextureWithStencil(width, height, &renderTargetStencilReady);
        renderTargetReady = renderTarget.id != 0;
        return renderTargetReady;
    }

    bool EnsureShader()
    {
        if (shaderReady)
        {
            return true;
        }

        shader = LoadShaderFromMemory(nullptr, kWindowDisplayFragmentShader);
        if (shader.id == 0)
        {
            return false;
        }

        brightnessLocation = GetShaderLocation(shader, "brightness");
        invertColorsLocation = GetShaderLocation(shader, "invertColors");
        shaderReady = brightnessLocation >= 0 && invertColorsLocation >= 0;

        if (!shaderReady)
        {
            UnloadShader(shader);
            shader = {};
            brightnessLocation = -1;
            invertColorsLocation = -1;
        }

        return shaderReady;
    }

    void SetTextFontFile(std::filesystem::path path)
    {
        if (!path.empty())
        {
            path = path.lexically_normal();
        }

        if (fontFile == path)
        {
            return;
        }

        ResetTextFont();
        fontFile = std::move(path);
    }

    bool EnsureTextFont()
    {
        if (fontFile.empty())
        {
            return true;
        }

        if (textFontReady)
        {
            return true;
        }

        if (textFontLoadAttempted)
        {
            return false;
        }

        textFontLoadAttempted = true;
        const std::string path = fontFile.string();
        Font loadedFont = LoadFont(path.c_str());
        if (loadedFont.texture.id == 0)
        {
            return false;
        }

        ApplyBilinearFilterToFont(loadedFont);
        textFont = loadedFont;
        textFontReady = true;
        return true;
    }

    const Font* ActiveTextFont() const noexcept
    {
        return textFontReady ? &textFont : nullptr;
    }
};

MfdRenderer::MfdRenderer()
    : impl_(std::make_unique<Impl>())
{
}

MfdRenderer::~MfdRenderer() = default;

MfdRenderer::MfdRenderer(MfdRenderer&&) noexcept = default;

MfdRenderer& MfdRenderer::operator=(MfdRenderer&&) noexcept = default;

void MfdRenderer::SetTextFontFile(std::filesystem::path fontFile)
{
    if (impl_ != nullptr)
    {
        impl_->SetTextFontFile(std::move(fontFile));
    }
}

void MfdRenderer::DrawActivePage(const SceneRegistry& scene)
{
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    DrawActivePage(scene, screenWidth, screenHeight);
}

void MfdRenderer::DrawActivePage(const SceneRegistry& scene, const int viewportWidth, const int viewportHeight)
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    if (screenWidth <= 0 || screenHeight <= 0)
    {
        return;
    }

    const WindowDisplayState display = scene.WindowDisplay();
    if (display.disabled)
    {
        DrawRectangle(0, 0, viewportWidth, viewportHeight, BLACK);
        return;
    }

    const auto activePage = scene.ActivePageSummary();
    if (!activePage.has_value())
    {
        return;
    }

    if (impl_ != nullptr)
    {
        impl_->EnsureTextFont();
    }

    const Font* textFont = impl_ == nullptr ? nullptr : impl_->ActiveTextFont();
    ApplyBilinearFilterToFont(ResolveTextFont(textFont));
    std::vector<ReticleRenderView> fallbackActiveReticles;
    const std::vector<ReticleRenderView>* activeReticles = nullptr;
    if (impl_ != nullptr)
    {
        scene.CollectActiveReticleViews(impl_->activeReticlesScratch);
        activeReticles = &impl_->activeReticlesScratch;
    }
    else
    {
        fallbackActiveReticles = scene.CollectActiveReticleViews();
        activeReticles = &fallbackActiveReticles;
    }

    const bool usesFullScreenViewport = viewportWidth == screenWidth && viewportHeight == screenHeight;
    const bool needsRenderTarget =
        !usesFullScreenViewport ||
        NeedsWindowPostProcess(display) ||
        ActiveReticleViewsUseClipping(*activeReticles);
    if (!needsRenderTarget)
    {
        DrawActivePageContent(
            scene,
            *activeReticles,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache);
        return;
    }

    const bool postProcessNeeded = NeedsWindowPostProcess(display);
    const bool renderTargetReady =
        impl_ != nullptr && impl_->EnsureRenderTarget(viewportWidth, viewportHeight);
    const bool shaderReady = !postProcessNeeded || (impl_ != nullptr && impl_->EnsureShader());
    if (!renderTargetReady || !shaderReady)
    {
        DrawActivePageContent(
            scene,
            *activeReticles,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache);

        if (display.brightness < 0.9995f)
        {
            const std::uint8_t overlayAlpha =
                static_cast<std::uint8_t>(std::clamp(1.0f - display.brightness, 0.0f, 1.0f) * 255.0f + 0.5f);
            DrawRectangle(0, 0, viewportWidth, viewportHeight, Color {0, 0, 0, overlayAlpha});
        }

        return;
    }

    BeginTextureMode(impl_->renderTarget);
    ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
    DrawActivePageContent(
        scene,
        *activeReticles,
        viewportWidth,
        viewportHeight,
        textFont,
        impl_->renderTargetStencilReady,
        &impl_->bezierCache,
        &impl_->imageCache);
    EndTextureMode();

    const Rectangle source {
        0.0f,
        0.0f,
        static_cast<float>(impl_->renderTarget.texture.width),
        -static_cast<float>(impl_->renderTarget.texture.height)};
    const Rectangle destination {0.0f, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight)};

    if (!postProcessNeeded)
    {
        DrawTexturePro(impl_->renderTarget.texture, source, destination, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }

    const float brightness = std::clamp(display.brightness, 0.0f, 1.0f);
    const float invertColors = display.invertColors ? 1.0f : 0.0f;
    SetShaderValue(impl_->shader, impl_->brightnessLocation, &brightness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(impl_->shader, impl_->invertColorsLocation, &invertColors, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(impl_->shader);
    DrawTexturePro(impl_->renderTarget.texture, source, destination, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
    EndShaderMode();
}
} // namespace mfd
