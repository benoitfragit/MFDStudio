# Project User Space To Page Space

This tutorial shows how to keep the MFD page in normal logical coordinates
while your external application keeps working in its own physical frame.

This is the recommended approach when your client already uses:

- positions in nautical miles
- rotations in radians
- its own reference origin

## At A Glance

\startuml
left to right direction
rectangle "Aircraft position in NM" as AircraftPosition
rectangle "Aircraft heading in radians" as AircraftHeading
rectangle "Track positions in NM" as TrackPositions
rectangle "Track headings in radians" as TrackHeadings
rectangle "UserSpaceProjector" as Projector
rectangle "Page positions in [-1, 1]" as PagePositions
rectangle "Page rotations in degrees" as PageRotations
rectangle "CommandClient" as CommandClient

AircraftPosition --> Projector
AircraftHeading --> Projector
TrackPositions --> Projector
TrackHeadings --> Projector
Projector --> PagePositions
Projector --> PageRotations
PagePositions --> CommandClient
PageRotations --> CommandClient
\enduml

Important idea:

- the window still knows only page coordinates `[-1, 1]`
- the helper exists only on the client side
- the protobuf API does not need a new special message for this
- if you do not need such a conversion, do not use this helper and keep sending
  normal page coordinates directly in `[-1, 1]`

## Step 1 - Use a page designed for an offset origin anchor

The project now ships an example page:

- `assets/pages/aircraft_centric.json`

Its goal is to visualize a classic offset-origin use case:

- the aircraft symbol is anchored at `(0.0, -0.3)` in page space
- the page still uses normal logical coordinates
- labels explain the axis mapping used by the example

This page is already listed in:

- `assets/windows/demo_pages.json`

## Step 2 - Understand the frame you want to project

In this example:

- the client-side reference origin is a moving origin
- the frame heading is expressed in radians
- physical `+X` must appear along page `+Y`
- physical `+Y` must appear along page `+X`
- one physical unit is one nautical mile

Important clarification:

- the page itself is not expressed in nautical miles
- the page itself is not expressed in radians
- nautical miles and radians are only client-side domain units
- the helper converts them to normal page coordinates and page rotations

The page anchor stays fixed at:

```cpp
mfd::Vec2 {0.0f, -0.3f}
```

That means the aircraft itself stays visible slightly below the page center.

## Step 3 - Build the projection frame

```cpp
#include "mfd/control/UserSpaceProjector.h"

mfd::UserSpaceFrame frame;
frame.userOrigin = aircraftPositionNm;
frame.pageAnchor = {0.0f, -0.3f};
frame.originRotationRadians = aircraftHeadingRadians;
frame.pageUnitsPerUserUnit = 0.04f;
frame.userXAxisInPage = {0.0f, 1.0f};
frame.userYAxisInPage = {1.0f, 0.0f};
```

Read this frame as:

- `userOrigin`
  the origin of the client-side reference frame, in user-space units
- `pageAnchor`
  where the user-space origin `(0, 0)` must appear in the page
- `originRotationRadians`
  the rotation of the client-side reference frame, in radians
- `pageUnitsPerUserUnit`
  how many page units one user-space unit represents
- `userXAxisInPage`
  physical `+X` is drawn along page `+Y`
- `userYAxisInPage`
  physical `+Y` is drawn along page `+X`

The scale line deserves a concrete reading:

```cpp
frame.pageUnitsPerUserUnit = 0.04f;
```

This means:

- `1` user-space unit becomes `0.04` page units
- if one user-space unit is one nautical mile, then `1 NM -> 0.04`

So in this example:

- `5 NM` becomes `0.20` page units
- `10 NM` becomes `0.40` page units

That is why the example page contains `5 NM` and `10 NM` reference rings.

If your client already works directly in page space:

- do not use `UserSpaceProjector`
- keep sending positions in `[-1, 1]`
- keep sending rotations in page-space degrees as usual

## Step 4 - Create the projector

```cpp
mfd::UserSpaceProjector projector(frame);
```

This helper will:

- subtract the moving origin
- remove the origin rotation
- apply the axis mapping
- apply the page scale
- return a page position in `[-1, 1]`

If `userOrigin = {0, 0}`, `pageAnchor = {0, 0}`, identity axes, and
`pageUnitsPerUserUnit = 1.0f`, then the helper behaves like an identity
mapping and your user-space values already match page-space values.

## Step 5 - Project one position and one rotation

```cpp
const mfd::Vec2 pagePosition = projector.ToPagePosition(trackWorldPositionNm);
const float pageRotationDegrees = projector.ToPageRotationDegrees(trackHeadingRadians);
```

At this point:

- `trackWorldPositionNm` is still in nautical miles
- `trackHeadingRadians` is still in radians
- `pagePosition` is ready for `ReticlePatch.position`
- `pageRotationDegrees` is ready for `ReticlePatch.rotationDegrees`

## Step 6 - Send dynamic reticles with the regular API

```cpp
#include "mfd/control/CommandClient.h"

struct TrackSample
{
    std::string id;
    mfd::Vec2 worldPositionNm;
    float headingRadians = 0.0f;
    std::string label;
};

std::vector<TrackSample> tracks {
    {"trk_001", {aircraftPositionNm.x + 8.0f, aircraftPositionNm.y + 1.5f}, 0.20f, "T001"},
    {"trk_002", {aircraftPositionNm.x + 3.0f, aircraftPositionNm.y - 5.0f}, 1.10f, "T002"},
    {"trk_003", {aircraftPositionNm.x - 2.0f, aircraftPositionNm.y + 7.0f}, -0.45f, "T003"}
};

std::vector<mfd::DynamicReticleState> states;
states.reserve(tracks.size());

for (const TrackSample& track : tracks)
{
    mfd::ReticlePatch patch;
    patch.visible = true;
    patch.position = projector.ToPagePosition(track.worldPositionNm);
    patch.rotationDegrees = projector.ToPageRotationDegrees(track.headingRadians);
    patch.color = mfd::ColorRgba {122, 227, 255, 255};
    patch.thickness = 0.004f;
    patch.text = track.label;

    mfd::DynamicReticleState state;
    state.reticleId = track.id;
    state.patch = std::move(patch);
    states.push_back(std::move(state));
}

client.UpsertDynamicReticles("AircraftCentric", "radar_track", states);
```

This is the important result:

- the window receives only normal page-space commands
- the client keeps its own physical units
- the helper isolates the projection logic in one place

## Step 7 - Update the frame every cycle

If the aircraft moves every `20 ms`, update the frame every cycle before
projecting the tracks:

```cpp
frame.userOrigin = aircraftPositionNm;
frame.originRotationRadians = aircraftHeadingRadians;
projector.SetFrame(frame);
```

Then project and send the new batch.

## Step 8 - What you should see

On the `AircraftCentric` page:

- the aircraft symbol stays fixed at `(0.0, -0.3)`
- a target with larger physical `X` moves upward on the page
- a target with larger physical `Y` moves to the right on the page
- heading values expressed in radians still render correctly after projection

If you skip the helper entirely, nothing changes in the runtime:

- page space still remains `[-1, 1]`
- commands still use normal page-space values
- this helper is purely optional

## Why this design is clean

This approach keeps responsibilities separate:

- the window runtime stays simple
- the protobuf command API stays generic
- the external application keeps its domain units
- the projection rule is explicit, testable, and reusable

## Result

You now have a clean pattern for aircraft-centric pages:

- page space remains `[-1, 1]`
- physical positions can stay in nautical miles
- physical rotations can stay in radians
- the client performs the conversion through `mfd::UserSpaceProjector`
