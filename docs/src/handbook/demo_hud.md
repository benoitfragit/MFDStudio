# Demo HUD

The HUD example is a standalone asset/client pair under `examples/hud`.

- Assets: `examples/hud/assets`
- Client: `examples/hud/client`
- Runtime window: `examples/hud/assets/windows/demo_hud_window.json`
- Client target: `demo_hud_client`
- Tests: `demo_hud_client_tests`

The HUD window uses the bundled `ShareTechMono-Regular.ttf` font under the SIL
Open Font License 1.1. The `.ttf` and `OFL-ShareTechMono.txt` files live under
`examples/hud/assets/fonts` and are staged with the HUD assets. Text primitives
also define explicit letter spacing so the HUD does not fall back to raylib's
default font metrics.

Build the runtime and client:

```powershell
cmake --build --preset debug-win32 --target mfd_window demo_hud_client demo_hud_client_tests
```

Launch the runtime window:

```powershell
.\Scripts\Start-DemoHUD.bat
```

Then start the client:

```powershell
.\build\vs2022-win32\examples\hud\client\Debug\demo_hud_client.exe
```

The client loads `assets/windows/demo_hud_window.json` from its own staged
runtime directory and drives the generated transport map next to the authored
window JSON. The runtime staging also copies the HUD asset tree into the shared
`assets` directory so the launch script works from `_Exec` without relying on
repository-root assets.

## Replacing the ImGui demo panel

The Dear ImGui window is not part of the HUD integration contract. It is only a
local control panel used by `demo_hud_client` to drive the bundled
`DemoHudSimulation`.

The current panel fills a compact native demo-client window instead of opening
a floating child panel. Controls are grouped into collapsible categories for
aircraft, maneuver, stick POV, HUD mode, master/target, missile, landing/ILS
and telemetry. The circular stick POV writes normalized pitch/roll intent into
`PilotControls`; dragging the knob down is treated like pulling the stick and
commands nose-up pitch.

An external simulator, aircraft model, or plugin can remove the ImGui shell and
drive the same HUD by keeping the generated UI and the semantic adapter:

- Keep `examples/hud/assets`; these are the authored HUD window, page and
  reticles.
- Keep the generated `DemoHudUi` wrapper emitted from
  `demo_hud_window.json`.
- Keep `demo_hud::HudInputSample` from `DemoHudTypes.h`.
- Keep `demo_hud::BuildHudFrame()` from `DemoHudProjection.h`; this is where
  the HUD projection and EEGS funnel algorithm live.
- Keep `demo_hud::DemoHudController::Populate()` from
  `DemoHudController.h`.
- Keep the normal MFD client publishing path: startup client,
  `mfd::client::LatestBatchPublisher`, sequence number and optional feedback
  polling.
- Remove `Win32Dx11ImGuiHost`, the ImGui draw panels, keyboard handling and the
  bundled `DemoHudSimulation` if the external model already provides aircraft
  state.

The replacement application has one responsibility: build a complete
`demo_hud::HudInputSample` from the external model once per HUD frame, then let
`DemoHudController` convert it to generated UI commands.

