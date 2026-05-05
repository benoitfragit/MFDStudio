/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for WindowLauncher.
 */

#include "mfd/window/WindowLauncher.h"
#include "mfd/window/WindowLauncherPlugin.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
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
extern "C" __declspec(dllimport) void* APIENTRY wglGetProcAddress(const char* name);
#endif

#include <raylib.h>
#include <rlgl.h>

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/render/OpenGlFramebufferReader.h"
#include "mfd/render/WindowBranding.h"
#include "mfd/runtime/SceneRegistry.h"

#include "RuntimeDebugOverlay.hpp"

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

namespace
{
constexpr float kStrobeFeedbackIntervalSeconds = 0.020f;
constexpr unsigned int kCaptureRingSize = 2;
constexpr std::size_t kMaxCommandsPerFrame = 512;
constexpr std::size_t kPluginErrorBufferCapacity = 1024U;
constexpr int kFramebufferResizeCooldownFrames = 3;
using GlSyncHandle = void*;

void ResetPluginErrorBuffer(MfdWindowUtf8Buffer& buffer, char* storage, const std::size_t capacity) noexcept
{
    if (storage != nullptr && capacity > 0U)
    {
        storage[0] = '\0';
    }

    buffer.data = storage;
    buffer.capacity = capacity;
    buffer.size = 0U;
}

std::string PluginErrorBufferToString(const MfdWindowUtf8Buffer& buffer)
{
    if (buffer.data == nullptr)
    {
        return {};
    }

    const std::size_t readable = buffer.capacity == 0U ? 0U : (std::min)(buffer.size, buffer.capacity - 1U);
    return std::string(buffer.data, buffer.data + readable);
}

std::string PluginResultCodeDescription(const MfdWindowFramebufferPluginResultCode code)
{
    switch (code)
    {
    case MfdWindowFramebufferPluginResultCode_Success:
        return "success";
    case MfdWindowFramebufferPluginResultCode_BufferTooSmall:
        return "buffer too small";
    case MfdWindowFramebufferPluginResultCode_InvalidArgument:
        return "invalid argument";
    case MfdWindowFramebufferPluginResultCode_InvalidState:
        return "invalid state";
    case MfdWindowFramebufferPluginResultCode_Unsupported:
        return "unsupported";
    case MfdWindowFramebufferPluginResultCode_InternalFailure:
        return "internal failure";
    case MfdWindowFramebufferPluginResultCode_AbiMismatch:
        return "ABI mismatch";
    }

    return "unknown failure";
}

#if defined(_WIN32)
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLsizei = int;
using GLbitfield = unsigned int;
using GLboolean = unsigned char;

#ifndef GL_TRUE
#define GL_TRUE 1
#endif
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_STREAM_READ
#define GL_STREAM_READ 0x88E1
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED 0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED 0x911C
#endif
#endif

class PboReadbackApi
{
public:
    PboReadbackApi() noexcept;

    static const PboReadbackApi& Instance() noexcept
    {
        static const PboReadbackApi api;
        return api;
    }

    [[nodiscard]] bool Available() const noexcept;
    void GenBuffers(int count, unsigned int* buffers) const noexcept;
    void DeleteBuffers(int count, const unsigned int* buffers) const noexcept;
    void BindPixelPackBuffer(unsigned int bufferId) const noexcept;
    void AllocatePixelPackBuffer(std::size_t sizeInBytes) const noexcept;
    void ReadPixelsRgba32(int width, int height) const noexcept;
    [[nodiscard]] void* MapPixelPackBufferReadOnly() const noexcept;
    [[nodiscard]] bool UnmapPixelPackBuffer() const noexcept;
    [[nodiscard]] GlSyncHandle CreateFence() const noexcept;
    [[nodiscard]] bool IsFenceReady(GlSyncHandle fence) const noexcept;
    void DeleteFence(GlSyncHandle fence) const noexcept;

private:
#if defined(_WIN32)
    using GlGenBuffersProc = void(APIENTRY*)(GLsizei, GLuint*);
    using GlDeleteBuffersProc = void(APIENTRY*)(GLsizei, const GLuint*);
    using GlBindBufferProc = void(APIENTRY*)(GLenum, GLuint);
    using GlBufferDataProc = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
    using GlReadPixelsProc = void(APIENTRY*)(int, int, int, int, GLenum, GLenum, const void*);
    using GlMapBufferProc = void*(APIENTRY*)(GLenum, GLenum);
    using GlUnmapBufferProc = GLboolean(APIENTRY*)(GLenum);
    using GlFenceSyncProc = GlSyncHandle(APIENTRY*)(GLenum, GLbitfield);
    using GlClientWaitSyncProc = GLenum(APIENTRY*)(GlSyncHandle, GLbitfield, std::uint64_t);
    using GlDeleteSyncProc = void(APIENTRY*)(GlSyncHandle);

