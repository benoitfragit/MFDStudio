# MFDStudio Generated Client API Standardization

Status: Living standardization note  
Aligned release baseline: MFDStudio current documentation set  
Intended audience: client integrators, generator maintainers, reviewers, and standardization work

<div class="mfd-banner"><strong>Preferred client surface:</strong> for normal client code, the official integration path is the generated API emitted from the window JSON, with <code>CommandClient</code> kept as the final transport sender.</div>

@tableofcontents

## 1. Purpose

This document standardizes the client-facing API that MFDStudio expects
application code to use in normal integrations.

Its purpose is to make three things explicit:

- which generated surfaces are part of the intended public contract
- which authored concepts must be preserved by the generator
- where the low-level `CommandClient` boundary begins and ends

This document is stronger than an architecture note but lighter than the
replacement-client interoperability specification. It is the reference to use
when the question is:

- "what should generated client code look like?"
- "which features must be reachable from generated handles?"
- "what is considered official client API versus compatibility fallback?"

## 2. Scope

This standardization note defines:

- the preferred generated navigation model
- the expected generated root, page, reticle, primitive, strobe, and dynamic
  set surfaces
- the required batch-building bridge to `CommandClient`
- the authoring rules that decide which primitives become generated handles
- the expected coverage for text, geometry, image, dynamic, and runtime-feedback features

This note does not define:

- the protobuf wire format itself
- editor workflows
- renderer internals
- how third-party non-C++ clients should implement the transport contract

Those topics are covered by the architecture notes, tutorials, and the broader
interoperability specification.

## 3. Standardized Client Model

The standardized generated navigation graph is:

```text
ui -> page -> reticle -> primitive
ui -> page -> strobe
ui -> page -> dynamic set -> dynamic reticle -> primitive
ui -> window
```

<div class="mfd-standard-pills">
<span class="mfd-standard-pill">Generated API first</span>
<span class="mfd-standard-pill">Transport IDs hidden</span>
<span class="mfd-standard-pill">Typed primitive handles</span>
<span class="mfd-standard-pill">Batch bridge preserved</span>
</div>

Normative expectations:

- application code SHOULD start from the generated UI root
- application code SHOULD mutate generated handles, not raw transport IDs
- low-level authored-name helpers MUST be treated as a compatibility path
- generated client code MUST preserve the authored page / reticle / primitive
  structure visible to the user

## 4. Design Principles

The generated API is standardized around the following principles:

1. authored JSON remains the source of truth
2. generated code exposes authored structure, not runtime implementation detail
3. fixed authored objects use generated transport IDs internally
4. dynamic runtime identities stay hidden behind generated dynamic handles
5. batching remains explicit so realtime clients can control publication cadence

## 5. Required Public Surface

### 5.1 Generated Root

The generated UI root is the official entry point for client code.

It MUST provide:

- `Window()` for whole-window display state
- one accessor per generated page
- `BuildBatch()`
- `BuildCommandBatch(sequence)`
- `SubmitLatest(publisher, sequence)`
- `ApplyFeedback(const mfd::StrobeStatusFeedback&)`
- `ApplyFeedback(const mfd::ActivePageFeedback&)`
- `ApplyFeedbackPayload(std::string_view payload, std::string* error = nullptr)`
- `PollFeedback(mfd::IExchangeChannel& channel, std::size_t maxMessages = 64, std::string* error = nullptr)`

Representative usage:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

