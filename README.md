# MFDStudio

`MFDStudio` is a C++17/CMake toolkit for authoring and operating 2D
multi-function display windows from JSON.

In the codebase, the historical technical prefix remains `mfd`
(namespaces, targets, folders, and APIs).

It is built for projects that need:

- a data-driven display model
- reusable reticle templates
- a real-time runtime API
- an integrated runtime debug overlay in `mfd_window`
- local scene control or remote control over UDP
- optional typed client-side API generation
- optional strobe feedback and framebuffer capture
- example applications, editor tooling, and automated tests

## New Here

Use the entry point that matches your goal:

| Goal | Start here |
| --- | --- |
| I want to see something working quickly | [Quick Start](./docs/QUICKSTART.md) |
| I want the documentation map first | [Documentation Guide](./docs/README.md) |
| I want to work on the repository itself | [Development Guide](./docs/DEVELOPMENT.md) |
| I need the project vocabulary | [Core Concepts](./docs/CONCEPTS.md) |
| I need exact JSON fields and syntax | [JSON Reference](./docs/reference/README.md) |
| I need the generated client API standard | [Generated Client API Standardization](./docs/standards/mfd_generated_client_api_standardization.md) |
| I need the third-party client replacement contract | [Interoperability Standards](./docs/standards/README.md) |
| I want step-by-step workflows | [Tutorial Index](./docs/tutorials/README.md) |

## First 10 Minutes

Requirements:

- Visual Studio 2022
- CMake 3.26 or newer
- Python 3

Recommended first build on Windows:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

Build defaults worth knowing:

- the shipped presets target Visual Studio/MSVC on Windows
- `POSITION_INDEPENDENT_CODE` is not forced on Windows by default
- the MSVC CRT runtime now follows the toolchain default unless you override it explicitly with `-DMFD_MSVC_RUNTIME=dll` or `-DMFD_MSVC_RUNTIME=static`

Then:

1. launch `.\Start-MfdDemo.bat`
2. launch `client_mockup`
3. activate the `Radar` page
4. move or recolor one reticle from the mockup
5. press `F1` in `mfd_window` to inspect the live runtime and test local bypasses

If you want the full walkthrough, read [Quick Start](./docs/QUICKSTART.md).
If you want the full build, test, and target reference, read
[Development Guide](./docs/DEVELOPMENT.md).

The pull request CI, release packaging, and hosted Doxygen publication
workflows all run on Windows GitHub runners. Every published GitHub release can
also republish the generated Doxygen API reference to GitHub Pages once the
repository Pages source is configured to use GitHub Actions.

## What The Project Does

The normal workflow is:

1. describe a window, its pages, and reusable reticles in JSON
2. launch a runtime window that renders the active page
3. drive that window locally or from an external client
4. optionally receive strobe feedback or capture the framebuffer

In practice:

- JSON files define what exists
- the runtime owns the live scene state
- clients send runtime updates through typed commands
- the renderer draws only the active page

Authoring covers text primitives, image primitives, and geometric primitives
such as line, circle, ring, rectangle, ellipse, square, diamond, triangle,
polyline, bezier, and arc.

On the wire, fixed authored objects now travel through transport IDs plus
`mappingHash` only. Generated client APIs hide those IDs completely, including
page-level calls such as `client.ActivatePage(ui.Radar())` and
`client.SetPageView(ui.Radar(), center, zoom)`, which carry both the generated
page ID and generated `mappingHash`. This generated path is the normal client
API. Raw `CommandClient` helpers can still accept page, reticle, template,
blink, and primitive names, but they are now the fallback path for tooling or
migration only, and they must be constructed with the companion generated
transport map so those names are resolved locally before serialization.

## Choose Your Path

### I Want To Author JSON Assets

Start with:

1. [Core Concepts](./docs/CONCEPTS.md)
2. [Create Reticles From Primitives](./docs/tutorials/01_create_reticles_from_primitives.md)
3. [Create Pages And Windows](./docs/tutorials/02_create_pages_and_windows.md)
4. [JSON Reference](./docs/reference/README.md)

### I Want To Integrate An External Client

Start with:

1. [Quick Start](./docs/QUICKSTART.md)
2. [Drive A Window From A Live Client](./docs/tutorials/04_drive_a_window_from_a_live_client.md)
3. [Add And Remove Dynamic Reticles](./docs/tutorials/05_dynamic_reticles.md)
4. [Use The Mockup As A Client API Reference](./docs/tutorials/11_use_the_mockup_as_a_client_api_reference.md)

The recommended end-user path is: generated UI accessors for pages, reticles,
primitives, and dynamic sets, then `CommandClient` only for the final send.

### I Want To Use The Generated Client API

Start with:

1. [Use The Mockup As A Client API Reference](./docs/tutorials/11_use_the_mockup_as_a_client_api_reference.md)
2. [Generated Client API Architecture](./docs/architecture/generated_client_api.md)
3. [Generated Client API Standardization](./docs/standards/mfd_generated_client_api_standardization.md)
4. [Documentation Guide](./docs/README.md)

This is the preferred client-facing API surface.

### I Want To Use The Editor

Start with:

1. [Quick Start](./docs/QUICKSTART.md)
2. [Create A Window From Scratch In `mfd_editor`](./docs/tutorials/13_create_window_from_editor.md)
3. [Test A Window With The Mockup](./docs/tutorials/03_test_with_mfd_mockup.md)