    GlGenBuffersProc genBuffers_ = nullptr;
    GlDeleteBuffersProc deleteBuffers_ = nullptr;
    GlBindBufferProc bindBuffer_ = nullptr;
    GlBufferDataProc bufferData_ = nullptr;
    GlReadPixelsProc readPixels_ = nullptr;
    GlMapBufferProc mapBuffer_ = nullptr;
    GlUnmapBufferProc unmapBuffer_ = nullptr;
    GlFenceSyncProc fenceSync_ = nullptr;
    GlClientWaitSyncProc clientWaitSync_ = nullptr;
    GlDeleteSyncProc deleteSync_ = nullptr;
#endif
};

#if defined(_WIN32)
void* LoadGlProcAddress(const char* name) noexcept
{
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (address != nullptr &&
        address != reinterpret_cast<void*>(0x1) &&
        address != reinterpret_cast<void*>(0x2) &&
        address != reinterpret_cast<void*>(0x3) &&
        address != reinterpret_cast<void*>(-1))
    {
        return address;
    }

    static HMODULE openglModule = GetModuleHandleA("opengl32.dll");
    return openglModule == nullptr ? nullptr : reinterpret_cast<void*>(GetProcAddress(openglModule, name));
}

void* LoadFirstGlProcAddress(const std::initializer_list<const char*> names) noexcept
{
    for (const char* name : names)
    {
        if (void* address = LoadGlProcAddress(name); address != nullptr)
        {
            return address;
        }
    }

    return nullptr;
}
#endif

PboReadbackApi::PboReadbackApi() noexcept
{
#if defined(_WIN32)
    genBuffers_ = reinterpret_cast<GlGenBuffersProc>(LoadFirstGlProcAddress({"glGenBuffers", "glGenBuffersARB"}));
    deleteBuffers_ =
        reinterpret_cast<GlDeleteBuffersProc>(LoadFirstGlProcAddress({"glDeleteBuffers", "glDeleteBuffersARB"}));
    bindBuffer_ = reinterpret_cast<GlBindBufferProc>(LoadFirstGlProcAddress({"glBindBuffer", "glBindBufferARB"}));
    bufferData_ = reinterpret_cast<GlBufferDataProc>(LoadFirstGlProcAddress({"glBufferData", "glBufferDataARB"}));
    readPixels_ = reinterpret_cast<GlReadPixelsProc>(LoadGlProcAddress("glReadPixels"));
    mapBuffer_ = reinterpret_cast<GlMapBufferProc>(LoadFirstGlProcAddress({"glMapBuffer", "glMapBufferARB"}));
    unmapBuffer_ = reinterpret_cast<GlUnmapBufferProc>(LoadFirstGlProcAddress({"glUnmapBuffer", "glUnmapBufferARB"}));
    fenceSync_ = reinterpret_cast<GlFenceSyncProc>(LoadGlProcAddress("glFenceSync"));
    clientWaitSync_ = reinterpret_cast<GlClientWaitSyncProc>(LoadGlProcAddress("glClientWaitSync"));
    deleteSync_ = reinterpret_cast<GlDeleteSyncProc>(LoadGlProcAddress("glDeleteSync"));
#endif
}

bool PboReadbackApi::Available() const noexcept
{
#if defined(_WIN32)
    return genBuffers_ != nullptr &&
           deleteBuffers_ != nullptr &&
           bindBuffer_ != nullptr &&
           bufferData_ != nullptr &&
           readPixels_ != nullptr &&
           mapBuffer_ != nullptr &&
           unmapBuffer_ != nullptr &&
           fenceSync_ != nullptr &&
           clientWaitSync_ != nullptr &&
           deleteSync_ != nullptr;
#else
    return false;
#endif
}

void PboReadbackApi::GenBuffers([[maybe_unused]] const int count,
                                [[maybe_unused]] unsigned int* buffers) const noexcept
{
#if defined(_WIN32)
    if (genBuffers_ != nullptr && count > 0 && buffers != nullptr)
    {
        genBuffers_(static_cast<GLsizei>(count), reinterpret_cast<GLuint*>(buffers));
    }
#endif
}

void PboReadbackApi::DeleteBuffers([[maybe_unused]] const int count,
                                   [[maybe_unused]] const unsigned int* buffers) const noexcept
{
#if defined(_WIN32)
    if (deleteBuffers_ != nullptr && count > 0 && buffers != nullptr)
    {
        deleteBuffers_(static_cast<GLsizei>(count), reinterpret_cast<const GLuint*>(buffers));
    }
#endif
}

void PboReadbackApi::BindPixelPackBuffer([[maybe_unused]] const unsigned int bufferId) const noexcept
{
#if defined(_WIN32)
    if (bindBuffer_ != nullptr)
    {
        bindBuffer_(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(bufferId));
    }
#endif
}

void PboReadbackApi::AllocatePixelPackBuffer([[maybe_unused]] const std::size_t sizeInBytes) const noexcept
{
#if defined(_WIN32)
    if (bufferData_ != nullptr)
    {
        bufferData_(GL_PIXEL_PACK_BUFFER, static_cast<std::ptrdiff_t>(sizeInBytes), nullptr, GL_STREAM_READ);
    }
#endif
}

void PboReadbackApi::ReadPixelsRgba32([[maybe_unused]] const int width,
                                      [[maybe_unused]] const int height) const noexcept
{
#if defined(_WIN32)
    if (readPixels_ != nullptr)
    {
        readPixels_(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
#endif
}

void* PboReadbackApi::MapPixelPackBufferReadOnly() const noexcept
{
#if defined(_WIN32)
    return mapBuffer_ == nullptr ? nullptr : mapBuffer_(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
#else
    return nullptr;
#endif
}

bool PboReadbackApi::UnmapPixelPackBuffer() const noexcept
{
#if defined(_WIN32)
    return unmapBuffer_ != nullptr && unmapBuffer_(GL_PIXEL_PACK_BUFFER) == GL_TRUE;
#else
    return false;
#endif
}

GlSyncHandle PboReadbackApi::CreateFence() const noexcept
{
#if defined(_WIN32)
    return fenceSync_ == nullptr ? nullptr : fenceSync_(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#else
    return nullptr;
#endif
}

bool PboReadbackApi::IsFenceReady([[maybe_unused]] const GlSyncHandle fence) const noexcept
{
#if defined(_WIN32)
    if (clientWaitSync_ == nullptr || fence == nullptr)
    {
        return false;
    }

    const GLenum waitResult = clientWaitSync_(fence, 0, 0);
    return waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED;
#else
    return false;
#endif
}

void PboReadbackApi::DeleteFence([[maybe_unused]] const GlSyncHandle fence) const noexcept
{
#if defined(_WIN32)
    if (deleteSync_ != nullptr && fence != nullptr)
    {
        deleteSync_(fence);
    }
#endif
}

class AsyncFramebufferCapture
{
public:
    ~AsyncFramebufferCapture()
    {
        if (IsWindowReady())
        {
            Release();
        }
    }

    [[nodiscard]] std::optional<mfd::Rgba32Framebuffer> Capture()
    {
        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        if (renderWidth <= 0 || renderHeight <= 0)
        {
            return std::nullopt;
        }

        if (!pboSupportChecked_)
        {
            pboSupportChecked_ = true;
            pboAvailable_ = (rlGetVersion() >= RL_OPENGL_33) && PboReadbackApi::Instance().Available();
        }

        if (!pboAvailable_)
        {
            return CaptureSynchronously();
        }

        if (!EnsureResources(renderWidth, renderHeight))
        {
            DisablePbo();
            return CaptureSynchronously();
        }

        rlDrawRenderBatchActive();

        const unsigned int writeIndex = frameIndex_ % kCaptureRingSize;
        if (!SubmitReadback(writeIndex, renderWidth, renderHeight))
        {
            DisablePbo();
            return CaptureSynchronously();
        }

        std::optional<mfd::Rgba32Framebuffer> readyFrame;
        if (frameIndex_ + 1 >= kCaptureRingSize)
        {
            const unsigned int readIndex = (frameIndex_ + 1) % kCaptureRingSize;
            readyFrame = TryConsume(readIndex);
            if (!pboAvailable_)
            {
                return CaptureSynchronously();
            }
        }

        ++frameIndex_;
        if (readyFrame.has_value())
        {
            hasLatestFramebuffer_ = true;
            return readyFrame;
        }

        if (hasLatestFramebuffer_)
        {
            return latestFramebuffer_;
        }

        return readyFrame;
    }

private:
    struct CaptureSlot
    {
        unsigned int pboId = 0;
        std::size_t capacityBytes = 0;
        int width = 0;
        int height = 0;
        GlSyncHandle fence = nullptr;

        void ReleaseFence(const PboReadbackApi& api) noexcept
        {
            if (fence != nullptr)
            {
                api.DeleteFence(fence);
                fence = nullptr;
            }
        }

        void ResetMetadata() noexcept
        {
            capacityBytes = 0;
            width = 0;
            height = 0;
        }
    };

    static void FlipRows(std::vector<mfd::Rgba8Pixel>& pixels, const int width, const int height)
    {
        if (width <= 0 || height <= 1)
        {
            return;
        }

        const std::size_t stride = static_cast<std::size_t>(width) * sizeof(mfd::Rgba8Pixel);
        std::vector<mfd::Rgba8Pixel> scratchRow(static_cast<std::size_t>(width));

        for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
        {
            mfd::Rgba8Pixel* topRow = pixels.data() + static_cast<std::size_t>(top) * static_cast<std::size_t>(width);
            mfd::Rgba8Pixel* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * static_cast<std::size_t>(width);
            std::memcpy(scratchRow.data(), topRow, stride);
            std::memcpy(topRow, bottomRow, stride);
            std::memcpy(bottomRow, scratchRow.data(), stride);
        }
    }

    [[nodiscard]] std::optional<mfd::Rgba32Framebuffer> CaptureSynchronously()
    {
        latestFramebuffer_ = mfd::OpenGlFramebufferReader::ReadRgba32();
        hasLatestFramebuffer_ = !latestFramebuffer_.Empty();
        return latestFramebuffer_;
    }

    void DisablePbo() noexcept
    {
        Release();
        pboAvailable_ = false;
    }

    bool EnsureResources(const int width, const int height)
    {
        if (!pboAvailable_)
        {
            return false;
        }

        const PboReadbackApi& api = PboReadbackApi::Instance();
        const std::size_t requiredBytes =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * sizeof(mfd::Rgba8Pixel);
        if (requiredBytes == 0)
        {
            return false;
        }

        if (!initialized_)
        {
            for (CaptureSlot& slot : slots_)
            {
                api.GenBuffers(1, &slot.pboId);
                if (slot.pboId == 0)
                {
                    Release();
                    return false;
                }
            }
            initialized_ = true;
            frameIndex_ = 0;
        }

        for (CaptureSlot& slot : slots_)
        {
            if (slot.capacityBytes == requiredBytes && slot.width == width && slot.height == height)
            {
                continue;
            }

            slot.ReleaseFence(api);
            api.BindPixelPackBuffer(slot.pboId);
            api.AllocatePixelPackBuffer(requiredBytes);
            api.BindPixelPackBuffer(0);
            slot.capacityBytes = requiredBytes;
            slot.width = width;
            slot.height = height;
        }

        return true;
    }

    bool SubmitReadback(const unsigned int slotIndex, const int width, const int height)
    {
        const PboReadbackApi& api = PboReadbackApi::Instance();
        CaptureSlot& slot = slots_[slotIndex];
        if (slot.pboId == 0 || slot.capacityBytes == 0 || slot.width != width || slot.height != height)
        {
            return false;
        }

        slot.ReleaseFence(api);
        api.BindPixelPackBuffer(slot.pboId);
        api.ReadPixelsRgba32(width, height);
        api.BindPixelPackBuffer(0);

        slot.fence = api.CreateFence();
        return slot.fence != nullptr;
    }

    [[nodiscard]] std::optional<mfd::Rgba32Framebuffer> TryConsume(const unsigned int slotIndex)
    {
        const PboReadbackApi& api = PboReadbackApi::Instance();
        CaptureSlot& slot = slots_[slotIndex];
        if (slot.pboId == 0 || slot.capacityBytes == 0 || slot.fence == nullptr)
        {
            return std::nullopt;
        }

        if (!api.IsFenceReady(slot.fence))
        {
            return std::nullopt;
        }

        api.BindPixelPackBuffer(slot.pboId);
        void* mappedBuffer = api.MapPixelPackBufferReadOnly();
        if (mappedBuffer == nullptr)
        {
            api.BindPixelPackBuffer(0);
            slot.ReleaseFence(api);
            DisablePbo();
            return std::nullopt;
        }

        latestFramebuffer_.width = slot.width;
        latestFramebuffer_.height = slot.height;
        latestFramebuffer_.pixels.resize(static_cast<std::size_t>(slot.width) * static_cast<std::size_t>(slot.height));
        std::memcpy(latestFramebuffer_.pixels.data(), mappedBuffer, slot.capacityBytes);

        const bool unmapSucceeded = api.UnmapPixelPackBuffer();
        api.BindPixelPackBuffer(0);
        slot.ReleaseFence(api);
        if (!unmapSucceeded)
        {
            DisablePbo();
            return std::nullopt;
        }

        FlipRows(latestFramebuffer_.pixels, latestFramebuffer_.width, latestFramebuffer_.height);
        return latestFramebuffer_;
    }

    void Release() noexcept
    {
        const PboReadbackApi& api = PboReadbackApi::Instance();
        for (CaptureSlot& slot : slots_)
        {
            slot.ReleaseFence(api);
            if (slot.pboId != 0)
            {
                api.DeleteBuffers(1, &slot.pboId);
                slot.pboId = 0;
            }

            slot.ResetMetadata();
        }

        initialized_ = false;
        frameIndex_ = 0;
    }

    std::array<CaptureSlot, kCaptureRingSize> slots_ {};
    mfd::Rgba32Framebuffer latestFramebuffer_ {};
    std::size_t frameIndex_ = 0;
    bool initialized_ = false;
    bool pboSupportChecked_ = false;
    bool pboAvailable_ = true;
    bool hasLatestFramebuffer_ = false;
};

struct CommandLineOptions
{
    std::filesystem::path windowFile;
    std::filesystem::path framebufferPluginFile;
    bool showHelp = false;
};

Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

bool TextureReady(const Texture2D& texture) noexcept
{
    return texture.id != 0 && texture.width > 0 && texture.height > 0;
}

mfd::StrobeFeedbackCapture ToFeedbackCapture(const mfd::StrobeCaptureResult& capture)
{
    return mfd::StrobeFeedbackCapture {
        capture.runtimeReticleId,
        capture.sourceTemplateTransportId,
        capture.reticleId,
        capture.sourceTemplateId,
        capture.label,
        capture.category,
        capture.position,
        capture.distance,
        capture.metadata};
}

mfd::StrobeFeedbackMagnet ToFeedbackMagnet(const std::optional<mfd::StrobeMagnetSummary>& magnetSummary)
{
    if (!magnetSummary.has_value())
    {
        return {};
    }

    return mfd::StrobeFeedbackMagnet {
        magnetSummary->enabled,
        magnetSummary->radius,
        magnetSummary->strength,
        magnetSummary->magnetized,
        magnetSummary->runtimeReticleId,
        magnetSummary->reticleId,
        magnetSummary->targetPosition,
        magnetSummary->distance};
}

#if defined(_WIN32)
std::string NarrowWideString(const std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }

    const int outputSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (outputSize <= 0)
    {
        return {};
    }

    std::string output(static_cast<std::size_t>(outputSize), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        output.data(),
        outputSize,
        nullptr,
        nullptr);
    return output;
}

std::string FormatWindowsErrorMessage(const DWORD errorCode)
{
    LPWSTR rawBuffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
                                        nullptr,
                                        errorCode,
                                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&rawBuffer),
                                        0,
                                        nullptr);
    std::wstring message = (length > 0 && rawBuffer != nullptr)
                               ? std::wstring(rawBuffer, rawBuffer + length)
                               : std::wstring {L"Unknown Win32 error"};
    if (rawBuffer != nullptr)
    {
        LocalFree(rawBuffer);
    }

    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
    {
        message.pop_back();
    }

    return NarrowWideString(message);
}
#endif

class LoadedFramebufferPlugin
{
public:
    ~LoadedFramebufferPlugin() noexcept
    {
        Unload();
    }

    LoadedFramebufferPlugin(const LoadedFramebufferPlugin&) = delete;
    LoadedFramebufferPlugin& operator=(const LoadedFramebufferPlugin&) = delete;

    LoadedFramebufferPlugin() = default;

    [[nodiscard]] bool Load([[maybe_unused]] const std::filesystem::path& pluginFile, std::string& error)
    {
        Unload();

        if (pluginFile.empty())
        {
            error = "The framebuffer plugin DLL path is empty.";
            return false;
        }

#if defined(_WIN32)
        handle_ = ::LoadLibraryW(pluginFile.c_str());
        if (handle_ == nullptr)
        {
            error = "Unable to load framebuffer plugin '" + pluginFile.string() +
                    "': " + FormatWindowsErrorMessage(GetLastError());
            return false;
        }

        const FARPROC symbol = ::GetProcAddress(handle_, mfd::window::kLauncherFramebufferPluginEntryPointName);
        if (symbol == nullptr)
        {
            error = "The framebuffer plugin '" + pluginFile.string() +
                    "' does not export '" + mfd::window::kLauncherFramebufferPluginEntryPointName + "'.";
            Unload();
            return false;
        }

        const auto getApi = reinterpret_cast<MfdGetWindowFramebufferPluginApiFn>(symbol);
        if (getApi == nullptr)
        {
            error = "The framebuffer plugin '" + pluginFile.string() + "' exposes an invalid stable entry point.";
            Unload();
            return false;
        }

        hostApi_ = {};
        hostApi_.struct_size = sizeof(hostApi_);
        hostApi_.abi_version = MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION;

        std::array<char, kPluginErrorBufferCapacity> errorStorage {};
        MfdWindowUtf8Buffer errorBuffer {};
        ResetPluginErrorBuffer(errorBuffer, errorStorage.data(), errorStorage.size());

        pluginApi_ = {};
        pluginApi_.struct_size = sizeof(pluginApi_);
        const MfdWindowFramebufferPluginResultCode getApiStatus = getApi(&pluginApi_, &errorBuffer);
        if (getApiStatus != MfdWindowFramebufferPluginResultCode_Success)
        {
            error = PluginErrorBufferToString(errorBuffer);
            if (error.empty())
            {
                error = "The framebuffer plugin factory failed: " + PluginResultCodeDescription(getApiStatus) + ".";
            }
            Unload();
            return false;
        }

        if (pluginApi_.struct_size < offsetof(MfdWindowFramebufferPluginApi, destroy) + sizeof(pluginApi_.destroy) ||
            pluginApi_.info.struct_size < offsetof(MfdWindowFramebufferPluginInfo, display_name) +
                                               sizeof(pluginApi_.info.display_name))
        {
            error = "The framebuffer plugin '" + pluginFile.string() + "' returned an incomplete ABI descriptor.";
            Unload();
            return false;
        }

        if (pluginApi_.info.abi_version != MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION)
        {
            error = "The framebuffer plugin '" + pluginFile.string() + "' targets ABI version " +
                    std::to_string(pluginApi_.info.abi_version) + " instead of " +
                    std::to_string(MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION) + ".";
            Unload();
            return false;
        }

        if (pluginApi_.init == nullptr ||
            pluginApi_.submit_frame == nullptr ||
            pluginApi_.close == nullptr ||
            pluginApi_.destroy == nullptr)
        {
            error = "The framebuffer plugin '" + pluginFile.string() + "' returned one incomplete callback table.";
            Unload();
            return false;
        }

        ResetPluginErrorBuffer(errorBuffer, errorStorage.data(), errorStorage.size());
        try
        {
            const MfdWindowFramebufferPluginResultCode initStatus =
                pluginApi_.init(pluginApi_.plugin_context, &hostApi_, &errorBuffer);
            if (initStatus != MfdWindowFramebufferPluginResultCode_Success)
            {
                error = PluginErrorBufferToString(errorBuffer);
                if (error.empty())
                {
                    error = "The framebuffer plugin refused to initialize: " +
                            PluginResultCodeDescription(initStatus) + ".";
                }
                Unload();
                return false;
            }
        }
        catch (const std::exception& exception)
        {
            error = "The framebuffer plugin init routine threw: " + std::string(exception.what());
            Unload();
            return false;
        }
        catch (...)
        {
            error = "The framebuffer plugin init routine threw an unknown exception.";
            Unload();
            return false;
        }

        pluginInitialized_ = true;
        return true;
#else
        error = "Framebuffer plugins are only supported on Windows.";
        return false;
#endif
    }

    [[nodiscard]] mfd::window::LauncherFramebufferCallback CreateCallback()
    {
        if (!pluginInitialized_ || pluginApi_.submit_frame == nullptr)
        {
            return {};
        }

        return [this](const int width, const int height, const mfd::ByteView rgba32Bytes)
        {
            SubmitFrame(width, height, rgba32Bytes);
        };
    }

private:
    void SubmitFrame(const int width, const int height, const mfd::ByteView rgba32Bytes) noexcept
    {
        if (!pluginInitialized_ || pluginApi_.submit_frame == nullptr)
        {
            return;
        }

        const std::size_t byteCount = mfd::window::ComputeFramebufferRgba32ByteCount(width, height);

        MfdWindowFramebufferFrame frame {};
        frame.struct_size = sizeof(frame);
        frame.pixel_format = MfdWindowFramebufferPixelFormat_Rgba32;
        frame.width = width;
        frame.height = height;
        frame.row_stride_bytes =
            (byteCount != 0U && height > 0) ? byteCount / static_cast<std::size_t>(height) : 0U;
        frame.pixels = reinterpret_cast<const std::uint8_t*>(rgba32Bytes.data());
        frame.pixel_bytes = rgba32Bytes.size();

        std::array<char, kPluginErrorBufferCapacity> errorStorage {};
        MfdWindowUtf8Buffer errorBuffer {};
        ResetPluginErrorBuffer(errorBuffer, errorStorage.data(), errorStorage.size());

        try
        {
            const MfdWindowFramebufferPluginResultCode submitStatus =
                pluginApi_.submit_frame(pluginApi_.plugin_context, &frame, &errorBuffer);
            if (submitStatus == MfdWindowFramebufferPluginResultCode_Success)
            {
                lastRuntimeError_.clear();
                lastReportedRuntimeError_.clear();
                return;
            }

            std::string runtimeError = PluginErrorBufferToString(errorBuffer);
            if (runtimeError.empty())
            {
                runtimeError = PluginResultCodeDescription(submitStatus);
            }
            ReportRuntimeError(std::move(runtimeError));
        }
        catch (const std::exception& exception)
        {
            ReportRuntimeError(exception.what());
        }
        catch (...)
        {
            ReportRuntimeError("Unknown framebuffer plugin exception.");
        }
    }

    void ReportRuntimeError(std::string message) noexcept
    {
        if (message.empty())
        {
            message = "Unknown framebuffer plugin runtime failure.";
        }

        lastRuntimeError_ = message;
        if (lastReportedRuntimeError_ == lastRuntimeError_)
        {
            return;
        }

        lastReportedRuntimeError_ = lastRuntimeError_;
        std::cerr << "Framebuffer plugin runtime error: " << lastRuntimeError_ << '\n';
    }

    void Unload() noexcept
    {
        if (pluginInitialized_ && pluginApi_.close != nullptr)
        {
            try
            {
                pluginApi_.close(pluginApi_.plugin_context);
            }
            catch (...)
            {
            }
        }

        if (pluginApi_.destroy != nullptr)
        {
            try
            {
                pluginApi_.destroy(pluginApi_.plugin_context);
            }
            catch (...)
            {
            }
        }

        pluginInitialized_ = false;
        pluginApi_ = {};
        hostApi_ = {};
        lastRuntimeError_.clear();
        lastReportedRuntimeError_.clear();

#if defined(_WIN32)
        if (handle_ != nullptr)
        {
            ::FreeLibrary(handle_);
            handle_ = nullptr;
        }
#endif
    }

#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#endif
    MfdWindowFramebufferPluginHostApi hostApi_ {};
    MfdWindowFramebufferPluginApi pluginApi_ {};
    bool pluginInitialized_ = false;
    std::string lastRuntimeError_ {};
    std::string lastReportedRuntimeError_ {};
};

bool ParseCommandLine(const int argc,
                      char** argv,
                      const mfd::window::LauncherConfig& config,
                      CommandLineOptions& options,
                      std::string& error)
{
    options.windowFile = config.defaultWindowFile;
    bool positionalWindowConsumed = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index] != nullptr ? std::string_view {argv[index]} : std::string_view {};

        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            return true;
        }

