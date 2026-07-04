# Test A Window With The Demo Client

This tutorial validates the simplified demo asset with the standalone DX11
client.

## Step 1 - Build

```powershell
cmake --build --preset debug-win32 --target mfd_window demo_client
```

## Step 2 - Start The Runtime

```powershell
.\Scripts\Start-MfdDemo.bat
```

The runtime loads `examples/demo/assets/windows/demo_window.json`.

## Step 3 - Start The Client

```powershell
.\build\vs2022-win32\examples\demo\client\Debug\demo_client.exe
```

The client has no control panel. It automatically cycles through:

- `Primitives`
- `BlinkStrobe`
- `Clipping`

It publishes reticle, blink, strobe and clipping updates over UDP. If the
runtime view changes, the window JSON, generated transport map and command path
are all wired correctly.

## Step 4 - Inspect

Press `F1` inside `mfd_window` to open the runtime debug overlay. Use it to
inspect the active page, command status and reticle tree while the client keeps
publishing.

## Next

- [04 Drive A Window From A Live Client](./04_drive_a_window_from_a_live_client.md)
- [10 Radar Load Demo](./10_radar_load_demo.md)
- [11 Use The Demo Client As A Client API Reference](./11_use_the_demo_client_as_a_client_api_reference.md)
