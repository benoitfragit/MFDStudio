/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Sample stable framebuffer plugin printing one confirmation line on the first frame.
 */

#include <cstring>
#include <iostream>
#include <new>

#include "mfd/window/WindowLauncherPlugin.h"

namespace
{
/**
 * @brief Minimal runtime state owned by the sample plugin instance.
 */
struct FramebufferStdoutPluginContext
{
    bool printed = false;
};

/**
 * @brief Writes one human-readable message into an ABI output buffer.
 * @param buffer Destination UTF-8 buffer.
 * @param message Null-terminated message to copy.
 */
void WriteMessage(MfdWindowUtf8Buffer* buffer, const char* message) noexcept
{
    if (buffer == nullptr)
    {
        return;
    }

    const std::size_t length = message == nullptr ? 0U : std::strlen(message);
    buffer->size = length;
    if (buffer->data == nullptr || buffer->capacity == 0U)
    {
        return;
    }

    const std::size_t writable = (length < (buffer->capacity - 1U)) ? length : (buffer->capacity - 1U);
    if (writable > 0U && message != nullptr)
    {
        std::memcpy(buffer->data, message, writable);
    }
    buffer->data[writable] = '\0';
}

/**
 * @brief Builds one borrowed string view from a null-terminated UTF-8 literal.
 * @param text Source literal.
 * @return Borrowed string view over `text`.
 */
MfdWindowStringView MakeStringView(const char* text) noexcept
{
    return MfdWindowStringView {text, text == nullptr ? 0U : std::strlen(text)};
}

/**
 * @brief Initializes the sample plugin before the host enters the render loop.
 * @param pluginContext Opaque plugin context created by the factory.
 * @param host Host ABI descriptor.
 * @param error Optional UTF-8 error buffer written on failure.
 * @return Stable framebuffer-plugin result code.
 */
MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL InitPlugin(
    void* pluginContext,
    const MfdWindowFramebufferPluginHostApi* host,
    MfdWindowUtf8Buffer* error) noexcept
{
    if (pluginContext == nullptr || host == nullptr || host->abi_version != MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION)
    {
        WriteMessage(error, "The sample framebuffer plugin received an invalid host initialization request.");
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    auto* context = static_cast<FramebufferStdoutPluginContext*>(pluginContext);
    context->printed = false;
    return MfdWindowFramebufferPluginResultCode_Success;
}

/**
 * @brief Receives one raw `RGBA32` framebuffer from `mfd_window`.
 * @param pluginContext Opaque plugin context created by the factory.
 * @param frame Raw framebuffer descriptor owned by the host for the duration of the call.
 * @param error Optional UTF-8 error buffer written on failure.
 * @return Stable framebuffer-plugin result code.
 */
MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL SubmitFramePlugin(
    void* pluginContext,
    const MfdWindowFramebufferFrame* frame,
    MfdWindowUtf8Buffer* error) noexcept
{
    auto* context = static_cast<FramebufferStdoutPluginContext*>(pluginContext);
    if (context == nullptr || frame == nullptr)
    {
        WriteMessage(error, "The sample framebuffer plugin received a null frame request.");
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    if (MfdWindowValidateFramebufferFrame(frame) == 0)
    {
        WriteMessage(error, "Unexpected framebuffer layout received from mfd_window.");
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    if (!context->printed)
    {
        std::cout << "RGBA32 framebuffer plugin active: " << frame->width << "x" << frame->height
                  << " pixels=" << frame->pixel_bytes << '\n';
        context->printed = true;
    }

    return MfdWindowFramebufferPluginResultCode_Success;
}

/**
 * @brief Closes the plugin after the render loop stops.
 * @param pluginContext Opaque plugin context created by the factory.
 */
void MFD_WINDOW_PLUGIN_CALL ClosePlugin(void* pluginContext) noexcept
{
    if (auto* context = static_cast<FramebufferStdoutPluginContext*>(pluginContext); context != nullptr)
    {
        context->printed = false;
    }
}

/**
 * @brief Destroys the plugin-owned context allocated by the factory.
 * @param pluginContext Opaque plugin context created by the factory.
 */
void MFD_WINDOW_PLUGIN_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<FramebufferStdoutPluginContext*>(pluginContext);
}
} // namespace

/**
 * @brief Exports the stable framebuffer plugin callback table expected by `mfd_window`.
 * @param outApi Descriptor filled by the plugin.
 * @param error Optional UTF-8 error buffer written on failure.
 * @return Stable framebuffer-plugin result code.
 */
extern "C" __declspec(dllexport) MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL
MfdGetWindowFramebufferPluginApi(MfdWindowFramebufferPluginApi* outApi, MfdWindowUtf8Buffer* error) noexcept
{
    if (outApi == nullptr)
    {
        WriteMessage(error, "The sample framebuffer plugin received a null API descriptor.");
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    auto* context = new (std::nothrow) FramebufferStdoutPluginContext();
    if (context == nullptr)
    {
        WriteMessage(error, "The sample framebuffer plugin could not allocate its context.");
        return MfdWindowFramebufferPluginResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MakeStringView("mfd.framebuffer.stdout");
    outApi->info.display_name = MakeStringView("Framebuffer Stdout Sample");
    outApi->plugin_context = context;
    outApi->init = &InitPlugin;
    outApi->submit_frame = &SubmitFramePlugin;
    outApi->close = &ClosePlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdWindowFramebufferPluginResultCode_Success;
}
