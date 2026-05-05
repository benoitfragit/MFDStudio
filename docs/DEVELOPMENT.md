# Development Guide

This page is the contributor-oriented entry point for building, testing, and
navigating the repository.

## Toolchain

The repository currently targets:

- Visual Studio 2022
- CMake 3.25 or newer
- Python 3
- C++17

The first configure downloads third-party dependencies automatically, so the
first build is expected to be noticeably slower than incremental builds.

## Contributor Standard

Before making structural or public-surface changes, read
[MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md).
That document is the versioned source of truth for repository expectations
around C++17, architecture, API discipline, performance, tests, and Doxygen.

A root `AGENTS.md` may exist locally for user- or tool-specific instructions,
but it is intentionally ignored by Git. Do not treat that local file as the
repository source of truth.

## Recommended First Build

For day-to-day debugging on Windows, start with the Win32 debug preset:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

The Visual Studio presets explicitly target the `v142` toolset so the local
build trees match the delivery packaging layout.

Use the x64 presets when you specifically want the 64-bit binaries:

```powershell
cmake --preset vs2022-x64
cmake --build --preset debug-x64
```

## Available Presets

| Preset | Purpose |
| --- | --- |
| `vs2022-win32` | Configure a Win32 build tree under `build/vs2022-win32` |
| `vs2022-win32-no-tests` | Configure a Win32 build tree with `MFD_BUILD_TESTS=OFF` |
| `vs2022-x64` | Configure an x64 build tree under `build/vs2022-x64` |
| `vs2022-x64-no-tests` | Configure an x64 build tree with `MFD_BUILD_TESTS=OFF` |
| `debug-win32` | Build Win32 Debug |
| `release-win32` | Build Win32 Release |
| `debug-win32-no-tests` | Build Win32 Debug from the no-tests configure preset |
| `release-win32-no-tests` | Build Win32 Release from the no-tests configure preset |
| `debug-x64` | Build x64 Debug |
| `release-x64` | Build x64 Release |
| `debug-x64-no-tests` | Build x64 Debug from the no-tests configure preset |
| `release-x64-no-tests` | Build x64 Release from the no-tests configure preset |
| `test-debug-win32` | Run the registered Win32 Debug test suite with `ctest --preset` |
| `test-debug-x64` | Run the registered x64 Debug test suite with `ctest --preset` |

## Common CMake Options

| Option | Default | Effect |
| --- | --- | --- |
| `MFD_BUILD_DEMO` | `ON` | Builds the runtime applications, examples, and editor |
| `MFD_BUILD_TESTS` | `ON` | Builds and registers the automated test suite |
| `MFD_ENABLE_WARNINGS` | `ON` | Enables the stricter compiler warning profile |
| `USE_LOCAL_PACKAGE` | `OFF` | Keeps the remote Git-based `FetchContent` behavior; set it to `ON` to use local third-party source archives from `MFD_LOCAL_PACKAGE_ROOT` |
| `MFD_LOCAL_PACKAGE_ROOT` | `third_party/archives` | Root folder containing local third-party source archives grouped by dependency name |
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
cmake --preset vs2022-win32 -DUSE_LOCAL_PACKAGE=ON
```

When `USE_LOCAL_PACKAGE` is set to `ON`, `FetchContent` expects
source archives under `MFD_LOCAL_PACKAGE_ROOT` with this layout:

```text
third_party/archives/
  raylib/raylib-5.5.zip
  nlohmann_json/json-3.11.3.zip
  entt/entt-3.13.2.zip
  protobuf/protobuf-29.4.zip
  imgui/imgui-1.92.1.zip
  rlimgui/rlImGui-286e11acd6c785004c9550c7ed3762add2ae3d47.zip
  googletest/googletest-1.15.2.zip
