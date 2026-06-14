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
#include <cmath>
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
#include "TextLayoutCache.h"

namespace mfd
{
namespace
{
Color ToRayColor(const ColorRgba& color) noexcept
{
    return Color {color.r, color.g, color.b, color.a};
}

std::uint8_t ClampUnitFloatToByte(const float value) noexcept
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

void ApplyPointFilterToFont(const Font& font) noexcept
{
    if (font.texture.id != 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
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

bool HasSamePageTitleDisplay(const PageTitleDisplayDefinition& lhs, const PageTitleDisplayDefinition& rhs) noexcept
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

class TextureModeScope
{
public:
    explicit TextureModeScope(const RenderTexture2D& target) noexcept
        : active_(true)
    {
        BeginTextureMode(target);
    }

    ~TextureModeScope()
    {
        if (active_)
        {
            EndTextureMode();
        }
    }

    TextureModeScope(const TextureModeScope&) = delete;
    TextureModeScope& operator=(const TextureModeScope&) = delete;

private:
    bool active_ = false;
};

class ShaderModeScope
{
public:
    explicit ShaderModeScope(const Shader shader) noexcept
        : active_(true)
    {
        BeginShaderMode(shader);
    }

    ~ShaderModeScope()
    {
        if (active_)
        {
            EndShaderMode();
        }
    }

    ShaderModeScope(const ShaderModeScope&) = delete;
    ShaderModeScope& operator=(const ShaderModeScope&) = delete;

private:
    bool active_ = false;
};

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
    Canvas2D canvas(
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

struct MfdRenderer::Impl
{
    struct TitleReticleCache
    {
        std::string pageName {};
        std::string pageTitle {};
        PageTitleDisplayDefinition display {};
        ReticleGroup reticle {};
        bool valid = false;
    };

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
    bool defaultFontFilterApplied = false;
    bool invertColorsFallbackWarned = false;
    std::vector<ReticleRenderView> activeReticlesScratch {};
    BezierPolylineCache bezierCache {};
    ImageTextureCache imageCache {};
    TextLayoutCache textLayoutCache {};
    TitleReticleCache titleReticleCache {};

    ~Impl()
    {
        Release();
    }

    void ResetTextFont() noexcept
    {
        textLayoutCache.Clear();
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

        ApplyPointFilterToFont(loadedFont);
        textFont = loadedFont;
        textFontReady = true;
        return true;
    }

    const Font* ActiveTextFont() const noexcept
    {
        return textFontReady ? &textFont : nullptr;
    }

    void WarnInvertColorsFallbackOnce() noexcept
    {
        // The brightness fallback overlay cannot reproduce colour inversion, so signal once that a
        // requested invertColors could not be honoured when the render target or shader is unavailable.
        if (invertColorsFallbackWarned)
        {
            return;
        }

        TraceLog(LOG_WARNING,
                 "MFD: window post-process unavailable; invertColors requested but not applied (brightness fallback only).");
        invertColorsFallbackWarned = true;
    }

    void EnsureDefaultFontPointFilter() noexcept
    {
        // The loaded font receives the point filter once at load time. The default font, used when no
        // custom font is ready, has no load step here, so apply its filter exactly once instead of
        // repeating the GL texture-parameter call every frame.
        if (defaultFontFilterApplied)
        {
            return;
        }

        ApplyPointFilterToFont(GetFontDefault());
        defaultFontFilterApplied = true;
    }

    const ReticleGroup& ResolveTitleReticle(const PageSummary& activePage)
    {
        if (!titleReticleCache.valid ||
            titleReticleCache.pageName != activePage.name ||
            titleReticleCache.pageTitle != activePage.title ||
            !HasSamePageTitleDisplay(titleReticleCache.display, activePage.titleDisplay))
        {
            titleReticleCache.pageName = activePage.name;
            titleReticleCache.pageTitle = activePage.title;
            titleReticleCache.display = activePage.titleDisplay;
            titleReticleCache.reticle =
                BuildPageTitleDisplayReticle(activePage.name, activePage.title, activePage.titleDisplay);
            titleReticleCache.valid = true;
        }

        return titleReticleCache.reticle;
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

    ReticleGroup fallbackTitleReticle {};
    const ReticleGroup* titleReticle = nullptr;
    if (impl_ != nullptr)
    {
        titleReticle = &impl_->ResolveTitleReticle(*activePage);
    }
    else
    {
        fallbackTitleReticle = BuildPageTitleDisplayReticle(activePage->name, activePage->title, activePage->titleDisplay);
        titleReticle = &fallbackTitleReticle;
    }

    const Font* textFont = impl_ == nullptr ? nullptr : impl_->ActiveTextFont();
    if (impl_ != nullptr)
    {
        if (textFont == nullptr)
        {
            impl_->EnsureDefaultFontPointFilter();
        }
    }
    else
    {
        ApplyPointFilterToFont(GetFontDefault());
    }
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
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache,
            impl_ == nullptr ? nullptr : &impl_->textLayoutCache);
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
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache,
            impl_ == nullptr ? nullptr : &impl_->textLayoutCache);

        if (display.brightness < 0.9995f)
        {
            const std::uint8_t overlayAlpha = ClampUnitFloatToByte(1.0f - display.brightness);
            DrawRectangle(0, 0, viewportWidth, viewportHeight, Color {0, 0, 0, overlayAlpha});
        }

        if (display.invertColors && impl_ != nullptr)
        {
            impl_->WarnInvertColorsFallbackOnce();
        }

        return;
    }

    {
        TextureModeScope textureMode(impl_->renderTarget);
        ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
        DrawActivePageContent(
            scene,
            *activeReticles,
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            impl_->renderTargetStencilReady,
            &impl_->bezierCache,
            &impl_->imageCache,
            &impl_->textLayoutCache);
    }

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

    {
        ShaderModeScope shaderMode(impl_->shader);
        DrawTexturePro(impl_->renderTarget.texture, source, destination, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
    }
}
} // namespace mfd