```cpp
#include "DemoHudController.h"
#include "DemoHudUi.h"

#include "mfd/client/ClientSdk.h"
#include "mfd/control/CommandClient.h"

demo_hud::HudInputSample BuildHudInputFromAircraft(const AircraftModel& aircraft)
{
    demo_hud::HudInputSample input;

    input.aircraft.elapsedSeconds = aircraft.ElapsedSeconds();
    input.aircraft.yawRad = aircraft.YawRadians();
    input.aircraft.pitchRad = aircraft.PitchRadians();
    input.aircraft.rollRad = aircraft.RollRadians();
    input.aircraft.headingRad = aircraft.HeadingRadians();
    input.aircraft.altitudeMeters = aircraft.AltitudeMetersMsl();
    input.aircraft.radioAltitudeMeters = aircraft.RadioAltitudeMetersAgl();
    input.aircraft.northSpeedMps = aircraft.NorthSpeedMetersPerSecond();
    input.aircraft.eastSpeedMps = aircraft.EastSpeedMetersPerSecond();
    input.aircraft.downSpeedMps = aircraft.DownSpeedMetersPerSecond();
    input.aircraft.mach = aircraft.Mach();
    input.aircraft.normalLoadFactor = aircraft.NormalLoadFactor();
    input.aircraft.throttleRatio = aircraft.ThrottleRatio();
    input.aircraft.specificEnergyRateMps = aircraft.SpecificEnergyRateMetersPerSecond();
    input.aircraft.afterburnerActive = aircraft.AfterburnerActive();

    input.target.valid = aircraft.HasAirToAirTarget();
    input.target.rangeMeters = aircraft.TargetRangeMeters();
    input.target.closingSpeedMps = aircraft.TargetClosingSpeedMetersPerSecond();
    input.target.aspectRad = aircraft.TargetAspectRadians();
    input.target.azimuthRad = aircraft.TargetAzimuthRadians();
    input.target.elevationRad = aircraft.TargetElevationRadians();
    input.target.altitudeMeters = aircraft.TargetAltitudeMetersMsl();

    input.weapon.masterMode = demo_hud::HudMasterMode::Nav;
    input.weapon.weaponMode = demo_hud::HudWeaponMode::None;
    input.weapon.gunMode = demo_hud::HudGunMode::None;
    if (aircraft.IsAirToAirGunMode())
    {
        input.weapon.masterMode = demo_hud::HudMasterMode::AirToAir;
        input.weapon.weaponMode = demo_hud::HudWeaponMode::AirToAirGun;
        input.weapon.gunMode = demo_hud::HudGunMode::Eegs;
    }
    else if (aircraft.IsAirToAirMissileMode())
    {
        input.weapon.masterMode = demo_hud::HudMasterMode::AirToAir;
        input.weapon.weaponMode = demo_hud::HudWeaponMode::AirToAirMissile;
    }
    else if (aircraft.IsCcipMode())
    {
        input.weapon.masterMode = demo_hud::HudMasterMode::AirToGround;
        input.weapon.weaponMode = demo_hud::HudWeaponMode::AirToGroundCcip;
    }
    else if (aircraft.IsStrafeMode())
    {
        input.weapon.masterMode = demo_hud::HudMasterMode::AirToGround;
        input.weapon.weaponMode = demo_hud::HudWeaponMode::AirToGroundStrafe;
        input.weapon.gunMode = demo_hud::HudGunMode::Strafe;
    }
    else if (aircraft.LandingModeActive())
    {
        input.weapon.masterMode = demo_hud::HudMasterMode::Landing;
    }
    input.weapon.masterArm = aircraft.MasterArm();
    input.weapon.simulateMode = aircraft.SimulateMode();
    input.weapon.selectedMissile = aircraft.SelectedMissileIsAim120()
        ? demo_hud::MissileType::Aim120C
        : demo_hud::MissileType::Aim9M;
    input.weapon.inventory.aim9m = aircraft.Aim9Remaining();
    input.weapon.inventory.aim120c = aircraft.Aim120Remaining();
    input.weapon.gunRoundsRemaining = aircraft.GunRoundsRemaining();
    input.weapon.triggerHeld = aircraft.GunTriggerHeld();
    input.weapon.targetLocked = aircraft.HasGunTargetLock();
    input.weapon.targetWingspanMeters = aircraft.TargetWingspanMeters();
    input.weapon.missileInFlight = aircraft.HasActiveMissile();
    input.weapon.activeMissileTimeRemainingSeconds = aircraft.ActiveMissileTimeRemainingSeconds();
    input.weapon.activeMissilePhase = aircraft.ActiveMissilePhaseForHud();

    input.airGround.valid = aircraft.HasAirToGroundSolution();
    input.airGround.slantRangeMeters = aircraft.AirGroundSlantRangeMeters();
    input.airGround.pipperAzimuthRad = aircraft.CcipPipperAzimuthRadians();
    input.airGround.pipperDepressionRad = aircraft.CcipPipperDepressionRadians();
    input.airGround.fallLineAzimuthRad = aircraft.BombFallLineAzimuthRadians();
    input.airGround.solutionCueValid = aircraft.HasCcipSolutionCue();
    input.airGround.solutionCueDepressionRad = aircraft.CcipSolutionCueDepressionRadians();
    input.airGround.pullupAnticipationCueValid = aircraft.HasPullupAnticipationCue();
    input.airGround.pullupAnticipationCueDepressionRad = aircraft.PullupCueDepressionRadians();
    input.airGround.timeToReleaseSeconds = aircraft.TimeToReleaseSeconds();
    input.airGround.timeToGoSeconds = aircraft.TimeToGoSeconds();

    input.approach.landingGearDown = aircraft.LandingGearDown();
    input.approach.weightOnWheels = aircraft.WeightOnWheels();
    input.approach.landingModeActive = aircraft.LandingModeActive();
    input.approach.landingDeclutterActive = aircraft.LandingDeclutterActive();
    input.approach.flightPathMarkerAvailable = aircraft.FlightPathMarkerAvailable();
    input.approach.runwayReferenceValid = aircraft.HasRunwayReference();
    input.approach.runwayHeadingRad = aircraft.RunwayHeadingRadians();
    input.approach.runwayDistanceMeters = aircraft.RunwayDistanceMeters();
    input.approach.runwayElevationMeters = aircraft.RunwayElevationMeters();

    input.ils.powered = aircraft.IlsPowered();
    input.ils.selected = aircraft.IlsSelected();
    input.ils.signalValid = aircraft.IlsSignalValid();
    input.ils.localizerDeviationDots = aircraft.IlsLocalizerDeviationDots();
    input.ils.glideslopeDeviationDots = aircraft.IlsGlideslopeDeviationDots();
    input.ils.courseRad = aircraft.IlsCourseRadians();
    input.ils.commandSteeringActive = aircraft.IlsCommandSteeringActive();

    return input;
}
```