```

Each dependency block in `cmake/Dependencies.cmake` carries a comment with the
exact upstream source-archive URL to download.

The deprecated `USE_LOCALE_PACKAGE` cache entry is still accepted as a
compatibility alias, but new configurations should use `USE_LOCAL_PACKAGE`.

## CMake Linking Convention

Keep dependency categories separated in repository `CMakeLists.txt` files:

- internal repository targets stay in plain `target_link_libraries(...)`
- system libraries stay in plain `target_link_libraries(...)`
- third-party dependencies go through `cmake/ExternalLibraries.cmake`
- the root `CMakeLists.txt` only registers first-level repository directories
- `tests/` mirrors the tested modules with dedicated `CMakeLists.txt` entry points
- non-test child trees such as `examples/*` stay owned by their local directory

Use the real target name when calling the helpers. The top-level project name is
still `MFD`, so `${PROJECT_NAME}` is not a per-directory target alias.

Example:

```cmake
target_link_libraries(mfd_editor
    PRIVATE
        mfd::io_json
        mfd::render_raylib
        mfd::editor_plugin_api)

mfd_private_library(mfd_editor imgui)
mfd_private_library(mfd_editor nlohmann_json::nlohmann_json)
mfd_private_library(mfd_editor raylib)
mfd_private_library(mfd_editor rlimgui)
mfd_enable_target_warnings(mfd_editor)
```

The historical `mfd::api` name now remains only as a convenience umbrella.
The real build graph is split into:

- `mfd::model`
- `mfd::transport`
- `mfd::io_json`
- `mfd::runtime`
- `mfd::render_raylib`

## Common Targets

| Target | Role |
| --- | --- |
| `mfd_window` | Generic runtime host that loads a window JSON and exposes the integrated `F1` debug overlay |
| `mfd_framebuffer_stdout_plugin` | Repository sample DLL exporting the stable framebuffer plugin ABI expected by `mfd_window` |
| `client_mockup` | Interactive Win32 + Dear ImGui + DX11 client used to exercise the public API without linking raylib |
| `client_mockup_minimal` | Minimal plain-loop client for the cockpit showcase |
| `client_tutorial` | Tutorial-oriented client using the generated API on `mfd_tutorial.json`, including the authored Page2 progress bar driven through one exposed primitive plus Page1 strobe feedback wired into `IsActive()` and `IsStrobeCaptured()`; this target is registered automatically in `examples/CMakeLists.txt` when the integrated tutorial completes once the tutorial asset set exists |
| `mfd_editor` | Visual authoring tool |
| `mfd_api_tests` | Runtime and JSON loading test executable |
| `client_api_tests` | Client-side helper test executable |
| `mfd_window_tests` | Window launcher and integrated runtime-debug tests |
| `mfd_editor_tests` | Editor-focused tests |

## Repository Launch Scripts

The window entry points intended for humans are the batch launchers
committed under `Scripts/`:

- `Scripts/Start-MfdDemo.bat`
- `Scripts/Start-MfdCockpit.bat`
- `Scripts/Start-MfdMinimal.bat`
- `Scripts/Start-MfdTutorial.bat`
- `Scripts/Start-MfdWindow.bat`

When `mfd_window` builds on Windows, these scripts are copied both next to the
built executable and into `_Exec/<toolset>/<platform>/<config>/`, without
creating an additional scripts subdirectory inside the staged layout.

Tutorial-oriented assets still feed `client_tutorial`, while the matching
window is launched through `Scripts/Start-MfdTutorial.bat` from the repository,
through the staged `Start-MfdTutorial.bat` copied next to `mfd_window`, or
directly with `mfd_window --window assets/windows/mfd_tutorial.json` once the
tutorial assets have been authored under `assets/` and the integrated
tutorial has registered `client_tutorial` into `examples/CMakeLists.txt`.

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
ctest --preset test-debug-win32
```

Run only one family of tests:

```powershell
ctest --preset test-debug-win32 -R "CommandProcessorTests|LatestBatchPublisherTests"
```

The equivalent explicit `ctest` form remains valid when you need to override
the preset manually:

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
The staged `_Exec/<toolset>/<platform>/<config>/tests/` bucket is reserved for
launcher-oriented smoke executables. The heavier low-level `mfd_api_tests`
binary stays in the build tree and is not copied into `_Exec`.

There are also convenience build targets:

- `mfd_api_tests_run`
- `client_api_tests_run`
- `mfd_window_tests_run`
- `mfd_editor_tests_run`

The generated-client Python validation is registered through `ctest` under the
name `client_api_generator_tests`.

A clean `_Exec` refresh is available through the dedicated `stage_exec` target:

```powershell
cmake --build --preset debug-win32 --target stage_exec
```

That target removes the staged `_Exec/<toolset>/<platform>/<config>` subtree
for each registered staging bucket and regenerates it from the current build
outputs, runtime DLLs, assets, branding, and launch scripts.

Delivery packages are produced through the dedicated repository script:

```powershell
Scripts\BuildDeliveries.bat
```

That script is the only supported entry point for `_Deliveries`. It performs
four explicit phases:

- configure `vs2022-win32` and `vs2022-x64`
- build `debug-win32`, `release-win32`, `debug-x64`, and `release-x64`
- copy the required SDK and install payloads from the build trees into `_Deliveries`
- verify the resulting delivery layout

`_Exec` and `_Deliveries` now have separate responsibilities:

- `_Exec` remains the local runtime staging area used for manual launches, smoke runs, and Visual Studio debugging
- `_Deliveries` is rebuilt from scratch only by `Scripts\BuildDeliveries.bat`

The delivery script populates `_Deliveries` with:

- `packages_windows/<PackageName>/build/native` for development packages
- `packages_bin_windows/<PackageName>/_Exec/<toolset>/<platform>/<config>` for install/runtime packages

Generated package families:

- `MFDStudioClientApi`: client headers, package config files, generator helpers, and Win32/x64 import libraries for `mfd_client_api` and `mfd_api`
- `MFDStudioEditorPlugin`: header-only SDK plus the sample editor automation plugin example
- `MFDStudioWindowLauncherPlugin`: public plugin headers, package config files, and Win32/x64 import libraries for `mfd_window_plugin_api`
- `MFDStudioClientApi.Install`: runtime DLL payload for `mfd_client_api` and `mfd_api`
- `MFDStudioEditor.Install`: `mfd_editor.exe` plus shared branding
- `MFDStudioWindowLauncher.Install`: `mfd_window.exe`, `mfd_window_plugin_api.dll`, `Start-MfdWindow.bat`, and shared branding

The delivery script deliberately does not package repository demo assets into
the install layouts, and it does not copy the repository demo launchers into
`MFDStudioWindowLauncher.Install`.

Typical delivery validation flow:

```powershell
Remove-Item -Recurse -Force .\_Deliveries -ErrorAction SilentlyContinue
Scripts\BuildDeliveries.bat
Test-Path .\_Deliveries\packages_windows
Test-Path .\_Deliveries\packages_bin_windows
Test-Path .\_Deliveries\packages_windows\MFDStudioClientApi\build\native\cmake\MFDStudioClientApiConfig.cmake
Test-Path .\_Deliveries\packages_bin_windows\MFDStudioClientApi.Install\_Exec
Test-Path .\_Deliveries\packages_bin_windows\MFDStudioWindowLauncher.Install\_Exec
```

In practice the automated suite now covers three complementary layers:

- `mfd_api_tests`: runtime logic, command protocol, generated transport-map resolution, runtime feedback, and unit-space projection
- `client_api_tests`: high-level client helpers plus a compiled integration fixture built from the real generated client API
- `client_api_generator_tests`: Python-side generator validation, including rejection of C++ name collisions before invalid code is emitted

## Repository Layout

| Path | Role |
| --- | --- |
| `mfd_common_api` | Internal shared low-level module with its own `include/`, `src/`, and `proto/` trees, owning the reusable `mfd_model` and `mfd_transport` static libraries |
| `mfd_api` | Core source tree producing `mfd_io_json`, `mfd_runtime`, the public low-level `mfd_api` DLL, and `mfd_render_raylib` plus the compatibility umbrella `mfd::api` |
| `mfd_client_api` | Client-side helper layer built on top of the low-level command API and producing `mfd_client_api` |
| `mfd_client_api/generator` | Python and CMake tooling generating typed client wrappers |
| `mfd_window_plugin_api` | Public framebuffer-plugin SDK used by `MFDStudioWindowLauncherPlugin` |
| `mfd_window` | Generic runtime host executable |
| `mfd_editor` | Visual authoring application |
| `tests` | Root test tree mirroring each module with dedicated CMake entry points |
| `examples` | Example clients and sample plugins |
| `assets/windows` | Root window JSON files |
| `assets/pages` | Page JSON files |
| `assets/reticles` | Reticle template JSON files |
| `branding` | Shared icons and branding files copied with the runtime hosts |
| `Scripts` | Repository launch scripts copied next to staged runtimes |
| `_Exec` | Local runtime staging area regenerated by target-level post-build staging and `stage_exec` |
| `_Deliveries` | SDK-style development and install packages generated only by `Scripts\BuildDeliveries.bat` |
| `docs` | Onboarding, concepts, tutorials, reference, and architecture notes |
| `third_party` | Vendored or staged third-party material when applicable |

## Documentation Responsibilities

Use the documentation layers intentionally:

- `README.md`: short landing page for newcomers
- [`docs/README.md`](../docs/README.md): documentation map and reading paths
- [`docs/QUICKSTART.md`](./QUICKSTART.md): first visible result
- [`docs/CONCEPTS.md`](./CONCEPTS.md): vocabulary and mental model
- [`docs/standards/mfd_cpp_repository_maintenance_standard.md`](./standards/mfd_cpp_repository_maintenance_standard.md): repository-wide engineering bar for C++17 maintenance
- [`docs/reference`](./reference/README.md): exact JSON and authoring rules
- [`docs/tutorials`](./tutorials/README.md): step-by-step workflows
- [`docs/standards`](./standards/README.md): generated client API standardization, replacement-client contract, and conformance evidence

If you change public behavior, examples, or onboarding flow, update the
relevant Markdown page in the same change. That includes the repository launch
scripts when the supported demo entry points change.

## API Reference Docs

The repository now carries a versioned Doxygen configuration in `docs/Doxyfile`
plus a generation helper script in `docs/GenerateDocs.ps1`. The published
portal includes the repository guides, tutorials, references, architecture
notes, the internal `mfd_common_api/README.md` build-graph note, and the public
headers under:

- `mfd_common_api/include`
- `mfd_api/include`
- `mfd_client_api/include`
- `mfd_editor_plugin_api/include`
- `mfd_window/include`
- `mfd_window_plugin_api/include`

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
3. let GitHub Actions publish the GitHub release metadata for the tag
4. let GitHub Actions regenerate and deploy the Doxygen HTML site to GitHub Pages

Example:

```bash
git tag 1.0.0
git push origin 1.0.0
```

The release workflow no longer uploads binary archives. If you need a local
delivery layout for validation or internal distribution, generate it explicitly
with `Scripts\BuildDeliveries.bat`.

Published documentation includes:

- GitHub Pages deployment of the Doxygen HTML portal generated with Graphviz
  and PlantUML

Repository setting required once:

1. open `Settings > Pages`
2. choose `GitHub Actions` as the build and deployment source
