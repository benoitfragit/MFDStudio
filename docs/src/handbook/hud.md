# HUD

The HUD example is split into a reusable shared library and a main client
under `examples/hud`.

- Assets: `examples/hud/assets`
- Reusable HUD runtime (DLL/shared library): `examples/hud/runtime`, target
  `hud_runtime`
- Main HUD client (executable): `examples/hud/main`, target `hud_client`
- Runtime window: `examples/hud/assets/windows/hud_window.json`
- Tests: `hud_client_tests` (white-box) and `hud_runtime_client_tests`
  (linked against the hud_runtime shared library)

`hud_runtime` contains everything an external aeronautical simulation needs to
publish HUD frames from a `hud::HudInputSample`: the semantic input contract,
the projection and symbology geometry (including the EEGS funnel), the
generated `HudUi` wrapper, the transport lifecycle and the runtime assets/font
staging. It never links ImGui, d3d11 or dxgi and never includes
`HudApplication`, `HudSimulation`, `SimulationControls`, `PilotControls`,
`EnvironmentControls`, `HudPhysics` or `Win32Dx11ImGuiHost`.

`hud_client` is the interactive Win32/DX11 ImGui client. It owns the operator
panel, the mini-simulation with its pure physics helpers (`HudPhysics`) and
the sample armament constants, and consumes `hud_runtime` exactly like an
external integration would. `hud_runtime.dll` is copied next to `hud_client`
by the build.

The client data flow keeps the `hud_main` / `hud_runtime` boundary explicit:

```
ImGui controls
  ├── PilotControls
  └── EnvironmentControls
          |
          v
  SimulationControls
          |
          v
  hud_main::HudSimulation
          |
          v
  hud::HudInputSample
          |
          v
  hud_runtime
          |
          v
  generated HUD window
```

The HUD window uses the bundled `ShareTechMono-Regular.ttf` font under the SIL
Open Font License 1.1. The `.ttf` and `OFL-ShareTechMono.txt` files live under
`examples/hud/assets/fonts` and are staged with the HUD assets. Text primitives
also define explicit letter spacing so the HUD does not fall back to raylib's
default font metrics.

Build the runtime window, the HUD library and the client:

```powershell
cmake --build --preset debug-win32 --target mfd_window hud_runtime hud_client hud_client_tests hud_runtime_client_tests
```

Launch the runtime window:

```powershell
.\Scripts\Start-HUD.bat
```

Then start the client:

```powershell
.\build\vs2022-win32\examples\hud\main\Debug\hud_client.exe
```

The runtime client resolves `assets/windows/hud_window.json` from the staged
layout next to its working directory first, then falls back to the
in-repository asset path, and drives the generated transport map next to the
authored window JSON. `mfd_stage_hud_runtime_assets()` copies the HUD asset
tree (window JSON, generated map, pages, reticles and the HUD font) next to
any consumer executable, and the `_Exec` staging also receives
`hud_runtime.dll` through the runtime-DLL copy step.

## Replacing the ImGui sample panel

The Dear ImGui window is not part of the HUD integration contract. It is only a
local control panel used by `hud_client` to drive the bundled
`HudSimulation`.

The current panel fills a compact native HUD-client window instead of opening
a floating child panel. Controls are grouped into collapsible categories for
aircraft, maneuver, stick POV, environment, HUD mode, master/target, missile,
landing/ILS and telemetry. The circular stick POV writes normalized pitch/roll
intent into `SimulationControls::pilot`; dragging the knob down is treated
like pulling the stick and commands nose-up pitch.

The `Environment` section writes wind, turbulence, terrain and atmosphere
settings into `SimulationControls::environment`. It is a control tool for the
bundled mini-simulation only: the wind is not a HUD symbology and no
environment value is sent to the HUD runtime. The wind direction uses the
meteorological FROM convention (0 degrees = wind coming from the North,
90 degrees = from the East, 180 degrees = from the South, 270 degrees = from
the West) and can be set either with the slider or by dragging the arrow
inside the compass disk. The mini-simulation resolves those settings through
the pure helpers of `hud_main/HudPhysics.h` (wind vector NED, speed of sound,
air density, Mach, radio altitude, specific energy rate, terrain elevation)
and publishes only the resulting physical facts: the aircraft NED velocity in
`hud::HudInputSample` is the ground velocity (air velocity + wind NED), the
Mach uses the temperature-dependent speed of sound and the radar altitude
follows the user-selected terrain elevation. Turbulence is a bounded,
deterministic sinusoidal perturbation; zero intensity has strictly no effect.

