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
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <raylib.h>

#include "Canvas2D.h"
#include "BezierPolylineCache.h"
#include "ContainerDecorationRenderer.h"
#include "ContainerPointerQuery.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/widgets/WidgetOcclusion.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"
#include "mfd/apilevel1/widgets/WidgetRuntimeHost.h"
#include "mfd/apilevel1/widgets/WidgetInputController.h"
#include "ImageTextureCache.h"
#include "ExternalStreamRegistry.h"
#include "RenderWorkBudget.h"
#include "RenderTextureUtils.h"
#include "OpenGlCompat.h"
#include "TextLayoutCache.h"
#include "mfd/apilevel1/ContainerRuntimeHost.h"
#include "mfd/apilevel1/ContainerInputRouter.h"

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

Rectangle ToScreenRect(const api1::ContainerRect& rect,
                       const PageViewState& view,
                       const int width,
                       const int height) noexcept
{
    const Vec2 center = ApplyPageView(rect.center, view);
    const float scale = static_cast<float>(std::min(width, height)) * 0.5f * SanitizeZoom(view.zoom);
    const float pixelWidth = std::max(0.0f, rect.size.x * scale);
    const float pixelHeight = std::max(0.0f, rect.size.y * scale);
    return {static_cast<float>(width) * 0.5f + center.x * static_cast<float>(std::min(width, height)) * 0.5f - pixelWidth * 0.5f,
            static_cast<float>(height) * 0.5f - center.y * static_cast<float>(std::min(width, height)) * 0.5f - pixelHeight * 0.5f,
            pixelWidth,
            pixelHeight};
}

bool Contains(const Rectangle rectangle, const Vector2 point) noexcept
{
    return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width &&
           point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
}

/**
 * @brief Mutable restoration context updated before each runtime reticle draw call.
 *
 * The single Canvas2D restore callback consults this context so it can decide, for the reticle
 * currently clipping, whether to keep the historical flat-background restore or to additionally
 * repaint the visible reticles of strictly earlier layers.
 */
bool SampleWidgetOcclusionImage(void* context, const ImageGeometry& image, const Vec2 uv) noexcept
{
    auto* cache = static_cast<ImageTextureCache*>(context);
    try { return cache->ContainsOpaquePixel(image.file, uv.x, uv.y); }
    catch (const std::exception&) { return true; }
}

struct LayerLocalRestoreContext
{
    const Canvas2D* canvas = nullptr;
    const std::vector<ReticleRenderView>* reticles = nullptr;
    const std::vector<api1::ContainerDefinition>* containerDefinitions = nullptr;
    const std::vector<api1::ContainerLayoutNode>* containerLayouts = nullptr;
    const std::vector<ReticleGroup>* widgetVisuals = nullptr;
    std::size_t firstWidget = 0U;
    PageViewState view {};
    detail::ContainerDecorationPointerState pointer {};
    Color background = BLACK;
    int width = 0;
    int height = 0;
    std::size_t currentLayerOrder = 0;
    bool eraseLayerOnly = false;
    detail::RenderWorkBudget restoreVisitBudget {detail::kMaxLayerRestoreVisits};
};

void DrawWidgetLeaf(const LayerLocalRestoreContext& context, const std::size_t sourceIndex)
{
    if (context.canvas == nullptr || context.widgetVisuals == nullptr || sourceIndex < context.firstWidget) return;
    const std::size_t index = sourceIndex - context.firstWidget;
    if (index < context.widgetVisuals->size()) context.canvas->DrawReticleWithoutClipping((*context.widgetVisuals)[index], (*context.widgetVisuals)[index].visible);
}

void RestoreLayerLocalBackground(LayerLocalRestoreContext& context)
{
    DrawRectangle(0, 0, context.width, context.height, context.background);
    if (context.containerDefinitions != nullptr && context.containerLayouts != nullptr)
    {
        detail::DrawContainerDecorations(
            *context.containerDefinitions,
            *context.containerLayouts,
            context.view,
            context.width,
            context.height,
            context.pointer, [&context](const std::size_t index) { DrawWidgetLeaf(context, index); });
    }
    if (!context.eraseLayerOnly || context.canvas == nullptr || context.reticles == nullptr)
    {
        return;
    }

    for (const ReticleRenderView& view : *context.reticles)
    {
        if (!context.restoreVisitBudget.TryConsume())
        {
            break;
        }

        if (view.group != nullptr && view.visible && view.layerOrder < context.currentLayerOrder)
        {
            context.canvas->DrawReticleWithoutClipping(*view.group, view.visible);
        }
    }
}

