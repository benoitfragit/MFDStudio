# Capture The Window As Raw Pixels

This tutorial shows how to capture the rendered window as a CPU buffer using
the OpenGL readback API.

These helpers belong to the repository host-side render layer used by
`mfd_window` and `mfd_editor`. They are not part of the standalone
`mfd_client_api` integration surface.

The direct host-side readback API returns `RGBA32`. The framebuffer plugin ABI
can now request either `RGBA32` or `BGRA32` output depending on what the
consumer expects.

## At A Glance

\startuml
left to right direction
rectangle "Rendered OpenGL framebuffer" as Framebuffer
rectangle "OpenGlFramebufferReader::ReadRgba32" as ReadRgba32
rectangle "Rgba32Framebuffer" as Rgba32Framebuffer
rectangle "Typed pixels" as TypedPixels
rectangle "Raw bytes" as RawBytes

Framebuffer --> ReadRgba32
ReadRgba32 --> Rgba32Framebuffer
Rgba32Framebuffer --> TypedPixels
Rgba32Framebuffer --> RawBytes
\enduml

The project exposes a typed `RGBA32` buffer:

- 4 channels
- 8 bits per channel
- one `Rgba8Pixel` per pixel

Internally the helper relies on the underlying OpenGL framebuffer readback
path.

## Step 1 - Include the API

```cpp
#include "mfd/render/OpenGlFramebufferReader.h"
```

## Step 2 - Capture the full framebuffer

```cpp
mfd::Rgba32Framebuffer capture = mfd::OpenGlFramebufferReader::ReadRgba32();
```

## Step 3 - Access typed pixels

```cpp
if (!capture.Empty())
{
    const mfd::Rgba8Pixel first = capture.Pixels().front();
}
```

## Step 4 - Access raw bytes

```cpp
mfd::ByteView raw = capture.Bytes();
```

This is useful when another system expects a generic byte buffer, for example:

- shared memory
- an IPC channel
- a recording pipeline
- a foreign image-processing API

## Step 5 - Capture only a sub-rectangle

```cpp
mfd::FramebufferCaptureRequest request;
request.x = 100;
request.y = 80;
request.width = 480;
request.height = 480;
request.origin = mfd::FramebufferOrigin::TopLeft;

mfd::Rgba32Framebuffer capture = mfd::OpenGlFramebufferReader::ReadRgba32(request);
```

## Step 6 - Understand the output layout

The returned object contains:

- `width`
- `height`
- `pixels`
- `Pixels()`
- `Bytes()`

The buffer is stored row by row.

## Step 7 - Typical use cases

Common use cases are:

- picture-in-picture rendering
- export to an external image consumer
- recording
- image-based integration tests
- shared processing pipelines after readback

## Step 8 - Forward frames from `mfd_window` through one DLL plugin

The generic runtime host now accepts one optional plugin DLL on the command
line:

```powershell
.\Scripts\Start-MfdDemo.bat
```

Under the hood this launches:

```powershell
mfd_window --window examples/demo/assets/windows/demo_window.json --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll --stdout-label demo
```

The sample plugin target is `mfd_framebuffer_stdout_plugin`. It exports one
stable entry point returning a versioned callback table:

```cpp
extern "C" __declspec(dllexport) MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL
MfdGetWindowFramebufferPluginApi(MfdWindowFramebufferPluginApi* outApi, MfdWindowUtf8Buffer*) noexcept
{
    outApi->info.requested_pixel_format = MfdWindowFramebufferPixelFormat_Bgra32;
    outApi->init = &InitPlugin;
    outApi->submit_frame = &SubmitFramePlugin;
    outApi->close = &ClosePlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdWindowFramebufferPluginResultCode_Success;
}
```

Before frames start flowing, `mfd_window` calls the plugin `init` callback with
the ABI host descriptor. That descriptor includes the same launch arguments the
host received:

