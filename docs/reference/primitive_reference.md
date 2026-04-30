# Primitive Reference

This page lists all supported primitive types and their JSON fields.

## Common Primitive Fields

Every primitive supports the following common fields.

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | recommended | Primitive identifier inside the reticle. |
| `type` | string | yes | Primitive type. |
| `transform` | object | no | Nested transform object. |
| `position` | vec2 | no | Primitive local translation. |
| `x`, `y` | number | no | Alternative way to set `position`. |
| `rotationDegrees` | number | no | Primitive local rotation in degrees. |
| `scale` | number or vec2 | no | Primitive local scale. |
| `style` | object | no | Nested primitive style object. |
| `visible` | bool | no | Primitive visibility. |
| `stroke` | color | no | Primitive stroke color. |
| `thickness` | number | no | Primitive stroke thickness. |
| `lineStyle` | string | no | Stroke pattern for outline-capable primitives: `solid`, `dotted`, or `dashed`. |
| `filled` | bool | no | Whether the primitive is filled. |
| `fill` | color or bool | no | Fill color or fill enable flag. |

For transform and color syntax, see [Common JSON Syntax](./common_json_syntax.md).

`lineStyle` applies to outline-capable primitives such as lines, circles,
rings, rectangles, ellipses, squares, diamonds, triangles, polylines, beziers,
and arcs. Text, time, and image primitives keep their default solid stroke.

## Supported Type Names

| Canonical type | Accepted aliases |
| --- | --- |
| `text` | `text` |
| `time` | `time`, `clock`, `heure` |
| `line` | `line` |
| `circle` | `circle` |
| `ring` | `ring`, `annulus`, `donut` |
| `rectangle` | `rectangle`, `rect`, `box` |
| `ellipse` | `ellipse`, `oval` |
| `square` | `square` |
| `diamond` | `diamond` |
| `triangle` | `triangle` |
| `polyline` | `polyline` |
| `bezier` | `bezier` |
| `arc` | `arc` |
| `image` | `image`, `picture`, `sprite` |

## `text`

Purpose:

- static labels
- numeric values
- track names

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `text` | string | no | Displayed text. |
| `fontSize` | number | no | Logical text size. |
| `size` | number | no | Alias of `fontSize`. |
| `letterSpacing` | number | no | Character spacing. |
| `spacing` | number | no | Alias of `letterSpacing`. |
| `tracking` | number | no | Alias of `letterSpacing`. |

Example:

```json
{
  "id": "track_label",
  "type": "text",
  "text": "AF042",
  "position": [0.10, 0.04],
  "fontSize": 0.035,
  "letterSpacing": 0.003,
  "stroke": "hud"
}
```

## `time`

Purpose:

- UTC clock
- local clock
- mission timer style displays based on formatted wall clock output

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `format` | string | no | `strftime`-style format string. |
| `pattern` | string | no | Alias of `format`. |
| `utc` | bool | no | Use UTC clock when `true`. |
| `useUtc` | bool | no | Alias of `utc`. |
| `fontSize` | number | no | Logical text size. |
| `size` | number | no | Alias of `fontSize`. |
| `letterSpacing` | number | no | Character spacing. |
| `spacing` | number | no | Alias of `letterSpacing`. |
| `tracking` | number | no | Alias of `letterSpacing`. |

Example:

```json
{
  "id": "clock_value",
  "type": "time",
  "format": "%H:%M:%S",
  "utc": true,
  "fontSize": 0.04,
  "letterSpacing": 0.002
}
```

## `line`

Purpose:

- reference marks
- brackets
- track vectors

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `start` | vec2 | no | Start point. |
| `from` | vec2 | no | Alias of `start`. |
| `end` | vec2 | no | End point. |
| `to` | vec2 | no | Alias of `end`. |

Example:

```json
{
  "id": "vector",
  "type": "line",
  "start": [-0.03, 0.0],
  "end": [0.03, 0.0],
  "stroke": "radar",
  "thickness": 0.003,
  "lineStyle": "dashed"
}
```

## `circle`

Purpose:

- circular markers
- circular capture zones
- center dots

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `radius` | number | no | Circle radius in logical units. |

Example:

