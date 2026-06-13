/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the public resizable offscreen surface.
 */

#include "mfd/runtime_api/OffscreenSurface.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif
extern "C" __declspec(dllimport) HDC APIENTRY wglGetCurrentDC();
extern "C" __declspec(dllimport) HGLRC APIENTRY wglGetCurrentContext();
extern "C" __declspec(dllimport) BOOL APIENTRY wglMakeCurrent(HDC hdc, HGLRC hglrc);
#endif

#include <raylib.h>
#include <rlgl.h>

#include "internal/OffscreenPageRenderer.hpp"
#include "internal/RuntimeSessionInternalAccess.hpp"
#include "render/RenderTextureUtils.h"

namespace mfd::runtime_api
{
namespace
{
Color ToRayColor(const ColorRgba& color) noexcept
{
    return Color {color.r, color.g, color.b, color.a};
}

bool TextureReady(const Texture2D& texture) noexcept
{
    return texture.id != 0U && texture.width > 0 && texture.height > 0;
}

std::uint8_t ScaleColorChannel(const std::uint8_t channel, const float brightness) noexcept
{
    const float scaled = std::clamp(static_cast<float>(channel) * brightness, 0.0f, 255.0f);
    return static_cast<std::uint8_t>(std::lround(scaled));
}

void ApplyWindowDisplayState(std::vector<std::uint8_t>& framePixels, const WindowDisplayState& display)
{
    if (framePixels.empty())
    {
        return;
    }

    if (display.disabled)
    {
        for (std::size_t index = 0; index + 3U < framePixels.size(); index += 4U)
        {
            framePixels[index] = 0U;
            framePixels[index + 1U] = 0U;
            framePixels[index + 2U] = 0U;
            framePixels[index + 3U] = 255U;
        }
        return;
    }

    const bool invertColors = display.invertColors;
    const float brightness = std::clamp(display.brightness, 0.0f, 1.0f);
    if (!invertColors && brightness >= 0.9995f)
    {
        return;
    }

    for (std::size_t index = 0; index + 3U < framePixels.size(); index += 4U)
    {
        std::uint8_t red = framePixels[index];
        std::uint8_t green = framePixels[index + 1U];
        std::uint8_t blue = framePixels[index + 2U];
        if (invertColors)
        {
            red = static_cast<std::uint8_t>(255U - red);
            green = static_cast<std::uint8_t>(255U - green);
            blue = static_cast<std::uint8_t>(255U - blue);
        }

        framePixels[index] = ScaleColorChannel(red, brightness);
        framePixels[index + 1U] = ScaleColorChannel(green, brightness);
        framePixels[index + 2U] = ScaleColorChannel(blue, brightness);
    }
}

bool ReadRgba8Framebuffer(const RenderTexture2D& target,
                          std::vector<std::uint8_t>& framePixels,
                          int& frameWidth,
                          int& frameHeight)
{
    if (target.id == 0U || !TextureReady(target.texture))
    {
        framePixels.clear();
        frameWidth = 0;
        frameHeight = 0;
        return false;
    }

    rlDrawRenderBatchActive();
    rlEnableFramebuffer(target.id);
    unsigned char* pixels = rlReadScreenPixels(target.texture.width, target.texture.height);
    rlDisableFramebuffer();

    if (pixels == nullptr)
    {
        framePixels.clear();
        frameWidth = 0;
        frameHeight = 0;
        return false;
    }

    frameWidth = target.texture.width;
    frameHeight = target.texture.height;
    const std::size_t rowStrideBytes = static_cast<std::size_t>(frameWidth) * 4U;
    const std::size_t byteCount = rowStrideBytes * static_cast<std::size_t>(frameHeight);
    framePixels.resize(byteCount);
    if (!framePixels.empty())
    {
        std::memcpy(framePixels.data(), pixels, byteCount);
    }

    MemFree(pixels);
    return true;
}

struct NativeGlContextHandle
{
#if defined(_WIN32)
    HDC deviceContext = nullptr;
    HGLRC renderContext = nullptr;

    [[nodiscard]] bool Valid() const noexcept
    {
        return deviceContext != nullptr && renderContext != nullptr;
    }
#endif
};

NativeGlContextHandle CaptureCurrentNativeGlContext() noexcept
{
    NativeGlContextHandle handle;
#if defined(_WIN32)
    handle.deviceContext = wglGetCurrentDC();
    handle.renderContext = wglGetCurrentContext();
#endif
    return handle;
}

#if defined(_WIN32)
bool RestoreNativeGlContext(const NativeGlContextHandle& handle) noexcept
{
    if (!handle.Valid())
    {
        return wglMakeCurrent(nullptr, nullptr) != FALSE;
    }

    return wglMakeCurrent(handle.deviceContext, handle.renderContext) != FALSE;
}
#else
bool RestoreNativeGlContext(const NativeGlContextHandle&) noexcept
{
    return true;
}
#endif

class ScopedNativeGlContextRestore
{
public:
    ScopedNativeGlContextRestore() noexcept
        : previousContext_(CaptureCurrentNativeGlContext())
    {
    }

