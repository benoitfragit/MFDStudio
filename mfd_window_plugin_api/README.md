# MFDStudioWindowLauncherPlugin

`MFDStudioWindowLauncherPlugin` is the public development package for stable
framebuffer-processing plugins loaded by `mfd_window` through
`--framebuffer-plugin`.

It provides:

- the public stable ABI header `mfd/window/WindowLauncherPlugin.h`
- the exported support helpers validating raw `RGBA32` and `BGRA32` frame layouts
- the stable entry point `MfdGetWindowFramebufferPluginApi`
- the `mfd_window` launch argument view passed to plugin initialization

Typical external use:

```cmake
find_package(MFDStudioWindowLauncherPlugin REQUIRED CONFIG)

add_library(my_framebuffer_plugin SHARED src/MyFramebufferPlugin.cpp)
target_link_libraries(my_framebuffer_plugin PRIVATE MFDStudio::WindowLauncherPlugin)
```

This package is intentionally a pure SDK package: it does not ship a ready-made
framebuffer plugin. External users are expected to build their own plugin DLL
against this contract.

Plugin entry point example:

```cpp
#include <cstddef>
#include <cstring>
#include <iostream>

#include "mfd/window/WindowLauncherPlugin.h"

namespace
{
struct PluginContext
{
    bool printed = false;
    int launchArgumentCount = 0;
    char programName[128] {};
    char label[64] {};
};

bool HostLaunchArgumentsAreValid(const MfdWindowFramebufferPluginHostApi& host) noexcept
{
    if (host.launch_argc < 0)
    {
        return false;
    }

    return host.launch_argc == 0 ? host.launch_argv == nullptr : host.launch_argv != nullptr;
}

void CopyCString(char* destination, const std::size_t capacity, const char* source) noexcept
{
    if (destination == nullptr || capacity == 0U)
    {
        return;
    }

    const std::size_t sourceLength = source == nullptr ? 0U : std::strlen(source);
    const std::size_t copiedLength = (sourceLength < capacity - 1U) ? sourceLength : capacity - 1U;
    if (copiedLength > 0U && source != nullptr)
    {
        std::memcpy(destination, source, copiedLength);
    }
    destination[copiedLength] = '\0';
}

MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL InitPlugin(
    void* pluginContext,
    const MfdWindowFramebufferPluginHostApi* host,
    MfdWindowUtf8Buffer*) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->struct_size < sizeof(MfdWindowFramebufferPluginHostApi) ||
        host->abi_version != MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION ||
        host->output_pixel_format != MfdWindowFramebufferPixelFormat_Bgra32 ||
        !HostLaunchArgumentsAreValid(*host))
    {
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    auto* context = static_cast<PluginContext*>(pluginContext);
    context->launchArgumentCount = host->launch_argc;
    CopyCString(
        context->programName,
        sizeof(context->programName),
        (host->launch_argc > 0 && host->launch_argv[0] != nullptr) ? host->launch_argv[0] : "");

    CopyCString(context->label, sizeof(context->label), "default");
    for (int index = 1; index < host->launch_argc; ++index)
    {
        if (host->launch_argv[index] != nullptr &&
            std::strcmp(host->launch_argv[index], "--stdout-label") == 0 &&
            index + 1 < host->launch_argc &&
            host->launch_argv[index + 1] != nullptr)
        {
            CopyCString(context->label, sizeof(context->label), host->launch_argv[index + 1]);
            ++index;
        }
    }
    return MfdWindowFramebufferPluginResultCode_Success;
}

MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL SubmitFramePlugin(
    void* pluginContext,
    const MfdWindowFramebufferFrame* frame,
    MfdWindowUtf8Buffer*) noexcept
{
    auto* context = static_cast<PluginContext*>(pluginContext);
    if (context == nullptr || frame == nullptr || MfdWindowValidateFramebufferFrame(frame) == 0)
    {
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    if (!context->printed)
    {
        std::cout << "Received " << frame->width << "x" << frame->height
                  << " pixels in format " << frame->pixel_format
                  << " bytes=" << frame->pixel_bytes
                  << " launch_args=" << context->launchArgumentCount;
        if (context->label[0] != '\0')
        {
            std::cout << " label=" << context->label;
        }
        if (context->programName[0] != '\0')
        {
            std::cout << " argv0=" << context->programName;
        }
        std::cout << '\n';
        context->printed = true;
    }

    return MfdWindowFramebufferPluginResultCode_Success;
}

void MFD_WINDOW_PLUGIN_CALL ClosePlugin(void*) noexcept
{
}

void MFD_WINDOW_PLUGIN_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<PluginContext*>(pluginContext);
}
} // namespace

extern "C" __declspec(dllexport) MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL
MfdGetWindowFramebufferPluginApi(MfdWindowFramebufferPluginApi* outApi, MfdWindowUtf8Buffer*) noexcept
{
    if (outApi == nullptr)
    {
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
    }

    auto* context = new PluginContext();
    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION;
    outApi->info.requested_pixel_format = MfdWindowFramebufferPixelFormat_Bgra32;
    outApi->plugin_context = context;
    outApi->init = &InitPlugin;
    outApi->submit_frame = &SubmitFramePlugin;
    outApi->close = &ClosePlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdWindowFramebufferPluginResultCode_Success;
}
```

During `init`, the plugin receives:

- `host->abi_version`
- `host->output_pixel_format`
- `host->launch_argc`, matching the `argc` passed to `mfd_window`
- `host->launch_argv`, matching the `argv` passed to `mfd_window`

The launch argument array is borrowed from the host. Treat every pointer as
read-only and copy any value the plugin needs to keep after it is unloaded.
`mfd_window` parses only the arguments needed by the window launcher. When a
framebuffer plugin is loaded, the plugin still receives the original `argc` /
`argv` view. The sample above reads `--stdout-label <text>` this way; no
host-side option needs to be added for that plugin-specific setting.

During `submit_frame`, the plugin receives:

- `frame->width`
- `frame->height`
- `frame->pixels`
- `frame->pixel_bytes`
- `frame->row_stride_bytes`
- `frame->pixel_format`, matching `outApi->info.requested_pixel_format`

The stable ABI currently supports:

- `MfdWindowFramebufferPixelFormat_Rgba32`
- `MfdWindowFramebufferPixelFormat_Bgra32`, compatible with `GL_BGRA_EXT`

The pixel buffer is borrowed and valid only for the duration of the
`submit_frame` call. Copy it if another system needs to keep it afterwards.
