# Create Reticles From Primitives

This tutorial shows how to build a reusable reticle template from primitives.

The goal is to create a library reticle that can later be reused by one or more
pages.

For the exact list of primitive fields and aliases, see:

- [JSON Syntax](../src/reference/json.md)
- [Primitives Reference](../src/reference/primitives.md)

## At A Glance

\startuml
left to right direction
rectangle "Primitive JSON" as PrimitiveJson
rectangle "Reticle template" as ReticleTemplate
rectangle "Page instance" as PageInstance
rectangle "Rendered page" as RenderedPage

PrimitiveJson --> ReticleTemplate
ReticleTemplate --> PageInstance
PageInstance --> RenderedPage
\enduml

## What you will build

You will build a small status widget composed of:

- one `rectangle`
- one `ellipse`
- one `text`
- one `time`

The project already ships a working example in
`examples/demo/assets/reticles/status_clock.json`.

Visually, think of something like this:

```text
+----------------------------------+
|  (ellipse)  UTC      12:34:56    |
+----------------------------------+
```

## Step 1 - Go to the reticle library folder

Reusable reticles live in `examples/demo/assets/reticles`.

Each `.json` file in that folder is loaded as one template.

## Step 2 - Create a new reticle file

Create a file such as `examples/demo/assets/reticles/my_status_widget.json`.

Start with this content:

```json
{
  "id": "my_status_widget",
  "elements": [
    {
      "id": "panel",
      "type": "rectangle",
      "width": 0.42,
      "height": 0.14,
      "thickness": 0.003,
      "filled": true,
      "fill": "#10202ECC"
    },
    {
      "id": "indicator",
      "type": "ellipse",
      "position": { "x": -0.1458, "y": 0.0 },
      "width": 0.0729,
      "height": 0.0729,
      "thickness": 0.0025,
      "filled": true,
      "fill": "friendly"
    },
    {
      "id": "mode_label",
      "type": "text",
      "text": "UTC",
      "position": { "x": -0.0104, "y": 0.025 },
      "fontSize": 0.0271
    },
    {
      "id": "clock_value",
      "type": "time",
      "format": "%H:%M:%S",
      "utc": true,
      "position": { "x": 0.0625, "y": -0.0208 },
      "fontSize": 0.0333,
      "letterSpacing": 0.0021
    }
  ]
}
```

## Step 3 - Understand the primitive types

The main primitive types are:

- `text`
- `time`
- `line`
- `circle`
- `ring`
- `rectangle`
- `ellipse`
- `square`
- `diamond`
- `triangle`
- `polyline`
- `bezier`
- `arc`

Notes:

- `time` is a text-like primitive that formats the current clock value.
- `type: "heure"` is also accepted as an alias for `time`.
- `rectangle` is a dedicated primitive, separate from `square`.
- `ellipse` uses `width` and `height`.
- `arc` uses `radius`, `startAngleDegrees`, and `endAngleDegrees`.

## Step 4 - Use normalized coordinates

The authoring space is always logical `[-1, 1]`.

That means:

- `(0, 0)` is the center of the page
- `x > 0` goes right
- `y > 0` goes up
- primitive values do not depend on window pixels

Because of that, the reticle stays consistent when the window is resized.

Visual reminder:

```text
          y = +1
            ^
            |
 x = -1 <---+---> x = +1
            |
            v
          y = -1
```

## Step 5 - Use primitive ids

Always give an `id` to each primitive.

Why this matters:

- the editor can target the primitive easily
- text primitives can be addressed by name
- runtime overrides stay readable

Example:

```json
{
  "id": "track_label",
  "type": "text",
  "text": "T42"
}
```

## Step 6 - Pick one of the authoring styles

For colors you can use:

- named colors such as `"hud"`, `"radar"`, `"amber"`, `"friendly"`
- hex colors such as `"#33FF88FF"`
- arrays such as `[51, 255, 136, 255]`
- `rgb(...)` and `rgba(...)`

For visibility and styling you can use:

- `visible` or `show`
- `stroke` or `color`
- `fill` or `fillColor`
- `thickness`, `lineWidth`, or `strokeWidth`

## Step 7 - Time primitive options

`time` supports:

- `format`
- `utc`
- `fontSize` or `size`
- `letterSpacing`

Example:

```json
{
  "id": "local_time",
  "type": "time",
  "format": "%H:%M:%S",
  "utc": false,
  "fontSize": 0.04
}
```

## Step 8 - Reuse the reticle in pages

Once the file is in `examples/demo/assets/reticles`, it becomes part of the template
library.

You can then instantiate it inside any page with:

```json
{
  "id": "status_clock",
  "template": "my_status_widget",
  "position": { "x": 0.6, "y": 0.75 }
}
```

## Step 9 - Optional: author the same reticle in the editor

You can also create the same template visually in `mfd_editor`:

1. Open the editor.
2. Create or load a document.
3. Create a new library reticle.
4. Append `Rectangle`, `Ellipse`, `Text`, and `Time` primitives.
5. Edit each primitive directly in the reticle studio.
6. Save with `Ctrl+S`.

## What You Should See

When this reticle is instantiated on a page, you should see:

- one rectangular background panel
- one ellipse indicator on the left
- one text label
- one live formatted clock value

If one part is missing, check:

- the primitive `type`
- the primitive `id`
- the color and fill settings
- the position values in normalized coordinates

## Result

You now have a reusable reticle template that can be used by many pages.
