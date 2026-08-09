# HUD Integration Notes

This example is split into two strictly separated targets:

- `hud_runtime` — a reusable DLL/shared library under `runtime/`. It contains
  everything needed to publish the HUD from a `hud::HudInputSample`: the
  semantic input contract, the projection and symbology geometry (including the
  EEGS funnel Bezier rails), the generated `HudUi` wrapper, the transport
  lifecycle and the asset/font staging. It has no ImGui, Win32, DX11 or
  main-client dependency.
- `hud_client` — the main interactive executable under `main/`. It owns the
  Win32/DX11 ImGui shell, the operator panel (including the `Environment`
  section for wind, turbulence, terrain and atmosphere), the client-local
  simulation with its six-degree-of-freedom aircraft dynamics, pure environment
  helpers (`HudPhysics`) and armament model (the AIM-120C/AIM-9M profiles and
  the STRF in-range threshold), and
  consumes `hud_runtime` like any external simulation would.

The full data flow of the interactive client is:

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

## Main Entry Points

| Area | File | Keep or Replace |
|---|---|---|
| Host startup | `main/src/main.cpp` | Keep unless the application shell changes. |
| ImGui/DX11 shell and operator controls | `main/.../HudApplication.*` | Replace when embedding the HUD in a real cockpit or another UI. |
| Client-local aircraft/weapons simulation and armament model | `main/.../HudSimulation.*` | Replace with the real INU / Air Data / environment producer. |
| Client-local aircraft force/moment integration | `main/.../HudAircraftDynamics.*` | Replace with the real flight model; it is never linked by `hud_runtime`. |
| Pure physics helpers (wind, atmosphere, terrain, energy) | `main/.../HudPhysics.*` | Replace together with the simulation; they feed it, never the runtime. |
| Reusable publishing facade | `runtime/include/hud/HudRuntimeClient.h` | Keep. This is the integration entry point. |
| Semantic HUD input contract | `runtime/include/hud/HudTypes.h` | Keep. This is the handoff boundary. |
| Stateless projection math | `runtime/.../HudProjection.*` | Keep unless the HUD symbology model changes. |
| Generated-UI adapter | `runtime/.../HudController.*` | Keep. It writes `HudUi` from `HudInputSample`. |
| Generated wrappers | `runtime/generated/HudUi.*` | Regenerate from assets, do not hand-edit. |
| Authored page assets | `assets/windows`, `assets/pages`, `assets/reticles`, `assets/fonts` | Keep as the authored HUD layout. |

## Replacement Boundary

An external aeronautical simulation that already runs its own main loop links
only `hud_runtime` and does:

```cpp
hud::HudRuntimeConfig config;
config.assetRoot = ".../assets";

hud::HudRuntimeClient hud;
std::string error;

if (!hud.Initialize(config, error))
{
    return false;
}

while (running)
{
    hud::HudInputSample sample {};
    // sample filled by the external simulation: INU, air data, avionics,
    // weapon system, etc.

    if (!hud.Publish(sample, error))
    {
        // handle the error on the integrator side
    }
}

hud.Shutdown();
```

`HudInputSample{}` is deliberately neutral: it describes a grounded,
stationary aircraft with no target and no armed weapon. The integrator fills it
once per rendered frame with semantic aircraft, target, weapon, navigation and
landing state; the bundled client writes its own airborne scene explicitly in
`hud_main::HudSimulation::Reset()` rather than relying on runtime defaults. Do
not send raw ImGui button states, keyboard states, HOTAS edge events or
simulator packet formats into the runtime.

Weapon facts are caller-resolved: the runtime receives a generic label,
mnemonic, quantity, an already-computed launch zone and time of flight, a
resolved missile phase, and the resolved `strafeInRangeFeet` threshold. The
runtime never decides which concrete missile type the aircraft carries and
never invents an ammunition in-range default (M56, PGU-28, …); a non-positive
`strafeInRangeFeet` simply hides the strafe in-range cue. In exchange, the
runtime keeps computing the HUD symbology geometry itself — including the EEGS
funnel Bezier control points — from those generic facts.

## Environment Controls

The operator panel has an `Environment` section (wind speed, wind direction
with an interactive compass disk, turbulence, terrain elevation, outside air
temperature and pressure). It edits `hud_main::SimulationControls::environment`
only: it is a control tool for the bundled mini-simulation, not a HUD
symbology input, and the wind is never drawn as HUD symbology.

The wind direction uses the meteorological FROM convention: 0 degrees is wind
coming from the North, 90 degrees from the East, 180 degrees from the South
and 270 degrees from the West.