        if (argument == "--window" || argument == "-w")
        {
            if (index + 1 >= argc || argv[index + 1] == nullptr)
            {
                error = "Missing path after '--window'.";
                return false;
            }

            options.windowFile = argv[++index];
            positionalWindowConsumed = true;
            continue;
        }

        if (argument == "--framebuffer-plugin" || argument == "-p")
        {
            if (index + 1 >= argc || argv[index + 1] == nullptr)
            {
                error = "Missing path after '--framebuffer-plugin'.";
                return false;
            }

            options.framebufferPluginFile = argv[++index];
            continue;
        }

        if (!argument.empty() && argument.front() == '-')
        {
            error = "Unknown option: " + std::string(argument);
            return false;
        }

        if (positionalWindowConsumed)
        {
            error = "Only one window JSON path can be provided.";
            return false;
        }

        options.windowFile = std::filesystem::path {std::string(argument)};
        positionalWindowConsumed = true;
    }

    if (options.windowFile.empty())
    {
        error = "No window JSON path was provided and no default launcher window is configured.";
        return false;
    }

    return true;
}

void ReportFatalError(const std::string_view applicationName, const std::string& message)
{
    std::cerr << applicationName << " fatal error: " << message << '\n';
}

class GenericWindowApplication
{
public:
    GenericWindowApplication(std::string applicationName,
                             std::filesystem::path windowFile,
                             mfd::window::LauncherFramebufferCallback framebufferCallback) :
        applicationName_(std::move(applicationName)),
        windowFile_(std::move(windowFile)),
        framebufferCallback_(std::move(framebufferCallback)),
        commandProcessor_(scene_)
    {
        if (framebufferCallback_)
        {
            framebufferCapture_ = std::make_unique<AsyncFramebufferCapture>();
        }
    }

