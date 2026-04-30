# Common JSON Syntax

This page describes the shared JSON conventions used across windows, pages,
reticles, and primitives.

It is the best page to read before the primitive and page references.

## Coordinates

All authoring coordinates use normalized logical space.

```text
          y = +1
            ^
            |
 x = -1 <---+---> x = +1
            |
            v
          y = -1
```

Rules:

- `(0, 0)` is the center of the page
- `x > 0` goes right
- `y > 0` goes up
- coordinates are not expressed in pixels

This applies to:

- reticle positions
- primitive positions
- page view center
- strobe position

## `Vec2` Syntax

The loader accepts a 2D vector in two forms.

Array form:

```json
[0.25, -0.10]
```

Object form:

```json
{ "x": 0.25, "y": -0.10 }
```

Both are valid for fields such as:

- `position`
- `center`
- `start`
- `end`
- points inside `points` arrays

## Transform Syntax

Many objects support the same transform model:

- position
- rotation
- scale

Canonical form:

```json
"transform": {
  "position": [0.25, -0.10],
  "rotationDegrees": 15.0,
  "scale": [1.0, 1.0]
}
```

Accepted aliases:

| Meaning | Canonical field | Supported aliases |
| --- | --- | --- |
| position | `position` | `at`, `pos` |
| position x | `x` | none |
| position y | `y` | none |
| rotation | `rotationDegrees` | `angle`, `rotation` |
| scale | `scale` | `zoom` |
| scale x | `sx` | `scaleX` |
| scale y | `sy` | `scaleY` |

Notes:

- transform fields can be placed inside a nested `transform` object
- the same fields can also be placed directly on the current object
- direct fields are applied after nested `transform`

Example:

```json
{
  "position": [0.40, 0.15],
  "rotation": 22.5,
  "scale": 1.2
}
```

## Visibility Syntax

Visibility can be written in several ways.

| Canonical meaning | Supported fields |
| --- | --- |
| visible | `visible`, `show` |
| hidden | `hidden` |

Examples:

```json
{ "visible": true }
```

```json
{ "show": false }
```

```json
{ "hidden": true }
```

## Color Syntax

The loader accepts several color formats.

### Named colors

Examples:

```json
"hud"
```

```json
"radar"
```

```json
"friendly"
```

Available built-in names include:

- `transparent`
- `black`
- `white`
- `green`
- `lime`
- `hud`
- `cyan`
- `radar`
- `amber`
- `yellow`
- `orange`
- `red`
- `warning`
- `danger`
- `friendly`
- `hostile`
- `ghost`

### Hex colors

Examples:

```json
"#33FF88"
```

```json
"#33FF88FF"
```

```json
"#3F8"
```

```json
"#3F8F"
```

### Arrays

Examples:

```json
[51, 255, 136]
```

```json
[51, 255, 136, 255]
```

### `rgb(...)` and `rgba(...)`

Examples:

```json
"rgb(51, 255, 136)"
```

```json
"rgba(51, 255, 136, 255)"
```

### Object form

Examples:

```json
{ "r": 51, "g": 255, "b": 136, "a": 255 }
```

```json
{ "name": "amber", "opacity": 70 }
```

```json
{ "rgb": [51, 255, 136], "alpha": 220 }
```

## Style Syntax

Most drawable objects support the same style fields.

| Meaning | Canonical field | Supported aliases |
| --- | --- | --- |
| stroke color | `stroke` | `color`, `strokeColor` |
| fill color | `fill` | `fillColor` |
| thickness | `thickness` | `lineWidth`, `strokeWidth`, `strokeThickness` |
| stroke pattern | `lineStyle` | `strokeStyle`, `strokePattern` |
| filled flag | `filled` | `fill` when boolean |
| visibility | `visible` | `show`, `hidden` |

Canonical example:

```json
{
  "stroke": "#33FF88FF",
  "thickness": 0.004,
  "lineStyle": "dashed",
  "filled": true,
  "fill": "#112233AA"
}
```

`style` blocks are also supported:

```json
{
  "style": {
    "stroke": "hud",
    "thickness": 0.004,
    "lineStyle": "dotted"
  }
}
```

Recommended `lineStyle` values are `solid`, `dotted`, and `dashed`.

## Text Overrides At Reticle Instance Level

A reticle instance can override text content and character spacing after a
template is instantiated.

Supported fields:

| Meaning | Canonical field | Supported aliases |
| --- | --- | --- |
| first text primitive content | `text` | none |
| named text primitive content | `texts` | none |
| first text-like primitive spacing | `letterSpacing` | `spacing`, `tracking` |
| named text-like primitive spacing | `letterSpacings` | `spacings` |

Example:

```json
{
  "id": "track_42",
  "template": "radar_track",
  "texts": {
    "track_label": "AF042"
  },
  "letterSpacings": {
    "track_label": 0.01
  }
}
```

## Recommendation

Even though the loader accepts many aliases, the recommended authoring style is:

- use `position`
- use `rotationDegrees`
- use `scale`
- use `stroke`
- use `fill`
- use `thickness`
- use `lineStyle`
- use `visible`

This keeps files easier to read and easier to maintain.