The realtime publishing loop stays small:

```cpp
demo_hud_ui::DemoHudUi ui;
demo_hud::DemoHudController controller;
std::uint32_t sequence = 1;

ui.Initialize();
ui.SendStartup(startupClient, pageView, "Demo HUD | READY");

while (model.IsRunning())
{
    const demo_hud::HudInputSample input = BuildHudInputFromAircraft(model.Aircraft());
    ui.Run();
    controller.Populate(ui, input);

    if (!ui.SubmitLatest(publisher, sequence++))
    {
        break;
    }
}

ui.SubmitShutdown(publisher, sequence++, "Demo HUD | CLIENT STOPPED");
publisher.Flush();
```

The important boundary is `HudInputSample`. It uses SI units and radians:

- altitude is meters, speed is meters per second, and attitude is radians;
- velocity uses an NED frame, so `downSpeedMps` is positive downward and
  negative while climbing;
- target azimuth/elevation are line-of-sight errors relative to the aircraft
  nose;
- `WeaponInputSample` must already contain resolved avionics state. Do not wire
  raw cockpit, panel, HOTAS or ImGui commands directly to generated HUD handles.
- `AirGroundInputSample`, `ApproachInputSample` and `IlsInputSample` are also
  semantic aircraft/avionics state. They are not generated UI coordinates.

For a concrete local reference, read
`DemoHudApplication::UpdateHudInputBufferFromUi()`. That function is the demo
client boundary where UI controls become a semantic `HudInputSample`; an
external integration should replace that data producer, not the HUD projection
or generated UI command path.

`DemoHudSimulation` and the ImGui panel are optional demo producers. A real
integration can remove them while keeping `DemoHudTypes.h`,
`DemoHudProjection.h/.cpp`, `DemoHudController.h/.cpp` and the generated
`DemoHudUi` wrapper. In that arrangement the external model only fills
`HudInputSample`; `BuildHudFrame()` still computes the EEGS funnel and
`DemoHudController` still sends the Bezier rails through the generated UI API.