void DrawActivePageContent(const SceneRegistry& scene,
                           const std::vector<api1::ContainerDefinition>& containerDefinitions,
                           const std::vector<api1::ContainerLayoutNode>& containerLayouts,
                           const std::vector<ReticleGroup>& widgetVisuals,
                           const std::size_t firstWidget,
                           const std::vector<ReticleRenderView>& activeReticles,
                           const ReticleGroup& titleReticle,
                           const int width,
                           const int height,
                           const Font* textFont,
                           const bool clippingEnabled,
                           const std::size_t activeScrollbarSourceIndex,
                           const bool activeScrollbarHorizontal,
                           BezierPolylineCache* bezierCache,
                           ImageTextureCache* imageCache,
                           TextLayoutCache* textLayoutCache,
                           ExternalStreamRegistry* externalStreams)
{
    const Vector2 mouse = GetMousePosition();
    const detail::ContainerDecorationPointerState containerPointer {
        {mouse.x, mouse.y},
        true,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        activeScrollbarSourceIndex,
        activeScrollbarHorizontal};
    LayerLocalRestoreContext restoreContext;
    restoreContext.reticles = &activeReticles;
    restoreContext.widgetVisuals = &widgetVisuals;
    restoreContext.firstWidget = firstWidget;
    restoreContext.containerDefinitions = &containerDefinitions;
    restoreContext.containerLayouts = &containerLayouts;
    restoreContext.view = scene.ActivePageView();
    restoreContext.pointer = containerPointer;
    restoreContext.background = ToRayColor(scene.ActiveBackgroundColor());
    restoreContext.width = width;
    restoreContext.height = height;

    Canvas2D::BackgroundRestoreCallback restore;
    if (clippingEnabled && ActiveReticleViewsUseClipping(activeReticles))
    {
        restore = [&restoreContext]()
        {
            RestoreLayerLocalBackground(restoreContext);
        };
    }

    Canvas2D canvas(
        width,
        height,
        scene.ActivePageView(),
        textFont,
        restoreContext.background,
        clippingEnabled,
        bezierCache,
        imageCache,
        textLayoutCache,
        std::move(restore), detail::kMaxCanvasDrawWorkUnits, externalStreams);
    restoreContext.canvas = &canvas;
    detail::DrawContainerDecorations(
        containerDefinitions,
        containerLayouts,
        scene.ActivePageView(),
        width,
        height,
        containerPointer, [&restoreContext](const std::size_t index) { DrawWidgetLeaf(restoreContext, index); });


    for (const ReticleRenderView& reticle : activeReticles)
    {
        if (reticle.group != nullptr)
        {
            restoreContext.currentLayerOrder = reticle.layerOrder;
            restoreContext.eraseLayerOnly = reticle.group->clipping.eraseLayerOnly;
            canvas.DrawReticle(*reticle.group, reticle.visible);
        }
    }

    // The title chrome is not part of a page layer: keep the historical flat-background restore.
    restoreContext.eraseLayerOnly = false;
    canvas.DrawReticle(titleReticle);
}
} // namespace

struct MfdRenderer::Impl
{
    struct ThumbDragState
    {
        std::string pageName;
        std::string containerId;
        std::size_t sourceIndex = 0U;
        float pointerToCenter = 0.0f;
        bool horizontal = false;
        bool active = false;
    };

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
    std::vector<api1::ContainerDefinition> containerDefinitionsScratch {};
    std::vector<api1::ContainerLayoutNode> containerLayoutsScratch {};
    api1::ContainerRuntimeHost containerRuntime {};
    api1::WidgetRuntimeHost widgetRuntime {};
    api1::WidgetInputController widgetInput {};
    std::vector<api1::WidgetInstance> widgets;
    std::vector<api1::WidgetState> widgetStates;
    std::vector<ReticleGroup> widgetVisuals;
    std::size_t firstWidget = 0U;
    api1::ContainerInputRouter containerInputRouter {};
    ThumbDragState thumbDrag {};
    bool containerRuntimeLoadAttempted = false;
    std::string containerLayoutDiagnostic;
    BezierPolylineCache bezierCache {};
    ImageTextureCache imageCache {};
    ExternalStreamRegistry externalStreams {};
    TextLayoutCache textLayoutCache {};
    TitleReticleCache titleReticleCache {};

