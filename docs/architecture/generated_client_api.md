# Generated Client API Architecture

## Scope

This note freezes the target generated client API for the primitive-addressable
refactor. It defines the public shape consumed by client applications, the
authored exposure rules, and the remaining low-level compatibility boundary.

## Goals

- keep authored JSON assets as the only source of truth
- generate a high-level client API from the parsed window model
- make fixed authored objects transportable through internal integer IDs
- keep transport IDs completely hidden from application code
- make primitive access follow the same navigation model as pages and reticles

## Public API Shape

The generated static navigation is:

- `ui -> page -> reticle -> primitive`

Representative usage:

```cpp
auto& line = ui.Page1().headingScale.Line1();
line.SetVisible(true);
line.SetColor({0, 255, 0, 255});
line.SetThickness(0.0035f);

ui.Page1().headingBox.HeadingValue().SetText("123");
```

The generated API is the normal client workflow. End users must not call
string-based primitive setters in normal usage.

## Naming Rules

- page accessors keep the current generated PascalCase accessor pattern:
  - `ui.Page1()`
- generated page members keep lower camel case for authored reticles:
  - `ui.Page1().headingScale`
- generated primitive accessors use PascalCase methods on the reticle handle:
  - `ui.Page1().headingScale.Line1()`
- blink types remain generated page members:
  - `ui.Page1().attention`
- the page strobe is exposed through one generic page member:
  - `ui.Page1().strobe`

## Handle Types

The generated API exposes typed handles:

- `ReticleHandle`
- `PrimitiveHandle`
- `LineHandle`
- `TextHandle`
- `CircleHandle`
- `RectangleHandle`
- `EllipseHandle`
- `ArcHandle`
- `PolygonHandle`
- `PolylineHandle`
- `TimeHandle`
- `StrobeHandle`

The exact emitted set depends on the authored primitives present in the parsed
window model.

## Reticle-Level Surface

Reticles remain the main authored control level. The generated reticle handle
keeps the common controls already available today:

- `SetVisible`
- `SetBlinkEnabled`
- `SetBlink`
- `SetBlinkType`
- `ClearBlinkType`
- `SetPosition`
- `SetRotationDegrees`
- `SetColor`
- `SetThickness`

Compatibility-only helpers may temporarily remain available on the low-level
reticle type, but generated code must not rely on string primitive addressing.

## Primitive-Level Surface

Every exposed primitive is generated under its owning reticle. Primitive
controls are split between a shared base and kind-specific specializations.

Shared primitive surface:

- `SetVisible`
- `SetColor`
- `SetThickness`
- `SetPosition`
- `SetRotationDegrees`
- `SetScale`
- `SetBlinkEnabled`
- `SetBlinkType`
- `ClearBlinkType`

Text-like specializations:

- `SetText`
- `SetLetterSpacing`

Geometry-specific specializations:

- line: `SetStart`, `SetEnd`
- circle: `SetRadius`
- ring: `SetInnerRadius`, `SetOuterRadius`
- rectangle and ellipse: `SetWidth`, `SetHeight`, `SetSize`

Generation must only expose coherent operations for the primitive kind.

## Primitive Exposure Rules

- decorative primitives are not exposed by default
- only named client-driven primitives are emitted
- an exposed primitive must have a stable authored identity
- exposure is opt-in in authored JSON
- non-exposed primitives remain an internal rendering detail of the reticle

## Strobe Model

The strobe is not part of the primitive ID mapping.

The generated page surface exposes one generic page-scoped handle:

```cpp
const auto& strobe = ui.Page1().strobe;
if (strobe.IsValid())
{
    const auto info = strobe.Info();
    if (info.active)
    {
        // ...
    }
}
```

Strobe invariants:

- no authored strobe transport ID is generated
- no strobe lookup is required from user code
- the handle is always present on the page surface
- the handle is invalid when the page has no strobe
- runtime strobe commands remain page-scoped
- strobe feedback remains page-scoped

The `StrobeHandle` is a lightweight page capability wrapper exposing:

- `IsValid()`
- `PageName()`
- `Info()`
- `SetActive(bool)`
- `SetPosition(Vec2)`

`Info()` returns a value object describing:

- whether the page actually owns a strobe
- whether the strobe is currently active
- current logical position
- capture configuration
- magnetization configuration

## Dynamic Reticles

Dynamic reticles are runtime-created, so they do not appear as fixed authored
page members. The generated page surface instead exposes one typed dynamic-set
accessor per authored template:

```cpp
auto& tracks = ui.Radar().DynamicRadarTrack();
auto& track = tracks.Create();
track.TrackLabel().SetText("AF001");
tracks.Remove(track);
```

Dynamic-reticle invariants:

- generated page APIs do not expose `Dynamic(std::string_view templateId)`
- generated client code does not ask the user for a runtime reticle id
- `Create()` allocates one hidden runtime-scoped integer id inside the generated set
- `Remove(...)` removes that generated instance by handle
- application code keeps the returned typed handle or pointer while the domain
  object is alive
- primitive-level typed access is still available on the generated dynamic
  reticle handle when the authored template exposes primitives

Low-level dynamic reticle patches may still carry primitive updates internally,
but the normal generated workflow is now handle-based rather than id-based.

## Compatibility Strategy

Compatibility stays low-level and explicit:

- raw `CommandClient` helpers may still accept authored names
- those helpers must be constructed with the companion generated transport map
- name-based primitive patching is resolved locally before serialization
- generated code must continue to use typed page, reticle, primitive, strobe,
  and dynamic-set handles
- serialized command payloads must omit duplicate authored-name fields and rely
  on `mappingHash` + transport IDs only

String-based addressing is therefore no longer part of the normal generated
client-facing workflow.
