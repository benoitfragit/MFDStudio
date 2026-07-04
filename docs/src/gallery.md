# Gallery

A visual tour of the main pieces. Every image here already ships in the
repository under `docs/src/images/`; each links to the page that explains it.

## Runtime

![Runtime](./images/mfd_window_runtime_capture.png)

`mfd_window` running a shipped asset driven live over UDP. Reproduce the simple
demo with `Scripts\Start-MfdDemo.bat` and `demo_client`, or the load scenario
with `Scripts\Start-RadarLoad.bat` and `radar_load_client`.

-> [Runtime](./handbook/runtime.md) | [Getting Started Tutorial](./getting-started.md)

## Visual editor

![Editor](./images/editor/editor-showcase.png)

`mfd_editor` authoring one loaded page with the main workspace surfaces visible
at the same time. JSON stays the source of truth, but the editor is the fastest
way to inspect authored pages, reticles, layers, and page-local diagnostics.

The matching handbook page zooms in on the main zones:

- left sidebar: page tree, reticle library, filters, and quick actions
- center preview: selection, zoom, smart tools, fullscreen, and helper overlays
- helper panels: layer inspector, minimap, and validation problems
- right inspector: page, reticle, and primitive settings

-> [Editor](./handbook/editor.md)

## Demo Client

![Demo client](./images/demo_client.png)

`demo_client`, the shipped GUI client. It loads a window JSON for discovery and
sends live commands to the runtime, which makes it a good reference client.

-> [Quick Start](./quickstart.md) | [Generated Client API](./handbook/generated_api.md)

## Object model

![Object model](./images/mfd_object_model.svg)

How a window owns pages, a page owns layers, a layer holds reticle instances, and
a reticle is built from primitives.

-> [Concepts](./concepts.md) | [Pages And Windows](./reference/pages_and_windows.md)

## Runtime round-trip

![Runtime round-trip](./images/mfd_runtime_roundtrip.svg)

The authored-assets -> runtime -> client commands -> render -> feedback loop that
ties the whole system together.

-> [Concepts](./concepts.md) | [Public API Contract](./reference/public_contract.md)
