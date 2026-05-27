# MFDStudioWindowLauncherPlugin

`MFDStudioWindowLauncherPlugin` is the public development package for stable
framebuffer-processing plugins loaded by `mfd_window` through
`--framebuffer-plugin`.

It provides:

- the public stable ABI header `mfd/window/WindowLauncherPlugin.h`
- the exported support helpers validating raw `RGBA32` and `BGRA32` frame layouts
- the stable entry point `MfdGetWindowFramebufferPluginApi`

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
#include "mfd/window/WindowLauncherPlugin.h"

namespace
{
struct PluginContext
{
    bool printed = false;
};

MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL InitPlugin(
    void* pluginContext,
    const MfdWindowFramebufferPluginHostApi* host,
    MfdWindowUtf8Buffer*) noexcept
{
    if (pluginContext == nullptr || host == nullptr ||
        host->output_pixel_format != MfdWindowFramebufferPixelFormat_Bgra32)
    {
        return MfdWindowFramebufferPluginResultCode_InvalidArgument;
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
                  << " bytes=" << frame->pixel_bytes << '\n';
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
