# MFDStudioWindowLauncherPlugin

`MFDStudioWindowLauncherPlugin` is the public development package for
framebuffer-processing plugins loaded by `mfd_window` through
`--framebuffer-plugin`.

It provides:

- the public plugin ABI header `mfd/window/WindowLauncherPlugin.h`
- the shared `mfd/core/ArrayView.h` byte-span helper used by the callback
- a small support library exposing framebuffer-layout validation helpers

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

extern "C" __declspec(dllexport) void MfdWindowFramebufferCallback(
    const int width,
    const int height,
    const mfd::ByteView pixels)
{
    if (!mfd::window::ValidateFramebufferRgba32Layout(width, height, pixels))
    {
        return;
    }

    // Process one RGBA32 frame here.
}
```
