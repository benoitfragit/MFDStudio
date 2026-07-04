# Getting Started Tutorial

A complete, minimal path for a new developer: build the runtime, launch a
shipped asset, run a standalone client, and verify that commands reach the view.

## What You Will Run

```text
window JSON -> mfd_window -> standalone client -> live UDP commands -> visible change
```

This walkthrough uses only repository files:

- `examples/demo/assets/windows/demo_window.json`
- `examples/demo/client`
- `examples/radar_load/assets/windows/radar_load_window.json`
- `examples/radar_load/client`

## 1. Build

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window demo_client radar_load_client mfd_framebuffer_stdout_plugin
```

## 2. Run The Simple Demo

Start the runtime:

```powershell
.\Scripts\Start-MfdDemo.bat
```

Start the client:

```powershell
.\build\vs2022-win32\examples\demo\client\Debug\demo_client.exe
```

`demo_client` is autonomous. It cycles the `Primitives`, `BlinkStrobe`, and
`Clipping` pages and sends reticle, strobe, blink and clipping updates over UDP.

## 3. Inspect The Runtime

Press `F1` inside `mfd_window`. The debug overlay shows the active page, reticle
tree, command status and feedback status. If the pages cycle and symbols move,
the window JSON, generated map and UDP command path are wired correctly.

## 4. Run The Radar Load Example

Start the radar runtime:

```powershell
.\Scripts\Start-RadarLoad.bat
```

Start the load client:

```powershell
.\build\vs2022-win32\examples\radar_load\client\Debug\radar_load_client.exe
```

The radar client ramps dynamic tracks from 10 to 300. The page prints the
current track count and client publish FPS at the top of the view.

## 5. Next

| Goal | Page |
| --- | --- |
| Use generated client handles | [Generated Client API](./handbook/generated_api.md) |
| Embed the runtime without a visible window | [Offscreen Embedding](./handbook/offscreen.md) |
| Capture raw pixels | [Framebuffer Capture](./handbook/framebuffer.md) |
| Exact JSON fields | [Pages And Windows](./reference/pages_and_windows.md) |