```cpp
MfdWindowFramebufferPluginResultCode MFD_WINDOW_PLUGIN_CALL InitPlugin(
    void* pluginContext,
    const MfdWindowFramebufferPluginHostApi* host,
    MfdWindowUtf8Buffer*) noexcept
{
    auto* context = static_cast<MyPluginContext*>(pluginContext);
    context->launchArgumentCount = host->launch_argc;
    context->programName =
        (host->launch_argc > 0 && host->launch_argv[0] != nullptr) ? host->launch_argv[0] : "";
    return MfdWindowFramebufferPluginResultCode_Success;
}
```

`host->launch_argc` and `host->launch_argv` match the `argc` / `argv` passed to
`mfd_window`. Treat them as borrowed, read-only pointers and copy values that
the plugin needs to keep after it is unloaded. `mfd_window` parses only the
arguments needed by the window launcher. When a framebuffer plugin is loaded,
the plugin receives the original command line, including plugin-specific
arguments such as the sample `--stdout-label demo` shown above.

During `submit_frame`, the plugin receives one raw descriptor containing the
rendered runtime page viewport only:

- `frame->width`
- `frame->height`
- `frame->pixels`
- `frame->pixel_bytes`
- `frame->row_stride_bytes`
- `frame->pixel_format`, matching the plugin request made through `outApi->info.requested_pixel_format`

The stable plugin ABI currently supports:

- `MfdWindowFramebufferPixelFormat_Rgba32`
- `MfdWindowFramebufferPixelFormat_Bgra32`, compatible with `GL_BGRA_EXT`

The repository sample plugin now requests `BGRA32` to exercise the selectable
plugin output path end to end.

When the integrated `F1` runtime debug overlay is visible, `mfd_window` keeps
that side panel out of the plugin buffer. The forwarded `width`, `height`, and
pixel payload still describe only the page image rendered by the runtime host.

If no callback is provided to `RunLauncher()` and no `--framebuffer-plugin`
argument is passed to `mfd_window`, the launcher keeps the framebuffer capture
path completely disabled.

## Step 9 - Forward frames directly from `RunLauncher`

If you embed the runtime in your own host executable, you can still pass the
callback directly instead of going through a plugin DLL:

```cpp
#include <cstddef>
#include <iostream>

#include "mfd/window/WindowLauncher.h"

int main(int argc, char** argv)
{
    mfd::window::LauncherConfig config;
    config.applicationName = "my_window_host";
    config.defaultWindowFile = "examples/demo/assets/windows/demo_window.json";

    return mfd::window::RunLauncher(
        argc,
        argv,
        config,
        [](int width, int height, mfd::ByteView pixels)
        {
            static bool printed = false;
            if (!printed)
            {
                std::cout << "Received " << width << "x" << height
                          << " bytes=" << pixels.size() << '\n';
                printed = true;
            }
        });
}
```

The callback receives:

- `width`
- `height`
- `mfd::ByteView` over the same `RGBA32` data

When the runtime is hosted on a compatible desktop OpenGL backend, the launcher
tries to keep this callback fed from an asynchronous PBO readback path. If the
required entry points are not available, it falls back to the synchronous
`OpenGlFramebufferReader` path automatically.

Copy the byte span inside the callback if another system needs to keep it after
the function returns.

Frame delivery uses one internal worker and a single pending-frame slot. If the
consumer is slower than rendering, the newest frame replaces the pending one;
frames are not queued without a bound. The callback can therefore run outside
the rendering thread and must be thread-safe. It must not rely on observing
every rendered frame.

## What You Should Get

After a capture:

- `width` and `height` match the requested area
- `Pixels()` gives typed `Rgba8Pixel` access
- `Bytes()` gives a raw `std::byte` view over the same buffer

This is the right API when another system wants raw `RGBA32` pixel buffers.
When you need `BGRA32` or a `GL_BGRA_EXT`-compatible byte layout, use the
plugin ABI and request `MfdWindowFramebufferPixelFormat_Bgra32`.

## Result

You now know how to pull raw framebuffer bytes from the window using the
public API, either directly as `RGBA32` through `OpenGlFramebufferReader`,
through `mfd_window --framebuffer-plugin` with plugin-selected `RGBA32` or
`BGRA32`, or through the optional `RunLauncher` callback.
