# Gallery

A visual tour of the main pieces. Every image here already ships in the
repository under `docs/images/`; each links to the page that explains it.

## Cockpit runtime

![Cockpit runtime](./images/mfd_window_cockpit_capture.png)

`mfd_window` running the cockpit demo: ADI, HUD, and radar panels driven live
over UDP. Reproduce it with `Scripts\Start-MfdCockpit.bat` and the
`client_mockup_minimal` headless client.

→ [Runtime](./handbook/runtime.md) · [Getting Started Tutorial](./getting-started.md)

## Visual editor

![Editor](./images/mfd_editor_capture.png)

`mfd_editor` authoring windows, pages, and reticles. Optional — JSON stays the
source of truth — but useful for laying out reticles and pages visually.

→ [Editor](./handbook/editor.md)

## Client mockup

![Client mockup](./images/client_mockup_demo.png)

`client_mockup`, the shipped GUI client. It loads a window JSON for discovery and
sends live commands to the runtime, which makes it a good reference client.

→ [Quick Start](./quickstart.md) · [Generated Client API](./handbook/generated_api.md)

## Object model

![Object model](./images/mfd_object_model.svg)

How a window owns pages, a page owns layers, a layer holds reticle instances, and
a reticle is built from primitives.

→ [Concepts](./concepts.md) · [Pages And Windows](./reference/pages_and_windows.md)

## Runtime round-trip

![Runtime round-trip](./images/mfd_runtime_roundtrip.svg)

The authored-assets → runtime → client-commands → render → feedback loop that
ties the whole system together.

→ [Concepts](./concepts.md) · [Public API Contract](./reference/public_contract.md)
