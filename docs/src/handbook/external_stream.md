# External GPU streams

The [integration review](./external_stream_review.md) records the inspected scope,
corrected issues and remaining hardware validation limits.

An `externalStream` primitive displays pixels supplied by a cooperating local
application. MFDStudio owns three D3D11 textures per stream. The producer opens
these textures and writes to an available surface through `StreamProducer`.
Text, symbols, clipping and other primitives compose normally over that surface.
This is the input counterpart of [framebuffer capture](./framebuffer.md).

## Supported configuration

The demo uses command UDP port 47230. Its window remains open after the Python
producer exits; close that window before repeating `producer.py --launch`, or
reuse it with `--instance <pid>-1` without `--launch`. WSA 10048 means the address
is already in use. The launcher checks availability before starting the window;
the runtime still performs the final bind, so concurrent launches can still race.

- Windows desktop processes on the same machine, user session and GPU adapter.
- D3D11, DXGI, OpenGL sync objects and `WGL_NV_DX_interop2` on the active context.
  Extension presence alone is insufficient: texture creation, registration and
  actual pixel sharing must work. Unsupported configurations report a diagnostic.
- One producer and one consuming primitive per logical stream. Use distinct IDs
  when instantiating multiple reticles. Four active endpoints per renderer.
  A template's stream ID is persistent configuration: creating two simultaneous
  instances of that template does not create two sources. Use separate authored
  stream IDs/templates for independent sources; duplicate consumers report an error.
  Leaving a page or removing a primitive releases its endpoint; returning starts
  a new session. Hiding a primitive keeps its endpoint and last image.
- Pixel dimensions: 1–1920 by 1–1080. The CPU protocol uses checked, bounded
  dimensions. Shared textures have one mip level and no multisampling.
- Shared texture budget: 128 MiB per native adapter library instance, including
  retained generations. GPU driver overhead and the host framebuffer are extra.
  At 640 × 360, two streams use approximately 5.27 MiB for their six surfaces.
- No new third-party production dependency: native libraries are `d3d11`,
  `dxgi`, `opengl32` and Windows kernel/user APIs.

This does not add network video transport, decoding, camera acquisition or a
source-specific abstraction. Producers own content acquisition and decoding.

## Authoring

```json
{
  "id": "surface",
  "type": "externalStream",
  "streamId": "camera_a",
  "width": 1.5,
  "height": 0.84375,
  "fit": "contain",
  "fallback": "#182438FF",
  "defaultPlayback": "play",
  "exposed": true
}
```

`streamId` accepts 1–64 ASCII letters, digits, underscores and hyphens.
`width` and `height` are positive logical dimensions, independent of the
producer's pixel resolution. `contain` preserves proportions; `stretch` fills
the authored rectangle. Resolution changes leave the authored layout unchanged.
`defaultPlayback` is `play` or `stop`; runtime Pause is not persisted.

The editor offers External stream in primitive creation, the fields above and
the preview instance/diagnostic. Exports without a live stream registry show the
replacement color; they do not embed producer pixels into the authored document.

## Playback and status

Generated `ExternalStreamHandle` provides `Play()`, `Pause()`, `Stop()`,
`SetSize()`, inherited visibility/transforms, `GetStatus()` and
`GetLastConsumedFrame()`. Commands use the existing command transport. No pixels
travel over that transport.

- Play consumes the most recent completed publication.
- Pause retains the displayed image and lets the producer recycle the other
  surfaces. A pending resolution change creates a new generation while the old
  displayed generation remains owned and counted against the memory budget.
- Stop removes the displayed image, draws the replacement color, and invalidates
  pending/in-progress writes through a consumption epoch. Stop followed by Play
  in the same command batch retains that invalidation. Restarting after Stop
  requires a publication acquired in the new epoch.
- Hidden primitives do not consume frames, draw or process pixels. Bounded
  metadata, liveness and resource housekeeping continue.
- Disconnect retains the last valid image. Before the first frame, only the
  replacement color is visible. A stopped producer heartbeat marks information
  stale, without interpreting an unchanged frame counter as a lost image.

`consumedFrame` is the number supplied by the producer for the last frame adopted
by the reader. It can skip values, is scoped to the reported generation and is
neither a publication count nor proof of screen presentation. It remains available
after Stop even though `hasImage` becomes false.

Feedback carries connection, image validity, playback, result, generation,
consumed number, instance and diagnostic. The generated getters only inspect the
client's cache; call the generated `PollFeedback()` regularly. Feedback can be
delayed or lost. `fresh` requires a recent producer heartbeat and is cleared
locally when the feedback observation is older than two seconds.

## Producer contract, version 1

`mfd_external_stream/src/StreamProtocol.h` specifies a fixed-width, little-endian
Windows mapping. Compile-time size/offset assertions cover x86 and x64: a
200-byte control block with three 32-byte slots. No STL object, pointer or C++
enum representation is exchanged.

