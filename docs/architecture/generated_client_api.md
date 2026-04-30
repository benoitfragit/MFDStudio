# Generated Client API Architecture

## Scope

This note explains how the generated client API is organized and why it is the
preferred client-facing surface for normal C++ integrations.

Read this page when you want:

- the structure behind the generated UI root
- the rationale for page, reticle, primitive, strobe, and dynamic-set handles
- the boundary between generated code and `CommandClient`
- concrete examples aligned with the current generated outputs in the repository

For the normative client-facing contract, also read
[Generated Client API Standardization](../standards/mfd_generated_client_api_standardization.md).

## Core Goal

The generated API exists so that application code can talk in authored UI
concepts instead of in raw runtime identifiers.

That means:

- JSON assets stay the source of truth
- generated code mirrors the authored structure
- transport IDs and `mappingHash` remain implementation detail
- `CommandClient` stays the last-mile transport sender

## Public Navigation Model

The generated static navigation is:

```text
ui -> page -> reticle -> primitive
ui -> page -> strobe
ui -> page -> dynamic set -> dynamic reticle -> primitive
ui -> window
```

\startuml
left to right direction
rectangle "Generated UI root" as Ui
rectangle "WindowDisplay" as Window
rectangle "Page handle" as Page
rectangle "Static reticle handle" as Reticle
rectangle "Primitive handle" as Primitive
rectangle "Strobe handle" as Strobe
rectangle "Dynamic set handle" as DynamicSet
rectangle "Dynamic reticle handle" as DynamicReticle

Ui --> Window
Ui --> Page
Page --> Reticle
Reticle --> Primitive
Page --> Strobe
Page --> DynamicSet
DynamicSet --> DynamicReticle
DynamicReticle --> Primitive
\enduml

Representative usage:

```cpp
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.TrackLabel().SetText("ALPHA");
radar.fixedTrackAlpha.SetColor({77, 224, 255, 255});

client.ActivatePage(radar);
client.SetPageView(radar, {0.0f, 0.0f}, 1.0f);
client.SendBatch(ui.BuildBatch());
```

## Root Responsibilities

The generated root has three jobs:

1. expose authored pages and the window display state
2. accumulate staged partial patches while user code mutates handles
3. build runtime command batches on demand

The root is therefore the bridge between the authored model and the transport
client:

```cpp
full_demo_ui::FullDemoMockupUi ui;
ui.Window().SetBrightness(0.55f);

auto& radar = ui.Radar();
radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.SetPosition({0.2f, -0.1f});

const mfd::CommandBatch batch = ui.BuildCommandBatch(9U);
client.SendBatch(batch);
```

Expected generated root helpers:

- `Window()`
- one accessor per page such as `Radar()` or `PictureDemo()`
- `BuildBatch()`
- `BuildCommandBatch(sequence)`
- `SubmitLatest(publisher, sequence)`

Some generated roots may also expose convenience helpers such as shutdown
batches when the authored model carries that concept. Those helpers are useful,
but they are not the main architectural contract.

## Page Model

Each generated page wrapper carries:

- the authored page name
- the generated page transport ID
- the generated `mappingHash`
- one member per static reticle on the page
- one member per page-local blink type
- one page-scoped `strobe` handle
- one typed generated dynamic-set accessor per authored reticle template

This lets `CommandClient` overloads consume a generated page directly:

```cpp
client.ActivatePage(ui.Radar());
client.SetPageView(ui.Radar(), {0.15f, -0.10f}, 1.25f);
```

The generated page object hides the transport details needed to serialize those
page-level commands correctly.

## Static Reticles

Static reticles remain the most common control surface.

The generated static reticle handle inherits the usual reticle-level controls:

- visibility
- blink enable and blink type
- position
- rotation
- color
- thickness

Example:

```cpp
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.SetBlinkType(radar.fast);
radar.fixedTrackAlpha.SetPosition({0.30f, 0.18f});
radar.fixedTrackAlpha.SetRotationDegrees(-15.0f);
radar.fixedTrackAlpha.TrackLabel().SetText("MOCK");

client.SendBatch(ui.BuildBatch());
```

The architecture is intentionally patch-based: untouched authored fields remain
owned by JSON and are not resent.

## Primitive Handles

Primitive handles exist so that one reticle can expose a few client-driven
sub-parts without turning the whole authored template into a runtime-only
object.

Shared primitive surface:

- `SetVisible`
- `SetPosition`
- `SetRotationDegrees`
- `SetScale`
- `SetColor`
- `SetFillColor`
- `SetFilled`
- `SetThickness`
- `SetLineStyle`

Specialized handle families currently exposed by the public client layer:

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

The generator emits the most specific handle type available for the authored
primitive kind.

## Exposed Primitive Pattern

The most important authoring rule is that client-facing primitive access is
opt-in.

The intended pattern is:

- author one decorative reticle normally
- mark only the client-driven primitives as `exposed`
- let the generator produce typed accessors for those exposed primitives only

This keeps the generated client surface small and meaningful.

### Progress Bar Example

The tutorial now uses this pattern for a progress bar on Page2:

