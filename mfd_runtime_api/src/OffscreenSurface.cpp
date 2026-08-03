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
#include <memory>
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
constexpr int kMaxOffscreenDimension = 8192;
constexpr std::size_t kMaxOffscreenRgbaBytes = 256U * 1024U * 1024U;

bool ResolveRgbaByteCount(const int width, const int height, std::size_t& byteCount) noexcept
{
    if (width <= 0 || height <= 0 || width > kMaxOffscreenDimension || height > kMaxOffscreenDimension)
    {
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixelCount > kMaxOffscreenRgbaBytes / 4U)
    {
        return false;
    }

    byteCount = pixelCount * 4U;
    return true;
}

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
    using RaylibPixels = std::unique_ptr<unsigned char, decltype(&MemFree)>;
    const RaylibPixels pixels(rlReadScreenPixels(target.texture.width, target.texture.height), &MemFree);
    rlDisableFramebuffer();

    std::size_t byteCount = 0U;
    if (pixels == nullptr || !ResolveRgbaByteCount(target.texture.width, target.texture.height, byteCount))
    {
        framePixels.clear();
        frameWidth = 0;
        frameHeight = 0;
        return false;
    }

    frameWidth = target.texture.width;
    frameHeight = target.texture.height;
    framePixels.resize(byteCount);
    if (!framePixels.empty())
    {
        std::memcpy(framePixels.data(), pixels.get(), byteCount);
    }
    return true;
}

struct NativeGlContextHandle
{
#if defined(_WIN32)
    HDC deviceContext = nullptr;
    HGLRC renderContext = nullptr;
#endif

    [[nodiscard]] bool Valid() const noexcept
    {
#if defined(_WIN32)
        return deviceContext != nullptr && renderContext != nullptr;
#else
        return true;
#endif
    }
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

    [[nodiscard]] bool OwnsWindow() noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return ownsHiddenWindow_;
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
    ScopedRuntimeRenderContext(const bool activateManagedContext,
                               const NativeGlContextHandle& borrowedContext) noexcept
        : active_(Activate(activateManagedContext, borrowedContext))
    {
    }

    ScopedRuntimeRenderContext(const ScopedRuntimeRenderContext&) = delete;
    ScopedRuntimeRenderContext& operator=(const ScopedRuntimeRenderContext&) = delete;

    [[nodiscard]] bool Active() const noexcept
    {
        return active_;
    }

private:
    static bool Activate(const bool activateManagedContext,
                         const NativeGlContextHandle& borrowedContext) noexcept
    {
        if (activateManagedContext)
        {
            return HeadlessRenderContext::Instance().Activate();
        }

        return IsWindowReady() && borrowedContext.Valid() && RestoreNativeGlContext(borrowedContext);
    }

    ScopedNativeGlContextRestore restore_ {};
    bool active_ = false;
};

class ScopedTextureMode
{
public:
    explicit ScopedTextureMode(const RenderTexture2D& target)
    {
        BeginTextureMode(target);
    }

    ~ScopedTextureMode() noexcept
    {
        EndTextureMode();
    }

    ScopedTextureMode(const ScopedTextureMode&) = delete;
    ScopedTextureMode& operator=(const ScopedTextureMode&) = delete;
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

        std::size_t requestedByteCount = 0U;
        if (!ResolveRgbaByteCount(requestedWidth, requestedHeight, requestedByteCount))
        {
            return false;
        }

        if (targetReady_ && width_ == requestedWidth && height_ == requestedHeight)
        {
            return true;
        }

        if (!EnsureRenderContext())
        {
            return false;
        }

        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_, borrowedContext_);
        if (!renderContext.Active())
        {
            return false;
        }

        bool candidateStencilReady = false;
        RenderTexture2D candidate = LoadRenderTextureWithStencil(
            requestedWidth, requestedHeight, &candidateStencilReady);
        const bool candidateReady = candidate.id != 0U && TextureReady(candidate.texture) &&
                                    rlFramebufferComplete(candidate.id);
        if (!candidateReady)
        {
            if (candidate.id != 0U)
            {
                UnloadRenderTexture(candidate);
            }
            return false;
        }

        SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
        ReleaseTarget();
        target_ = candidate;
        targetReady_ = true;
        targetStencilReady_ = candidateStencilReady;
        width_ = requestedWidth;
        height_ = requestedHeight;
        framePixels_.clear();
        frameWidth_ = 0;
        frameHeight_ = 0;
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
            const ScopedRuntimeRenderContext renderContext(ownsManagedContext_, borrowedContext_);
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
        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_, borrowedContext_);
        if (renderContext.Active())
        {
            renderer_.Release();
        }
    }

    [[nodiscard]] bool EnsureRenderContext()
    {
        if (ownsManagedContext_ || hasBorrowedContext_)
        {
            return IsWindowReady();
        }

        if (HeadlessRenderContext::Instance().OwnsWindow())
        {
            ownsManagedContext_ = HeadlessRenderContext::Instance().Acquire();
            return ownsManagedContext_;
        }

        if (IsWindowReady())
        {
            borrowedContext_ = CaptureCurrentNativeGlContext();
            hasBorrowedContext_ = borrowedContext_.Valid();
            return hasBorrowedContext_;
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

        const ScopedRuntimeRenderContext renderContext(ownsManagedContext_, borrowedContext_);
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

        {
            const ScopedTextureMode textureMode(target_);
            ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
            renderer_.DrawActivePage(scene, width_, height_, targetStencilReady_);
        }

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
    bool hasBorrowedContext_ = false;
    NativeGlContextHandle borrowedContext_ {};
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