    void ResolveContainers(const SceneRegistry& scene)
    {
        scene.CollectActiveContainerDefinitions(containerDefinitionsScratch);
        firstWidget = containerDefinitionsScratch.size();
        scene.CollectActiveWidgets(widgets, widgetStates);
        widgetVisuals.clear();
        if (!widgets.empty())
        {
            std::string error;
            if ((!widgetRuntime.IsLoaded() && !widgetRuntime.LoadForProcess({}, error)) ||
                !api1::EvaluateWidgetLayout(containerDefinitionsScratch, widgets, containerLayoutsScratch, error))
            {
                containerLayoutsScratch.clear();
                if (containerLayoutDiagnostic != error) TraceLog(LOG_ERROR, "%s", error.c_str());
                containerLayoutDiagnostic = error;
                return;
            }
            auto nodes = api1::ProjectWidgetLayoutNodes(containerDefinitionsScratch, widgets);
            for (std::size_t index = 0; index < firstWidget; ++index) nodes[index].id = containerDefinitionsScratch[index].id;
            widgetVisuals.resize(widgets.size());
            for (auto& layout : containerLayoutsScratch)
            {
                if (layout.sourceIndex < firstWidget) layout.id = nodes[layout.sourceIndex].id;
                else
                {
                    const auto index = layout.sourceIndex-firstWidget;
                    auto state = widgetStates[index];
                    state.visible = layout.effectiveVisible;
                    state.enabled = layout.effectiveEnabled;
                    widgetVisuals[index] = api1::BuildWidgetVisual(widgets[index], state, layout.bounds);
                }
            }
            containerDefinitionsScratch = std::move(nodes);
            return;
        }
        containerLayoutsScratch.clear();
        if (containerDefinitionsScratch.empty()) return;
        if (!containerRuntime.IsAvailable() && !containerRuntimeLoadAttempted)
        {
            containerRuntimeLoadAttempted = true;
            std::string loadError;
            if (!containerRuntime.LoadForProcess({}, loadError))
            {
                TraceLog(LOG_ERROR, "%s", loadError.c_str());
                return;
            }
        }
        if (containerRuntime.IsAvailable())
        {
            std::string layoutError;
            if (!containerRuntime.Evaluate(containerDefinitionsScratch, containerLayoutsScratch, layoutError))
            {
                containerLayoutsScratch.clear();
                if (containerLayoutDiagnostic != layoutError)
                {
                    TraceLog(LOG_ERROR, "Container layout: %s", layoutError.c_str());
                    containerLayoutDiagnostic = std::move(layoutError);
                }
            }
            else
            {
                containerLayoutDiagnostic.clear();
            }
        }
    }

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
        externalStreams.Clear();
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

void MfdRenderer::HandleContainerInput(SceneRegistry& scene,
                                       const int viewportWidth,
                                       const int viewportHeight)
{
    if (impl_ == nullptr || viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }
    // Input cannot change layout on an idle frame. Leave the one authoritative
    // evaluation to DrawActivePage; actual wheel/drag frames still resolve both
    // before input and after mutation, so no persistent invalidation cache exists.
    const Vector2 wheel = GetMouseWheelMoveV();
    const auto* activePage = FindPageDefinition(scene.Document(), scene.ActivePageName());
    const bool hasWidgets = activePage != nullptr && !activePage->widgets.empty();
    if (!hasWidgets && !impl_->widgetInput.NeedsUpdate(scene.ActivePageName()) && !impl_->thumbDrag.active && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        wheel.x == 0.0f && wheel.y == 0.0f)
    {
        return;
    }
    impl_->ResolveContainers(scene);
    if (impl_->thumbDrag.active && impl_->thumbDrag.pageName != scene.ActivePageName())
    {
        impl_->thumbDrag.active = false;
    }
    if (impl_->containerLayoutsScratch.empty())
    {
        impl_->thumbDrag.active = false;
        impl_->widgetInput.Update(scene, impl_->widgetRuntime, impl_->containerDefinitionsScratch,
            impl_->containerLayoutsScratch, impl_->widgets, impl_->widgetStates, {}, false, false, false, true, GetFrameTime());
        return;
    }

    const Vector2 mouse = GetMousePosition();
    if (!hasWidgets && (mouse.x < 0.0f || mouse.y < 0.0f ||
        mouse.x > static_cast<float>(viewportWidth) || mouse.y > static_cast<float>(viewportHeight)))
    {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) impl_->thumbDrag.active = false;
        return;
    }

