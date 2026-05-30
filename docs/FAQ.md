# FAQ

This page answers the practical questions that usually come up during the
first sessions with the project.

## Which executable should I launch first?

Use:

- `mfd_window` to run one authored window
- `client_mockup` to drive that window live over UDP
- `mfd_editor` to create or edit assets visually

If you want a ready-to-run session, start with:

- `.\Scripts\Start-MfdDemo.bat`
- then `client_mockup`

## Which document should I read first?

The shortest useful path is:

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. the path matching your role in [Documentation Guide](./README.md)

## Do I need to write JSON by hand?

No.

You can:

- author directly in JSON
- use `mfd_editor`
- mix both approaches

The editor is the easiest way to discover the model, but the JSON files remain
the source of truth.

## Are page coordinates pixels?

No.

The authored and runtime coordinate system is normalized in `[-1, 1]`.

Read [Core Concepts](./CONCEPTS.md) before debugging placement issues.

## What is the difference between a reticle template and a page reticle?

- a reticle template is the reusable definition stored in the library
- a page reticle is one instance of that template placed on one page

This distinction matters everywhere:

- in JSON authoring
- in the editor
- in client-side runtime updates

## Should I use the generated client API or raw `CommandClient` calls?

Use the generated client API first whenever your application targets one known
authored window.

Use raw name-based `CommandClient` helpers only when you need:

- generic tooling
- migration code
- low-level debugging

## Do I need the `.generated.map` when I use generated C++ bindings?

Yes.

Treat `Ui.h`, `Ui.cpp`, and `<window>.generated.map` as one generated contract:

- generation now expects an explicit `OUTPUT_MAP`
- the client uses the map for raw-name fallback helpers
- `mfd_window` must load the matching map to accept generated id-based batches

If the generated C++ and the generated map drift apart, or if the runtime did
not load the matching sidecar, generated batches are rejected.

## What do generated `ui.Run()` and `ui.Initialize()` actually do?

Generated-root `Run()` starts one new local client cycle. It drops staged dirty
state without asking the runtime to go back to the authored baseline.

Generated-root `Initialize()` is the user-facing authored-state reset.

It:

- restores the local generated UI baseline to the authored window/page/strobe state
- invalidates previously created generated dynamic-reticle handles
- clears cached runtime feedback state
- makes the next built batch prepend one runtime `ResetWindowCommand`

Call `Initialize()` when you explicitly want that full runtime reinitialization.
Call `Run()` for the normal per-cycle housekeeping path.

## Does `client_mockup` share memory with `mfd_window`?

No.

`client_mockup` is a normal standalone UDP client. It loads the same window
JSON locally for discovery, then sends public commands to the runtime.

That is exactly why it is a good reference implementation.

## Can I use the project without the editor?

Yes.

The editor is optional. The core workflow is still:

1. author assets
2. load them in `mfd_window`
3. drive them from a client

## Can I use the editor without auto-loading staged `_Exec` assets?

Yes.

The editor starts empty by design. Open a source window JSON or create a new
window from scratch.

This avoids accidentally editing staged runtime copies instead of the real
repository assets.

## Which Windows build preset should I use for day-to-day work?

Prefer Win32 debug first:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

That is the recommended default for local debugging in this repository.

## What is the fastest way to check that UDP control works?

Use `client_mockup` and start with the `Window display` controls:

- toggle `Invert colors`
- change `Brightness`
- click `Send window display`

If the runtime reacts, the transport path is alive.

## Where can I inspect the live runtime state?

Inside `mfd_window`, press `F1`.

The integrated runtime debug overlay lets you inspect:

- active page
- reticle tree
- transport state
- temporary local bypasses

## How do I capture the framebuffer?

Two common paths exist:

- use the sample framebuffer plugin flow described in [Capture The Window As Raw Pixels](./tutorials/07_framebuffer_rgba32_capture.md)
- use the runtime/plugin integration points from `mfd_window_plugin_api`

## Where are the screenshots and diagrams stored?

Under [docs/images](./images/README.md).

The repo now versions:

- PlantUML sources
- rendered SVG diagrams
- runtime/editor/client screenshots

## Where are the deep architecture notes?

Use:

- [Architecture Notes](./architecture/README.md)
- [Generated Transport Map Specification](./architecture/generated_transport_map.md)

Do not start there unless you already know the normal workflow.