Editor page-preview essentials:

- `Ctrl+click` adds or removes one page reticle from the current selection
- `Esc` clears the current page-reticle selection
- dragging one selected page reticle moves the whole selected group together
- `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, and `Del` operate on the current page-reticle selection
- right-click lists every hovered reticle, then exposes per-reticle clipping submenus so overlapping reticles stay reachable
- `Undo` restores one whole paste, cut, delete, or drag gesture at a time
- a small `?` button is shown at the top-left of the page preview and reticle studio, with the current zoom and mouse `X/Y` displayed next to it
- the reticle studio now exposes a top-right `View` menu to show or hide the page context, primitive names, and gizmos

### I Want To Work On The Codebase

Start with:

1. [Development Guide](./docs/DEVELOPMENT.md)
2. [Core Concepts](./docs/CONCEPTS.md)
3. [Run The Automated Runtime Tests](./docs/tutorials/12_run_the_automated_runtime_tests.md)

The automated suite covers the low-level runtime API, the higher-level
`client_api`, and the generated client API path, including compiled fixture
coverage and generator validation.

## Shipped Tools And Entry Points

| Entry point | Purpose |
| --- | --- |
| `mfd_window` | Generic runtime launcher that accepts a window JSON file |
| `Start-MfdDemo.bat` | Root and staged launcher opening `assets/windows/demo_pages.json` |
| `Start-MfdCockpit.bat` | Root and staged launcher opening `assets/windows/demo_pages_cockpit.json` |
| `Start-MfdMinimal.bat` | Root and staged launcher opening `assets/windows/demo_pages_minimal.json` with the sample framebuffer plugin |
| `Start-MfdTutorial.bat` | Root and staged launcher for the tutorial window once `assets/windows/mfd_tutorial.json` has been authored; also passes the sample framebuffer plugin |
| `mfd_framebuffer_stdout_plugin` | Sample DLL exporting one framebuffer callback for `mfd_window --framebuffer-plugin` |
| `client_mockup` | Interactive GUI client for page control, reticle updates, dynamic reticles, and feedback inspection |
| `client_mockup_minimal` | Minimal plain-loop client for the cockpit showcase |
| `client_tutorial` | Tutorial-specific client demonstrating the generated API on `mfd_tutorial.json`, including Page2 progress-bar animation through an exposed primitive; configured only once the tutorial assets exist |
| `mfd_editor` | Visual authoring tool for windows, pages, and reticles |

The repository also ships inspired sample assets under `assets/windows` and
`assets/pages`, including the `PictureDemo` page backed by bitmap files stored
under `assets/picture`. Open them with `mfd_window`, one of the root
`Start-Mfd*.bat` scripts, or through `mfd_editor`.

Inside `mfd_window`, press `F1` to open the integrated runtime debug overlay.
It can display transport health, the active page, the current reticle tree, and
temporary local bypasses without changing the client API.

`Start-MfdTutorial.bat` and `client_tutorial` are intentionally gated behind the
editor tutorial assets. Until `assets/windows/mfd_tutorial.json` and its page /
reticle companions exist, the script exits with a clear message and CMake skips
`client_tutorial`.

## Repository Layout

| Path | Role |
| --- | --- |
| `mfd_api` | Core runtime library and public API |
| `client_api` | Higher-level client helpers |
| `client_api_generator` | Typed client API generator |
| `mfd_window` | Generic window host |
| `mfd_editor` | Visual authoring application |
| `examples` | Example clients and sample plugins |
| `assets` | Window, page, and reticle JSON assets |
| `docs` | Onboarding, concepts, tutorials, reference, and architecture notes |

For the contributor-oriented build, test, and layout view, see
[Development Guide](./docs/DEVELOPMENT.md).

## Documentation

Documentation is intentionally split by job:

| Page | Use it when |
| --- | --- |
| [Documentation Guide](./docs/README.md) | you want the shortest reading path for your goal |
| [Quick Start](./docs/QUICKSTART.md) | you want a visible end-to-end result fast |
| [Core Concepts](./docs/CONCEPTS.md) | you need the project vocabulary and mental model |
| [Tutorial Index](./docs/tutorials/README.md) | you want step-by-step workflows |
| [JSON Reference](./docs/reference/README.md) | you need exact authoring fields and syntax |
| [Development Guide](./docs/DEVELOPMENT.md) | you are building, testing, or contributing to the repo |
| [Architecture Notes](./docs/architecture/README.md) | you need advanced design details for generated APIs or transport maps |
| [Interoperability Standards](./docs/standards/README.md) | you want the generated client API standard, the formal replacement-client contract, and the conformance target |

The public headers in `mfd_api/include/mfd` are also documented with Doxygen
using `@brief`, `@param`, `@return`, and `@note`.

The generated HTML API portal uses the official `doxygen-awesome-css` theme
with a small MFDStudio-specific override layer instead of the previous large
custom stylesheet.

On Windows, `docs/GenerateDocs.ps1` also auto-detects a Java 11+ runtime for
PlantUML instead of assuming that the first `java` found in `PATH` is recent
enough.

## Third-Party Licenses

An inventory of external dependencies and copied license texts is available in
[THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md).
