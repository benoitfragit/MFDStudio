# LHLD Integration Notes

This example hosts a tactical left-hand MFD display in a Win32/DX11 ImGui
application. The MFD page itself is rendered offscreen through the runtime API,
then composited inside an ImGui-drawn bezel. The bundled radar/navigation/stores
simulation is replaceable by design.

## Main Entry Points

| Area | File | Keep or Replace |
|---|---|---|
| Host startup | `client/src/main.cpp` | Keep unless the application shell changes. |
| ImGui bezel, cockpit console and publishing loop | `client/src/MfdApplication.*` | Replace or thin down when embedding in a real cockpit. |
| Cockpit-only drawing primitives | `client/src/CockpitUi.*` | Keep for the demo host; it does not affect authored MFD pages. |
| Replaceable semantic input source | `client/src/MfdInputSource.h` | Keep. Implement this for a real simulator feed. |
| Fake radar/navigation/stores source | `client/src/MfdRadarSimulation.*` | Replace with a real aircraft, avionics or network source. |
| Semantic MFD contract | `client/src/MfdTypes.h` | Keep. This is the stable frame data boundary. |
| Shared radar geometry | `client/src/MfdRadarGeometry.*` | Keep. The radar source uses it for visibility and the projection uses it only for coordinate conversion. |
| Stateless FCR/HSD projection | `client/src/MfdProjection.*` | Keep unless page geometry changes. It converts semantic units into page coordinates without deciding radar track visibility. |
| Generated-UI adapter | `client/src/MfdController.*` | Keep. It owns generated dynamic-reticle handles and maps published track states to authored assets without applying radar detection rules. |
| Offscreen runtime embedding | `client/src/OffscreenMfdView.*` | Keep if the MFD still renders inside another host window. |
| Generated wrappers | `client/generated/LhldUi.*` | Regenerate from assets, do not hand-edit. |
| Authored MFD assets | `assets/windows`, `assets/pages`, `assets/reticles` | Keep as the authored LHLD layout. |

## Replacement Boundary

The real integration point is `MfdInputSource`:

```cpp
class RealAircraftMfdSource final : public lhld::MfdInputSource
{
    // Fill and return one complete lhld::MfdInputSample per frame.
};
```

`MfdApplication` accepts a `std::unique_ptr<MfdInputSource>`. Passing a custom
source replaces the bundled `MfdRadarSimulation` without changing
`MfdController`, `MfdProjection` or the generated `LhldUi` wrappers.

## Replacing the Mini Simulation

Replace `MfdRadarSimulation` with an implementation that:

- owns transport, simulator packets and aircraft-specific state internally;
- returns a complete `MfdInputSample` from `Inputs()`;
- advances only in `Step(float deltaSeconds)`;
- clamps or validates non-finite external values before publishing them;
- keeps radar controls, master mode, stores and HSD navigation state semantic.
- owns radar detection volume, track loss, extrapolation, designation and STT
  lifecycle decisions before publishing tracks.

The projection layer expects meaningful aircraft/MFD state, not raw panel
button events. In particular, an implementation must publish
`RadarTrack::active == false` when a track is absent from the radar picture
(outside the applicable search volume, lost, or unavailable because the radar
is off/silent). It must also publish the radar-owned `RadarTrackState` used for
RWS search returns, TWS trackfiles, system targets and STT tracks.

## FCR Input Contract

The radar boundary deliberately uses sensor-native values. Each `RadarTrack`
publishes slant range in nautical miles, azimuth and elevation in degrees, plus
track motion/classification data. Target altitude is not an input: the
projection derives it from ownship altitude, slant range and elevation.

`RadarTrack::active` and `RadarTrack::state` are authoritative input data. The
projection never removes an active track based on range, azimuth, elevation,
radar power or submode; it only clamps coordinates defensively to the authored
B-scope. The controller then chooses the matching RWS, TWS, bugged or STT asset
from the published state. The bundled `MfdRadarSimulation` implements the demo
detection volume and is the only place where the example decides that a track
enters or leaves the radar picture.

`RadarSettings::fieldOfView` selects NORM or EXP. In EXP, the input source owns
the displayed-field decision and the projection applies the documented 4:1
range/azimuth magnification about the acquisition cursor. The ACQ cursor remains
at its true unexpanded position, the antenna scan coverage is unchanged, and
STT forces the effective field of view back to NORM. Target-driven auto range
and bump-azimuth logic must remain disabled in an EXP-capable real source; the
demo does not implement either automatic function.

The acquisition cursor is the only normalized input. Its `x` and `y` axes stay
in `[-1, 1]`, which is natural for an independent UI device. `MfdProjection`
maps that cursor to the authored B-scope and derives the displayed cursor
altitude limits; callers never calculate page coordinates or normalized range.

The FCR side controls follow the documented OSB layout: OSB 17 changes bars,
OSB 18 selects azimuth width, and OSB 19/20 decrease/increase the
5/10/20/40/80/160 NM range rotary without wrapping. TWS initializes to A2/3B
and the radar source constrains its selections to A6/2B, A2/3B and A1/4B.
The panel publishes only the requested azimuth/bar selection; the input source
publishes the effective width, bars and cursor-centered scan-volume location
consumed by the projection and controller.

FCR symbols are dynamic reticles allocated only when needed. RWS search tracks
are white, TWS trackfiles and bugged/STT tracks are yellow, and any extrapolated
track is red. BUG designates only the dynamic track reported captured by the
runtime strobe through `IsStrobeCaptured()`; there is no nearest-track fallback.
The operating state drives the centered `FCR OFF` and `NO RAD` messages. The
Falcon-style perimeter keeps the range/DLZ scale on the outer right border and
shows a compact search-context line below the top OSB labels; after designation,
that line switches to target aspect, heading, NCTR state, speed and closure.
STT removes the search-volume gates and adds a CATA steering cross at target
range when the computed collision angle remains within 60 degrees. The cross is
an authored filled `✠`-style vector symbol rather than a font-dependent glyph.

The demo starts with four closely grouped tracks around the ACQ cursor. Toggle
EXP with FCR OSB 3 or the panel **EXP** control to separate them while keeping
the cursor, scan volume, range scale and antenna coverage fixed.

## Replacing the ImGui Panel

The ImGui panel in `MfdApplication` is a sample operator surface. Its responsive
cockpit shell and physical-looking controls live in `CockpitUi`; the MFD texture,
authored pages and projection remain independent. A real
cockpit, plugin, network bridge or scripted test can replace it. Keep these
contracts intact:

The demo FCR console uses independent **FCR PWR** and **RF** two-position
switches, an **ANT ELEV** thumbwheel and a normalized two-axis cursor-slew pad.
Those widgets only collect semantic operator intent; radar detection and track
publication remain owned by the input source.

- `MfdInputSample` is the only data consumed by `MfdController::Populate()`;
- panel commands should update the real input source or semantic state first;
- generated UI output should be written only through `MfdController`;
- authored assets and `lhld_window.generated.map` must stay in sync with
  regenerated `LhldUi.*`;
- offscreen rendering can stay in `OffscreenMfdView` if the page is embedded
  into another UI.

For waypoint editing, the demo panel now treats `ADD WPT` as insertion after the
selected steerpoint, up to the authored HSD slot limit. `OVERWRITE` only edits
the selected steerpoint.

## Local Validation

Preferred local build and focused tests:

```powershell
cmake --build --preset debug-win32 --target lhld_client lhld_client_tests
ctest --preset test-debug-win32 -R "Lhld" --output-on-failure
```

Use the visual references under `visual_ref/` only for audit. They are not
runtime assets and should not be wired into `lhld_window.json`.