The endpoint namespace is `Local\MFDStudio.Stream.v1.<instance>.<streamId>`.
The runtime prints its process/renderer-specific instance. Each endpoint has a
new session token, resource generation and Stop epoch. Producer requests contain
dimensions, pixel order and a request serial; the host validates them and
publishes its effective configuration and adapter LUID. Processes open the same
adapter. A new connection/configuration obtains distinct textures.

Pixel formats are `Rgba32=1` and `Bgra32=2`: straight alpha, row zero at the top,
four bytes per pixel, no color-space conversion. GPU producers use the negotiated
D3D texture format. The demo accepts tightly packed CPU rows.

Slots carry opaque legacy DXGI share tokens, frame numbers, epochs and states.
These tokens are zero-extended 32-bit values compatible across process bitness;
they are not OpenGL IDs, process pointers or closeable NT handles. The mapping
and its named control mutex use default Windows access control for cooperating
local applications. This protocol is not a sandbox for hostile GPU clients.

The adapter's single-threaded lifecycle is:

1. `Open(instance, streamId, width, height, format)` requests configuration.
2. Retry after `Unavailable`; handle `Reconfigured` as a configuration transition.
3. `TryAcquire(texture, context)` obtains borrowed, process-local D3D objects.
4. Submit writes on the supplied immediate context, then `Publish(frameNumber)`.
   The number must be nonzero and strictly increase within the connection.
5. `Cancel()` abandons an acquired write, including already submitted GPU work.
6. Call `Poll()` even for a still image to complete submissions and maintain
   liveness. `Close()` releases the connection; reopen to request new dimensions.

`Ok` from Publish means accepted submission. A D3D11 event query must complete
before the slot becomes Ready. A discarded write is also fenced before reuse.
The renderer retains GL ownership while displaying a surface and polls a GL
fence before releasing it. Role changes exchange ownership metadata and do not
copy pixels. A producer may replace the oldest Ready frame, but never Writing,
Pending or Reading. All scans visit exactly three slots; unavailable control
locks and surfaces return immediately without a retry loop.

The control mutex uses a zero timeout. The producer lease allows one connected
producer and is released by Windows on process death. A crashed producer is
replaced through a fresh resource generation; its unfinished surface is not
reused. Driver errors and abandoned control metadata stop that endpoint with a
diagnostic; recreate the endpoint/runtime after such a failure.

Native allocation, command submission and WGL ownership operations are driver
calls: no hard real-time bound is promised. Finite buffering deliberately allows
dropped frames. A 50 Hz publishing target is not an end-to-end latency guarantee.

## Python demonstration

