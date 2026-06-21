# MFDStudio

`MFDStudio` is a C++17 / CMake toolkit for authoring and running 2D
multi-function display (MFD) windows from JSON.

The historical technical prefix `mfd` remains in namespaces, targets, folders,
and APIs.

![Cockpit runtime screenshot](./docs/images/mfd_window_cockpit_capture.png)

## Documentation

The full documentation is published as a site, combining a guide portal (mdBook)
and the C++ API reference (Doxygen):

**➡️ https://benoitfragit.github.io/MFDStudio**

| Step | Page |
| --- | --- |
| 1. First live window | [Quick Start](https://benoitfragit.github.io/MFDStudio/quickstart.html) |
| 2. End-to-end walkthrough | [Getting Started Tutorial](https://benoitfragit.github.io/MFDStudio/getting-started.html) |
| 3. The JSON model | [Concepts](https://benoitfragit.github.io/MFDStudio/concepts.html) |
| 4. Drive from typed C++ | [Generated Client API](https://benoitfragit.github.io/MFDStudio/handbook/generated_api.html) |
| 5. Run the runtime | [Runtime](https://benoitfragit.github.io/MFDStudio/handbook/runtime.html) |
| 6. Embed offscreen | [Offscreen Embedding](https://benoitfragit.github.io/MFDStudio/handbook/offscreen.html) |
| 7. Build and contribute | [Development](https://benoitfragit.github.io/MFDStudio/dev/build.html) |
| 8. Browse the public headers | [C++ API Reference (Doxygen)](https://benoitfragit.github.io/MFDStudio/api/index.html) |

Task-focused recipes live in the
[Cookbook](https://benoitfragit.github.io/MFDStudio/cookbook.html), and what you
can safely depend on is in the
[Public API Contract](https://benoitfragit.github.io/MFDStudio/reference/public_contract.html).

The documentation sources live under [`docs/src/`](./docs/src/index.md).

## Build

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

Requires Visual Studio 2022, CMake 3.25+, Python 3, and C++17. The first
configure downloads third-party dependencies. See the
[Build page](https://benoitfragit.github.io/MFDStudio/dev/build.html) for presets
and options.

## Run

```powershell
.\Scripts\Start-MfdDemo.bat
```

Then launch `client_mockup`, activate a page, and move a reticle. Press `F1` in
`mfd_window` for the runtime debug overlay.

## Build the documentation locally

```powershell
pwsh -File docs/BuildDocsSite.ps1 -Version local
```

The combined site (mdBook portal plus the Doxygen C++ API under `/api/`) is
written to `build/docs/site`. To check the guide portal only:

```powershell
pwsh -File docs/CheckDocs.ps1
```

## Where to go next

- New to MFDStudio? Start with the published
  [Quick Start](https://benoitfragit.github.io/MFDStudio/quickstart.html) and
  [Getting Started Tutorial](https://benoitfragit.github.io/MFDStudio/getting-started.html).
- Contributing? Read
  [`AGENTS.md`](./AGENTS.md) and the
  [Quality page](https://benoitfragit.github.io/MFDStudio/dev/quality.html).

## Licenses

- project license: [LICENSE](./LICENSE)
- third-party notices: [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)
