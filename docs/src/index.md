# MFDStudio Documentation

`MFDStudio` is a C++17 / CMake toolkit for authoring and running 2D
multi-function display (MFD) windows from JSON.

The historical technical prefix `mfd` remains in namespaces, targets, folders,
and APIs.

![Cockpit runtime screenshot](./images/mfd_window_cockpit_capture.png)

## Pick Your Path

You do not need to read this book linearly. Most users only need one path first.

| Goal | Start here |
| --- | --- |
| Launch a window and see something live | [Quick Start](./quickstart.md) |
| Understand the JSON model | [Concepts](./concepts.md) |
| Drive a window from a generated C++ client | [Generated Client API](./handbook/generated_api.md) |
| Embed the runtime offscreen in your app | [Offscreen Embedding](./handbook/offscreen.md) |
| Capture raw runtime pixels | [Framebuffer Capture](./handbook/framebuffer.md) |
| Author assets visually | [Editor](./handbook/editor.md) |
| Look up exact JSON fields | [JSON Syntax](./reference/json.md) |
| Build, test, or contribute | [Build](./dev/build.md) |
| Browse the public C++ headers | [C++ API Reference](./api.md) |

## Main Applications

| Entry point | Purpose |
| --- | --- |
| `mfd_window` | Runtime host that loads one window JSON file |
| `client_mockup` | Live UDP client to validate pages, reticles, blink, strobe, feedback |
| `mfd_editor` | Visual authoring tool for windows, pages, and reticles |
| `offscreen_viewer` | Example embedding `mfd_runtime_api` and displaying offscreen images |
| `mfd_framebuffer_stdout_plugin` | Sample framebuffer plugin for the `mfd_window` capture ABI |

## Where This Book Lives

- Source pages: `docs/src/`
- Published site: `build/docs/site`
- C++ API reference (Doxygen): [`/api/`](./api.md)
