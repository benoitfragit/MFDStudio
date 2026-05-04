# What's New

This page summarizes the highlights of the current repository state.

It is intentionally not a full commit-by-commit changelog.
It answers a simpler question:

- if you already knew the project before, what is worth noticing now?

## At A Glance

The current tree is stronger in four areas:

- clearer client-facing workflows
- deeper editor authoring workflows
- better runtime inspection and capture hooks
- better documentation structure and visual onboarding

## For People Using The Product

### The Runtime Is Easier To Inspect

`mfd_window` includes an integrated `F1` runtime debug overlay.

You can inspect:

- transport health
- active page
- reticle tree
- temporary local bypasses

This makes it much easier to separate authored-data problems from live-command
problems.

### The Cockpit And Demo Flows Are More Useful

The repository ships clearer launch paths through:

- `Start-MfdDemo.bat`
- `Start-MfdCockpit.bat`
- `Start-MfdMinimal.bat`

The demo set is now a much better onboarding surface for operators and
integrators.

### Framebuffer Capture Is Part Of The Story

The runtime supports framebuffer plugin flows, and the repository includes a
sample plugin plus dedicated documentation and examples around RGBA capture.

## For Client Integrators

### The Generated Client API Is The Preferred Surface

The documentation and repository now push one consistent rule:

- generated page / reticle / primitive handles first
- `CommandClient` as the final send boundary

This makes the public usage model cleaner and reduces the need to manipulate
raw names manually.

### `client_mockup` Is A Better Reference Client

`client_mockup` is a standalone Win32 + Dear ImGui + DX11 client.

It is useful not just as a test tool, but as a concrete example for:

- page activation
- reticle patches
- dynamic reticles
- strobe commands and feedback
- cockpit and radar simulations

### The Transport Story Is Clearer

The current doc set is much more explicit about:

- command UDP
- optional feedback UDP
- generated transport maps
- when to use generated helpers versus low-level calls

## For Editor Users

### The Editor Workflow Is Much Richer

`mfd_editor` now covers far more than basic asset creation.

The documented workflow includes:

- guided creation of a new window
- page import with dependency staging
- safe page rename across source assets
- safe reticle rename across pages
- reticle usage highlighting
- layer inspector
- full-width problems panel
- multi-selection and drag workflows

### The Editor Is Safer About Source Versus Staged Assets

The current workflow is explicit about staying in the real source `assets/`
tree instead of accidentally editing staged `_Exec` copies.

## For Contributors

### The Internal Layering Is Cleaner

The repository now exposes the internal split more clearly:

- `mfd_common_api` for shared low-level model and transport code
- `mfd_api` for JSON loading, runtime, and the low-level public API
- `mfd_client_api` for higher-level client helpers

This reduces confusion between authored data, transport rules, runtime logic,
and host rendering concerns.

### Build And Packaging Flows Are Better Documented

The docs now explain more clearly:

- the Win32 debug-first workflow
- staged runtime outputs under `_Exec`
- package staging under `_Deliveries`
- the role of the shipped launcher scripts

## For Documentation Readers

### The Entry Points Are Less Dense

The top-level documentation has been reorganized to be more usable:

- lighter `README`
- clearer documentation hub
- more direct `Quick Start`
- simplified `Core Concepts`
- grouped tutorial index

### The Docs Are Now More Visual

The repository now includes versioned visual assets:

- PlantUML diagrams committed as source
- rendered SVG diagrams for GitHub and Doxygen
- real screenshots of `mfd_window`, `client_mockup`, and `mfd_editor`

## Good Follow-Up Pages

- [FAQ](./FAQ.md)
- [Documentation Guide](./README.md)
- [Quick Start](./QUICKSTART.md)
- [Architecture Notes](./architecture/README.md)