An external simulator, aircraft model, or plugin removes the ImGui shell by
linking only `hud_runtime`:

- Keep `examples/hud/assets` (staged by `mfd_stage_hud_runtime_assets()`);
  these are the authored HUD window, page, reticles and font.
- Keep `hud::HudRuntimeClient` from `hud/HudRuntimeClient.h`; it owns the
  generated UI, the startup client, the realtime publisher and the feedback
  liveness internally.
- Keep `hud::HudInputSample` from `hud/HudTypes.h`.
- Do not link ImGui, d3d11 or dxgi, and do not embed `HudApplication`,
  `HudSimulation`, `SimulationControls`, `PilotControls`,
  `EnvironmentControls` or `HudPhysics`; they belong to the main client only.

The replacement application has one responsibility: build a complete
`hud::HudInputSample` from the external model once per HUD frame, then let
`hud::HudRuntimeClient::Publish()` convert it to generated UI commands.

```cpp
#include "hud/HudRuntimeClient.h"

hud::HudInputSample BuildHudInputFromAircraft(const AircraftModel& aircraft)
{
    hud::HudInputSample input;

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

    input.weapon.masterMode = hud::HudMasterMode::Nav;
    input.weapon.weaponMode = hud::HudWeaponMode::None;
    input.weapon.gunMode = hud::HudGunMode::None;
    if (aircraft.IsAirToAirGunMode())
    {
        input.weapon.masterMode = hud::HudMasterMode::AirToAir;
        input.weapon.weaponMode = hud::HudWeaponMode::AirToAirGun;
        input.weapon.gunMode = hud::HudGunMode::Eegs;
    }
    else if (aircraft.IsAirToAirMissileMode())
    {
        input.weapon.masterMode = hud::HudMasterMode::AirToAir;
        input.weapon.weaponMode = hud::HudWeaponMode::AirToAirMissile;
    }
    else if (aircraft.IsCcipMode())
    {
        input.weapon.masterMode = hud::HudMasterMode::AirToGround;
        input.weapon.weaponMode = hud::HudWeaponMode::AirToGroundCcip;
    }
    else if (aircraft.IsStrafeMode())
    {
        input.weapon.masterMode = hud::HudMasterMode::AirToGround;
        input.weapon.weaponMode = hud::HudWeaponMode::AirToGroundStrafe;
        input.weapon.gunMode = hud::HudGunMode::Strafe;
    }
    else if (aircraft.LandingModeActive())
    {
        input.weapon.masterMode = hud::HudMasterMode::Landing;
    }
    input.weapon.masterArm = aircraft.MasterArm();
    input.weapon.simulateMode = aircraft.SimulateMode();
    // Weapon facts are already resolved by the aircraft weapon system. The HUD
    // never decides which concrete missile type is loaded.
    input.weapon.selectedWeaponLabel = aircraft.SelectedWeaponHudLabel();
    input.weapon.selectedWeaponMnemonic = aircraft.SelectedWeaponHudMnemonic();
    input.weapon.selectedWeaponQuantity = aircraft.SelectedWeaponRemaining();
    input.weapon.missileDiamondScale = aircraft.SelectedWeaponHudDiamondScale();
    input.weapon.launchZone = aircraft.ComputedLaunchZoneForHud();
    input.weapon.selectedMissileTimeOfFlightSeconds = aircraft.SelectedWeaponTimeOfFlightSeconds();
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

The realtime publishing loop stays small; the external simulation keeps its
own main loop and decides when a frame is published:

```cpp
hud::HudRuntimeConfig config;
config.assetRoot = ".../assets";

hud::HudRuntimeClient hud;
std::string error;

if (!hud.Initialize(config, error))
{
    // handle error
}

while (running)
{
    hud::HudInputSample input {};
    // fill input from external aircraft simulation
    hud.Publish(input, error);
}

