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
Page and starts the generated client. The client explicitly activates the Page
and sends its view using `SendStartup`, even when the `MainPanel()` handle's
visibility, enabled state and scroll offset are unchanged from authored defaults.
It then exits; it is not an interactive control panel. UDP send success is not
an acknowledgement from the Window. If only the Window was launched, run
`apilevel1_containers_client.exe` from `_Exec/v142/win32/Debug` afterwards.
