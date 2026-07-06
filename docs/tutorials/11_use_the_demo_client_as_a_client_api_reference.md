# Use The Demo Clients As Client API References

The shipped clients are intentionally small references for public UDP command
usage.

| Client | Purpose |
| --- | --- |
| `demo_client` | cycles pages and publishes static reticle, strobe, blink and clipping updates |
| `radar_load_client` | ramps dynamic radar contacts from 10 to 300 tracks |
| `hud_client` | drives the dedicated HUD asset with deterministic simulated input |

## Generated Map Setup

Each client loads the same window JSON as the runtime and expects the generated
transport map beside it:

```cmake
mfd_generate_client_api(
    TARGET_NAME demo_client_ui
    WINDOW_JSON "${MFD_ROOT_DIR}/examples/demo/assets/windows/demo_window.json"
    OUTPUT_MAP "${MFD_ROOT_DIR}/examples/demo/assets/windows/demo_window.generated.map"
    NAMESPACE "demo_ui"
)
```

The runtime and client stay separate processes. The client uses the window JSON
only for local discovery and command normalization.

## Static Reticle Pattern

`demo_client` uses `UpdateReticleCommand` for authored reticles:

```cpp
mfd::UpdateReticleCommand command;
command.target.page = "BlinkStrobe";
command.target.reticle = "track_fast";
command.patch.position = mfd::Vec2 {0.2f, -0.1f};
command.patch.texts.emplace("track_label", "AUTO");
```

Batch commands with `mfd::CommandClient::SendBatch()` so page activation and
reticle changes are applied together.

## Strobe Pattern

`demo_client` moves the active strobe with `UpdateStrobeCommand`:

```cpp
mfd::UpdateStrobeCommand command;
command.page = "BlinkStrobe";
command.strobe = "Default";
command.active = true;
command.position = mfd::Vec2 {0.3f, 0.2f};
```

## Dynamic Reticle Pattern

`radar_load_client` keeps one publication state per track and reuses one
`std::vector<mfd::DynamicReticleState>` for the current tick. New tracks send
their static fields once; already published tracks send only the motion fields
that change every tick:

```cpp
track.patch.position = frame.position;
track.patch.rotationDegrees = frame.rotationDegrees;

if (!trackState.published)
{
    track.patch.visible = true;
    track.patch.color = frame.color;
    track.patch.texts.emplace("track_label", label);
}
```

Keep stable dynamic ids such as `load_track_001` so the runtime updates existing
reticles instead of recreating unrelated entries each frame. Keep static fields
out of the steady-state tick unless their values actually changed.

## Validation

```powershell
cmake --build --preset debug-win32 --target demo_client radar_load_client hud_client
ctest --preset test-debug-win32 -R "runtime_layout|hud" --output-on-failure
```
