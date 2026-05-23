# Create Pages And Windows

This tutorial shows how to:

- create a page JSON file
- create a window JSON file
- connect them together

If you prefer visual authoring first, start with the optional editor bootstrap in **Step 0**.

For the exact page and window field reference, see:

- [Common JSON Syntax](../reference/common_json_syntax.md)
- [Page And Window Reference](../reference/page_and_window_reference.md)

## At A Glance

\startuml
top to bottom direction
rectangle "Window JSON" as WindowJson
rectangle "Page file 1" as Page1
rectangle "Page file 2" as Page2
rectangle "Page file N" as PageN
rectangle "Reticle library folder" as ReticleLibrary

WindowJson --> Page1
WindowJson --> Page2
WindowJson --> PageN
WindowJson --> ReticleLibrary
\enduml

## Step 0 - (Optional) Bootstrap from `mfd_editor`

You can create the same window shell directly in the editor UI before editing JSON manually:

1. Open `mfd_editor`.
2. The editor starts empty. Use **File > Open window asset...** to browse an existing window JSON, or continue with **File > New window from scratch**.
3. Set window file, size, position, font, default page, and UDP transports.
4. Click **Create window**.
5. Click **File > Save**.

Then continue below to understand the exact JSON fields produced by that workflow.

## Step 1 - Create a page file

Each page lives in its own JSON file inside `assets/pages`.

Create `assets/pages/tutorial_page.json`:

```json
{
  "name": "Tutorial",
  "title": "Tutorial Page",
  "backgroundColor": "#08131BFF",
  "view": {
    "center": [0.0, 0.0],
    "zoom": 1.0
  },
  "staticReticles": [
    {
      "id": "aircraft",
      "template": "aircraft_symbol",
      "stroke": "hud",
      "thickness": 0.0042
    },
    {
      "id": "status",
      "template": "status_clock",
      "position": { "x": 0.625, "y": 0.7708 },
      "stroke": "#EAD29BFF"
    }
  ]
}
```

## Step 2 - Understand the page fields

Important page fields are:

- `name`
- `title`
- optional `titleDisplay`
- `backgroundColor`
- `view.center`
- `view.zoom`
- `staticReticles`
- optional `activeStrobe`
- optional `strobes`

`name` is the page identifier used by the runtime and the client API.

Useful page-reticle details:

- `staticReticles[].drawOnTop` keeps one reticle in a later overlay pass
- image primitive file paths are resolved relative to the page or reticle JSON file that references them

`titleDisplay` is the authored chrome of the page title. It can hide the title,
move it, scale it, recolor it, and choose whether the title uses no decoration,
one underline, or one frame.

## Step 3 - Create a window file

The root window JSON defines:

- window title
- window size in pixels
- window screen position
- optional text font file for text/time primitives and page overlays
- optional window icon used by the runtime host window
- UDP command transport
- optional UDP feedback transport
- list of page files
- optional default page name

Create `assets/windows/tutorial_window.json`:

```json
{
  "title": "Tutorial Window",
  "size": [1280, 800],
  "position": [80, 60],
  "targetFps": 60,
  "fontFile": "../fonts/ocr_a.ttf",
  "iconFile": "../../branding/mfdstudio_app_icon.png",
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
  "pages": [
    "../pages/tutorial_page.json"
  ]
}
```

## Step 4 - Reuse templates across pages

One library reticle can be reused many times.

For example, the same `status_clock` can appear in:

- `tutorial_page.json`
- `navigation.json`
- any future page file

That is why the template library lives at the window level, not inside one
page.

## Step 5 - Load the window from C++

Use `mfd::JsonLoader`:

```cpp
#include "mfd/io/JsonLoader.h"

mfd::JsonLoader loader;
const mfd::LoadedWindowConfiguration loaded =
    loader.LoadWindowConfiguration("assets/windows/tutorial_window.json");
```

`loaded.window` gives you:

- title
- size
- position
- optional font file
- UDP transport settings

Keep those UDP addresses on `127.0.0.1` when the client and the window run on
the same machine. Both command and feedback `maxPacketSize` values must stay in
the supported `[64, 65507]` range.

`loaded.document` gives you:

- reticle library
- pages

If a window contains several pages, mark the startup page directly in the
window JSON:

```json
"defaultPage": "Radar",
"pages": [
  "../pages/pfd.json",
  "../pages/radar.json"
]
```

If `defaultPage` is omitted, the runtime starts on the first page.

## Step 6 - Activate the page in a runtime scene

```cpp
#include "mfd/runtime/SceneRegistry.h"

mfd::SceneRegistry scene(loaded.document);
scene.SetActivePage("Tutorial");
```

## Step 7 - Optional: add one or more strobes

If a page needs cursor or designation behavior, prefer the explicit catalog
form:

```json
"activeStrobe": "Default",
"strobes": [
  {
    "name": "Default",
    "id": "tutorial_strobe_default",
    "template": "strobe_cursor",
    "position": { "x": 0.0, "y": 0.0 },
    "capture": {
      "shape": "circle",
      "radius": 0.10
    },
    "magnet": {
      "enabled": true,
      "radius": 0.075,
      "strength": 1.0
    }
  },
  {
    "name": "Designator",
    "id": "tutorial_strobe_designator",
    "template": "designator_cursor",
    "position": { "x": 0.0, "y": 0.0 },
    "capture": {
      "shape": "rectangle",
      "size": [0.24, 0.16]
    }
  }
]
```

If you only need one strobe, the legacy singular `strobe` object is still
accepted and is loaded as one catalog containing a single `Default` entry.

## What You Should See

Once the window JSON is loaded by an application:

- the window opens with the configured title
- the configured page exists in the page list
- its reticles are visible when the page is active
- zoom and center affect the whole page, not one reticle

Common mistakes:

- placing reticle templates in the page folder instead of the library folder
- using a page `name` that does not match the client commands
- using pixel values instead of normalized `[-1, 1]` coordinates

## Result

You now have:

- one reusable reticle library
- one page file
- one root window file that loads the page