ui.Window().SetBrightness(0.65f);
client.ActivatePage(radar);
client.SendBatch(ui.BuildCommandBatch(42U));
```

### 5.2 Page Surface

Each generated page MUST expose:

- one typed page accessor on the root
- `Name()`
- `GeneratedId()`
- `MappingHash()`
- `IsActive()`
- one generated member per authored static reticle on that page
- one generated member per authored page-local blink type
- one generated `strobe` handle
- one generated dynamic-set accessor per authored page dynamic binding

Dynamic-set accessors are authored from `dynamicReticleBindings`, not from one
client-side layer API. The generator MUST validate the page runtime contract
(`layers`, static `layerId`, and dynamic binding coherence), but the generated
client surface MUST NOT expose any operation that changes one reticle layer at
runtime.

### 5.3 Reticle Surface

Each generated static reticle and generated dynamic reticle MUST preserve the
common reticle controls already exposed by `mfd::client::Reticle` and
`mfd::client::DynamicReticle`:

- `SetVisible`
- `SetBlinkEnabled`
- `SetBlink`
- `SetBlinkType`
- `ClearBlinkType`
- `SetPosition`
- `SetRotationDegrees`
- `SetColor`
- `SetThickness`

Each generated dynamic reticle MUST also expose `IsStrobeCaptured()` as the
authoritative runtime capture query backed by the feedback stream.

### 5.4 Primitive Surface

Every authored primitive explicitly marked for client control MUST be reachable
through a generated typed accessor under its owning reticle or dynamic
reticle.

The public primitive-handle families currently standardized are:

- `PrimitiveHandle`
- `TextHandle`
- `TimeHandle`
- `LineHandle`
- `CircleHandle`
- `RingHandle`
- `RectangleHandle`
- `EllipseHandle`
- `SquareHandle`
- `DiamondHandle`
- `TriangleHandle`
- `PolylineHandle`
- `BezierHandle`
- `ArcHandle`
- `ImageHandle`

The generator MUST expose the most specific handle type available for the
authored primitive kind.

Shared generated primitive controls MUST include:

- `SetVisible`
- `SetPosition`
- `SetRotationDegrees`
- `SetScale`
- `SetColor`
- `SetFillColor`
- `SetFilled`
- `SetThickness`
- `SetLineStyle`

## 6. Standardized Feature Coverage

The generated API is expected to cover the following client-visible features.

| Area | Standardized expectation |
| --- | --- |
| Window | Brightness, inversion, and disabled state are staged through `Window()` |
| Pages | Generated page wrappers carry the page transport ID and mapping hash used by page-level helpers |
| Static reticles | Authored page members remain directly reachable and patchable through typed setters |
| Exposed primitives | Client-driven authored primitives are exposed as typed handles under their owning reticles, including `SetLineStyle` for outline-capable primitives |
| Images | Bitmap primitives are first-class generated handles through `ImageHandle` |
| Dynamic reticles | Generated sets own hidden runtime IDs and return typed handles from `Create()` |
| Strobe | Strobe control stays page-scoped through one generated `strobe` handle |
| Runtime feedback | Generated roots absorb runtime feedback, pages expose `IsActive()`, and dynamic reticles expose `IsStrobeCaptured()` |
| Batches | The generated root stages partial patches locally, then emits one coherent command batch on demand |

### 6.1 Static Reticles

Static authored reticles are part of the page surface and are addressed
through generated page members:

```cpp
auto& radar = ui.Radar();
radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.SetPosition({0.20f, -0.15f});
radar.fixedTrackAlpha.TrackLabel().SetText("ALPHA");

client.SendBatch(ui.BuildBatch());
```

### 6.2 Exposed Primitive Example: Progress Bar

The supported pattern for client-driven gauges is to author a decorative frame
plus one exposed fill primitive, then mutate only that exposed primitive from
the generated API.

```cpp
tutorial_ui::TutorialUi ui;
auto& progressBar = ui.Page2().mfdTutorialProgressBar;
auto& fillBar = progressBar.FillBar();

fillBar.SetVisible(true);
fillBar.SetSize({0.22f, 0.06f});
fillBar.SetPosition({-0.11f, 0.0f});

client.SendBatch(ui.BuildBatch());
```

### 6.3 Image Primitive Example

Authored image primitives are standardized exactly like geometry or text
primitives when they are exposed.

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& picture = ui.PictureDemo().pictureDemo;

picture.DemoPicture().SetVisible(true);
picture.DemoPicture().SetPosition({0.0f, 0.0f});
picture.DemoPicture().SetScale({1.10f, 1.10f});
picture.DemoPicture().SetRotationDegrees(8.0f);

client.SendBatch(ui.BuildBatch());
```

### 6.4 Dynamic Reticles

Runtime-owned objects are standardized through typed generated sets:

```cpp
auto& tracks = ui.Radar().DynamicRadarTrack();
auto& track = tracks.Create();

track.SetVisible(true);
track.SetPosition({0.18f, -0.24f});
track.SetRotationDegrees(55.0f);
track.TrackLabel().SetText("B21");
track.Primitive01().SetLineStyle(full_demo_ui::LineStyle::Dashed);

client.SendBatch(ui.BuildBatch());
```

### 6.5 Page-Scoped Strobe

The strobe remains a page capability, not a generated transport object:

```cpp
auto& radar = ui.Radar();
if (radar.strobe.IsValid())
{
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.15f, -0.08f});
    client.SendBatch(ui.BuildBatch());
}
```

### 6.6 Runtime Feedback Convenience Queries

When the runtime feedback stream is consumed, the generated API MUST expose the
authoritative rendering-side state without forcing the user to decode payloads
and correlate reticle ids manually.

Representative usage:

```cpp
auto& page1 = ui.Page1();
auto& tracks = page1.DynamicMfdTutorialRadarTrack();
auto& track = tracks.Create();

std::string feedbackError;
if (ui.PollFeedback(*feedbackChannel, 8U, &feedbackError) > 0U)
{
    const bool page1Active = page1.IsActive();
    const bool trackCaptured = track.IsStrobeCaptured();
}
```

