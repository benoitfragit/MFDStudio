# Radar Load Demo

The radar load example is the dedicated high-churn runtime demo. It uses one
window, one radar page, and one headless client.

## Step 1 - Build

```powershell
cmake --build --preset debug-win32 --target mfd_window radar_load_client
```

## Step 2 - Start The Runtime

```powershell
.\Scripts\Start-RadarLoad.bat
```

The runtime loads `examples/radar_load/assets/windows/radar_load_window.json`.

## Step 3 - Start The Load Client

```powershell
.\build\vs2022-win32\examples\radar_load\client\Debug\radar_load_client.exe
```

The client is autonomous. It ramps dynamic tracks through:

```text
10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200, 250, 300
```

The page prints the current track count and the current client publish FPS at
the top. Its generated page-title chrome is hidden so the load readout stays
uncluttered. No runtime control panel is required.

## What It Exercises

- `UpsertDynamicReticlesCommand` with a growing dynamic reticle set
- client-side track state retention, so static track fields are sent only when
  a track is first created
- generated transport-map normalization for one dynamic template
- blink types on a subset of radar contacts
- page-local status text updated only when its displayed value changes
- runtime feedback while the scene is under load