```json
{
  "id": "center_dot",
  "type": "circle",
  "radius": 0.018,
  "filled": true,
  "fill": "friendly"
}
```

Recommendation:

- always set `radius` explicitly

## `ring`

Purpose:

- bezels
- circular masks
- annular highlights

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `outerRadius` | number | no | Outer radius of the ring. |
| `radius` | number | no | Alias fallback for `outerRadius`. |
| `innerRadius` | number | no | Inner radius of the ring. |
| `holeRadius` | number | no | Alias fallback for `innerRadius`. |
| `bandWidth` | number | no | Width of the ring band used when `innerRadius` is omitted. |
| `ringWidth` | number | no | Alias of `bandWidth`. |
| `segments` | integer | no | Circle sampling count used for the annulus. |

Example:

```json
{
  "id": "adi_bezel",
  "type": "ring",
  "innerRadius": 0.29,
  "outerRadius": 0.40,
  "filled": true,
  "fill": "#0A1119FF",
  "stroke": "#6A8DA6FF"
}
```

Recommendation:

- prefer `innerRadius` plus `outerRadius`
- use `bandWidth` only as a convenience shortcut

## `rectangle`

Purpose:

- panels
- frames
- bounding boxes

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `size` | number | no | Uniform size used as fallback for width and height. |
| `width` | number | no | Rectangle width. |
| `height` | number | no | Rectangle height. |
| `diameterX` | number | no | Alias fallback for width. |
| `diameterY` | number | no | Alias fallback for height. |

Example:

```json
{
  "id": "panel",
  "type": "rectangle",
  "width": 0.42,
  "height": 0.14,
  "filled": true,
  "fill": "#10202ECC",
  "stroke": "#EAD29BFF"
}
```

Recommendation:

- prefer `width` and `height` for readability

## `ellipse`

Purpose:

- indicators
- elliptical marks
- oval widgets

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `size` | number | no | Base size used to derive width and height. |
| `radius` | number | no | Shared radius used when no explicit axis is given. |
| `width` | number | no | Ellipse width. |
| `height` | number | no | Ellipse height. |
| `radiusX` | number | no | Horizontal radius. |
| `radiusY` | number | no | Vertical radius. |
| `rx` | number | no | Alias of `radiusX`. |
| `ry` | number | no | Alias of `radiusY`. |
| `diameterX` | number | no | Alias fallback for width. |
| `diameterY` | number | no | Alias fallback for height. |

Example:

```json
{
  "id": "indicator",
  "type": "ellipse",
  "width": 0.08,
  "height": 0.05,
  "filled": true,
  "fill": "friendly"
}
```

Recommendation:

- prefer `width` and `height`
- use `radiusX` and `radiusY` only when that matches your source data better

## `square`

Purpose:

- square markers
- cursor corners
- tactical tiles

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `size` | number | no | Base size used as width and height fallback. |
| `width` | number | no | Square width. |
| `height` | number | no | Square height. |

Example:

```json
{
  "id": "box",
  "type": "square",
  "size": 0.06,
  "stroke": "amber"
}
```

Recommendation:

- if you want a true square, set `size`
- if you set different `width` and `height`, the renderer still treats it as the square primitive geometry

## `diamond`

Purpose:

- target markers
- waypoint symbols
- rhombus overlays

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `size` | number | no | Base size used as width and height fallback. |
| `width` | number | no | Diamond width. |
| `height` | number | no | Diamond height. |

Example:

```json
{
  "id": "waypoint",
  "type": "diamond",
  "width": 0.05,
  "height": 0.07,
  "stroke": "radar"
}
```

## `triangle`

Purpose:

- pointers
- directional markers
- custom three-point symbols

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `points` | array of vec2 | yes | Exactly three points are used. |

Example:

```json
{
  "id": "pointer",
  "type": "triangle",
  "points": [
    [0.0, 0.04],
    [-0.03, -0.02],
    [0.03, -0.02]
  ]
}
```

## `polyline`

Purpose:

- route lines
- frame outlines
- arbitrary multi-segment shapes

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `points` | array of vec2 | yes | At least two points. |
| `closed` | bool | no | Closes the line loop when `true`. |

Example:

