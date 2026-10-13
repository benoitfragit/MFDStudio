# Color presets

MFDStudio shares one built-in palette between the editor, JSON loader, C++ client
and Window runtime. `mfd::ColorRgba` remains four 8-bit RGBA channels.

| C++ preset | Wire value | RGBA | Hex |
| --- | --- | --- | --- |
| `Black` | 1 | 0, 0, 0, 255 | `#000000FF` |
| `White` | 2 | 255, 255, 255, 255 | `#FFFFFFFF` |
| `Red` | 3 | 255, 96, 96, 255 | `#FF6060FF` |
| `Green` | 4 | 0, 255, 102, 255 | `#00FF66FF` |
| `Blue` | 5 | 0, 120, 255, 255 | `#0078FFFF` |
| `Yellow` | 6 | 249, 214, 111, 255 | `#F9D66FFF` |
| `Cyan` | 7 | 77, 224, 255, 255 | `#4DE0FFFF` |
| `Amber` | 8 | 255, 203, 107, 255 | `#FFCB6BFF` |

`Custom = 0` requires explicit RGBA, including alpha. It has no fixed color.
The shared C++17 header `mfd/model/Types.h` declares `mfd::ColorPreset` and
`constexpr std::optional<ColorRgba> TryColorPreset(ColorPreset) noexcept`.
The conversion returns no value for Custom or unknown identifiers.

## Editor

Every authored color control uses the same compact grid:

```text
Black  White  Red
Green  Blue   Yellow
Cyan   Amber  Custom (+)
```

Click a swatch to apply its exact RGBA. A contrasting outline marks the matching
color; Custom is selected when no built-in RGBA matches, including alpha.
Hover for the English name, RGBA channels and hex value. Custom (+) opens the
standard RGBA picker initialized with the existing alpha. Edit the channels and
click **Apply** to commit one undoable change, or **Cancel** to keep the original.
Opening a control never creates a color override. Inherited or disabled controls
keep their existing rules; a primitive fill masked by a reticle default stays disabled.

The control covers primitive stroke/fill, reticle instance and template defaults,
strobe appearance, page background/title, external-stream replacement color,
and new-window/new-page backgrounds. JSON continues storing effective colors
in its existing format; preset identity is a runtime command choice, not new
authored metadata. Existing JSON assets and named aliases retain their values;
`blue` is also accepted. See [JSON colors](json.md#color).

## Generated C++ client

Static/dynamic reticles and primitive handles expose both overloads:

```cpp
void SetColor(mfd::ColorPreset preset);
void SetColor(mfd::ColorRgba color);
// Only fill-capable primitive handles:
void SetFillColor(mfd::ColorPreset preset);
void SetFillColor(mfd::ColorRgba color);
```

For example, using the generated tutorial client:

```cpp
// Here circle is your generated static or dynamic reticle handle.
circle.SetColor(mfd::ColorPreset::Green);
circle.SetColor(mfd::ColorRgba{80, 160, 255, 180});
```

For a handle supplied by your generated API, this reusable function is complete:

```cpp
#include "mfd/client/Animation.h"

void UpdateArea(mfd::client::RectangleHandle& area, bool warning)
{
    area.SetColor(mfd::ColorPreset::Blue);
    if (warning)
        area.SetFillColor(mfd::ColorPreset::Yellow);
    else
        area.SetFillColor(mfd::ColorRgba{255, 80, 50, 120});
}
```

Calling the preset overload with Custom or an unknown enum throws
`std::invalid_argument` before staging any change. Use the RGBA overload for Custom.
Generated wrappers expose the overload set through their existing `using`
declarations; there is no per-window color enum. Rebuild clients when updating
the SDK because patch structs gain optional members.

## Protobuf and external clients

The authoritative schema is `mfd_common_api/proto/mfd_commands.proto`.
`ColorPreset` values are `COLOR_PRESET_CUSTOM = 0` through
`COLOR_PRESET_AMBER = 8`, in the table order above.

| Message/property | Existing RGBA field | New optional enum field |
| --- | --- | --- |
| `ReticlePatch` color | `optional fixed32 packed_rgba = 4` | `optional ColorPreset color_preset = 14` |
| `PrimitivePatch` stroke | `optional fixed32 packed_rgba = 5` | `optional ColorPreset color_preset = 31` |
| `PrimitivePatch` fill | `optional fixed32 fill_packed_rgba = 6` | `optional ColorPreset fill_color_preset = 32` |

Packed RGBA remains `r | (g << 8) | (b << 16) | (a << 24)`.
No previous field number or wire type changes.

| Preset field | RGBA field | Meaning |
| --- | --- | --- |
| Built-in 1..8 | absent | Apply the preset |
| Explicit Custom 0 | present | Apply custom RGBA |
| absent | present | Accept legacy RGBA-only update |
| absent | absent | Leave color unchanged |

A built-in plus RGBA, Custom without RGBA, or any unknown enum is rejected.
Preserve proto3 **optional presence**, especially explicitly transmitted zero.
Stroke and fill are validated independently before mutation. The Window resolves
the effective RGBA through its existing runtime styling path. Invalid color
updates do not partially change a reticle or its nested primitives; reset restores
the authored baseline.

C++ patches retain their original `color` / `fillColor` optional members and add
`colorPreset` / `fillColorPreset`. Each enum/RGBA pair is one logical property.
Staging, delta generation, latest-batch publishing and UDP coalescing all preserve
last-write-wins: changing to a preset discards old RGBA; changing to custom discards
an old built-in. Repeated identical selections generate no extra delta.

Old RGBA-only clients work with the updated Window. New preset commands require
an updated Window; older runtimes cannot resolve preset-only commands. Rebuild
and deploy the Window with the new schema before enabling preset commands.