    int Run()
    {
        if (!ReloadConfiguration())
        {
            throw std::runtime_error("Unable to load window JSON '" + windowFile_.string() + "': " + lastReloadError_);
        }

        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(windowDefinition_.width, windowDefinition_.height, windowDefinition_.title.c_str());
        debugOverlay_.Initialize();
        RefreshBranding();
        ApplyWindowHostLayout(false);

        PrintStartupSummary();

        while (!WindowShouldClose())
        {
            const float deltaSeconds = GetFrameTime();

            try
            {
                HandleShortcuts();
                Update(deltaSeconds);
            }
            catch (const std::exception& exception)
            {
                lastRuntimeError_ = exception.what();
            }
            catch (...)
            {
                lastRuntimeError_ = "Unknown exception during update";
            }

            BeginDrawing();

            try
            {
                const mfd::SceneRegistry& renderScene = debugOverlay_.RenderScene(scene_);
                const bool debugWasActiveDuringDraw = debugOverlay_.Active();
                if (ShouldDrawStartupSplash())
                {
                    DrawStartupSplash();
                }
                else
                {
                    ClearBackground(ToRayColor(renderScene.ActiveBackgroundColor()));
                    renderer_.DrawActivePage(renderScene, RenderViewportWidth(), GetScreenHeight());
                }
                debugOverlay_.Draw(scene_, windowDefinition_, applicationName_, lastRuntimeError_);
                if (debugWasActiveDuringDraw != debugOverlay_.Active())
                {
                    ApplyWindowHostLayout(true);
                }
                PublishFramebuffer();
            }
            catch (const std::exception& exception)
            {
                lastRuntimeError_ = exception.what();
            }
            catch (...)
            {
                lastRuntimeError_ = "Unknown exception during rendering";
            }

            EndDrawing();

            if (windowDefinition_.targetFps <= 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
        }

        debugOverlay_.Shutdown();
        framebufferCapture_.reset();
        CloseWindow();
        return 0;
    }

private:
    void RefreshBranding()
    {
        resolvedIconFile_ = mfd::ResolveWindowBrandingIconFile(windowDefinition_.iconFile, windowFile_);
        brandingStatus_.clear();

        if (IsWindowReady() && !resolvedIconFile_.empty())
        {
            std::string iconError;
            if (!mfd::ApplyWindowIconFile(resolvedIconFile_, &iconError) && !iconError.empty())
            {
                brandingStatus_ = iconError;
            }
        }
    }

    [[nodiscard]] bool ShouldDrawStartupSplash() const noexcept
    {
        return !debugOverlay_.SuppressStartupSplash() &&
               udpRuntimeBridge_ != nullptr &&
               udpRuntimeBridge_->HasCommandReceiver() &&
               !receivedFirstClientCommand_;
    }

    void DrawStartupSplash() const
    {
        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        if (renderWidth <= 0 || renderHeight <= 0)
        {
            return;
        }

        DrawRectangleGradientV(0, 0, renderWidth, renderHeight, Color {7, 15, 21, 255}, Color {2, 7, 11, 255});

        const float screenMin = static_cast<float>(std::min(renderWidth, renderHeight));
        const Rectangle cardBounds {
            std::max(36.0f, (static_cast<float>(renderWidth) - std::clamp(screenMin * 0.78f, 360.0f, 760.0f)) * 0.5f),
            std::max(30.0f, static_cast<float>(renderHeight) * 0.5f - std::clamp(screenMin * 0.18f, 108.0f, 160.0f)),
            std::clamp(screenMin * 0.78f, 360.0f, 760.0f),
            std::clamp(screenMin * 0.36f, 216.0f, 320.0f)};

        DrawRectangleRounded(cardBounds, 0.08f, 18, Color {9, 19, 26, 232});
        DrawRectangleRoundedLinesEx(cardBounds, 0.08f, 18, 2.0f, Color {42, 86, 78, 255});

        const std::string title = windowDefinition_.title.empty() ? applicationName_ : windowDefinition_.title;
        const int titleFontSize = std::clamp(static_cast<int>(screenMin * 0.048f), 26, 42);
        const int subtitleFontSize = std::clamp(static_cast<int>(screenMin * 0.022f), 16, 22);
        const int statusFontSize = 16;
        const char* subtitle = "Waiting for the first client command...";
        const float accentHeight = 4.0f;
        const float contentTop = cardBounds.y + 34.0f;

        const int titleWidth = MeasureText(title.c_str(), titleFontSize);
        const int subtitleWidth = MeasureText(subtitle, subtitleFontSize);
        const int statusWidth = brandingStatus_.empty() ? 0 : MeasureText(brandingStatus_.c_str(), statusFontSize);
        const int titleX = (renderWidth - titleWidth) / 2;
        const int subtitleX = (renderWidth - subtitleWidth) / 2;
        const int statusX = (renderWidth - statusWidth) / 2;
        const int titleY = static_cast<int>(contentTop + 42.0f);
        const int subtitleY = titleY + titleFontSize + 12;
        const int statusY = subtitleY + subtitleFontSize + 18;

        DrawRectangleRounded(
            Rectangle {cardBounds.x + 28.0f, cardBounds.y + 18.0f, cardBounds.width - 56.0f, accentHeight},
            1.0f,
            8,
            Color {78, 168, 142, 255});

        DrawText(title.c_str(), titleX, titleY, titleFontSize, Color {227, 242, 236, 255});
        DrawText(subtitle, subtitleX, subtitleY, subtitleFontSize, Color {117, 198, 176, 255});

        if (!brandingStatus_.empty())
        {
            DrawText(brandingStatus_.c_str(), statusX, statusY, statusFontSize, Color {250, 176, 96, 255});
        }
    }

    [[nodiscard]] int RenderViewportWidth() const noexcept
    {
        const int hostWidth = GetScreenWidth();
        if (!debugOverlay_.Active())
        {
            return hostWidth;
        }

        return std::max(1, hostWidth - debugOverlay_.PreferredPanelWidth());
    }

    void ApplyWindowHostLayout(const bool preserveCurrentPosition)
    {
        if (!IsWindowReady())
        {
            return;
        }

        int positionX = windowDefinition_.positionX;
        int positionY = windowDefinition_.positionY;
        if (preserveCurrentPosition)
        {
            const Vector2 currentPosition = GetWindowPosition();
            positionX = static_cast<int>(currentPosition.x);
            positionY = static_cast<int>(currentPosition.y);
        }

        const int targetWidth =
            std::max(1, windowDefinition_.width + (debugOverlay_.Active() ? debugOverlay_.PreferredPanelWidth() : 0));
        const int targetHeight = std::max(1, windowDefinition_.height);

        SetWindowMinSize(targetWidth, targetHeight);
        SetWindowSize(targetWidth, targetHeight);
        SetWindowPosition(positionX, positionY);

        if (windowDefinition_.targetFps > 0)
        {
            SetTargetFPS(windowDefinition_.targetFps);
        }
    }

    bool ReloadConfiguration()
    {
        try
        {
            const std::string previousPage = scene_.ActivePageName();
            mfd::LoadedWindowConfiguration loaded = loader_.LoadWindowConfiguration(windowFile_);
            if (loaded.document.pages.empty())
            {
                throw std::runtime_error("The window JSON does not contain any page.");
            }

            windowDefinition_ = loaded.window;
            scene_.LoadDocument(std::move(loaded.document), std::move(loaded.generatedTransportMap));
            renderer_.SetTextFontFile(windowDefinition_.fontFile);
            udpRuntimeBridge_ = std::make_unique<mfd::UdpRuntimeBridge>(
                windowDefinition_.commandTransports,
                windowDefinition_.feedbackTransports);
            udpRuntimeBridge_->Start();

            if (scene_.HasPage(previousPage))
            {
                scene_.SetActivePage(previousPage);
            }

            strobeFeedbackAccumulator_ = 0.0f;
            nextStrobeFeedbackSequence_ = 1;
            receivedFirstClientCommand_ = false;
            lastRuntimeError_.clear();

            if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->HasCommandReceiver())
            {
                lastCommandStatus_ = "No UDP command transport configured in the window JSON.";
            }
            else
            {
                lastCommandStatus_ = udpRuntimeBridge_->LastCommandStatus();
            }

            if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->HasFeedbackSender())
            {
                lastFeedbackStatus_ = "No UDP feedback transport configured in the window JSON.";
            }
            else
            {
                lastFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
            }

            if (IsWindowReady())
            {
                SetWindowTitle(windowDefinition_.title.c_str());
                ApplyWindowHostLayout(false);
                RefreshBranding();
            }

            debugOverlay_.OnRuntimeReloaded(scene_);
            lastReloadError_.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            lastReloadError_ = exception.what();
            return false;
        }
    }

    void HandleShortcuts()
    {
        const bool debugWasActive = debugOverlay_.Active();
        if (debugOverlay_.HandleShortcut(scene_))
        {
            if (debugWasActive != debugOverlay_.Active())
            {
                ApplyWindowHostLayout(true);
            }
            return;
        }

        if (debugOverlay_.ConsumeRuntimeShortcuts())
        {
            return;
        }

        if (IsKeyPressed(KEY_R))
        {
            if (!ReloadConfiguration())
            {
                throw std::runtime_error(lastReloadError_);
            }
        }

        static constexpr std::array<int, 9> keyBindings {
            KEY_ONE,
            KEY_TWO,
            KEY_THREE,
            KEY_FOUR,
            KEY_FIVE,
            KEY_SIX,
            KEY_SEVEN,
            KEY_EIGHT,
            KEY_NINE};

        const auto pages = scene_.Pages();
        for (std::size_t index = 0; index < pages.size() && index < keyBindings.size(); ++index)
        {
            if (IsKeyPressed(keyBindings[index]))
            {
                scene_.SetActivePage(pages[index].name);
            }
        }
    }

    void Update(const float deltaSeconds)
    {
        appliedCommandBatches_.clear();

        if (udpRuntimeBridge_ != nullptr)
        {
            pendingCommandBatches_.clear();
            const std::size_t drainedBatches = udpRuntimeBridge_->DrainReceivedBatches(pendingCommandBatches_);
            if (drainedBatches > 0)
            {
                receivedFirstClientCommand_ = true;
                std::size_t drainedCommands = 0;
                bool success = true;
                bool truncated = false;

                for (const mfd::CommandBatch& batch : pendingCommandBatches_)
                {
                    drainedCommands += batch.commands.size();
                }

                std::size_t appliedCommands = 0;
                for (const mfd::CommandBatch& batch : pendingCommandBatches_)
                {
                    if (appliedCommands >= kMaxCommandsPerFrame)
                    {
                        break;
                    }

                    const std::size_t batchCommandCount = batch.commands.size();
                    if (appliedCommands + batchCommandCount > kMaxCommandsPerFrame)
                    {
                        truncated = true;
                        break;
                    }

                    success = commandProcessor_.Submit(batch) && success;
                    appliedCommands += batchCommandCount;
                    appliedCommandBatches_.push_back(batch);
                }

                if (success)
                {
                    lastCommandStatus_ = truncated
                                             ? "Applied " + std::to_string(appliedCommands) +
                                                   " command(s) from the UDP I/O thread (truncated from " +
                                                   std::to_string(drainedCommands) + ")."
                                             : "Applied " + std::to_string(appliedCommands) +
                                                   " command(s) from the UDP I/O thread.";
                }
                else if (!commandProcessor_.LastError().empty())
                {
                    lastCommandStatus_ = commandProcessor_.LastError();
                }
            }
            else if (!udpRuntimeBridge_->LastCommandStatus().empty())
            {
                lastCommandStatus_ = udpRuntimeBridge_->LastCommandStatus();
            }
        }

        PublishStrobeFeedbacks(deltaSeconds);
        debugOverlay_.Synchronize(
            scene_,
            udpRuntimeBridge_.get(),
            lastCommandStatus_,
            lastFeedbackStatus_,
            appliedCommandBatches_);
    }

    void PublishStrobeFeedbacks(const float deltaSeconds)
    {
        if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->FeedbackTransportReady())
        {
            return;
        }

        strobeFeedbackAccumulator_ += std::max(deltaSeconds, 0.0f);
        if (strobeFeedbackAccumulator_ < kStrobeFeedbackIntervalSeconds)
        {
            return;
        }

        strobeFeedbackAccumulator_ = std::fmod(strobeFeedbackAccumulator_, kStrobeFeedbackIntervalSeconds);

        for (const mfd::PageSummary& page : scene_.Pages())
        {
            if (!page.hasStrobe)
            {
                continue;
            }

            const auto strobe = scene_.StrobeForPage(page.name);
            if (!strobe.has_value())
            {
                continue;
            }

            mfd::StrobeStatusFeedback feedback;
            feedback.sequence = nextStrobeFeedbackSequence_++;
            feedback.pageId = strobe->pageId;
            feedback.pageName = strobe->pageName;
            feedback.strobeId = strobe->reticleId;
            feedback.active = strobe->visible;
            feedback.position = strobe->position;
            feedback.capture = strobe->capture;
            feedback.magnet = ToFeedbackMagnet(scene_.StrobeMagnetForPage(page.name));

            if (const auto capture = scene_.CaptureWithStrobe(page.name); capture.has_value())
            {
                feedback.captureResult = ToFeedbackCapture(*capture);
            }

            udpRuntimeBridge_->EnqueueStrobeFeedback(std::move(feedback));
        }

        mfd::ActivePageFeedback activePageFeedback;
        activePageFeedback.sequence = nextStrobeFeedbackSequence_++;
        activePageFeedback.pageName = scene_.ActivePageName();
        udpRuntimeBridge_->EnqueueActivePageFeedback(std::move(activePageFeedback));

        if (!udpRuntimeBridge_->LastFeedbackStatus().empty())
        {
            lastFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
        }
    }

    void PrintStartupSummary() const
    {
        std::cout << applicationName_ << '\n';
        std::cout << "Window JSON: " << windowFile_.string() << '\n';
        std::cout << "Window title: " << windowDefinition_.title << '\n';
        if (!resolvedIconFile_.empty())
        {
            std::cout << "Branding icon: " << resolvedIconFile_.string() << '\n';
        }
        std::cout << "Pages: " << scene_.Pages().size() << '\n';
        std::cout << "Shortcuts: F1 toggles runtime debug, R reloads, 1..9 switch pages\n";
    }

    [[nodiscard]] bool PrepareFramebufferCapture()
    {
        if (!framebufferCallback_)
        {
            return false;
        }

        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        if (renderWidth <= 0 || renderHeight <= 0)
        {
            framebufferCapture_.reset();
            lastObservedRenderWidth_ = 0;
            lastObservedRenderHeight_ = 0;
            framebufferResizeCooldownFrames_ = 0;
            return false;
        }

        if (lastObservedRenderWidth_ <= 0 || lastObservedRenderHeight_ <= 0)
        {
            lastObservedRenderWidth_ = renderWidth;
            lastObservedRenderHeight_ = renderHeight;
        }
        else if (renderWidth != lastObservedRenderWidth_ || renderHeight != lastObservedRenderHeight_)
        {
            // Tear down the readback path while the host recreates its swapchain-sized resources.
            lastObservedRenderWidth_ = renderWidth;
            lastObservedRenderHeight_ = renderHeight;
            framebufferCapture_.reset();
            framebufferResizeCooldownFrames_ = kFramebufferResizeCooldownFrames;
            return false;
        }

        if (framebufferResizeCooldownFrames_ > 0)
        {
            --framebufferResizeCooldownFrames_;
            return false;
        }

        if (framebufferCapture_ == nullptr)
        {
            framebufferCapture_ = std::make_unique<AsyncFramebufferCapture>();
        }

        return framebufferCapture_ != nullptr;
    }

    void PublishFramebuffer()
    {
        if (!PrepareFramebufferCapture())
        {
            return;
        }

        const std::optional<mfd::Rgba32Framebuffer> framebuffer = framebufferCapture_->Capture();
        if (!framebuffer.has_value())
        {
            return;
        }

        framebufferCallback_(framebuffer->width, framebuffer->height, framebuffer->Bytes());
    }

    std::string applicationName_;
    std::filesystem::path windowFile_;
    mfd::window::LauncherFramebufferCallback framebufferCallback_ {};
    mfd::JsonLoader loader_ {};
    mfd::SceneRegistry scene_ {};
    mfd::CommandProcessor commandProcessor_;
    mfd::MfdRenderer renderer_ {};
    mfd::window::debug::RuntimeDebugOverlay debugOverlay_ {};
    std::unique_ptr<AsyncFramebufferCapture> framebufferCapture_ {};
    mfd::WindowAssetDefinition windowDefinition_ {};
    std::unique_ptr<mfd::UdpRuntimeBridge> udpRuntimeBridge_ {};
    std::vector<mfd::CommandBatch> pendingCommandBatches_ {};
    std::vector<mfd::CommandBatch> appliedCommandBatches_ {};
    float strobeFeedbackAccumulator_ = 0.0f;
    std::uint32_t nextStrobeFeedbackSequence_ = 1;
    bool receivedFirstClientCommand_ = false;
    int lastObservedRenderWidth_ = 0;
    int lastObservedRenderHeight_ = 0;
    int framebufferResizeCooldownFrames_ = 0;
    std::filesystem::path resolvedIconFile_ {};
    std::string brandingStatus_ {};
    std::string lastCommandStatus_ {};
    std::string lastFeedbackStatus_ {};
    std::string lastReloadError_ {};
    std::string lastRuntimeError_ {};
};
} // namespace