Normative expectations:

- `Page::IsActive()` MUST return `true` only when the latest authoritative
  active-page feedback reports that page as the currently rendered page
- `DynamicReticle::IsStrobeCaptured()` MUST return `true` only when the latest
  authoritative strobe feedback reports that exact dynamic reticle as captured
- `DynamicReticle::IsStrobeCaptured()` MUST return `false` again when capture
  is lost or when the strobe captures another dynamic reticle
- clients MUST NOT have to manage the low-level strobe capture bookkeeping
  themselves when they stay on the generated API path

## 7. Authoring Rules That Affect Generation

### 7.1 Primitive Exposure

An authored primitive becomes part of the generated client API only when it is
explicitly marked for client control.

Practical rules:

- decorative primitives SHOULD remain unexposed
- client-driven primitives SHOULD have stable authored IDs
- the `exposed` flag is the canonical trigger for generated primitive accessors
- round-tripping authored assets through the editor MUST preserve that flag

### 7.2 Draw Order

`drawOnTop` is an authored rendering rule on the reticle model.

That means:

- it affects authored and runtime draw ordering
- it is preserved by JSON loading, serialization, and runtime scene ordering
- it is NOT a per-frame runtime toggle exposed by the generated client API

This distinction is deliberate: drawing order belongs to authored scene
structure, while visibility, color, position, rotation, and primitive content
belong to runtime patching.

### 7.3 Images

Image primitives are standardized as patchable authored primitives.

The generated API supports:

- `SetVisible`
- `SetPosition`
- `SetRotationDegrees`
- `SetScale`

The referenced bitmap asset path itself remains authored JSON data.

## 8. Standardized Transport Boundary

The generated API is a staging layer. It does not replace `CommandClient`.

The standard transport boundary is:

1. application code mutates generated handles
2. the generated root builds partial runtime commands
3. `CommandClient` publishes the resulting batch

Representative publication patterns:

```cpp
client.SendBatch(ui.BuildBatch());
client.SendBatch(ui.BuildCommandBatch(sequence));
ui.SubmitLatest(publisher, sequence);
```

Normative rules:

- generated code MUST hide raw transport IDs from normal user code
- generated page-level helpers MUST carry the matching generated `mappingHash`
- low-level name-based helpers MAY exist, but they are not the preferred API
- generated batches MUST preserve the same command semantics as manually built
  `CommandBatch` instances

## 9. Low-Level Compatibility Boundary

The following remain valid but secondary:

- string-based `CommandClient` helpers
- explicit typed command construction
- raw name resolution through the companion `.generated.map`

Those paths exist for:

- generic tools
- migration code
- replacement-client work
- debugging transport issues

They are not the recommended API for new client applications specific to one
window model.

## 10. Standardization Checklist

Use this checklist when reviewing generator changes or a new generated client
surface.

| Area | Standardized expectation |
| --- | --- |
| Root API | `Window()`, page accessors, `BuildBatch()`, `BuildCommandBatch()`, `SubmitLatest()` |
| Root feedback API | `ApplyFeedback(...)`, `ApplyFeedbackPayload(...)`, `PollFeedback(...)` |
| Page API | stable `Name()`, `GeneratedId()`, `MappingHash()`, `IsActive()`, reticles, blink types, `strobe`, dynamic sets |
| Static reticles | common reticle setters stay available |
| Primitive handles | exposed primitives use the most specific handle type available |
| Image primitives | exposed bitmap primitives use `ImageHandle` |
| Dynamic sets | `Create()` and `Remove(handle)` hide runtime IDs |
| Dynamic runtime queries | `IsStrobeCaptured()` exposes capture state without manual feedback correlation |
| Strobe | page-scoped handle only, no generated strobe transport object |
| Transport bridge | generated batches preserve `mappingHash` and command semantics |
| Authoring link | `exposed` and `drawOnTop` remain preserved by the model and serializer |

## 11. Recommended Reading

- [Use The Mockup As A Client API Reference](../tutorials/11_use_the_mockup_as_a_client_api_reference.md)
- [Drive A Window From A Live Client](../tutorials/04_drive_a_window_from_a_live_client.md)
- [Add And Remove Dynamic Reticles](../tutorials/05_dynamic_reticles.md)
- [Control The Strobe And Receive Feedback](../tutorials/06_strobe_control_and_feedback.md)
- [Generated Client API Architecture](../architecture/generated_client_api.md)
- [Generated Transport Map Specification](../architecture/generated_transport_map.md)
- [MFDStudio External Client Interoperability Specification](./mfd_client_interoperability_specification.md)
