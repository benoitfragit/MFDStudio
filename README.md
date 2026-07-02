# MFDStudio

`MFDStudio` is a C++17 / CMake toolkit for authoring and running 2D
multi-function display (MFD) windows from JSON.

The historical technical prefix `mfd` remains in namespaces, targets, folders,
and APIs.

[![PR CI](https://img.shields.io/github/actions/workflow/status/benoitfragit/MFDStudio/ci.yml?label=PR%20CI&style=for-the-badge)](https://github.com/benoitfragit/MFDStudio/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/benoitfragit/MFDStudio?display_name=tag&style=for-the-badge)](https://github.com/benoitfragit/MFDStudio/releases/latest)
[![Release Workflow](https://img.shields.io/github/actions/workflow/status/benoitfragit/MFDStudio/release.yml?label=Release%20Workflow&style=for-the-badge)](https://github.com/benoitfragit/MFDStudio/actions/workflows/release.yml)
[![Docs Deploy](https://img.shields.io/github/actions/workflow/status/benoitfragit/MFDStudio/docs-pages.yml?label=Docs%20Deploy&style=for-the-badge)](https://github.com/benoitfragit/MFDStudio/actions/workflows/docs-pages.yml)
[![License](https://img.shields.io/github/license/benoitfragit/MFDStudio?style=for-the-badge)](https://github.com/benoitfragit/MFDStudio/blob/master/LICENSE)
[![Docs Online](https://img.shields.io/badge/Docs-Online-8250df?style=for-the-badge)](https://benoitfragit.github.io/MFDStudio)

![Cockpit runtime screenshot](./docs/src/images/mfd_window_cockpit_capture.png)

## Documentation

The full documentation is published as a site, combining a guide portal (mdBook)
and the C++ API reference (Doxygen). `Latest Release` tracks the published GitHub
release, while `Release Workflow` reflects the most recent `release.yml` run:

**https://benoitfragit.github.io/MFDStudio**

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
Every GitHub release page also includes a validation summary for the Win32
delivery lane and ships the published documentation as a downloadable ZIP
archive. The GitHub Win32 automation lane excludes a narrow render/offscreen
test subset that passes locally but fails on the hosted `windows-2022` runner.

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

For heavy live-UDP debug sessions, `mfd_window` and `offscreen_viewer` also
accept `--no-snapshot` to keep earlier commands of one runtime batch applied
when a later command fails. The default remains transactional.

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
