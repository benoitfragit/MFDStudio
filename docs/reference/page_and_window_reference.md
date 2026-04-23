# Page And Window Reference

This page describes the exact JSON fields supported by:

- root window files
- page files
- static reticle instances inside pages
- inline reticles
- strobe definitions

## 1. Window JSON

A window JSON is the root file loaded by an application.
By convention, root window files live in `assets/windows` and page files live in
`assets/pages`.

Canonical example:

```json
{
  "title": "Tutorial Window",
  "size": [1280, 800],
  "position": [80, 60],
  "targetFps": 60,
  "fontFile": "../fonts/ocr_a.ttf",
  "iconFile": "../branding/mfdstudio_app_icon.png",
  "reticleLibraryFolder": "../reticles",
  "commands": {
    "udp": {
      "enabled": true,
      "address": "127.0.0.1",
      "port": 47300,
      "maxPacketSize": 16384
    }
  },
  "feedback": {
    "udp": {
      "enabled": true,
      "address": "127.0.0.1",
      "port": 47301,
      "maxPacketSize": 4096
    }
  },
  "defaultPage": "Radar",
  "pages": [
    "../pages/pfd.json",
    "../pages/radar.json"
  ]
}
```

### Window fields

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `title` | string | no | Window title. | none |
| `size` | array or object | recommended | Window size in pixels. | `dimensions`, `windowSize` |
| `width` | integer | no | Alternative direct width. | none |
| `height` | integer | no | Alternative direct height. | none |
| `position` | array or object | no | Initial screen position in pixels. | `windowPosition`, `screenPosition` |
| `x` | integer | no | Alternative direct X position. | none |
| `y` | integer | no | Alternative direct Y position. | none |
| `targetFps` | integer | no | Requested update rate. | `fps` |
| `fontFile` | string | no | Optional font file used by the host renderer for text primitives and page overlays. | `font`, `fontPath` |
| `iconFile` | string | no | Optional icon image used by the host window. In `mfd_window`, the same image is reused as a startup splash until the first client command arrives. | `icon`, `windowIcon`, `windowIconFile`, `iconPath` |
| `reticleLibraryFolder` | string | recommended | Folder containing reticle templates. | `reticles`, `reticleFolder` |
| `commands` | object | no | Command transport configuration. | `commandTransport`, `commandTransports` |
| `feedback` | object | no | Feedback transport configuration. | `feedbackTransport`, `feedbackTransports`, `strobeFeedback`, `events` |
| `defaultPage` | string | no | Page name opened by default when the window loads. | none |
| `pages` | array | yes | Page JSON files loaded by this window. | `pageFiles`, `pageJsons` |

Notes:

- `fontFile` is resolved relative to the root window JSON file
- `iconFile` is resolved relative to the root window JSON file
- the font choice is authoring-time configuration, not a runtime user command

\startuml
left to right direction
rectangle "window.json" as WindowJson
rectangle "fontFile" as FontFile
rectangle "iconFile" as IconFile
rectangle "mfd_window" as HostWindow
rectangle "Startup splash" as Splash
rectangle "Window taskbar/icon" as WindowIcon

WindowJson --> FontFile : resolve relative path
WindowJson --> IconFile : resolve relative path
IconFile --> HostWindow : load branding image
HostWindow --> Splash : reuse until first client command
HostWindow --> WindowIcon : apply as runtime window icon
\enduml

### `size` syntax

Array form:

```json
"size": [1280, 800]
```

Object form:

```json
"size": { "width": 1280, "height": 800 }
```

### `position` syntax

Array form:

```json
"position": [80, 60]
```

Object form:

```json
"position": { "x": 80, "y": 60 }
```

### `pages` syntax

The page list accepts:

- strings
- objects with `file`, `path`, or `json`

Example:

```json
"pages": [
  "../pages/navigation.json",
  { "file": "../pages/radar.json" }
]
```

Rules:

- if `defaultPage` is omitted, the runtime opens the first page in the list
- if `defaultPage` is provided, it must match one page name in the loaded window

## 2. UDP Command Transport

Canonical example:

```json
"commands": {
  "udp": {
    "enabled": true,
    "address": "127.0.0.1",
    "port": 47220,
    "maxPacketSize": 16384
  }
}
```

Fields:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `enabled` | bool | no | Enables the UDP command endpoint. | none |
| `address` | string | no | Bind or target address. | `bindAddress`, `host` |
| `port` | integer | recommended | UDP port. | `listenPort` |
| `maxPacketSize` | integer | no | Maximum protobuf UDP payload size. | `packetSize`, `bufferSize` |

