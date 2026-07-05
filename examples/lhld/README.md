# LHLD Integration Notes

This example hosts a tactical left-hand MFD display in a Win32/DX11 ImGui
application. The MFD page itself is rendered offscreen through the runtime API,
then composited inside an ImGui-drawn bezel. The bundled radar/navigation/stores
simulation is replaceable by design.

## Main Entry Points

| Area | File | Keep or Replace |
|---|---|---|
| Host startup | `client/src/main.cpp` | Keep unless the application shell changes. |
| ImGui bezel, control panel and publishing loop | `client/src/MfdApplication.*` | Replace or thin down when embedding in a real cockpit. |
| Replaceable semantic input source | `client/src/MfdInputSource.h` | Keep. Implement this for a real simulator feed. |
| Fake radar/navigation/stores source | `client/src/MfdRadarSimulation.*` | Replace with a real aircraft, avionics or network source. |
| Semantic MFD contract | `client/src/MfdTypes.h` | Keep. This is the stable frame data boundary. |
| Stateless FCR/HSD projection | `client/src/MfdProjection.*` | Keep unless page geometry or avionics rules change. |
| Generated-UI adapter | `client/src/MfdController.*` | Keep. It writes `LhldUi` from `MfdInputSample`. |
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

The projection layer expects meaningful aircraft/MFD state, not raw panel
button events.

## Replacing the ImGui Panel

The ImGui panel in `MfdApplication` is a sample operator surface. A real
cockpit, plugin, network bridge or scripted test can replace it. Keep these
contracts intact:

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
