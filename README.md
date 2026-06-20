# MFDStudio

`MFDStudio` is a C++17 / CMake toolkit for authoring and running 2D
multi-function display windows from JSON.

The historical technical prefix remains `mfd` in namespaces, targets, folders,
and APIs.

![Cockpit runtime screenshot](./docs/images/mfd_window_cockpit_capture.png)

## Build

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

## Run

```powershell
.\Scripts\Start-MfdDemo.bat
```

Then launch `client_mockup`, activate a page, and move a reticle. Press `F1` in
`mfd_window` for the runtime debug overlay.

## Documentation

The main documentation is an mdBook portal under [`docs/src/`](./docs/src/index.md).

Build it locally:

```powershell
pwsh -File docs/BuildDocsSite.ps1 -Version local
```

The combined site (mdBook portal plus the Doxygen C++ API under `/api/`) is
published to GitHub Pages from `build/docs/site`.

## Where To Go Next

| Goal | Page |
| --- | --- |
| First live window | [Quick Start](./docs/src/quickstart.md) |
| Understand the JSON model | [Concepts](./docs/src/concepts.md) |
| Drive a window from a generated C++ client | [Generated Client API](./docs/src/handbook/generated_api.md) |
| Embed the runtime offscreen | [Offscreen Embedding](./docs/src/handbook/offscreen.md) |
| Capture raw pixels | [Framebuffer Capture](./docs/src/handbook/framebuffer.md) |
| Author assets visually | [Editor](./docs/src/handbook/editor.md) |
| Exact JSON fields | [JSON Reference](./docs/src/reference/json.md) |
| Build, test, contribute | [Build](./docs/src/dev/build.md) |
| Browse the public C++ headers | [C++ API Reference](./docs/src/api.md) |

## Licenses

- project license: [LICENSE](./LICENSE)
- third-party notices: [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)