The simulation resolves these settings into physical facts through the pure
helpers in `main/.../HudPhysics.*` (wind vector NED, speed of sound, air
density, Mach, radio altitude, specific energy rate, terrain elevation). The
aircraft NED velocity published in `hud::HudInputSample` is the ground
velocity (air velocity + wind), so wind, atmosphere and terrain influence the
HUD — including the EEGS funnel — only through the resolved physical data.
`hud_runtime` stays a passive consumer and never reads a simulated weather.

A settled, spatially uniform wind does not create a permanent lateral EEGS
offset by itself: the aircraft and every virtual round share the same air-mass
translation, and each round inherits the aircraft ground velocity at launch.
The wind remains visible in ground-referenced aircraft data and during history
transients. A persistent relative gun-funnel drift would require a wind gradient
or target-motion model; adding an arbitrary HUD offset would double-count the
uniform wind.

## Replaceable aircraft dynamics

`hud_main::HudAircraftDynamics` is a client-only demonstration model. Its
continuous state is position and ground velocity in local NED axes, a normalized
body-to-NED quaternion, body angular velocity and throttle response. Mass,
diagonal body inertia, wing area, lift/drag/side-force coefficients, thrust,
control moments, angular damping and numerical bounds live in
`AircraftDynamicsConfig`; they are parameters, not HUD inputs.

At each 20 ms tick the model computes:

```
airVelocityNed  = groundVelocityNed - windVelocityNed
airVelocityBody = RotateNedToBody(attitudeBodyToNed, airVelocityNed)
qbar            = 0.5 * density * airspeed^2
I * omegaDot    = moment - omega x (I * omega)
velocityDotNed  = RotateBodyToNed(forceBody) / mass + gravityNed
```

Angle of attack, sideslip, lift, drag and side force come from the air-relative
body velocity. Lift is progressively attenuated beyond the configured stall
angle; lift, drag, side-force coefficients, angular rates, airspeed used by the
coefficient model and integration step are explicitly bounded. Below the
minimum aerodynamic speed the aerodynamic force is zero while thrust and
gravity remain active. The model integrates velocity and position
semi-implicitly and integrates the quaternion from body angular velocity before
normalizing it. No equation uses a folded Euler display angle or a target
flight-path slope.

This is a stable pedagogical model, not certified aircraft data. It omits fuel
mass change, compressibility, detailed control-surface/engine dynamics, spatial
weather gradients, ground contact and structural limits. The configuration is
therefore suitable for the HUD demonstration only. A production simulator owns
those behaviors and publishes the same semantic `HudInputSample`.

Euler yaw/pitch/roll are derived only for publication and telemetry. The
publisher chooses the equivalent 3-2-1 branch nearest the preceding sample so
the ladder remains readable through a loop, but the physical state and every
force/moment equation continue to use the quaternion.

The interactive client maps the arrow keys to normalized pitch/roll intent and
Q/E to normalized yaw intent. LOOP and BARREL still emit only constant stick
commands; neither the dynamics nor the runtime receives a maneuver name.

## Flight Path Marker

The runtime derives the FPM exclusively from the published inertial ground
velocity. Its private spatial helper transforms NED velocity into body axes
(X-forward, Y-right, Z-down), computes
`atan2(right, forward)` and `atan2(-down, hypot(forward, right))`, then uses the
same conformal angular projection as the other boresight-relative cues. The
folded pitch-ladder angles never participate in this calculation. The ballistic
trajectory projection uses the same private NED-to-body helper, preventing the
two transformations from drifting apart.

A non-finite, near-zero or rear-facing velocity has no meaningful forward-HUD
intersection. In that case the runtime-private direction result is invalid and
the controller hides both the FPM and its limit X; it does not invent a frontal
position. A valid forward direction outside the FPM aperture remains edge
limited and displays the existing limit X.

## Replacing the Mini Simulation

`HudSimulation` (in `main/`) is only one possible producer. It is replaceable
by a real INU / Air Data / environment producer, which should:

- own all simulator-specific state and unit conversion;
- publish one complete `HudInputSample` per frame;
- resolve raw cockpit commands and concrete armament into the generic
  `WeaponInputSample` fields before publishing;
- resolve wind, atmosphere and terrain into the aircraft ground velocity,
  Mach, radio altitude and energy fields instead of expecting the runtime to
  simulate an environment;
- keep SI-unit fields in the semantic buffer where the current contract expects
  SI values;
- let `hud_runtime` remain a stateless consumer.

The replacement does not include or instantiate `HudAircraftDynamics`,
`HudSimulation`, `PilotControls` or `EnvironmentControls`. It links
`hud_runtime`, supplies physical 3-2-1 attitude plus ground velocity NED, and
may otherwise use any internal state representation or flight model.

## Local Validation

Preferred local build:

```powershell
cmake --build --preset debug-win32 --target hud_runtime hud_client
```

If HUD tests are enabled in the current checkout, rebuild and run the matching
HUD test targets (`hud_client_tests`, `hud_runtime_client_tests`) before
trusting a visual or projection change.

