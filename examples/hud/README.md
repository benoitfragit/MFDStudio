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
  simulation with its pure physics helpers (`HudPhysics`) and its armament
  model (the AIM-120C/AIM-9M profiles and the STRF in-range threshold), and
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

## Local Validation

Preferred local build:

```powershell
cmake --build --preset debug-win32 --target hud_runtime hud_client
```

If HUD tests are enabled in the current checkout, rebuild and run the matching
HUD test targets (`hud_client_tests`, `hud_runtime_client_tests`) before
trusting a visual or projection change.
