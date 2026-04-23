# Development Guide

This page is the contributor-oriented entry point for building, testing, and
navigating the repository.

## Toolchain

The repository currently targets:

- Visual Studio 2022
- CMake 3.26 or newer
- Python 3
- C++17

The first configure downloads third-party dependencies automatically, so the
first build is expected to be noticeably slower than incremental builds.

## Recommended First Build

For day-to-day debugging on Windows, start with the Win32 debug preset:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

Use the x64 presets when you specifically want the 64-bit binaries:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug-x64
```

## Available Presets

| Preset | Purpose |
| --- | --- |
| `vs2022-win32` | Configure a Win32 build tree under `build/vs2022-win32` |
| `vs2022-x64` | Configure an x64 build tree under `build/vs2022-x64` |
| `debug-win32` | Build Win32 Debug |
| `release-win32` | Build Win32 Release |
| `debug-x64` | Build x64 Debug |
| `release-x64` | Build x64 Release |

## Common CMake Options

| Option | Default | Effect |
| --- | --- | --- |
| `MFD_BUILD_DEMO` | `ON` | Builds the runtime applications, examples, and editor |
| `MFD_BUILD_TESTS` | `ON` | Builds and registers the automated test suite |
| `MFD_ENABLE_WARNINGS` | `ON` | Enables the stricter compiler warning profile |

## Common Targets

| Target | Role |
| --- | --- |
| `mfd_window` | Generic runtime host that loads a window JSON |
| `mfd_framebuffer_stdout_plugin` | Sample DLL exporting the framebuffer callback symbol expected by `mfd_window` |
| `client_mockup` | Interactive GUI client used to exercise the public API |
| `client_mockup_minimal` | Minimal plain-loop client for the cockpit showcase |
| `client_tutorial` | Tutorial-oriented client using the generated API on `mfd_tutorial.json`, built only after the tutorial asset set exists |
| `mfd_editor` | Visual authoring tool |
| `mfd_api_tests` | Runtime and JSON loading test executable |
| `client_api_tests` | Client-side helper test executable |
| `mfd_window_tests` | Window launcher tests |
| `mfd_editor_tests` | Editor-focused tests |

## Root Launch Scripts

The window entry points intended for humans are the batch launchers
committed at the repository root:

- `Start-MfdDemo.bat`
- `Start-MfdCockpit.bat`
- `Start-MfdMinimal.bat`
- `Start-MfdTutorial.bat`
- `Start-MfdWindow.bat`

When `mfd_window` builds on Windows, these scripts are copied both next to the
built executable and into `_Exec/<toolset>/<platform>/<config>/`.

Tutorial-oriented assets still feed `client_tutorial`, while the matching
window is launched through `Start-MfdTutorial.bat` or directly with
`mfd_window --window assets/windows/mfd_tutorial.json` once the tutorial assets
have been authored under `assets/`.

## Fast Onboarding Commands

Build just enough to validate the main user workflow:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

Build the editor directly:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

Build the main test executables:

```powershell
cmake --build --preset debug-win32 --target mfd_api_tests client_api_tests mfd_window_tests mfd_editor_tests
```

## Running Tests

Run the whole registered test suite:

```powershell
ctest -C Debug --test-dir build/vs2022-win32 --output-on-failure
```

Run only one family of tests:

```powershell
ctest -C Debug --test-dir build/vs2022-win32 -R "CommandProcessorTests|LatestBatchPublisherTests" --output-on-failure
```

Run one executable directly:

```powershell
.\build\vs2022-win32\mfd_api\tests\Debug\mfd_api_tests.exe
```

There are also convenience build targets:

- `mfd_api_tests_run`
- `client_api_tests_run`
- `mfd_window_tests_run`
- `mfd_editor_tests_run`

The generated-client Python validation is registered through `ctest` under the
name `client_api_generator_tests`.

## Repository Layout

| Path | Role |
| --- | --- |
| `mfd_api` | Core runtime library, JSON loading, command protocol, and public headers |
| `client_api` | Client-side helper layer built on top of the low-level command API |
| `client_api_generator` | Python and CMake tooling generating typed client wrappers |
| `mfd_window` | Generic runtime host executable |
| `mfd_editor` | Visual authoring application |
| `examples` | Example clients and sample plugins |
| `assets/windows` | Root window JSON files |
| `assets/pages` | Page JSON files |
| `assets/reticles` | Reticle template JSON files |
| `docs` | Onboarding, concepts, tutorials, reference, and architecture notes |
| `third_party` | Vendored or staged third-party material when applicable |

## Documentation Responsibilities

Use the documentation layers intentionally:

- `README.md`: short landing page for newcomers
- [`docs/README.md`](./README.md): documentation map and reading paths
- [`docs/QUICKSTART.md`](./QUICKSTART.md): first visible result
- [`docs/CONCEPTS.md`](./CONCEPTS.md): vocabulary and mental model
- [`docs/reference`](./reference/README.md): exact JSON and authoring rules
- [`docs/tutorials`](./tutorials/README.md): step-by-step workflows

If you change public behavior, examples, or onboarding flow, update the
relevant Markdown page in the same change. That includes the root launch
scripts when the supported demo entry points change.

## API Reference Docs

The repository now carries a versioned Doxygen configuration in
[`docs/Doxyfile`](./Doxyfile). It targets the public headers under:

- `mfd_api/include`
- `client_api/include`
- `mfd_window/include`

The generated HTML enables Graphviz-backed diagrams when the `dot` executable
is available in `PATH`.

Typical local generation flow:

```powershell
New-Item -ItemType Directory -Force build/docs/doxygen | Out-Null
doxygen docs/Doxyfile
```

The generated site lands under `build/docs/doxygen/html/index.html`.

## Release Workflow

The repository includes a release pipeline in
`.github/workflows/release.yml` and a GitHub Pages publication workflow in
`.github/workflows/docs-pages.yml`. The Pages workflow follows the successful
release workflow automatically. The pull request CI, release packaging, and
docs publication jobs all run on `windows-latest`.

Recommended release flow:

1. ensure the main branch is green on CI
2. create and push a semantic tag such as `1.0.0` or `v1.0.0`
3. let GitHub Actions build the Win32 and x64 archives and publish the release
4. let GitHub Actions regenerate and deploy the Doxygen HTML site to GitHub Pages

Example:

```bash
git tag 1.0.0
git push origin 1.0.0
```

Published assets include:

- `mfd-<tag>-x64.zip`
- `mfd-<tag>-win32.zip`
- `SHA256SUMS.txt`

Published documentation includes:

- GitHub Pages deployment of the Doxygen HTML site generated with Graphviz

Repository setting required once:

1. open `Settings > Pages`
2. choose `GitHub Actions` as the build and deployment source
