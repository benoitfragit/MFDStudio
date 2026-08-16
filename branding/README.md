# MFDStudio branding

The MFDStudio identity is a restrained `M` monogram inside a rounded display
frame. The mark deliberately avoids cockpit controls, targeting symbols,
gradients, glow, and small decorative details so it remains readable in a
16-pixel Windows title-bar icon.

## Source assets

- `mfdstudio_mark.svg`: dark application mark for light surfaces.
- `mfdstudio_mark_inverse.svg`: inverse mark for dark surfaces.
- `Generate-BrandingAssets.ps1`: deterministic PNG and multi-resolution ICO
  generator based on the same proportions.

## Generated assets

- `mfdstudio_app_icon.png`: 1024-pixel runtime icon used by raylib.
- `mfdstudio_app_icon.ico`: Windows resource containing 16, 20, 24, 32, 40,
  48, 64, 128, and 256-pixel images.
- `mfdstudio_mark_dark.png` and `mfdstudio_mark_light.png`: transparent raster
  marks for documentation and non-vector consumers.

Regenerate the raster files from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File branding/Generate-BrandingAssets.ps1
```

The production palette is intentionally limited to graphite `#202327` and
warm white `#F7F5F0`.

The raster branding directory is optional at runtime. `mfd_window` and
`mfd_editor` draw their internal mark from compiled geometry and continue to
start normally when these files are absent. A missing PNG only prevents the
late title-bar icon override; the ICO embedded in Windows executables remains
available through the resource script.