    ~ScopedNativeGlContextRestore()
    {
        RestoreNativeGlContext(previousContext_);
    }

    ScopedNativeGlContextRestore(const ScopedNativeGlContextRestore&) = delete;
    ScopedNativeGlContextRestore& operator=(const ScopedNativeGlContextRestore&) = delete;

private:
    NativeGlContextHandle previousContext_ {};
};

class HeadlessRenderContext
{
public:
    static HeadlessRenderContext& Instance()
    {
        static HeadlessRenderContext instance;
        return instance;
    }

    [[nodiscard]] bool Acquire()
    {
        const ScopedNativeGlContextRestore restore;
        const std::lock_guard<std::mutex> lock(mutex_);
        if (referenceCount_ == 0U)
        {
            SetConfigFlags(FLAG_WINDOW_HIDDEN);
            InitWindow(1, 1, "mfd_runtime_api_headless");
            if (!IsWindowReady())
            {
                ownsHiddenWindow_ = false;
                return false;
            }

            nativeContext_ = CaptureCurrentNativeGlContext();
            ownsHiddenWindow_ = true;
        }

        ++referenceCount_;
        return true;
    }

    [[nodiscard]] bool Activate() noexcept
    {
#if defined(_WIN32)
        const std::lock_guard<std::mutex> lock(mutex_);
        return ownsHiddenWindow_ && nativeContext_.Valid() && RestoreNativeGlContext(nativeContext_);
#else
        return ownsHiddenWindow_;
#endif
    }

    void Release() noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (referenceCount_ == 0U)
        {
            return;
        }

        --referenceCount_;
        if (referenceCount_ == 0U && ownsHiddenWindow_)
        {
            const ScopedNativeGlContextRestore restore;
            RestoreNativeGlContext(nativeContext_);
            if (IsWindowReady())
            {
                CloseWindow();
            }

            nativeContext_ = {};
            ownsHiddenWindow_ = false;
        }
    }

private:
    std::mutex mutex_ {};
    std::size_t referenceCount_ = 0U;
    NativeGlContextHandle nativeContext_ {};
    bool ownsHiddenWindow_ = false;
};

class ScopedRuntimeRenderContext
{
public:
    explicit ScopedRuntimeRenderContext(const bool activateManagedContext) noexcept
        : active_(!activateManagedContext || HeadlessRenderContext::Instance().Activate())
    {
    }

    ScopedRuntimeRenderContext(const ScopedRuntimeRenderContext&) = delete;
    ScopedRuntimeRenderContext& operator=(const ScopedRuntimeRenderContext&) = delete;

    [[nodiscard]] bool Active() const noexcept
    {
        return active_;
    }

private:
    ScopedNativeGlContextRestore restore_ {};
    bool active_ = false;
};
} // namespace

class OffscreenSurface::Impl
{
public:
    Impl() = default;

    ~Impl()
    {
        ReleaseRenderer();
        ReleaseTarget();
        if (ownsManagedContext_)
        {
            HeadlessRenderContext::Instance().Release();
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    bool Resize(const int requestedWidth, const int requestedHeight)
    {
        if (requestedWidth <= 0 || requestedHeight <= 0)
        {
            Reset();
            return false;
        }

        if (targetReady_ && width_ == requestedWidth && height_ == requestedHeight)
        {
            return true;
        }

        ReleaseTarget();
        framePixels_.clear();
        frameWidth_ = 0;
        frameHeight_ = 0;

        if (!EnsureRenderContext())
        {
            width_ = requestedWidth;
            height_ = requestedHeight;
            return false;
        }

        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_);
        if (!renderContext.Active())
        {
            width_ = requestedWidth;
            height_ = requestedHeight;
            return false;
        }

        target_ = LoadRenderTextureWithStencil(requestedWidth, requestedHeight, &targetStencilReady_);
        targetReady_ = target_.id != 0U && TextureReady(target_.texture);
        if (!targetReady_)
        {
            target_ = {};
            targetStencilReady_ = false;
            width_ = 0;
            height_ = 0;
            return false;
        }

        SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
        width_ = requestedWidth;
        height_ = requestedHeight;
        return true;
    }

    void Reset() noexcept
    {
        ReleaseTarget();
        width_ = 0;
        height_ = 0;
        framePixels_.clear();
        frameWidth_ = 0;
        frameHeight_ = 0;
    }

    void ReleaseTarget() noexcept
    {
        if (targetReady_)
        {
            const ScopedRuntimeRenderContext renderContext(ownsManagedContext_);
            if (renderContext.Active() && IsWindowReady())
            {
                UnloadRenderTexture(target_);
            }

            target_ = {};
            targetReady_ = false;
            targetStencilReady_ = false;
        }
    }

    void ReleaseRenderer() noexcept
    {
        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_);
        if (renderContext.Active())
        {
            renderer_.Release();
        }
    }

