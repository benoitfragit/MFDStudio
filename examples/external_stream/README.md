# External stream example

This example displays two consumer-owned shared D3D11 surfaces, filled by a
Python process, with native MFD text, frames and crosshairs overlaid.

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_stream_demo
py -3.7-32 examples/external_stream/producer.py --launch
```

Use matching Python/DLL bitness. No Python package installation is needed.
The default is two 640 × 360 RGBA streams at a target 50 Hz.
The color bars are intentional: a moving white bar and a decimal frame counter
make updates visible. Once Python stops, MFDStudio retains the last image.

Python console: Space pauses the producer, P/A/S selects reader Play/Pause/Stop,
V toggles visibility, Q closes the producer. MFDStudio remains open.

Before another `--launch`, close the previous demo window: its command UDP port
47230 stays occupied while that window is open. The launcher detects an unavailable
port and reports how to proceed. To reuse an open window, run
`py -3.7-32 producer.py --instance <pid>-1` from the staged Debug directory,
without `--launch`. WSA 10048 is a port conflict, not a Python/DLL bitness error.

For separate launch, run `Scripts/Start-ExternalStream.bat`, then pass the logged
instance to `producer.py --instance <pid>-1`. Options include repeated `--stream`,
`--width`, `--height`, `--format rgba|bgra`, `--fps`, `--duration`, `--dll` and
`--runtime`. BGRA and different resolutions can be tested by restarting Python.

The [full guide](../../docs/src/handbook/external_stream.md) documents the native
contract, synchronization, limits, commands and GPU test procedure. This CPU
pattern/upload demonstration does not represent direct GPU producer performance.

If your application already owns a GPU texture, see
[native GPU integration](../../docs/src/handbook/external_stream.md#native-gpu-integration).
A C++ producer can link `mfd::external_stream` without this demo DLL. It still
acquires and publishes MFDStudio-owned surfaces: either copy a compatible GPU
source into them, or render directly into them. The guide explains the device
compatibility and synchronization requirements.