```cpp
tutorial_ui::TutorialUi ui;
auto& progressBar = ui.Page2().mfdTutorialProgressBar;
auto& fillBar = progressBar.FillBar();

fillBar.SetVisible(true);
fillBar.SetSize({0.22f, 0.06f});
fillBar.SetPosition({-0.11f, 0.0f});

client.SendBatch(ui.BuildBatch());
```

Architecturally, this is better than rebuilding the whole reticle every frame:

- the static frame stays authored
- only the fill rectangle is mutated
- the generated API stays typed and discoverable

### Image Primitive Example

Images follow the same model when they are exposed:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& picture = ui.PictureDemo().pictureDemo;

picture.DemoPicture().SetVisible(true);
picture.DemoPicture().SetPosition({0.0f, 0.0f});
picture.DemoPicture().SetScale({1.10f, 1.10f});
picture.DemoPicture().SetRotationDegrees(8.0f);

client.SendBatch(ui.BuildBatch());
```

This is the supported way to animate or reposition authored bitmap content
without changing the referenced asset file itself.

## Dynamic Reticles

Dynamic reticles are runtime-owned instances, so the generated API exposes them
through typed sets instead of static page members.

The main architectural rule is:

- generated code hides runtime dynamic IDs
- application code keeps typed handles returned by `Create()`

Example:

```cpp
auto& tracks = ui.Radar().DynamicRadarTrack();
auto& track = tracks.Create();

track.SetVisible(true);
track.SetPosition({0.18f, -0.24f});
track.SetRotationDegrees(55.0f);
track.TrackLabel().SetText("B21");
track.Primitive01().SetLineStyle(full_demo_ui::LineStyle::Dashed);

client.SendBatch(ui.BuildBatch());

tracks.Remove(track);
client.SendBatch(ui.BuildBatch());
```

This model gives three benefits:

- user code does not invent transport identities
- the generated API still exposes typed primitive access on dynamic instances
- lifecycle remains explicit through `Create()` and `Remove(...)`

## Strobe Model

The strobe is modeled as a page capability, not as an addressable generated
transport object.

That is why generated pages expose a single page-scoped `strobe` member:

```cpp
auto& radar = ui.Radar();
if (radar.strobe.IsValid())
{
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.15f, -0.08f});
    client.SendBatch(ui.BuildBatch());
}
```

The strobe handle intentionally stays page-scoped because:

- authored pages own the strobe definition
- strobe feedback is also page-scoped
- no independent strobe transport table is generated

## Batch Building And Transport Boundary

The generated API is a local staging layer. It does not send by itself.

The normal publication sequence is:

1. mutate generated handles
2. build commands from the generated root
3. publish them through `CommandClient` or `LatestBatchPublisher`

\startuml
top to bottom direction
actor "Application code" as App
rectangle "Generated UI root" as Ui
rectangle "CommandBatch builder" as Builder
rectangle "CommandClient / LatestBatchPublisher" as Transport
rectangle "Runtime window" as Runtime

App --> Ui : mutate typed handles
Ui --> Builder : BuildBatch / BuildCommandBatch
Builder --> Transport : commands + mappingHash
Transport --> Runtime : UDP protobuf batch
\enduml

The important design constraint is that the generated path preserves the same
runtime semantics as manually authored `CommandBatch` instances:

- same command families
- same `mappingHash`
- same sequence handling
- same dynamic lifecycle behavior

## Generated Page IDs And Mapping Hash

When user code passes a generated page to `CommandClient`, the client extracts:

- the generated page ID
- the generated `mappingHash`

This is why page-level helpers can remain ergonomic:

```cpp
client.ActivatePage(ui.Radar());
client.SetPageView(ui.Radar(), {0.0f, 0.0f}, 1.0f);
```

Raw name-based helpers still exist, but they are a fallback path that requires
the companion `.generated.map` for local authored-name resolution.

## Draw Order And Other Authored Rules

Some authored properties intentionally stay outside the generated runtime patch
surface.

`drawOnTop` is the main example:

- it is authored JSON data
- it is preserved by loading and serialization
- it affects runtime draw ordering
- it is not meant to be toggled by the generated per-frame client API

This keeps the generated surface focused on runtime state changes, not on
re-authoring the scene graph from the client.

## Practical Takeaways

- Start from the generated root, not from raw transport concepts.
- Use static reticle members for authored page content.
- Use exposed primitive handles for partial client-driven reticle updates.
- Use `ImageHandle` exactly like other exposed primitive types when a bitmap is
  client-driven.
- Use generated dynamic sets when objects appear and disappear at runtime.
- Keep `CommandClient` as the final sender, not as the primary author-facing
  API.

## Related Documents

- [Generated Client API Standardization](../standards/mfd_generated_client_api_standardization.md)
- [Generated Transport Map Specification](./generated_transport_map.md)
- [Use The Mockup As A Client API Reference](../tutorials/11_use_the_mockup_as_a_client_api_reference.md)
- [Drive A Window From A Live Client](../tutorials/04_drive_a_window_from_a_live_client.md)
