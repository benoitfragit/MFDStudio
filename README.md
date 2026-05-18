# MFDStudio

`MFDStudio` is a C++17 / CMake toolkit for authoring and operating 2D
multi-function display windows from JSON.

The historical technical prefix remains `mfd` in namespaces, targets, folders,
and APIs.

![Cockpit runtime screenshot](./docs/images/mfd_window_cockpit_capture.png)

## What You Can Do

- author windows, pages, and reticle libraries in JSON
- edit those assets visually in `mfd_editor`
- run them in `mfd_window`
- drive the live runtime from `client_mockup` or your own UDP client
- capture the rendered window through a stable `RGBA32` framebuffer plugin ABI
- generate typed C++ client helpers for one authored window
- inspect the live runtime with the integrated `F1` debug overlay, including UDP pressure diagnostics

## Start With The Right Page

| Goal | Read first | Then |
| --- | --- | --- |
| See a working demo quickly | [Quick Start](./docs/QUICKSTART.md) | [Test A Window With The Mockup](./docs/tutorials/03_test_with_mfd_mockup.md) |
| Learn the product vocabulary | [Core Concepts](./docs/CONCEPTS.md) | [Tutorial Index](./docs/tutorials/README.md) |
| Author assets in JSON | [Core Concepts](./docs/CONCEPTS.md) | [JSON Reference](./docs/reference/README.md) |
| Work mainly in the editor | [Quick Start](./docs/QUICKSTART.md) | [Create A Window From Scratch In `mfd_editor`](./docs/tutorials/13_create_window_from_editor.md) |
| Integrate a live client | [Quick Start](./docs/QUICKSTART.md) | [Drive A Window From A Live Client](./docs/tutorials/04_drive_a_window_from_a_live_client.md) |
| Contribute to the repository | [Development Guide](./docs/DEVELOPMENT.md) | [MFDStudio C++ Repository Maintenance Standard](./docs/standards/mfd_cpp_repository_maintenance_standard.md) |

If you are not sure where to go, start with the
[Documentation Guide](./docs/README.md).

If you need one long-form user manual covering the editor, the generated API,
the framebuffer plugin, and launch scripts in one place, use the
[Detailed User Guide (.docx)](./docs/user_guide/MFDStudio_End_To_End_User_Guide.docx).

## Five-Minute First Run

Build the minimum useful set on Windows:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup mfd_editor
```

Then:

1. launch `.\Scripts\Start-MfdDemo.bat`
2. launch `client_mockup`
3. activate one page and edit one reticle
4. press `F1` in `mfd_window`

For the guided version, use [Quick Start](./docs/QUICKSTART.md).

## Runtime Mental Model

![Runtime roundtrip](./docs/images/mfd_runtime_roundtrip.svg)

- JSON assets define what exists
- `mfd_window` renders the active page
- a client sends live commands over UDP
- the runtime can send feedback back to the client

## Main Applications

| Entry point | Purpose |
| --- | --- |
| `mfd_window` | Generic runtime launcher that loads one window JSON file |
| `client_mockup` | Live UDP client for pages, reticles, strobe, feedback, and stress tests |
| `mfd_editor` | Visual authoring tool for windows, pages, and reticles |
| `mfd_framebuffer_stdout_plugin` | Sample framebuffer plugin implementing the stable capture ABI |

## Documentation Shelves

Use the documentation tree by intent instead of reading it linearly:

- [docs/QUICKSTART.md](./docs/QUICKSTART.md): fastest path to a visible result
- [docs/CONCEPTS.md](./docs/CONCEPTS.md): minimum vocabulary
- [docs/tutorials](./docs/tutorials/README.md): guided workflows
- [docs/reference](./docs/reference/README.md): exact JSON and authoring rules
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md): build, test, packaging, and contributor workflow
- [docs/standards](./docs/standards/README.md): repository and interoperability standards

For a first product session, start with `Quick Start` and `Core Concepts`.
The standards pages are normative references and are not required to get a
window running.

Useful companion pages:

- [FAQ](./docs/FAQ.md)
- [What's New](./docs/WHATS_NEW.md)

## Build And Test

For day-to-day work on Windows, prefer the Win32 debug flow:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
ctest --preset test-debug-win32
```

Generate delivery packages with:

```powershell
.\Scripts\BuildDeliveries.bat
```

Contributor-oriented details live in
[Development Guide](./docs/DEVELOPMENT.md).

## Repository Layout

| Path | Role |
| --- | --- |
| `assets` | Window, page, reticle, and image assets |
| `docs` | Onboarding, tutorials, reference, standards, and architecture notes |
| `examples` | Example clients and runtime plugin samples |
| `mfd_common_api` | Shared low-level authored-model and transport layer |
| `mfd_api` | JSON loading, runtime, and public low-level API |
| `mfd_client_api` | Higher-level client helpers and generated client integration |
| `mfd_editor` | Editor application |
| `mfd_window` | Runtime host |
| `tests` | Automated test suites |

## Licenses

- project license: [LICENSE](./LICENSE)
- third-party notices: [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)
