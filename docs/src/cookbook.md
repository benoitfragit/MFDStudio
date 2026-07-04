# Cookbook

Short, task-focused recipes. Each one is self-contained: an objective, the files
involved, a minimal snippet, the expected result, and the trap that usually
catches people first.

For full field tables see the [Reference](./reference/json.md) section; for the
typed client surface see [Generated Client API](./handbook/generated_api.md).

All coordinates are normalized logical space in `[-1, 1]`, never pixels.

C++ snippets here use real, verified method and class names from the
generated client API and `mfd_client_api` headers, but most are condensed
patterns, not standalone compilable units (transports, error handling, and
loop boilerplate are cut for brevity). The one fully compilable, buildable
reference is [`examples/tutorial`](https://github.com/benoitfragit/MFDStudio/tree/master/examples/tutorial)
(target `tutorial_client`); the "Drive a page from the generated API" recipe
below is condensed directly from it.

**Recipes on this page:**

- [Create a minimal page](#create-a-minimal-page)
- [Show a simple reticle](#show-a-simple-reticle)
- [Drive a page from the generated API](#drive-a-page-from-the-generated-api)
- [Update an exposed text primitive](#update-an-exposed-text-primitive)
- [Use dynamic reticles](#use-dynamic-reticles)
- [Rotate a reticle](#rotate-a-reticle)
- [Keep a primitive upright or fixed-size under rotation/scale](#keep-a-primitive-upright-or-fixed-size-under-rotationscale)
- [Clip with a stencil mask](#clip-with-a-stencil-mask)
- [Add a strobe (cursor)](#add-a-strobe-cursor)
- [Offer an alternative strobe](#offer-an-alternative-strobe)
- [Exploit runtime feedback](#exploit-runtime-feedback)
- [Render offscreen (no window)](#render-offscreen-no-window)
- [Capture the framebuffer through a plugin](#capture-the-framebuffer-through-a-plugin)

---

## Create a minimal page

**Objective.** Get one named, loadable page with a single layer.

**Files.** A page JSON under `examples/demo/assets/pages/`, referenced from a window JSON.

```json
{
  "name": "Radar",
  "backgroundColor": "#08131BFF",
  "layers": [ { "id": "base" } ],
  "view": { "center": [0.0, 0.0], "zoom": 1.0 }
}
```

**Result.** The runtime loads the page; `defaultPage: "Radar"` in the window
makes it the startup view.

**Trap.** `layers` is required and is the draw-order source of truth. Every
static reticle must reference an existing `layerId`.

---

## Show a simple reticle

**Objective.** Draw one symbol on a page without a library template.

**Files.** The page JSON (inline `elements`).

```json
{
  "id": "center_cross",
  "layerId": "base",
  "elements": [
    { "id": "h", "type": "line", "start": [-0.1, 0.0], "end": [0.1, 0.0], "stroke": "hud" },
    { "id": "v", "type": "line", "start": [0.0, -0.1], "end": [0.0, 0.1], "stroke": "hud" }
  ]
}
```

**Result.** A green cross at page center.

**Trap.** Inline reticles need an explicit `id`; reuse across pages is better
served by a library `template` (see [Pages And Windows](./reference/pages_and_windows.md)).

---

## Drive a page from the generated API

**Objective.** Activate a page and publish one batch from C++.

**Files.** Generated `*_Ui.h/.cpp`, the window `.generated.map`, your client.
**Status: Supported** — condensed from the compiling
[`tutorial_client`](https://github.com/benoitfragit/MFDStudio/blob/master/examples/tutorial/client/src/main.cpp)
example; see the full, runnable version there and in
[Generated Client API](./handbook/generated_api.md#minimal-usage).

```cpp
// loaded: mfd::LoadedWindowConfiguration from mfd::JsonLoader::LoadWindowConfiguration(...)
mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
tutorial_ui::TutorialUi ui;
auto& page = ui.Page1();

client.ActivatePage(page);
ui.Initialize();
// ... mutate handles ...
client.SendBatch(ui.BuildBatch());
```

**Result.** The runtime switches to the page and applies the batch.

**Trap.** `mfd_window` must load the `.generated.map` that matches your client,
or id-based batches are rejected.

---

## Update an exposed text primitive

**Objective.** Change live text such as a status caption or a value box.

**Files.** The reticle JSON (mark the text `exposed`), your client.

```json
{ "id": "status", "type": "text", "text": "READY", "exposed": true, "stroke": "hud" }
```

```cpp
page.statusReticle.StatusText().SetText("SEARCH");
```

**Result.** The text updates on the next batch.

**Trap.** A primitive must be `exposed: true` to receive typed patches from the
generated client. Non-exposed primitives stay authored-only.

---

## Use dynamic reticles

**Objective.** Create and remove reticles at runtime (radar tracks, contacts).

**Files.** The page `dynamicReticleBindings`, a library template, your client.

```json
"dynamicReticleBindings": [
  { "templateId": "radar_track", "layerId": "overlay", "orderInLayer": 0 }
]
```

```cpp
auto& tracks = page.DynamicRadarTrack();
auto& contact = tracks.Create();      // spawn
contact.SetPosition({0.2f, 0.1f});
tracks.Remove(contact);               // remove later
```

**Result.** Tracks appear and disappear without re-authoring the page.

**Trap.** A dynamic template must be bound to a layer in
`dynamicReticleBindings` before the client can spawn it.

---

## Rotate a reticle

**Objective.** Spin a whole symbol (a sweep, a heading card).

**Files.** Your client, or static authoring.

```cpp
page.headingCard.SetRotationDegrees(-headingDegrees);
```

```json
{ "id": "sweep", "template": "radar_sweep", "layerId": "overlay", "rotationDegrees": 45.0 }
```

**Result.** The reticle and its primitives rotate together.

**Trap.** Rotation is in degrees and applies to the whole reticle; child
primitives inherit it unless they opt out (next recipe).

---

## Keep a primitive upright or fixed-size under rotation/scale

**Objective.** A label that stays readable while its parent reticle rotates or
scales.

**Files.** The reticle JSON.

```json
{
  "id": "aircraft_label",
  "type": "text",
  "text": "UPRT",
  "reticleRotationSensitive": false,
  "reticleScaleSensitive": false
}
```

**Result.** The parent reticle can rotate and scale; this primitive stays
upright and size-stable until you rotate/scale it explicitly.

**Trap.** Both flags default to `true`. Set them to `false` only on the
primitives that must stay decoupled.

---

## Clip with a stencil mask

**Objective.** Restrict drawing to the inside or outside of a mask primitive (an
ADI ball, a scope window).

**Files.** The static reticle instance JSON.

```json
"clipping": { "mode": "outer", "primitive": "ball_outer_circle" }
```

**Result.** `inner` keeps content inside the mask; `outer` erases inside it.

**Trap.** `mode` is `none`, `inner`, or `outer`; the mask `primitive` geometry
must be `circle`, `rectangle`, `ellipse`, `square`, or `triangle`. A missing or
unusable mask raises diagnostics `MFD014`/`MFD015`.

---

## Add a strobe (cursor)

**Objective.** Give a page a movable, capturing cursor.

**Files.** The page JSON.

```json
"activeStrobe": "Default",
"strobes": [
  { "name": "Default", "id": "radar_strobe", "template": "strobe_cursor",
    "capture": { "shape": "circle", "radius": 0.10 } }
]
```

**Result.** The page exposes one active strobe that can move, capture, and
magnetize to targets.

**Trap.** `activeStrobe` must name an entry in `strobes`. Use the modern
`strobes` + `activeStrobe` form, not the legacy singular `strobe`.

---

## Offer an alternative strobe

**Objective.** Let the client switch between named strobe variants at runtime.

**Files.** The page JSON (several `strobes`), your client.

```json
"strobes": [
  { "name": "Default", "template": "strobe_cursor", "capture": { "shape": "circle", "radius": 0.10 } },
  { "name": "Wide",    "template": "strobe_cursor", "capture": { "shape": "circle", "radius": 0.18 } }
]
```

```cpp
page.strobe = page.strobe1;      // switch to the alternative
page.strobe = page.defaultStrobe; // back to the default
```

**Result.** The active strobe swaps without re-authoring the page.

**Trap.** Each strobe entry needs a unique `id` on the page; template entries
may reuse the template id, inline entries must declare one.

---

## Exploit runtime feedback

**Objective.** React to authoritative runtime state (capture, page activity,
window shutdown).

**Files.** The window `feedback` transport, your client.

```cpp
const bool captured = track.reticle->IsStrobeCaptured();

mfd::client::WindowLivenessMonitor monitor(2.0);
ui.PollFeedback(*feedbackReceiver, 8, &error);
monitor.Observe(ui.TotalDecodedFeedbackPackets(), ui.WindowReportedClosing(), nowSeconds);
if (monitor.ConsumeDisconnect()) { /* window closed: rebuild transports */ }
```

**Result.** The client restyles captured tracks and stops cleanly when the
window closes.

**Trap.** Feedback only flows when the window JSON enables a `feedback` UDP
transport; live strobe/capture state is reported for the **active** page only.

---

## Render offscreen (no window)

**Objective.** Render an MFD inside your own application and read the pixels.

**Files.** `mfd_runtime_api`, your host loop. See
[Offscreen Embedding](./handbook/offscreen.md).

```cpp
mfd::runtime_api::RuntimeSession session;
session.LoadWindowFile("examples/demo/assets/windows/demo_window.json", error);
mfd::runtime_api::OffscreenSurface surface(960, 540);

session.Advance(deltaSeconds);
if (surface.Render(session)) {
    const auto frame = surface.FrameView();   // RGBA8 pixels
}
```

**Result.** One or more independent offscreen surfaces, sized by your host.

**Trap.** Call `surface.Resize(w, h)` when your host window changes size, and
check `OffscreenFrameView::Ready()` before sampling the buffer.

---

## Capture the framebuffer through a plugin

**Objective.** Sink the standalone runtime image as raw pixels from `mfd_window`.

**Files.** A plugin implementing the `mfd_window_plugin_api` ABI, the launcher.
See [Framebuffer Capture](./handbook/framebuffer.md).

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json `
  --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll
```

**Result.** The runtime streams frames to your plugin; the shipped sample
requests `BGRA32`.

**Trap.** The plugin selects the pixel format (`RGBA32` or `BGRA32`); model new
sinks on the shipped `mfd_framebuffer_stdout_plugin`.