Before choosing the Python demo, see the [native GPU integration](#native-gpu-integration)
section below if your application already renders on the GPU.

From the repository root:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_editor mfd_stream_demo
py -3.7-32 examples/external_stream/producer.py --launch
```

Use any matching 32-bit Python if that launcher selector is unavailable. For
64-bit Python, build the `vs2022-x64` / `debug-x64` presets and select that DLL.
Python and its DLL must have the same architecture; the runtime process may
have a different architecture. The script requires only Python's standard
library and supports Python 3.7+.

To start each side separately:

```powershell
.\Scripts\Start-ExternalStream.bat
py -3.7-32 examples/external_stream/producer.py --instance 1234-1 --stream camera_a --stream camera_b --format bgra --fps 50
```

Replace `1234-1` with the instance from the runtime log. Supply `--dll` or
`--runtime` for other executable locations; `--duration 10` bounds a smoke run.
`--width` and `--height` choose source resolution. Multiple simultaneous runtime
instances use distinct texture namespaces; the example's command ports are fixed
at 47230/47231 and therefore its keyboard controls target one example window.

Focus the Python console for keys: Space pauses/resumes production; P/A/S control
reader Play/Pause/Stop; V hides/shows; Q closes the producer. MFDStudio stays open
to show its disconnected state. The console counts accepted submissions,
abandoned attempts and errors. The pattern includes color bars, a moving white
bar, a white top-left marker, a six-digit decimal frame counter, 32 binary frame
bits and a translucent bottom band. These color bars are the intended demo
content. After the producer exits, the retained image and its counter stop moving.

The demonstration DLL links the actual producer adapter and exercises generated
commands. It is never a runtime dependency of MFDStudio. CPU pattern generation
and upload validate the GPU publication path, but do not measure a producer
drawing directly on the GPU.

## Native GPU integration

### Producer, receiver and demo DLL

`StreamProducer` is the native writer used by the external application.
`StreamReceiver` is the reader used inside MFDStudio; it allocates the shared
surfaces and makes completed images available to the renderer. They are the two
ends of one local GPU stream, not two independent image transports.

`mfd_stream_demo.dll` is a demonstration wrapper. Python loads it with `ctypes`,
generates pixels in RAM and calls `mfd_stream_submit`; the wrapper uploads those
pixels with `UpdateSubresource` through the real `StreamProducer` adapter.
Its C interface accepts CPU pixels, not arbitrary GPU texture handles.

A native C++ application can link the static `mfd::external_stream` target and
use `mfd::stream::StreamProducer` directly. It does not need Python or
`mfd_stream_demo.dll`. Within a CMake build that includes MFDStudio:

```cmake
target_compile_features(my_producer PRIVATE cxx_std_17)
target_link_libraries(my_producer PRIVATE mfd::external_stream)
```

The producer header is `mfd/stream/StreamProducer.h`. The connection lifecycle
and result handling are described in [Producer contract, version 1](#producer-contract-version-1).
Omitting the demo DLL does not omit acquisition, publication or synchronization.

### Choose the pixel path

| Source | Write into the acquired shared surface | Transfer cost |
|---|---|---|
| Python demo / CPU image | Upload through `UpdateSubresource` | RAM to GPU upload |
| Existing compatible GPU texture | GPU copy into the acquired surface | GPU to GPU copy; no CPU readback |
| Native rendering into the acquired surface | Render on the supplied D3D11 immediate context | No extra image copy between producer and MFDStudio |

The current API always uses the three surfaces owned by MFDStudio. It does not
accept a caller-owned texture as a replacement for those surfaces. Having a GPU
texture alone is therefore insufficient to connect a producer.

### Device compatibility and synchronization

The adapter creates its own D3D11 device on the GPU selected by MFDStudio and
returns that device's immediate context from `TryAcquire`. There is currently
no API for injecting an application's existing D3D11 device or context.

For an existing texture, being on the same physical GPU is not sufficient:
the source must be accessible as a resource of the device associated with the
supplied context. A texture belonging to another device requires compatible
resource sharing/opening and synchronization of the source before the copy.
The application must provide that integration; `StreamProducer` does not import
arbitrary Direct3D, OpenGL, Vulkan or CUDA texture identifiers automatically.

A direct D3D11 copy requires compatible source/destination resource descriptions.
The shared destination has the negotiated dimensions and RGBA/BGRA format,
one mip level and no multisampling. The adapter does not resize, convert formats
or resolve multisampling for a caller's source image. Those operations, when
needed, belong to the producer's integration.

For direct rendering, create the required render-target view and rendering
resources on the device of the acquired surface and submit drawing on the
supplied immediate context. Then call `Publish(frameNumber)`. Do not submit
writes on an unrelated context and assume that `Publish` waits for them: its
completion query tracks work submitted on the adapter's context.

In both paths, write only after `TryAcquire` returns `Ok`; on `Unavailable`,
skip the attempt and retry on a later application tick. Call `Cancel` if an
acquired frame is abandoned, and keep polling even when the image is static.
The returned texture/context pointers are borrowed: do not release them or keep
using them after publication, cancellation, closure or reconfiguration. Rebuild
any application-side views/resources when the negotiated generation changes.

The GPU tests validate sharing, pixels and stream synchronization. They do not
certify the resource-sharing bridge of an arbitrary existing rendering engine.

## Tests

```powershell
cmake --build --preset debug-win32 --target external_stream_tests external_stream_gpu_test mfd_api_tests client_api_tests mfd_editor_tests
ctest --preset test-debug-win32 -R ExternalStream --output-on-failure
ctest --preset test-debug-win32 -R client_api_generator_tests --output-on-failure
```

Protocol and model tests do not require a GPU. `ExternalStreamGpuInterop` launches
a real producer process and tests pixel integrity, playback, hiding, crash,
reconnection, format and dimension changes, with a 25-second CTest deadline.
Missing GPU interoperability returns skip code 77, not success. This one test is
explicitly excluded by `test-debug-win32-ci`, using the existing name exclusion
mechanism; it is still compiled in CI.

The same GPU test can use a separately built producer executable of the other
architecture. Set `MFD_STREAM_TEST_PRODUCER` to the absolute path of that
architecture's `external_stream_gpu_test.exe`, then run the consumer test.
The helper receives `--producer <instance> <variant>` and does not open a window.
Unset the variable after this check.

Local validation on 2026-10-10 used Intel UHD Graphics 620 (driver
27.20.100.8681): real GPU pixel checks passed for RGBA/BGRA, alpha, row order,
Pause/Stop/hiding, producer crash/restart and resolution changes. The same
checks passed with a 64-bit producer and a 32-bit consumer. The Python 3.7
32-bit demonstration also exercised two 640 x 360 streams and verified their
runtime UDP feedback through Pause, Stop, hiding, resumption and disconnection.

For local load validation, run two/four streams, vary `--fps` above and below the
reader cadence, pause/hide/stop the reader, kill/restart the producer and change
resolution/format between connections. Inspect alpha, the top-left marker and
frame bits. Measure publication rate and latency separately. Test on the actual
deployment adapter/driver; success on one adapter does not certify another.
