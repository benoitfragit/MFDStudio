# Capture The Window As RGBA32

This tutorial shows how to capture the rendered window as a CPU buffer using
the OpenGL readback API.

## At A Glance

```mermaid
flowchart LR
    A[Rendered OpenGL framebuffer] --> B[OpenGlFramebufferReader::ReadRgba32]
    B --> C[Rgba32Framebuffer]
    C --> D[Typed pixels]
    C --> E[Raw bytes]
```

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
std::span<const std::byte> raw = capture.Bytes();
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

## What You Should Get

After a capture:

- `width` and `height` match the requested area
- `Pixels()` gives typed `Rgba8Pixel` access
- `Bytes()` gives a raw `std::byte` view over the same buffer

This is the right API when another system wants raw `RGBA32` pixel buffers.

## Result

You now know how to pull an `RGBA32` framebuffer buffer from the window using
the public API.