## 3. UDP Feedback Transport

Canonical example:

```json
"feedback": {
  "udp": {
    "enabled": true,
    "address": "127.0.0.1",
    "port": 47221,
    "maxPacketSize": 4096
  }
}
```

Fields are identical to the command transport:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `enabled` | bool | no | Enables UDP feedback. | none |
| `address` | string | no | Bind or target address. | `bindAddress`, `host` |
| `port` | integer | recommended | UDP port. | `listenPort` |
| `maxPacketSize` | integer | no | Maximum protobuf UDP payload size. | `packetSize`, `bufferSize` |

## 4. Page JSON

Each page lives in its own JSON file.

Canonical example:

```json
{
  "name": "Radar",
  "title": "Radar",
  "backgroundColor": "#08131BFF",
  "blinkTypes": [
    { "name": "slow", "durationMs": 1000 },
    { "name": "fast", "durationMs": 320 },
    { "name": "caution", "durationMs": 1000 }
  ],
  "defaultBlink": "slow",
  "view": {
    "center": [0.0, 0.0],
    "zoom": 1.0
  },
  "staticReticles": [
    {
      "id": "grid",
      "template": "radar_grid",
      "blink": "fast"
    }
  ],
  "strobe": {
    "id": "radar_strobe",
    "template": "strobe_cursor",
    "position": [0.0, 0.0],
    "capture": {
      "shape": "circle",
      "radius": 0.10
    }
  }
}
```

### Page fields

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `name` | string | yes unless `id` is used | Public page name used by the API. | none |
| `id` | string | yes unless `name` is used | Alternative page identifier. | none |
| `title` | string | no | Human-readable page title. | none |
| `backgroundColor` | color | no | Page background color. | `background`, `bgColor`, `bg` |
| `blinkTypes` | array | no | Named blink types owned by this page. | `blinks` |
| `defaultBlink` | string | no | Default page blink type used when a reticle enables blink without naming a type. | `defaultBlinkType` |
| `view` | object | no | Page view settings. | none |
| `center` | vec2 | no | Page view center at top level. | `viewCenter`, `zoomCenter` |
| `x`, `y` | number | no | Alternative top-level page view center coordinates. | none |
| `zoom` | number | no | Top-level page zoom. | `zoomLevel` |
| `staticReticles` | array | no | Static reticles instantiated on the page. | none |
| `strobe` | object | no | Optional strobe definition. | none |

Notes:

- the page name must be unique within the window
- blink synchronization is done per page and by effective duration
- static reticle ids must be unique within the page
- the strobe id must also be unique within the page

## 5. `view` object

Canonical example:

```json
"view": {
  "center": [0.10, -0.05],
  "zoom": 1.5
}
```

Fields:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `center` | vec2 | no | Logical point centered in the viewport. | `viewCenter`, `zoomCenter` |
| `x` | number | no | Alternative center X. | none |
| `y` | number | no | Alternative center Y. | none |
| `zoom` | number | no | Page zoom factor. | `zoomLevel` |

Recommendation:

- prefer the nested `view` object
- keep page-level `center` and `zoom` only for compatibility or very short files

## 6. Page Blink Types

Blink is declared at page level.

Canonical example:

```json
"blinkTypes": [
  { "name": "slow", "durationMs": 1000 },
  { "name": "fast", "durationMs": 320 },
  { "name": "caution", "durationMs": 1000 }
],
"defaultBlink": "slow"
```

Each blink entry supports:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `name` | string | yes | Public blink type name used by the runtime API. | `id`, `type` |
| `durationMs` | integer | yes | Effective blink period in milliseconds. Must be strictly positive. | `periodMs`, `duration`, `period` |

Important note:

- type names are used by the API
- synchronization is done by effective duration
- two different names using the same duration blink in phase inside the same page

## 7. Static Reticle Instance In A Page

A page reticle entry can either:

- instantiate a library template
- define an inline reticle directly

### 7.1 Template instance

Canonical example:

```json
{
  "id": "status_clock",
  "template": "status_clock",
  "position": [0.62, 0.77],
  "stroke": "#EAD29BFF"
}
```

Fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | recommended | Runtime reticle id on the page. Must be unique in the page. |
| `template` | string | yes for template instance | Reticle template id from the library. |
| `label` | string | no | User-facing label override. |
| `name` | string | no | Alias source for `label` in info metadata. |
| `category` | string | no | User-facing category override. |
| `kind` | string | no | Alias of `category`. |
| `metadata` | object | no | Arbitrary key/value metadata. |
| `data` | object | no | Alias of `metadata`. |
| transform fields | various | no | Position, rotation, scale. |
| style fields | various | no | Stroke, thickness, fill, filled, visibility. |
| `style` | object | no | Nested style block. |
| `overrides` | object | no | Additional reticle-level style override block. |
| `blink` | bool, string, or object | no | Page-managed blink binding for this page instance. |
| `text` | string | no | Override the first text primitive. |
| `texts` | object | no | Override named text primitives by primitive id. |
| `letterSpacing` | number | no | Override the first text-like primitive spacing. |
| `letterSpacings` | object | no | Override named text-like primitive spacing. |

### Blink binding syntax

String shorthand:

```json
"blink": "fast"
```

Default blink shorthand:

```json
"blink": true
```

Object form:

```json
"blink": {
  "enabled": false,
  "type": "slow"
}
```

Rules:

- `true` means "blink using the page default"
- a string means "blink using this explicit page type"
- the object form can preserve a type while keeping the reticle currently disabled

### 7.2 Inline reticle

Canonical example:

```json
{
  "id": "inline_marker",
  "elements": [
    {
      "id": "shape",
      "type": "diamond",
      "size": 0.05
    },
    {
      "id": "label",
      "type": "text",
      "text": "M1",
      "position": [0.07, 0.0]
    }
  ]
}
```

Additional required field:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `elements` | array | yes for inline reticle | Primitive list composing the reticle. |

Notes:

- inline reticles use the same reticle-level transform and style override fields
- inline page reticles can also use `blink`
- each primitive inside `elements` follows the primitive reference

## 8. Strobe Definition

A page can expose one optional strobe.

The strobe object combines:

- a reticle definition
- capture parameters
- optional magnetization parameters

Canonical example:

```json
"strobe": {
  "id": "radar_strobe",
  "template": "strobe_cursor",
  "position": [0.0, 0.0],
  "capture": {
    "shape": "circle",
    "radius": 0.10
  },
  "magnet": {
    "enabled": true,
    "radius": 0.075,
    "strength": 1.0
  }
}
```

### Strobe reticle part

The strobe object supports the same reticle fields as a normal page reticle:

- `id`
- `template`
- `elements`
- transform fields
- style fields
- `blink`
- text overrides
- metadata fields

### `capture` object

Canonical example:

```json
"capture": {
  "shape": "rectangle",
  "size": [0.18, 0.12]
}
```

Fields:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `shape` | string | no | Capture shape. | none |
| `radius` | number | no | Radius for circular capture. | `captureRadius` |
| `size` | number or vec2 | no | Rectangle capture size. | `captureSize` |
| `width` | number | no | Rectangle capture width. | none |
| `height` | number | no | Rectangle capture height. | none |

Accepted `shape` values:

- `circle`
- `radius`
- `rectangle`
- `rect`
- `box`
- `square`

### `magnet` object

Canonical example:

```json
"magnet": {
  "enabled": true,
  "radius": 0.075,
  "strength": 0.85
}
```

Fields:

| Field | Type | Required | Description | Aliases |
| --- | --- | --- | --- | --- |
| `enabled` | bool | no | Enable magnetization. | none |
| `radius` | number | no | Attraction radius. | `magnetRadius`, `snapRadius`, `distance` |
| `strength` | number | no | Blend factor in `[0, 1]`. | `magnetStrength`, `snapStrength`, `blend` |

Accepted container aliases:

- `magnet`
- `magnetization`
- `aimantation`
- `snap`

Short form:

```json
"magnet": true
```

This enables magnetization with default parameters.

## 9. Recommended Authoring Style

The loader accepts many aliases for convenience, but the recommended authoring
style is:

- use `name` for pages
- use `title` for human-readable labels
- use a nested `view` object
- declare blink types at page level with `blinkTypes`
- use `defaultBlink` when one type should be the normal page default
- use `blink` only on page instances or on the page strobe, not in reticle templates
- use `staticReticles` for page instances
- use `template` for normal reuse
- use `capture` and `magnet` as nested objects inside `strobe`
- use canonical transform fields:
  `position`, `rotationDegrees`, `scale`
- use canonical style fields:
  `visible`, `stroke`, `fill`, `filled`, `thickness`

That keeps page files much easier to read and much easier to share.