namespace mfd::window
{
std::string BuildUsageText(const LauncherConfig& config)
{
    const std::string applicationName = config.applicationName.empty() ? std::string {"mfd_window"} : config.applicationName;

    std::ostringstream output;
    output << "Usage:\n";
    output << "  " << applicationName << " <window.json>\n";
    output << "  " << applicationName << " --window <window.json>\n";
    output << "  " << applicationName << " --window <window.json> --framebuffer-plugin <plugin.dll>\n";
    output << "  " << applicationName << " --help\n";
    output << '\n';
    if (config.defaultWindowFile.empty())
    {
        output << "No default window JSON is configured. Pass one explicitly.\n";
    }
    else
    {
        output << "If no window JSON is provided, the launcher defaults to '"
               << config.defaultWindowFile.string() << "'.\n";
    }
    output << "Optional framebuffer plugins must export '" << kLauncherFramebufferPluginEntryPointName
           << "' and implement the stable framebuffer-plugin ABI.\n";
    output << "Shortcuts:\n";
    output << "  F1 toggles the integrated runtime debug overlay\n";
    output << "  R reloads the current window JSON from disk\n";
    output << "  1..9 activate the first nine authored pages\n";
    return output.str();
}

bool ParseLauncherCommandLine(const int argc,
                              char** argv,
                              const LauncherConfig& config,
                              LauncherOptions& options,
                              std::string& error)
{
    CommandLineOptions internalOptions;
    const bool parsed = ParseCommandLine(argc, argv, config, internalOptions, error);
    options.windowFile = std::move(internalOptions.windowFile);
    options.framebufferPluginFile = std::move(internalOptions.framebufferPluginFile);
    options.showHelp = internalOptions.showHelp;
    return parsed;
}

int RunLauncher(int argc, char** argv, const LauncherConfig& config, LauncherFramebufferCallback framebufferCallback)
{
    const std::string applicationName = config.applicationName.empty() ? std::string {"mfd_window"} : config.applicationName;

    LauncherOptions options;
    std::string error;
    if (!ParseLauncherCommandLine(argc, argv, config, options, error))
    {
        std::cerr << error << '\n';
        LauncherConfig usageConfig;
        usageConfig.applicationName = applicationName;
        usageConfig.defaultWindowFile = config.defaultWindowFile;
        std::cerr << BuildUsageText(usageConfig);
        return 1;
    }

    if (options.showHelp)
    {
        LauncherConfig usageConfig;
        usageConfig.applicationName = applicationName;
        usageConfig.defaultWindowFile = config.defaultWindowFile;
        std::cout << BuildUsageText(usageConfig);
        return 0;
    }

    std::unique_ptr<LoadedFramebufferPlugin> framebufferPlugin;
    LauncherFramebufferCallback effectiveFramebufferCallback = std::move(framebufferCallback);
    if (!options.framebufferPluginFile.empty())
    {
        if (effectiveFramebufferCallback)
        {
            ReportFatalError(
                applicationName,
                "The launcher received both a direct framebuffer callback and '--framebuffer-plugin'. Choose one.");
            return 1;
        }

        framebufferPlugin = std::make_unique<LoadedFramebufferPlugin>();
        if (!framebufferPlugin->Load(options.framebufferPluginFile, error))
        {
            ReportFatalError(applicationName, error);
            return 1;
        }

        effectiveFramebufferCallback = framebufferPlugin->CreateCallback();
    }

#if defined(_WIN32)
    if (::IsDebuggerPresent() != 0)
    {
        GenericWindowApplication application(applicationName, options.windowFile, std::move(effectiveFramebufferCallback));
        return application.Run();
    }
#endif

    try
    {
        GenericWindowApplication application(applicationName, options.windowFile, std::move(effectiveFramebufferCallback));
        return application.Run();
    }
    catch (const std::exception& exception)
    {
        ReportFatalError(applicationName, exception.what());
        return 1;
    }
}
} // namespace mfd::window