Run the focused validation:

```powershell
ctest --preset test-debug-win32 -R "demo_hud|DemoHudSimulation" --output-on-failure
```

## HUD angular projection

Every conformal HUD symbol shares one explicit field of view, resolved in a
single place in `DemoHudProjection.cpp`. This removes the previous mix of
independent scales (a ~58 degree pitch scale, an 18 degree azimuth scale and a
14 degree elevation scale) that made the pitch ladder, radar/target cues and
A-G cues disagree with each other.

The default field of view is:

- `kHudPitchLadderVerticalFovDeg = 30.0f` (total vertical);
- `kHudConformalVerticalFovDeg = 30.0f` and
  `kHudConformalHorizontalFovDeg = 30.0f`;
- `kHudConformalHalfWidthUnits = 1.0f` and `kHudConformalHalfHeightUnits = 1.0f`.

This is a deliberate product choice to reduce visual saturation. The BMS ACM
mode is 30 degrees horizontal by 20 degrees vertical and covers slightly more
than the HUD field of view; the demo instead uses a 30 degree total vertical
HUD so the pitch ladder is legible at a glance.

The authored HUD page spans `[-1, +1]`, and half the field of view maps to the
`1.0` unit half-extent. The scale is therefore:

- `2.0 / 30.0 = 0.0666667` HUD units per degree, identical horizontally and
  vertically, so conformal symbols keep the BMS 1:1 attitude-bar ratio;
- pitch ladder bars every 5 degrees at `y = degrees * 0.0666667`
  (`5 -> 0.333333`, `10 -> 0.666667`, `15 -> 1.0`).

At level flight only the `+/-5`, `+/-10` and `+/-15` bars (plus the horizon)
reach the aperture, so `+/-20`, `+/-25` and `+/-30` are no longer drawn. Bars
above the horizon are solid; bars below are dashed; the `-2.5` degree bar is a
separate landing-mode reticle. The authored
`examples/hud/assets/reticles/demo_hud_pitch_ladder.json` encodes exactly this
scale, and `DemoHudProjectionTests.PitchLadderJsonMatchesCppAngularScale` fails
if the JSON and C++ scales ever drift apart.

### Conformal versus non-conformal symbology

Only symbols that represent a boresight-relative angle go through the shared
projection: the target designator, missile diamond, TD circle, CCIP pipper,
strafe pipper, bomb fall line, CCIP solution cue and pull-up anticipation cue.
They all call `ProjectBoresightAngularOffsetToHud()` (or the pitch equivalent
`ProjectPitchOffsetToHud()`), so a given angle always lands at the same HUD
position regardless of which cue it drives.

Non-conformal elements keep their own reference frames and are intentionally
excluded: the dynamic launch zone, range cue, speed/altitude tapes, heading
tape, textual readouts, timers and range scales. They encode magnitudes or
ranges, not line-of-sight angles, so forcing them through the angular
projection would be meaningless.

### Out-of-field-of-view behavior

The projection never silently clamps a conformal symbol to the glass edge.
`ProjectBoresightAngularOffsetToHud()` returns a `ProjectedHudPoint` with an
explicit `insideFov` flag, and the projection hides the corresponding cue when
it is false: a target, TD circle, CCIP pipper or strafe pipper outside the field
of view is masked rather than pinned to the edge where it would misreport the
angle. No new off-boresight locator symbology is invented for this task.

Missile threat detection is out of scope. `WeaponInputSample` exposes
`missileInFlight`, `activeMissileTimeRemainingSeconds` and `activeMissilePhase`,
which describe the ownship's own launched missile timeline, not a detected
hostile missile. There is no `missileThreatDetected` / inbound-missile
line-of-sight input, so no inbound-missile symbology is projected. Adding one
would require a new documented and tested semantic input.

## HUD modes and reticles

The HUD remains a single-page asset. `examples/hud/assets/pages/demo_hud.json`
contains one `HUD` page, and mode-specific symbology is represented by named
reticle instances on that page. The controller changes visibility, position,
rotation and scale from the projected `HudFrame`; it does not switch pages.