hud.Shutdown();
```

`HudRuntimeConfig::assetRoot` points to the directory containing the staged
HUD assets (`windows/`, `pages/`, `reticles/`, `fonts/`). With the default
`Initialize(error)` overload the client looks for `assets/windows/
hud_window.json` next to the working directory (the staged layout), then falls
back to the in-repository asset path.

The important boundary is `HudInputSample`. It uses SI units and radians:

- altitude is meters, speed is meters per second, and attitude is radians;
- velocity uses an NED frame, so `downSpeedMps` is positive downward and
  negative while climbing;
- target azimuth/elevation are line-of-sight errors relative to the aircraft
  nose;
- `WeaponInputSample` must already contain resolved avionics state, including
  the generic selected-weapon label/mnemonic/quantity, the computed launch zone
  and the computed time of flight. Do not wire raw cockpit, panel, HOTAS or
  ImGui commands directly to generated HUD handles.
- `AirGroundInputSample`, `ApproachInputSample` and `IlsInputSample` are also
  semantic aircraft/avionics state. They are not generated UI coordinates.

For a concrete local reference, read
`hud_main::HudApplication::UpdateHudInputBufferFromUi()`,
`hud_main::HudSimulation::Reset()` and
`hud_main::HudSimulation::RefreshWeaponPresentation()`. `HudInputSample{}` is a
deliberately neutral runtime contract (grounded aircraft, no target, unarmed),
so the client writes its own airborne scene explicitly in `Reset()` instead of
inheriting scene values from runtime defaults. Those functions are the
client boundary where UI controls and the client-local armament model become a
semantic `HudInputSample`; an external integration replaces those data
producers, not the HUD projection or generated UI command path.

`HudSimulation`, `HudPhysics` and the ImGui panel are client-local producers
only. `hud_main::HudSimulation` is replaceable by a real INU / Air Data /
environment producer: a real integration removes them and links `hud_runtime`;
the external model only fills `HudInputSample`, `BuildHudFrame()` still
computes the EEGS funnel and the runtime still sends the Bezier rails through
the generated UI API. `hud_runtime` stays passive in both setups: it consumes
resolved physical data and never simulates wind, atmosphere or terrain
itself.

Run the focused validation:

```powershell
ctest --preset test-debug-win32 -R "hud|HudSimulation" --output-on-failure
```

## HUD angular projection

Every conformal HUD symbol shares one explicit field of view, resolved in a
single place in `runtime/src/HudProjection.cpp`. This removes the previous mix of
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
than the HUD field of view; the HUD implementation instead uses a 30 degree total vertical
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
`examples/hud/assets/reticles/hud_pitch_ladder.json` encodes exactly this
scale, and `HudProjectionTests.PitchLadderJsonMatchesCppAngularScale` fails
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

Conformal symbols are not hidden just because they leave the field of view.
`ProjectBoresightAngularOffsetToHud()` returns a `ProjectedHudPoint` with an
explicit `insideFov` flag; the owning symbology clamps the cue to the field-of-
view edge, keeps it visible, and reports the clamp through a `limited` flag.
This follows BMS/F-16 behavior rather than blanking the cue where the angle is
lost.

The reference behaviors implemented here are:

- **AIM-120 / missile diamond**: when the target falls outside the HUD FOV the
  diamond stays at the FOV edge (`missileDiamondLimited`) and a geometric
  limit-X is overlaid on it (`missileLimitXVisible`, `missileLimitX` reticle).
- **AIM-9 / SRM diamond**: the HUD sample does not separate the seeker line of sight
  from the target line of sight, so the same clamped target projection is used
  and the same limit-X marks a restricted diamond. A real, independent AIM-9
  seeker LOS is out of scope until a dedicated semantic input exists.
- **EEGS TD circle**: a locked A-A gun track outside the FOV is clamped and
  flagged (`tdCircleLimited`) with its own limit-X (`tdCircleLimitX`).
- **CCIP pipper**: when the pipper reaches the total-FOV edge it stays visible,
  clamped, and a limit-X is overlaid (`ccipPipperLimited`, `ccipLimitX`). The
  CCIP solution cue and pull-up anticipation cue are likewise clamped and
  flagged (`solutionCueLimited`, `pullupAnticipationCueLimited`), never dropped
  silently.
- **STRF pipper**: clamped to the edge and flagged (`strafePipperLimited`).
  There is no dedicated strafe limit-X reticle in the current model, so only the
  flag is exposed.

These distinct semantics must not be confused:

- the **geometric limit-X** means "this conformal cue is clamped at the FOV
  edge"; it uses the `hud_limit_x` reticle;
- the **Break-X** (`breakXVisible`) is the missile *too-close* caution tied to
  `launchZone.tooClose`; it is a separate field and reticle and can be active at
  the same time as a limit-X. The Break-X is never reused to signal out-of-FOV;
- the **A/G target locator line (AGTLL)** is a CCRP/DTOS presentation that
  replaces the TD box when the target is outside the HUD FOV. The HUD does
  not model CCRP/DTOS target designation, so the AGTLL is out of scope until
  that mode and its primitives exist;
- the **dynamic launch zone** is a range scale, not an angular projection, so it
  never depends on `insideFov` and is not affected by FOV limiting.

Missile threat detection is out of scope. `WeaponInputSample` exposes
`missileInFlight`, `activeMissileTimeRemainingSeconds` and `activeMissilePhase`,
which describe the ownship's own launched missile timeline, not a detected
hostile missile. There is no `missileThreatDetected` / inbound-missile
line-of-sight input, so no inbound-missile symbology is projected. Adding one
would require a new documented and tested semantic input.

## HUD modes and reticles

The HUD remains a single-page asset. `examples/hud/assets/pages/hud.json`
contains one `HUD` page, and mode-specific symbology is represented by named
reticle instances on that page. The controller changes visibility, position,
rotation and scale from the projected `HudFrame`; it does not switch pages.

The current contextual reticles are:

- Air-to-air missile: target designator, missile diamond, missile limit-X,
  missile circle, dynamic launch zone, range cue, break X and attack steering
  cue.
- Air-to-air gun / EEGS: funnel, MRGS, FEDS, TD circle, TD circle limit-X, 1G
  pipper, Max-G pipper, solution circle and BATR.
- Air-to-ground / STRF: 50 mR / 40 mR strafe reticle, 2 mR in-range cue,
  moving-target indices, 1 mR pipper and bullet-track line.
- Air-to-ground / CCIP: bomb-fall line, CCIP pipper, CCIP limit-X, solution cue
  and pull-up anticipation cue.
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
The HUD EEGS funnel is not a certified ballistic solver, but it is not a
static cone: its exposed Bezier control points are derived from the semantic
aircraft/target sample, including flight path, load factor, target line of
sight, range/wingspan and target acceleration. The authored funnel now uses a
central spine and range-sampled wall half-widths, keeping the visible cue as a
long, narrow gunnery corridor instead of a decorative V shape. The runtime
never reads a simulated weather: wind influences the funnel only indirectly,
through the physical facts already resolved by the producer (ground velocity,
flight path, load factor, energy and target geometry in
`hud::HudInputSample`). In the bundled client those facts come from
`hud_main::HudSimulation` and its `EnvironmentControls`; the funnel control
points themselves stay computed inside `hud_runtime`.

Visual reference captures used for the current HUD mode work are kept in
`examples/hud/visual_ref`. They are documentation/reference material only and
must not be added to the runtime asset graph.

## Semantic input contract

`hud::HudInputSample` is the integration boundary. Its defaults are
deliberately neutral: a default-constructed sample describes a grounded,
stationary aircraft with no target and no armed weapon, so an integrator sees
no scene state leaking from the runtime. Scene values are the caller's
responsibility; in the bundled client they are written by
`hud_main::HudSimulation`. The sample is split into semantic sub-samples:

- `aircraft`: attitude, velocity, altitude, energy and throttle in SI units.
- `target`: air-to-air target range, closure, aspect and line-of-sight in SI
  units and radians.
- `weapon`: resolved master mode, weapon mode, gun mode, arm/sim state, the
  generic selected-weapon label/mnemonic/quantity, the computed launch zone and
  time of flight, gun rounds, trigger, target lock, target wingspan and the
  caller-resolved `strafeInRangeFeet` threshold. The runtime never resolves a
  concrete ammunition type (M56, PGU-28, …) or an in-range default itself.
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
realistic HUD presentation model for this HUD client, not a certified F-16 ballistic
computer. The simplifications are localized in `BuildHudFrame()` helpers and
covered by focused tests for visibility, clamp and finite output behavior.

### EEGS funnel projection

External simulations do not send Bezier points directly. They provide semantic
aircraft and target values through `HudInputSample`, and
`HudProjection.cpp` computes the five-point Bezier rails used by the
generated `eegsFunnel` reticle.

The funnel is built from a central spine. Each Bezier sample represents a
range station; the wall offset at that station is the normalized angular
half-width of the target wingspan at that range. The projection then adjusts
scale, drift, curvature and skew from flight-path marker position, normal load
factor, airspeed, target line-of-sight position and
`targetAccelerationMps2`. The far end stays narrow and the near end opens
without creating a broad vase-shaped cue.
