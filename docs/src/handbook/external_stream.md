# External GPU streams

The [integration review](./external_stream_review.md) records the inspected scope,
corrected issues and remaining hardware validation limits.

An `externalStream` primitive displays pixels supplied by a cooperating local
application. MFDStudio owns three D3D11 textures per stream. The producer opens
these textures and writes to an available surface through `StreamProducer`.
MFDStudio automatically selects direct GPU interop or a CPU readback/upload path;
the producer API and version-one publication contract are identical in both modes.
Text, symbols, clipping and other primitives compose normally over that surface.
This is the input counterpart of [framebuffer capture](./framebuffer.md).

## Integration checklist

Validate the complete path with the shipped producer first:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_editor mfd_stream_demo
py -3.7-32 examples/external_stream/producer.py --launch
```

Then integrate a native producer in this order:

1. Author one `externalStream` primitive for each source and give each a unique
   `streamId`. Set `exposed: true` only when generated playback/status control is
   required.
2. Start the runtime and read the instance printed in its log (for example
   `1234-1`). The producer cannot invent or reuse an instance from an old runtime.
3. In a CMake build that includes the MFDStudio source tree, link the producer to
   `mfd::external_stream` and include `mfd/stream/StreamProducer.h`.
4. Call `Open(instance, streamId, width, height, format)` on the producer's owning
   thread. Retry `Unavailable` later; do not spin or block the render thread.
5. For each submitted frame, call `TryAcquire`, write or copy through the returned
   D3D11 immediate context, then call `Publish` with a nonzero strictly increasing
   frame number. Call `Cancel` if the acquired write cannot be published.
6. Call `Poll` even while the image is unchanged, and call `Close` before changing
   the requested configuration or destroying the producer.
7. If the primitive is exposed, deploy the generated client/map together and poll
   feedback for `GetStatus()`/`GetLastConsumedFrame()` observations. Playback
   commands never transport pixels.

`mfd::external_stream` is currently a build-tree target, not an exported target in
the installed SDK package. An out-of-tree product must add the MFDStudio source
tree (for example with `add_subdirectory`/`FetchContent`) or package that static
library and its public headers as part of its own delivery. The producer itself
has no runtime dependency on `mfd_stream_demo.dll`; that DLL exists only for the
Python example.

The native loop is intentionally nonblocking. `Unavailable` means "try on a later
application tick"; `Reconfigured` means refresh the acquired generation before
writing again; `Disconnected` means close and wait for a live runtime endpoint.
Never retain the borrowed texture or context after `Publish`, `Cancel`, `Close` or
reconfiguration.

## Supported configuration

The demo uses command UDP port 47230. Its window remains open after the Python
producer exits; close that window before repeating `producer.py --launch`, or
reuse it with `--instance <pid>-1` without `--launch`. WSA 10048 means the address
is already in use. The launcher checks availability before starting the window;
the runtime still performs the final bind, so concurrent launches can still race.

- Windows desktop processes on the same machine, user session and GPU adapter.
- D3D11 and compatible legacy DXGI shared textures on the selected adapter, plus
  a current desktop OpenGL context with sync objects. The CPU path also requires
  pixel buffer binding support (available in the renderer's OpenGL 3.3 context).
  `WGL_NV_DX_interop2` is optional when CPU fallback works. Its presence alone does
  not establish direct interop: device opening, texture registration and ownership
  acquisition must succeed. Adapter/driver sharing restrictions still apply;
  fallback cannot repair a failed D3D11 device or unsupported DXGI sharing.
- One producer and one consuming primitive per logical stream. Use distinct IDs
  when instantiating multiple reticles. Four active endpoints per renderer.
  A template's stream ID is persistent configuration: creating two simultaneous
  instances of that template does not create two sources. Use separate authored
  stream IDs/templates for independent sources; duplicate consumers report an error.
  Leaving a page or removing a primitive releases its endpoint; returning starts
  a new session. Hiding a primitive keeps its endpoint and last image.
- Pixel dimensions: 1–1920 by 1–1080. The CPU protocol uses checked, bounded
  dimensions. Shared textures have one mip level and no multisampling.
- Resource payload budget: 128 MiB per native adapter library instance, including
  retained generations. Direct mode accounts for three shared surfaces; CPU mode
  also reserves one staging surface, one OpenGL texture and one packed CPU image
  per generation. Driver allocation alignment/overhead and the host framebuffer
  are extra. At 640 x 360, two streams reserve approximately 5.27 MiB in direct
  mode or 10.55 MiB in CPU mode. At 1920 x 1080, each CPU generation reserves
  about 47.46 MiB: four maximum-resolution CPU streams do not fit simultaneously.
- No new third-party production dependency: native libraries are `d3d11`,
  `dxgi`, `opengl32` and Windows kernel/user APIs.

This does not add network video transport, decoding, camera acquisition or a
source-specific abstraction. Producers own content acquisition and decoding.

## Display paths and performance

| Mode | Consumer operation | Requirements | Transfers and cost |
|---|---|---|---|
| Direct GPU interop | GL samples the shared D3D11 surface | Working `WGL_NV_DX_interop2`, including real registration and ownership operations | No consumer image transfer through RAM; preferred path |
| CPU fallback | Copy to D3D11 staging, read CPU rows, upload an ordinary GL texture | Compatible D3D11/DXGI sharing and the GL context described above | GPU to CPU to GPU transfers, plus row packing; higher bandwidth, CPU work and memory usage |

Each generation first tries direct interop. Failed WGL opening, registration or
lock acquisition selects CPU fallback on the same shared textures. A later WGL
acquisition failure can also switch that generation to CPU mode without restarting
the producer. An unlock/unregistration failure is fatal because safe D3D ownership
cannot be established. Device loss is fatal and never triggers a fallback loop.

`Generation` owns the three shared surfaces and their registrations.
`CpuReadback` owns reusable staging, a completion query, packed rows and a GL
texture; there are no per-frame texture allocations. `StreamReceiver` coordinates
publication, display ownership, Stop epochs and retirement. `MemoryReservation`
accounts for allocations before construction and releases them through RAII.
These are private implementation components, not additions to the producer API.

One readback may be in flight per stream. Its source stays `Reading` until a
D3D11 event query completes. The query is polled once per update with
`D3D11_ASYNC_GETDATA_DONOTFLUSH`; submission explicitly flushes without waiting.
Mapping uses [`D3D11_MAP_FLAG_DO_NOT_WAIT`](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_map_flag).
If the driver still reports a busy resource, a later update retries the map.
The previous GL draw fence must complete before uploading into the display texture.
No busy-wait, blocking GPU wait, PBO ring or CPU shared-memory pixel transport is used.

Row packing respects D3D11 `RowPitch`, RGBA/BGRA byte order, straight alpha and
top-left row zero. Uploads restore the caller's texture binding, unpack PBO binding
and unpack alignment/row/skip settings. Hidden or paused streams retire completed
copies without mapping/uploading them. Copies from a previous Stop epoch are
also discarded. Reconfiguration waits for an in-flight copy to retire; a paused
image remains owned by its old generation until replaced or stopped.

CPU fallback trades compatibility for copies and resource usage. No frame rate or
latency is promised without measurement on the deployment adapter. Both paths
keep the newest complete publication and may deliberately drop frames.

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
  requires a publication acquired in the new epoch. Stop hides the image
  immediately even when the control mutex is busy; its epoch invalidation is
  latched until metadata can be updated, including a subsequent Play.
- Hidden primitives do not consume frames, draw or process pixels. Bounded
  metadata, liveness and resource housekeeping continue. If every registered
  stream is hidden or lacks a drawable image, the renderer also skips the external
  stream render-batch flush.
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
In direct mode the renderer retains GL ownership while displaying a surface
and polls a GL fence before releasing it. These role changes do not copy pixels.
In CPU mode the renderer holds the source during readback, then releases it;
the ordinary GL texture independently retains the displayed pixels. A producer
may replace the oldest Ready frame, but never Writing,
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

## Diagnostics and recovery

The existing feedback diagnostic identifies `Direct GPU interop`, `CPU fallback`
(with the WGL failure reason), or `Unsupported` when no display path is usable.
Connection/stale/disconnected information follows the mode, when configured.
No new status field, command, JSON format or generated client API is required.

| Diagnostic | Behavior and action |
|---|---|
| WGL unavailable / opening, registration or acquisition failed | CPU fallback is selected automatically if available. Existing producers need no changes. |
| Temporary resource budget shortage (128 MiB) | `Unavailable`, not a permanent failure. The effective generation, handles and last valid image remain intact. Release other streams or reduce their resolution; the request retries at most once per 250 ms. Pause and Stop still work. |
| Shared D3D11 texture creation/opening failed | Check the selected adapter, driver and legacy DXGI sharing support. CPU fallback still needs those shared surfaces. |
| `Unsupported` / CPU fallback initialization failed | The current GL context or readback resources cannot support display; inspect the accompanying reason. Recreate the endpoint after correcting the configuration. |
| Fatal D3D11 device loss, GL fence/upload error, WGL ownership release failure or invalid control metadata | The endpoint stops with an error. Fix the cause and recreate the endpoint/runtime; it does not retry indefinitely. |

A rejected resource reservation is rolled back before any configuration is
published. Only a fully constructed replacement updates the generation and share
handles. A retained image still consumes budget, so freeing enough space for both
the old and new generation may be necessary. Repeated ticks or new request serials
do not bypass the retry delay. Native out-of-memory/device/driver errors are not
misclassified as the recoverable application budget limit.

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
| Native rendering into the acquired surface | Render on the supplied D3D11 immediate context | No extra producer image copy |

These costs describe production into the shared surfaces; CPU display fallback
adds the consumer transfers described above. Existing producer applications need
no adaptation to benefit from automatic display selection.

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

Protocol, budget and model tests do not require a GPU and remain enabled in CI.
Budget tests cover refusal without altering existing reservations, rollback,
release/retry and a bounded retry cadence.

Hardware integration cases are compiled in CI but explicitly excluded from
`test-debug-win32-ci` by their `gpu;local` labels. Both the pull-request and release
workflows use that preset. The local preset keeps all of them enabled:

- `ExternalStreamGpuInterop`: real direct interop (skips when unavailable).
- `ExternalStreamCpuFallback`: forced CPU mode, even on interop-capable hardware.
- `ExternalStreamInteropOpenFailure`, `ExternalStreamInteropRegistrationFailure`
  and `ExternalStreamInteropLockFailure`: injected WGL initialization failures,
  including cleanup after a partially registered generation.
- `ExternalStreamInteropRuntimeFailure`: switches an already running direct
  generation to CPU mode without reconnecting the producer.
- `ExternalStreamUnsupportedDisplay`: both paths fail; initialization is terminal
  and partial resources are released without repeated attempts.

The hardware binary alone links a private test build of the stream library with
fault injection; the production library has no environment switch or test API.
Every hardware case has a 30-second CTest deadline and launches a real producer
process. Cases verify RGBA/BGRA pixels, alpha/row order, Play/Pause/Stop, Stop then
Play in one batch, hiding, crash/reconnect, generation replacement, initial and
resize budget pressure, unchanged published handles after rejection, recovery,
resource retirement, GL unpack-state preservation and Stop while another thread
holds the control mutex. Missing hardware returns skip code 77, never success; an unexpected
fallback failure after prerequisite checks is a test failure.

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

Validation on 2026-10-11 used the same adapter and driver: the full Debug Win32
build and all 932 tests selected by the CI preset passed locally. The seven
hardware scenarios passed in Win32 and x64, including injected interop failures
and recoverable budget pressure. Direct and CPU display also passed with a
producer of the other architecture in both directions. Budget assertions wait
for the observed refusal with a bounded startup deadline instead of assuming
that a new producer initializes within 400 ms. The additional local render
suite passed 32 tests; three existing tutorial offscreen tests were skipped
because their tutorial assets were missing. Other adapters and physical device
removal remain outside this validation.

For local load validation, run two/four streams, vary `--fps` above and below the
reader cadence, pause/hide/stop the reader, kill/restart the producer and change
resolution/format between connections. Inspect alpha, the top-left marker and
frame bits. Measure publication rate and latency separately. Test on the actual
deployment adapter/driver; success on one adapter does not certify another.
