# HUD Integration Notes

This example is split into two strictly separated targets:

- `hud_runtime` — a reusable DLL/shared library under `runtime/`. It contains
  everything needed to publish the HUD from a `hud::HudInputSample`: the
  semantic input contract, the projection and symbology geometry (including the
  EEGS funnel Bezier rails), the generated `HudUi` wrapper, the transport
  lifecycle and the asset/font staging. It has no ImGui, Win32, DX11 or
  main-client dependency.
- `hud_client` — the main interactive executable under `main/`. It owns the
  Win32/DX11 ImGui shell, the operator panel, the mini-simulation and the
  sample armament, and consumes `hud_runtime` like any external simulation
  would.

## Main Entry Points

| Area | File | Keep or Replace |
|---|---|---|
| Host startup | `main/src/main.cpp` | Keep unless the application shell changes. |
| ImGui/DX11 shell and sample controls | `main/.../HudApplication.*` | Replace when embedding the HUD in a real cockpit or another UI. |
| Fake aircraft, weapons simulation and sample armament | `main/.../HudSimulation.*` | Replace with the real aircraft or simulator adapter. |
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

External aircraft code should fill `HudInputSample` once per rendered frame
with semantic aircraft, target, weapon, navigation and landing state. Do not
send raw ImGui button states, keyboard states, HOTAS edge events or simulator
packet formats into the runtime.

Weapon facts are caller-resolved: the runtime receives a generic label,
mnemonic, quantity, an already-computed launch zone and time of flight, and a
resolved missile phase. The runtime never decides which concrete missile type
the aircraft carries. In exchange, the runtime keeps computing the HUD
symbology geometry itself — including the EEGS funnel Bezier control points —
from those generic facts.

## Replacing the Mini Simulation

`HudSimulation` (in `main/`) is only one possible producer. A real source
should:

- own all simulator-specific state and unit conversion;
- publish one complete `HudInputSample` per frame;
- resolve raw cockpit commands and concrete armament into the generic
  `WeaponInputSample` fields before publishing;
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
