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
| `MFD_ENABLE_POSITION_INDEPENDENT_CODE` | `OFF` on Windows, `ON` elsewhere | Enables position-independent code only where it is actually needed |
| `MFD_MSVC_RUNTIME` | `default` | Leaves the MSVC CRT runtime to the toolchain default; accepts `dll` or `static` for explicit control |

On Windows/MSVC, the repository no longer forces one global CRT policy unless
you ask for it. In practice:

- `default` keeps the Visual Studio generator defaults
- `dll` forces `/MD` in release and `/MDd` in debug
- `static` forces `/MT` in release and `/MTd` in debug

Examples:

```powershell
cmake --preset vs2022-win32 -DMFD_MSVC_RUNTIME=static
cmake --preset vs2022-win32 -DMFD_ENABLE_POSITION_INDEPENDENT_CODE=ON
```

## CMake Linking Convention

Keep dependency categories separated in repository `CMakeLists.txt` files:

- internal repository targets stay in plain `target_link_libraries(...)`
- system libraries stay in plain `target_link_libraries(...)`
- third-party dependencies go through `cmake/ExternalLibraries.cmake`
- the root `CMakeLists.txt` only registers first-level repository directories
- each directory owns its local `add_subdirectory(...)` tree for children such as `tests` or `examples/*`

Use the real target name when calling the helpers. The top-level project name is
still `MFD`, so `${PROJECT_NAME}` is not a per-directory target alias.

Example:

```cmake
target_link_libraries(mfd_editor
    PRIVATE
        mfd::api
        mfd::editor_plugin_api)

mfd_begin_external_libraries(mfd_editor)
mfd_private_library(mfd_editor imgui)
mfd_private_library(mfd_editor rlimgui)
mfd_end_external_libraries(mfd_editor)
```

## Common Targets

| Target | Role |
| --- | --- |
| `mfd_window` | Generic runtime host that loads a window JSON and exposes the integrated `F1` debug overlay |
| `mfd_framebuffer_stdout_plugin` | Sample DLL exporting the framebuffer callback symbol expected by `mfd_window` |
| `client_mockup` | Interactive GUI client used to exercise the public API |
| `client_mockup_minimal` | Minimal plain-loop client for the cockpit showcase |
| `client_tutorial` | Tutorial-oriented client using the generated API on `mfd_tutorial.json`, including the authored Page2 progress bar driven through one exposed primitive; this target is meant to be added to `examples/CMakeLists.txt` by the tutorial flow once the tutorial asset set exists |
| `mfd_editor` | Visual authoring tool |
| `mfd_api_tests` | Runtime and JSON loading test executable |
| `client_api_tests` | Client-side helper test executable |
| `mfd_window_tests` | Window launcher and integrated runtime-debug tests |
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
have been authored under `assets/` and the tutorial has wired
`add_subdirectory(client_tutorial)` into `examples/CMakeLists.txt`.

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

Run the `mfd_window`-specific launcher and runtime-debug tests:

```powershell
ctest -C Debug --test-dir build/vs2022-win32 -R "WindowLauncherTests|RuntimeDebug" --output-on-failure
```

Run one executable directly:

```powershell
.\build\vs2022-win32\tests\Debug\mfd_api_tests.exe
```

All GoogleTest executables are grouped under `build/<preset>/tests/<config>/`.
Their staged copies are grouped under `_Exec/<toolset>/<platform>/<config>/tests/`.

There are also convenience build targets:

- `mfd_api_tests_run`
- `client_api_tests_run`
- `mfd_window_tests_run`
- `mfd_editor_tests_run`

The generated-client Python validation is registered through `ctest` under the
name `client_api_generator_tests`.

In practice the automated suite now covers three complementary layers:

- `mfd_api_tests`: runtime logic, command protocol, generated transport-map resolution, strobe feedback, and unit-space projection
- `client_api_tests`: high-level client helpers plus a compiled integration fixture built from the real generated client API
- `client_api_generator_tests`: Python-side generator validation, including rejection of C++ name collisions before invalid code is emitted

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
- [`docs/README.md`](../docs/README.md): documentation map and reading paths
- [`docs/QUICKSTART.md`](./QUICKSTART.md): first visible result
- [`docs/CONCEPTS.md`](./CONCEPTS.md): vocabulary and mental model
- [`docs/reference`](./reference/README.md): exact JSON and authoring rules
- [`docs/tutorials`](./tutorials/README.md): step-by-step workflows
- [`docs/standards`](./standards/README.md): generated client API standardization, replacement-client contract, and conformance evidence

If you change public behavior, examples, or onboarding flow, update the
relevant Markdown page in the same change. That includes the root launch
scripts when the supported demo entry points change.

## API Reference Docs

The repository now carries a versioned Doxygen configuration in `docs/Doxyfile`
plus a generation helper script in `docs/GenerateDocs.ps1`. The published
portal includes the repository guides, tutorials, references, architecture
notes, and the public headers under:

- `mfd_api/include`
- `client_api/include`
- `mfd_window/include`

The generated HTML enables Graphviz-backed include and dependency diagrams plus
PlantUML-driven architecture and tutorial diagrams. Its frontend now uses the
official `doxygen-awesome-css` theme with dark-mode toggle, fragment copy
button, and paragraph links.

For local generation, ensure:

- `doxygen` is installed and available in `PATH`
- `dot` from Graphviz is available in `PATH`
- PlantUML is installed or its jar path is known
- Java 11 or newer is installed somewhere on the machine

`docs/GenerateDocs.ps1` now resolves a compatible Java runtime automatically by
checking, in order:

- `JAVA_HOME`
- the current `PATH`
- common Windows JDK installation folders

If several JDKs are installed, the script keeps the first runtime that reports
Java 11 or newer and prepends its `bin` directory before invoking Doxygen.

Typical local generation flow:

```powershell
powershell -ExecutionPolicy Bypass -File docs/GenerateDocs.ps1 -Version local -PlantUmlJarPath C:\path\to\plantuml.jar
```

The generated site lands under `build/docs/doxygen/html/index.html`.

## Release Workflow

The repository includes a release pipeline in
`.github/workflows/release.yml` and a GitHub Pages publication workflow in
`.github/workflows/docs-pages.yml`. The Pages workflow follows the successful
release workflow automatically. The pull request CI, release packaging, and
docs publication jobs all run on `windows-latest`.

The Pages workflow pins Temurin Java 17 explicitly before running
`docs/GenerateDocs.ps1`, so the Doxygen / PlantUML publication path does not
depend on whatever legacy Java might also exist on the runner image.

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

- GitHub Pages deployment of the Doxygen HTML portal generated with Graphviz
  and PlantUML

Repository setting required once:

1. open `Settings > Pages`
2. choose `GitHub Actions` as the build and deployment source
