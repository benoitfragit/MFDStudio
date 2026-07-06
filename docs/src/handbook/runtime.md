# Runtime

`mfd_window` is the runtime host. It loads one window JSON, owns rendering and
runtime state, and accepts public commands over UDP from a client.

## Launch

The simplest path is a shipped launcher:

- `.\Scripts\Start-MfdDemo.bat`
- `.\Scripts\Start-RadarLoad.bat`
- `.\Scripts\Start-Tutorial.bat`
- `.\Scripts\Start-HUD.bat`
- `.\Scripts\Start-LHLD.bat`

For an explicit window, use the generic launcher:

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json
```

It can also pass a framebuffer plugin:

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll
```

For heavy live-debug sessions, `mfd_window` also accepts `--no-snapshot`:

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json --no-snapshot
```

This keeps earlier commands of the same runtime batch applied when a later
command fails. The default remains transactional: without `--no-snapshot`, a
multi-command batch still rolls back as one unit.

The transactional rollback snapshot only covers the pages a batch actually
touches (plus scene-wide state such as the active page and the window display),
so the default mode stays cheap even on large scenes with high-rate batches.
`--no-snapshot` is therefore mostly a debugging escape hatch, not a performance
requirement.

`Start-MfdDemo.bat` starts the simplified demo asset under
`examples/demo/assets`.

`Start-RadarLoad.bat` starts the radar load asset under
`examples/radar_load/assets`. Drive it with `radar_load_client.exe` to ramp
dynamic tracks from 10 to 300 and display track count versus client publish FPS
inside the page.

`Start-HUD.bat` starts the dedicated HUD asset from
`examples/hud/assets/windows/hud_window.json`. Drive it with
`hud_client.exe`; the HUD workflow is documented in
[HUD](./hud.md).

`Start-LHLD.bat` starts the LHLD in-process client from the staged runtime tree.
It hosts the runtime offscreen inside its own DX11/ImGui window, so it does not
launch `mfd_window`.

## Debug overlay

Press `F1` inside `mfd_window` to open the integrated runtime debug overlay. It
lets you inspect:

- the active page
- the reticle tree
- transport state
- temporary local bypasses, including structured time-primitive overrides

This is the fastest way to distinguish an authored-asset issue from a
client-side command issue or a runtime-side state issue.

## Checking the transport quickly

Run `demo_client` next to `Start-MfdDemo.bat`. The client automatically cycles
pages and sends reticle/strobe updates over UDP; if the runtime view changes,
the control path is alive.

## Client relationship

`demo_client` is a normal standalone UDP client. It loads the same window JSON
locally for discovery, then sends public commands to the runtime - it shares no
memory with `mfd_window`, which is exactly why it is a good reference client.
