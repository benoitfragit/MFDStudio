# API LEVEL 1 containers example

`assets/windows/containers_window.json` contains one legacy-only Page and one
container Page. The latter demonstrates nested Canvas, Grid, Linear and Overlay
layouts, a clipped vertical scrolling region, styled scrollbars, and the exposed
`main_panel` container.

Build the Debug Win32 runtime, extensions and `apilevel1_containers_client`, then
launch the staged example from the repository root:

```powershell
cmake --build --preset debug-win32
.\Scripts\Start-APILevel1Containers.bat
```

The script launches the mixed LEVEL 0/LEVEL 1 Window on its visible Containers
Page and starts the generated client. The client publishes visibility, enabled
state and scroll offset through the strongly typed `MainPanel()` handle.
