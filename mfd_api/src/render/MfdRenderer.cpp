/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/render/MfdRenderer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <raylib.h>

#include "mfd/render/Canvas2D.h"
#include "mfd/render/RenderTextureUtils.h"

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

bool ActiveSceneUsesReticleClipping(const SceneRegistry& scene)
{
    for (const ReticleRenderView& reticle : scene.CollectActiveReticleViews())
    {
        if (reticle.group != nullptr && reticle.visible && ResolveClipPrimitive(*reticle.group) != nullptr)
        {
            return true;
        }
    }

    return false;
}

void DrawActivePageContent(const SceneRegistry& scene,
                           const int width,
                           const int height,
                           const Font* textFont,
                           const bool clippingEnabled)
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
        clippingEnabled);

    for (const ReticleRenderView& reticle : scene.CollectActiveReticleViews())
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

    ~Impl()
    {
        Release();
    }

    void ResetTextFont() noexcept
    {
        if (textFontReady)
        {
            UnloadFont(textFont);
            textFont = {};
            textFontReady = false;
        }

        textFontLoadAttempted = false;
    }

    void Release() noexcept
    {
        ResetTextFont();

        if (renderTargetReady)
        {
            UnloadRenderTexture(renderTarget);
            renderTarget = {};
            renderTargetReady = false;
            renderTargetStencilReady = false;
        }

        if (shaderReady)
        {
            UnloadShader(shader);
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
    if (screenWidth <= 0 || screenHeight <= 0)
    {
        return;
    }

    const WindowDisplayState display = scene.WindowDisplay();
    if (display.disabled)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
        return;
    }

    const auto activePage = scene.ActivePageSummary();
    if (!activePage.has_value())
    {
        return;
    }

    if (impl_ != nullptr)
    {
        (void)impl_->EnsureTextFont();
    }

    const Font* textFont = impl_ == nullptr ? nullptr : impl_->ActiveTextFont();
    const bool needsRenderTarget = NeedsWindowPostProcess(display) || ActiveSceneUsesReticleClipping(scene);
    if (!needsRenderTarget)
    {
        DrawActivePageContent(scene, screenWidth, screenHeight, textFont, false);
        return;
    }

    if (impl_ == nullptr || !impl_->EnsureShader() || !impl_->EnsureRenderTarget(screenWidth, screenHeight))
    {
        DrawActivePageContent(scene, screenWidth, screenHeight, textFont, false);

        if (display.brightness < 0.9995f)
        {
            const std::uint8_t overlayAlpha =
                static_cast<std::uint8_t>(std::clamp(1.0f - display.brightness, 0.0f, 1.0f) * 255.0f + 0.5f);
            DrawRectangle(0, 0, screenWidth, screenHeight, Color {0, 0, 0, overlayAlpha});
        }

        return;
    }

    BeginTextureMode(impl_->renderTarget);
    ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
    DrawActivePageContent(scene, screenWidth, screenHeight, textFont, impl_->renderTargetStencilReady);
    EndTextureMode();

    const Rectangle source {
        0.0f,
        0.0f,
        static_cast<float>(impl_->renderTarget.texture.width),
        -static_cast<float>(impl_->renderTarget.texture.height)};
    const Rectangle destination {0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight)};

    if (!NeedsWindowPostProcess(display))
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
