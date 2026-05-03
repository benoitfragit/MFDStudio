# Documentation Images

This folder stores the stable visual assets used by the Markdown guides and the
generated Doxygen portal.

Contents:

- `*.puml`: PlantUML source files kept under version control
- `*.svg`: rendered diagrams intended to be visible directly from GitHub
- `*.png`: GUI screenshots captured from the real Windows executables

Refresh helpers:

- render diagrams: `.\docs\RenderPlantUmlDiagrams.ps1`
- capture GUI screenshots: `.\docs\CaptureScreenshots.ps1`

The screenshots currently target the shipped Win32 debug runtime staged under
`_Exec`, launch `mfd_window`, `client_mockup`, and `mfd_editor`, then capture
each window individually.