# Fixed-step EEGS ballistic funnel

The interactive sample owns a deliberately small ballistic model in
`hud_main::GunProjectileSimulation`; it does not belong to `hud_runtime` because
the reusable runtime must remain a stateless consumer of complete semantic
snapshots. The entire stateful HUD mini-simulation advances through the single
authoritative `hud_main::kHudSimulationStepSeconds` constant: one `Step()` is
20 ms (50 Hz). `HudApplication` converts irregular render deltas into fixed
ticks with a bounded eight-tick catch-up loop. If a frame is later than that
budget, overdue whole ticks are dropped and only the fractional remainder is
kept, preventing an unbounded catch-up loop.

The EEGS ballistic history is a fixed `std::array` of 76 virtual projectiles.
One projectile is launched on every 20 ms tick, giving the simulation a dense
1.5-second trail without allocation. `Reset()` reconstructs that history
deterministically by assuming the initial aircraft state and environment were
constant during the preceding 1.5 seconds. These samples are display geometry
only: they do not consume ammunition, do not depend on the trigger, and do not
alter MASTER ARM, SIM, FEDS or BATR rules.

Internally, aircraft and projectile positions use double-precision SI values in
a fixed local North/East/Down frame established at reset. Each launch captures
the aircraft absolute NED position, ground velocity, yaw/pitch/roll, body-frame
muzzle offset and muzzle velocity. After launch, the projectile is independent
of later aircraft attitude, speed, controls and load factor. Gravity is
`{0, 0, +9.80665}` m/s2. Wind uses the documented meteorological FROM
convention and acts only through quadratic drag against air-relative velocity:

`a_drag = -0.5 * density * (CdA / mass) * |v_projectile - v_wind| * (v_projectile - v_wind)`

The semi-implicit Euler update uses the same 20 ms tick as the aircraft and
missiles, without a ballistic sub-step. A panel wind change is intentionally
treated as an instantaneous change to one uniform field for every active
projectile. Invalid/non-finite atmosphere or motion values are sanitized or
invalidate the affected sample deterministically.

The dense history never crosses the reusable-runtime boundary. The simulation
interpolates between adjacent physical projectile samples at sixteen stable,
regularly spaced slant-range stations from 600 to 3000 feet (160-foot steps).
Sampling fixed ranges removes the former 5 Hz age-phase reset while preserving
the natural inertia of continuously fired rounds: during a turn, recent launch
directions reach the near station before they reach the far station.

At the semantic boundary, absolute positions, velocities and the 76-slot
history stay private. `HudInputSample::gunTrajectory` publishes only sixteen
finite points ordered nearest-to-farthest, expressed relative to the current
aircraft but still oriented North/East/Down. The stateless display chain is:

`relative NED -> current body axes -> azimuth/elevation -> UserSpaceProjector -> HUD`

Each station keeps its own physical angular half-width:
`atan2(targetWingspanMeters / 2, range)`. With the default 35-foot wingspan,
the 600-foot end is therefore approximately five times wider than the
3000-foot end. The display preserves those physical endpoint widths and
interpolates them monotonically along the rendered rails. A defensive display
bound handles nonsensical external wingspan values without affecting the
physical 600/3000-foot geometry of supported targets.

The projected rail uses the authored Gun Bore Cross at HUD position
`{0.0, 0.82}` as its local origin. This places the wide 600-foot end close to
the gun reference while retaining the upper margin visible in Falcon BMS and
DCS F-16 Level-II examples. The FPM is calculated and published independently;
changing aircraft flight-path velocity does not translate the funnel. The
runtime owns no ballistic history, clock, temporal filter or previous-frame
state.

The sixteen projected stations are summarized by one quadratic centerline.
Its near offset, useful length, control-point progress and curvature are bounded
to keep the Level-II sight readable during abrupt pitch and roll changes. Its
direction continuously blends the current roll-resolved gravity direction with
the broad ballistic lead direction, with that ballistic contribution faded out
continuously when its axis becomes too short to be reliable. One transverse
axis is then used for the entire frame, and the two walls are built as
`centerline +/- halfWidth`. This construction guarantees positive separation,
monotonic narrowing and a common curvature: a noisy ballistic history cannot
turn the sight into an S or make its walls exchange sides. The complete control
hull is translated only when needed to remain inside the HUD aperture after the
Gun Bore Cross anchor is applied, including during inverted flight. Exact degree
elevation converts the quadratic to the authored five-control-point quartic
Bezier without changing its shape; the primitive tessellates each rail into 36
smooth segments without allocation or projection-layer history.

Reference figures: Falcon BMS Dash-34 4.36.3, section 2.4.5.1, figure 41; DCS
F-16C Early Access Guide, EEGS Level-II pages 530-532.
