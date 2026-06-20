# JSON Syntax

Shared conventions used across windows, pages, reticles, and primitives. Read
this before the [Primitives](./primitives.md) and
[Pages And Windows](./pages_and_windows.md) references.

## Coordinates

All authoring coordinates use normalized logical space, not pixels.

```text
          y = +1
            ^
            |
 x = -1 <---+---> x = +1
            |
            v
          y = -1
```

`(0, 0)` is the page center, `x > 0` goes right, `y > 0` goes up. This applies
to reticle positions, primitive positions, page view center, and strobe position.

## Vec2

A 2D vector is accepted in two forms:

```json
[0.25, -0.10]
```

```json
{ "x": 0.25, "y": -0.10 }
```

Both are valid for `position`, `center`, `start`, `end`, and points inside
`points` arrays.

## Transform

```json
"transform": {
  "position": [0.25, -0.10],
  "rotationDegrees": 15.0,
  "scale": [1.0, 1.0]
}
```

The same fields may be placed directly on the object instead of inside
`transform`; direct fields are applied after the nested block.

| Meaning | Canonical | Aliases |
| --- | --- | --- |
| position | `position` | `at`, `pos` |
| rotation | `rotationDegrees` | `angle`, `rotation` |
| scale | `scale` | `zoom` |
| scale x / y | `sx` / `sy` | `scaleX` / `scaleY` |

## Visibility

| Meaning | Fields |
| --- | --- |
| visible | `visible`, `show` |
| hidden | `hidden` |

## Color

Several formats are accepted:

- **Named**: `"hud"`, `"radar"`, `"amber"`, `"friendly"`, … (also `transparent`,
  `black`, `white`, `green`, `lime`, `cyan`, `yellow`, `orange`, `red`,
  `warning`, `danger`, `hostile`, `ghost`)
- **Hex**: `"#33FF88"`, `"#33FF88FF"`, `"#3F8"`, `"#3F8F"`
- **Array**: `[51, 255, 136]` or `[51, 255, 136, 255]`
- **Functional**: `"rgb(51, 255, 136)"`, `"rgba(51, 255, 136, 255)"`
- **Object**: `{ "r": 51, "g": 255, "b": 136, "a": 255 }`,
  `{ "name": "amber", "opacity": 70 }`, `{ "rgb": [51, 255, 136], "alpha": 220 }`

## Style

| Meaning | Canonical | Aliases |
| --- | --- | --- |
| stroke color | `stroke` | `color`, `strokeColor` |
| fill color | `fill` | `fillColor` |
| thickness | `thickness` | `lineWidth`, `strokeWidth`, `strokeThickness` |
| stroke pattern | `lineStyle` | `strokeStyle`, `strokePattern` |
| filled flag | `filled` | `fill` when boolean |
| visibility | `visible` | `show`, `hidden` |

```json
{
  "stroke": "#33FF88FF",
  "thickness": 0.004,
  "lineStyle": "dashed",
  "filled": true,
  "fill": "#112233AA"
}
```

`filled` / `fill` apply only to fill-capable primitives (circles, rings,
rectangles, ellipses, squares, diamonds, triangles, arcs, and polylines with
`closed: true`). Recommended `lineStyle` values are `solid`, `dotted`, `dashed`.

`style` blocks are also supported:

```json
{ "style": { "stroke": "hud", "thickness": 0.004, "lineStyle": "dotted" } }
```

## Reticle-instance text overrides

A reticle instance can override text content and spacing after instantiation:

| Meaning | Canonical | Aliases |
| --- | --- | --- |
| first text content | `text` | none |
| named text content | `texts` | none |
| first text spacing | `letterSpacing` | `spacing`, `tracking` |
| named text spacing | `letterSpacings` | `spacings` |

```json
{
  "id": "track_42",
  "template": "radar_track",
  "texts": { "track_label": "AF042" },
  "letterSpacings": { "track_label": 0.01 }
}
```

## Recommended style

Even though many aliases are accepted, prefer the canonical names: `position`,
`rotationDegrees`, `scale`, `stroke`, `fill`, `thickness`, `lineStyle`,
`visible`. It keeps files easier to read and maintain.