```json
{
  "id": "route",
  "type": "polyline",
  "points": [
    [-0.30, -0.10],
    [-0.10, 0.20],
    [0.20, 0.10]
  ],
  "closed": false
}
```

Notes:

- when `closed` and `filled` are both `true`, the renderer fills the polygon area
- the current fill path assumes a convex polygon for correct results

## `bezier`

Purpose:

- curved guides
- smooth brackets
- custom curved shapes

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `controlPoints` | array of vec2 | yes unless `points` is used | Bezier control points. |
| `points` | array of vec2 | yes unless `controlPoints` is used | Alias input for control points. |
| `segments` | integer | no | Number of line segments used to render the curve. |

Example:

```json
{
  "id": "curve",
  "type": "bezier",
  "controlPoints": [
    [-0.20, 0.00],
    [-0.10, 0.15],
    [0.10, 0.15],
    [0.20, 0.00]
  ],
  "segments": 48
}
```

## `arc`

Purpose:

- scan sectors
- circular guide marks
- partial rings and angular indicators

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `radius` | number | no | Arc radius in logical units. |
| `startAngleDegrees` | number | no | Arc start angle in degrees. |
| `startAngle` | number | no | Alias of `startAngleDegrees`. |
| `fromDegrees` | number | no | Alias of `startAngleDegrees`. |
| `angleStart` | number | no | Alias of `startAngleDegrees`. |
| `endAngleDegrees` | number | no | Arc end angle in degrees. |
| `endAngle` | number | no | Alias of `endAngleDegrees`. |
| `toDegrees` | number | no | Alias of `endAngleDegrees`. |
| `angleEnd` | number | no | Alias of `endAngleDegrees`. |
| `segments` | integer | no | Number of line segments used to approximate the arc. |

Example:

```json
{
  "id": "scan_arc",
  "type": "arc",
  "radius": 0.24,
  "startAngleDegrees": -45.0,
  "endAngleDegrees": 135.0,
  "segments": 40
}
```

Notes:

- when `filled` is `true`, the arc is rendered as a sector from the center to the sampled arc
- use a higher `segments` value if the arc must look smooth at large radius

## `image`

Purpose:

- logos
- bitmap overlays
- image-backed widgets or page annotations

Specific fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `file` | string | recommended | Image file path. |
| `image` | string | no | Alias of `file`. |
| `source` | string | no | Alias of `file`. |
| `path` | string | no | Alias of `file`. |
| `size` | number or vec2 | no | Uniform or explicit image size in logical units. |
| `width` | number | no | Image width. |
| `height` | number | no | Image height. |

Example:

```json
{
  "id": "demo_picture",
  "type": "image",
  "file": "../picture/mfdstudio_badge.png",
  "size": [0.30, 0.30]
}
```

Notes:

- image paths are resolved relative to the page or reticle JSON file that references them
- use the common transform fields `position`, `rotationDegrees`, and `scale` to animate the image at runtime
- the runtime loads image textures lazily and applies bilinear filtering

## Practical Recommendations

For authoring files that stay readable:

- always give each primitive an `id`
- always set geometric sizes explicitly
- prefer canonical field names over aliases
- prefer `width` and `height` for `rectangle` and `ellipse`
- use `controlPoints` instead of `points` for `bezier`
- use `startAngleDegrees` and `endAngleDegrees` for `arc`

## Full Example

```json
{
  "id": "status_clock",
  "elements": [
    {
      "id": "panel",
      "type": "rectangle",
      "width": 0.42,
      "height": 0.14,
      "filled": true,
      "fill": "#10202ECC"
    },
    {
      "id": "indicator",
      "type": "ellipse",
      "position": [-0.1458, 0.0],
      "width": 0.0729,
      "height": 0.0729,
      "filled": true,
      "fill": "friendly"
    },
    {
      "id": "mode_label",
      "type": "text",
      "text": "UTC",
      "position": [-0.0104, 0.025],
      "fontSize": 0.0271
    },
    {
      "id": "clock_value",
      "type": "time",
      "format": "%H:%M:%S",
      "utc": true,
      "position": [0.0625, -0.0208],
      "fontSize": 0.0333,
      "letterSpacing": 0.0021
    }
  ]
}
```
