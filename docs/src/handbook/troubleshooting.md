# Troubleshooting

Each entry follows the same shape: **Symptom → Likely cause → Diagnostic →
Fix**. Start with the runtime debug overlay (`F1` in `mfd_window`): it shows the
active page, the reticle tree, and transport state, and is the fastest way to
tell an authoring issue from a client or transport issue.

## Runtime and rendering

### Nothing renders, or placement looks wrong

- **Cause.** Coordinates were treated as pixels. Authoring space is normalized
  `[-1, 1]`, with `(0, 0)` at the page center.
- **Diagnostic.** Check positions against [Concepts](../concepts.md); open `F1`
  and confirm the reticle exists in the tree.
- **Fix.** Re-author positions in normalized space. The same page stays valid at
  any window size.

### The wrong page is loaded

- **Cause.** `defaultPage` does not match any page `name`, so the runtime falls
  back to the first page.
- **Diagnostic.** Compare `defaultPage` in the window JSON with each page `name`.
- **Fix.** Make `defaultPage` exactly match a page name, or activate the page
  from the client.

### A reticle update hits the wrong thing

- **Cause.** The update addressed a reticle **template** instead of a page
  **instance**.
- **Diagnostic.** A template is the reusable library definition; a page reticle
  is one placed instance. Runtime updates target the instance id on a page.
- **Fix.** Address the instance id (and primitive id when relevant), not the
  template id.

### A primitive cannot be found / id errors

- **Cause.** Duplicate or empty ids, or a clipping mask that points at a missing
  or unusable primitive.
- **Diagnostic.** Load diagnostics name the problem: `MFD014` missing clipping
  primitive, `MFD015` unusable mask, `MFD016` empty id, `MFD017` duplicate
  primitive ids, `MFD018` colliding reticle ids.
- **Fix.** Give every primitive and reticle a unique non-empty id; point
  clipping at a `circle`, `rectangle`, `ellipse`, `square`, or `triangle`.

### An image or font does not load

- **Cause.** A relative path that does not resolve from the referencing file.
- **Diagnostic.** `fontFile`/`iconFile` resolve relative to the window JSON;
  primitive `image` `file` paths resolve relative to the page/reticle JSON.
- **Fix.** Fix the relative path, and make sure the asset is staged next to the
  runtime (the example targets sync `assets/` on build).

### Invalid JSON

- **Cause.** A syntax error or an out-of-range numeric value.
- **Diagnostic.** Numeric fields must be finite; pixel integers must fit a
  signed 32-bit int; `maxPacketSize` must be in `[64, 65507]`.
- **Fix.** Validate the JSON and the value ranges in
  [Pages And Windows](../reference/pages_and_windows.md).

## Client and transport

### The client cannot drive the runtime / UDP not received

- **Cause.** Client and runtime are not on the same UDP endpoint.
- **Diagnostic.** In `client_mockup`, use **Window display** → **Send window
  display**. A reaction means the UDP path is alive and the problem is command
  content. No reaction means the transport itself is wrong.
- **Fix.** Match the client target to the window `commands.udp` `address`/`port`.

### Port already in use

- **Cause.** A previous `mfd_window` (or another process) still holds the UDP
  port.
- **Diagnostic.** Launch logs report the bind failure; check for a lingering
  runtime.
- **Fix.** Close the old runtime, or change the `commands`/`feedback` `port` in
  the window JSON.

### Generated batches are rejected

- **Cause.** The generated C++ and the `<window>.generated.map` drifted apart,
  or `mfd_window` did not load the matching sidecar.
- **Diagnostic.** Treat the generated header, source, and `.generated.map` as
  **one contract** (see [Public API Contract](../reference/public_contract.md)).
- **Fix.** Regenerate the client and load the matching `.generated.map` in the
  runtime.

### `.generated.map` missing or stale

- **Cause.** The sidecar was not produced, or it predates the current window
  JSON.
- **Diagnostic.** The map is a build artifact emitted next to the window JSON by
  `client_api_generate_ui(... OUTPUT_MAP ...)`.
- **Fix.** Re-run CMake configure so the generator rewrites the map from the
  current JSON; ship the map beside the window.

### No runtime feedback arrives

- **Cause.** The window JSON has no enabled `feedback` transport, or the client
  is not polling it.
- **Diagnostic.** Feedback needs `feedback.udp.enabled: true`; live
  strobe/capture state is only reported for the **active** page.
- **Fix.** Enable the feedback transport and poll it (`PollFeedback`), then use
  `WindowLivenessMonitor` to detect shutdown.

## Plugins and offscreen

### The framebuffer plugin is not loaded

- **Cause.** The plugin DLL was not passed, not staged, or does not implement
  the ABI.
- **Diagnostic.** Launch with `--framebuffer-plugin <name>.dll` and confirm the
  DLL sits next to `mfd_window`.
- **Fix.** Model the plugin on the shipped `mfd_framebuffer_stdout_plugin`; see
  [Framebuffer Capture](./framebuffer.md).

### Offscreen surface stays blank

- **Cause.** The session was not advanced, or the frame view was sampled before
  it was ready.
- **Diagnostic.** Call `RuntimeSession::Advance` every frame; check
  `OffscreenFrameView::Ready()` before uploading.
- **Fix.** Advance the session, render, then sample; call `Resize` when the host
  surface changes size. See [Offscreen Embedding](./offscreen.md).

## Authoring tools

### The editor edits the wrong files

- **Cause.** A staged runtime copy was opened instead of the source asset.
- **Diagnostic.** The editor starts empty on purpose and never auto-loads staged
  `_Exec` copies.
- **Fix.** Open the source window/page JSON under `assets/`, not a runtime copy.

### Do I need the editor at all?

No. The core workflow is author assets, load them in `mfd_window`, drive them
from a client. The editor is an optional convenience; JSON stays the source of
truth.

## Build and documentation tooling

### Build fails on Windows / CMake / Visual Studio

- **Cause.** Missing toolchain, or a first configure interrupted before
  dependencies finished downloading.
- **Diagnostic.** Required: Visual Studio 2022, CMake 3.25+, Python 3, C++17.
  The first configure fetches third-party sources.
- **Fix.** Use the documented presets and re-run configure; see
  [Build](../dev/build.md).

### Doxygen or Graphviz problems when building docs

- **Cause.** Doxygen, Graphviz (`dot`), Java, or PlantUML is not installed for
  the combined-site build.
- **Diagnostic.** `docs/GenerateDocs.ps1` validates these tools and emits a
  clear error naming the missing one.
- **Fix.** Install the missing tool, then re-run
  `docs/BuildDocsSite.ps1`. The diagrams in this book render client-side from
  Mermaid; the committed PlantUML diagrams ship as SVGs under `docs/src/images/`.

## Where are diagrams and screenshots?

Under `docs/src/images/`. The repository versions PlantUML sources, rendered
SVGs, and runtime/editor/client screenshots in one published asset folder. The
[Gallery](../gallery.md) collects the main captures with links to the matching
pages.