    [[nodiscard]] bool EnsureRenderContext()
    {
        if (ownsManagedContext_)
        {
            return IsWindowReady();
        }

        ownsManagedContext_ = HeadlessRenderContext::Instance().Acquire();
        return ownsManagedContext_ && IsWindowReady();
    }

    [[nodiscard]] bool Render(const RuntimeSession& session)
    {
        if ((!targetReady_ && !Resize(width_, height_)) || width_ <= 0 || height_ <= 0 ||
            !EnsureRenderContext())
        {
            return false;
        }

        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_);
        if (!renderContext.Active())
        {
            return false;
        }

        const SceneRegistry& scene = internal::RuntimeSessionInternalAccess::Scene(session);
        const WindowAssetDefinition& windowDefinition =
            internal::RuntimeSessionInternalAccess::WindowDefinition(session);
        if (textFontFile_ != windowDefinition.fontFile)
        {
            textFontFile_ = windowDefinition.fontFile;
            renderer_.SetTextFontFile(textFontFile_);
        }

        BeginTextureMode(target_);
        ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
        renderer_.DrawActivePage(scene, width_, height_, targetStencilReady_);
        EndTextureMode();

        if (!ReadRgba8Framebuffer(target_, framePixels_, frameWidth_, frameHeight_))
        {
            return false;
        }

        ApplyWindowDisplayState(framePixels_, scene.WindowDisplay());
        return true;
    }

    [[nodiscard]] OffscreenFrameView FrameView() const noexcept
    {
        if (framePixels_.empty() || frameWidth_ <= 0 || frameHeight_ <= 0)
        {
            return {};
        }

        return OffscreenFrameView {
            framePixels_.data(),
            framePixels_.size(),
            static_cast<std::size_t>(frameWidth_) * 4U,
            frameWidth_,
            frameHeight_,
            FramePixelFormat::Rgba8};
    }

    [[nodiscard]] bool Ready() const noexcept
    {
        return targetReady_;
    }

    [[nodiscard]] int Width() const noexcept
    {
        return width_;
    }

    [[nodiscard]] int Height() const noexcept
    {
        return height_;
    }

private:
    RenderTexture2D target_ {};
    internal::OffscreenPageRenderer renderer_ {};
    std::filesystem::path textFontFile_ {};
    std::vector<std::uint8_t> framePixels_ {};
    bool targetReady_ = false;
    bool targetStencilReady_ = false;
    bool ownsManagedContext_ = false;
    int width_ = 0;
    int height_ = 0;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
};

OffscreenSurface::OffscreenSurface()
    : impl_(std::make_unique<Impl>())
{
}

OffscreenSurface::OffscreenSurface(const int width, const int height)
    : OffscreenSurface()
{
    Resize(width, height);
}

OffscreenSurface::~OffscreenSurface() = default;

OffscreenSurface::OffscreenSurface(OffscreenSurface&&) noexcept = default;

OffscreenSurface& OffscreenSurface::operator=(OffscreenSurface&&) noexcept = default;

bool OffscreenSurface::Resize(const int width, const int height)
{
    return impl_ != nullptr && impl_->Resize(width, height);
}

void OffscreenSurface::Release() noexcept
{
    if (impl_ != nullptr)
    {
        impl_->Reset();
    }
}

bool OffscreenSurface::Render(const RuntimeSession& session)
{
    return impl_ != nullptr && impl_->Render(session);
}

bool OffscreenSurface::Ready() const noexcept
{
    return impl_ != nullptr && impl_->Ready();
}

int OffscreenSurface::Width() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->Width();
}

int OffscreenSurface::Height() const noexcept
{
    return impl_ == nullptr ? 0 : impl_->Height();
}

OffscreenFrameView OffscreenSurface::FrameView() const noexcept
{
    return impl_ == nullptr ? OffscreenFrameView {} : impl_->FrameView();
}
} // namespace mfd::runtime_api