    const PageViewState view = scene.ActivePageView();
    const float logicalScale = static_cast<float>(std::min(viewportWidth, viewportHeight)) * 0.5f;
    const float zoom = SanitizeZoom(view.zoom);
    const Vec2 pointer {
        (mouse.x - static_cast<float>(viewportWidth) * 0.5f) / logicalScale / zoom + view.center.x,
        -(mouse.y - static_cast<float>(viewportHeight) * 0.5f) / logicalScale / zoom + view.center.y};

    bool widgetPointerBlocked = false;
    if (hasWidgets)
    {
        scene.CollectActiveReticleViews(impl_->activeReticlesScratch);
        bool earlierLayerPaint = false, currentLayerPaint = false;
        std::size_t currentLayer = 0;
        for (const auto& reticle : impl_->activeReticlesScratch)
        {
            if (!reticle.visible || !reticle.group) continue;
            if (reticle.layerOrder != currentLayer)
            {
                earlierLayerPaint = earlierLayerPaint || currentLayerPaint;
                currentLayerPaint = false;
                currentLayer = reticle.layerOrder;
            }
            if (detail::OpenGlStencilApiAvailable() && api1::ReticleErasesWidgetOcclusion(*reticle.group, pointer))
                widgetPointerBlocked = reticle.group->clipping.eraseLayerOnly && earlierLayerPaint;
            const bool painted = api1::ReticleOccludesWidgetPointer(*reticle.group, pointer,
                SampleWidgetOcclusionImage, &impl_->imageCache);
            currentLayerPaint = currentLayerPaint || painted;
            widgetPointerBlocked = widgetPointerBlocked || painted;
        }
        if (const auto summary = scene.ActivePageSummary())
            widgetPointerBlocked = widgetPointerBlocked || api1::ReticleOccludesWidgetPointer(impl_->ResolveTitleReticle(*summary), pointer);
    }
    const bool widgetOwnsPointer = impl_->widgetInput.Update(scene, impl_->widgetRuntime,
        impl_->containerDefinitionsScratch, impl_->containerLayoutsScratch, impl_->widgets, impl_->widgetStates,
        pointer, IsMouseButtonDown(MOUSE_BUTTON_LEFT), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT), impl_->thumbDrag.active || !IsWindowFocused() || scene.WindowDisplay().disabled,
        GetFrameTime(), widgetPointerBlocked);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !widgetOwnsPointer)
    {
        const api1::detail::ContainerPointerQuery pointerQuery(
            impl_->containerDefinitionsScratch, impl_->containerLayoutsScratch, pointer);
        for (auto iterator = impl_->containerLayoutsScratch.rbegin();
             iterator != impl_->containerLayoutsScratch.rend();
             ++iterator)
        {
            const api1::ContainerLayoutNode& layout = *iterator;
            if (!pointerQuery.CanReceive(layout))
                continue;
            const bool verticalHit = layout.verticalScrollbar.visible &&
                                     Contains(ToScreenRect(layout.verticalScrollbar.thumb,
                                                           view, viewportWidth, viewportHeight), mouse);
            const bool horizontalHit = layout.horizontalScrollbar.visible &&
                                       Contains(ToScreenRect(layout.horizontalScrollbar.thumb,
                                                             view, viewportWidth, viewportHeight), mouse);
            if (!verticalHit && !horizontalHit) continue;
            impl_->thumbDrag.sourceIndex = layout.sourceIndex;
            impl_->thumbDrag.pageName = scene.ActivePageName();
            impl_->thumbDrag.containerId = layout.id;
            impl_->thumbDrag.horizontal = horizontalHit;
            const float pointerCoordinate = horizontalHit ? pointer.x : pointer.y;
            const float thumbCenter = horizontalHit
                                          ? layout.horizontalScrollbar.thumb.center.x
                                          : layout.verticalScrollbar.thumb.center.y;
            impl_->thumbDrag.pointerToCenter = pointerCoordinate - thumbCenter;
            impl_->thumbDrag.active = true;
            break;
        }
    }

    if (impl_->thumbDrag.active)
    {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            impl_->thumbDrag.active = false;
        }
        else
        {
            for (const api1::ContainerLayoutNode& layout : impl_->containerLayoutsScratch)
            {
                if (layout.sourceIndex != impl_->thumbDrag.sourceIndex ||
                    layout.sourceIndex >= impl_->containerDefinitionsScratch.size() ||
                    layout.id != impl_->thumbDrag.containerId ||
                    !layout.effectiveVisible || !layout.effectiveEnabled)
                {
                    continue;
                }
                Vec2 offset = layout.scrollOffset;
                const float pointerCoordinate = impl_->thumbDrag.horizontal ? pointer.x : pointer.y;
                const float updated = impl_->containerInputRouter.OffsetForThumb(
                    layout,
                    impl_->thumbDrag.horizontal,
                    pointerCoordinate - impl_->thumbDrag.pointerToCenter);
                if (impl_->thumbDrag.horizontal) offset.x = updated;
                else offset.y = updated;
                scene.SetActiveContainerScrollOffset(
                    impl_->containerDefinitionsScratch[layout.sourceIndex].id, offset);
                return;
            }
            impl_->thumbDrag.active = false;
        }
    }

    api1::ContainerScrollRequest request;
    if (impl_->containerInputRouter.RouteWheel(
            impl_->containerDefinitionsScratch,
            impl_->containerLayoutsScratch,
            pointer,
            {wheel.x, wheel.y},
            0.08f / zoom,
            request) && request.sourceIndex < impl_->containerDefinitionsScratch.size())
    {
        scene.SetActiveContainerScrollOffset(
            impl_->containerDefinitionsScratch[request.sourceIndex].id, request.offset);
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
    if (!impl_) return;
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
    impl_->ResolveContainers(scene);

    const auto activePage = scene.ActivePageSummary();
    if (!activePage.has_value())
    {
        impl_->externalStreams.Clear();
        if (display.disabled) DrawRectangle(0, 0, viewportWidth, viewportHeight, BLACK);
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

    ExternalStreamFrame streamFrame(impl_->externalStreams, *activeReticles, !display.disabled, &impl_->widgetVisuals);
    if (display.disabled)
    {
        DrawRectangle(0, 0, viewportWidth, viewportHeight, BLACK);
        return;
    }

    const bool usesFullScreenViewport = viewportWidth == screenWidth && viewportHeight == screenHeight;
    const std::size_t activeScrollbarSourceIndex =
        impl_ != nullptr && impl_->thumbDrag.active
            ? impl_->thumbDrag.sourceIndex
            : std::numeric_limits<std::size_t>::max();
    const bool activeScrollbarHorizontal = impl_ != nullptr && impl_->thumbDrag.horizontal;
    const bool needsRenderTarget =
        !usesFullScreenViewport ||
        NeedsWindowPostProcess(display) ||
        ActiveReticleViewsUseClipping(*activeReticles);
    if (!needsRenderTarget)
    {
        DrawActivePageContent(
            scene,
            impl_->containerDefinitionsScratch,
            impl_->containerLayoutsScratch,
            impl_->widgetVisuals,
            impl_->firstWidget,
            *activeReticles,
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            activeScrollbarSourceIndex,
            activeScrollbarHorizontal,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache,
            impl_ == nullptr ? nullptr : &impl_->textLayoutCache, &impl_->externalStreams);
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
            impl_->containerDefinitionsScratch,
            impl_->containerLayoutsScratch,
            impl_->widgetVisuals,
            impl_->firstWidget,
            *activeReticles,
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            false,
            activeScrollbarSourceIndex,
            activeScrollbarHorizontal,
            impl_ == nullptr ? nullptr : &impl_->bezierCache,
            impl_ == nullptr ? nullptr : &impl_->imageCache,
            impl_ == nullptr ? nullptr : &impl_->textLayoutCache, &impl_->externalStreams);

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
            impl_->containerDefinitionsScratch,
            impl_->containerLayoutsScratch,
            impl_->widgetVisuals,
            impl_->firstWidget,
            *activeReticles,
            *titleReticle,
            viewportWidth,
            viewportHeight,
            textFont,
            impl_->renderTargetStencilReady,
            activeScrollbarSourceIndex,
            activeScrollbarHorizontal,
            &impl_->bezierCache,
            &impl_->imageCache,
            &impl_->textLayoutCache, &impl_->externalStreams);
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
const std::vector<ExternalStreamFeedback>& MfdRenderer::ExternalStreams() const noexcept
{
    static const std::vector<ExternalStreamFeedback> empty;
    return impl_ ? impl_->externalStreams.Feedback() : empty;
}
} // namespace mfd
