# Demo HUD Integration Notes

This example is a standalone Win32/DX11 ImGui host for the generated HUD page
under `examples/hud/assets`. It is intentionally split so the bundled demo
simulation and operator panel can be removed without rewriting the HUD
projection.

## Main Entry Points

| Area | File | Keep or Replace |
|---|---|---|
| Host startup | `client/src/main.cpp` | Keep unless the application shell changes. |
| ImGui/DX11 shell and demo controls | `client/src/DemoHudApplication.*` | Replace when embedding the HUD in a real cockpit or another UI. |
| Fake aircraft and weapons simulation | `client/src/DemoHudSimulation.*` | Replace with the real aircraft or simulator adapter. |
| Semantic HUD input contract | `client/src/DemoHudTypes.h` | Keep. This is the handoff boundary. |
| Stateless projection math | `client/src/DemoHudProjection.*` | Keep unless the HUD symbology model changes. |
| Generated-UI adapter | `client/src/DemoHudController.*` | Keep. It writes `DemoHudUi` from `HudInputSample`. |
| Generated wrappers | `client/generated/DemoHudUi.*` | Regenerate from assets, do not hand-edit. |
| Authored page assets | `assets/windows`, `assets/pages`, `assets/reticles` | Keep as the authored HUD layout. |

## Replacement Boundary

The real integration point is:

```cpp
demo_hud::HudInputSample sample;
demo_hud::DemoHudController controller;
controller.Populate(ui, sample);
```

External aircraft code should fill `HudInputSample` once per rendered frame.
Use semantic aircraft, target, weapon, navigation and landing state. Do not send
raw ImGui button states, keyboard states, HOTAS edge events or simulator packet
formats directly into `DemoHudController`.

## Replacing the Mini Simulation

Remove or bypass `DemoHudSimulation` when a real source is available. The new
source should:

- own all simulator-specific state and unit conversion;
- publish one complete `HudInputSample` per frame;
- resolve raw cockpit commands before filling `WeaponInputSample`;
- keep SI-unit fields in the semantic buffer where the current contract expects
  SI values;
- let `DemoHudProjection` and `DemoHudController` remain stateless consumers.

## Replacing the ImGui Panel

`DemoHudApplication` is only the sample host. A production host can replace the
ImGui panel and still keep:

- `HudInputSample` as the frame contract;
- `DemoHudController::Populate()` as the only generated-UI write path;
- the generated `DemoHudUi` command lifecycle: `Run()`, `Populate()`,
  `SubmitLatest()`;
- `assets/windows/demo_hud_window.json` and generated map coherence.

The important rule is separation: UI commands may change the aircraft adapter's
state, but generated HUD output should come only from the resolved semantic
sample.

## Local Validation

Preferred local build:

```powershell
cmake --build --preset debug-win32 --target demo_hud_client
```

If HUD tests are enabled in the current checkout, rebuild and run the matching
HUD test target before trusting a visual or projection change.