The current contextual reticles are:

- Air-to-air missile: target designator, missile diamond, missile circle,
  dynamic launch zone, range cue, break X and attack steering cue.
- Air-to-air gun / EEGS: funnel, MRGS, FEDS, TD circle, 1G pipper, Max-G
  pipper, solution circle and BATR.
- Air-to-ground / STRF: 50 mR / 40 mR strafe reticle, 2 mR in-range cue,
  moving-target indices, 1 mR pipper and bullet-track line.
- Air-to-ground / CCIP: bomb-fall line, CCIP pipper, solution cue and pull-up
  anticipation cue.
- Landing / ILS: -2.5 degree landing pitch line, ILS localizer/glideslope
  bars, command steering cue and W-steering fallback.

The authored reticle proportions are intentionally conservative. The C++ layer
changes only the dimensions that are data-driven, such as EEGS funnel Bezier
rails and line endpoints, so adding a mode does not disturb the existing HUD
layout.
Weapon-delivery reticles are gated by resolved avionics state: `masterArm` or
`simulateMode` must be active before A-A missile, EEGS, STRF or CCIP delivery
cues are published. Every frame starts by hiding contextual weapon, landing and
ILS reticles before the active mode is applied, so changing modes cannot leave
an old reticle visible through retained runtime state.
The demo EEGS funnel is not a certified ballistic solver, but it is not a
static cone: its exposed Bezier control points are derived from the semantic
aircraft/target sample, including flight path, load factor, target line of
sight, range/wingspan and target acceleration. The authored funnel now uses a
central spine and range-sampled wall half-widths, keeping the visible cue as a
long, narrow gunnery corridor instead of a decorative V shape. Wind is
intentionally not faked; an external aircraft model should add a documented
semantic input before wind is projected into the HUD.

Visual reference captures used for the current HUD mode work are kept in
`examples/hud/visual_ref`. They are documentation/reference material only and
must not be added to the runtime asset graph.

## Semantic input contract

`demo_hud::HudInputSample` is the integration boundary. It is split into
semantic sub-samples:

- `aircraft`: attitude, velocity, altitude, energy and throttle in SI units.
- `target`: air-to-air target range, closure, aspect and line-of-sight in SI
  units and radians.
- `weapon`: resolved master mode, weapon mode, gun mode, arm/sim state,
  selected missile, inventory, gun rounds, trigger, target lock, target
  wingspan and ammunition family.
- `airGround`: CCIP/STRF impact-point range and angular offsets, bomb-fall-line
  azimuth, solution cue, PUAC and optional timing.
- `approach`: landing gear, weight-on-wheels, landing mode, landing declutter,
  FPM availability and runway reference data.
- `ils`: receiver power, ILS selection, signal validity, localizer/glideslope
  deviation dots, course and command-steering state.

All angular offsets are relative to aircraft boresight unless a field says
otherwise. ILS localizer deviation dots move the vertical localizer bar right
when positive. ILS glideslope deviation dots move the horizontal glideslope bar
up when positive. Both are clamped by the projection layer before generated UI
commands are emitted.

The gunnery and weapon-delivery projection is deliberately simplified. It is a
realistic HUD presentation model for the demo, not a certified F-16 ballistic
computer. The simplifications are localized in `BuildHudFrame()` helpers and
covered by focused tests for visibility, clamp and finite output behavior.

### EEGS funnel projection

External simulations do not send Bezier points directly. They provide semantic
aircraft and target values through `HudInputSample`, and
`DemoHudProjection.cpp` computes the five-point Bezier rails used by the
generated `eegsFunnel` reticle.

The funnel is built from a central spine. Each Bezier sample represents a
range station; the wall offset at that station is the normalized angular
half-width of the target wingspan at that range. The projection then adjusts
scale, drift, curvature and skew from flight-path marker position, normal load
factor, airspeed, target line-of-sight position and
`targetAccelerationMps2`. The far end stays narrow and the near end opens
without creating a broad vase-shaped cue.
