# Build

## Toolchain

- Visual Studio 2022 with **Desktop development with C++**
- MSVC v142 build tools (selected explicitly by the Visual Studio presets)
- a Windows 10 or Windows 11 SDK
- CMake 3.25 or newer
- Python 3.7 or newer
- C++17

The first configure downloads third-party dependencies automatically, so the
first build is slower than incremental builds and needs network access. For an
offline configure, populate `third_party/archives` and enable
`USE_LOCAL_PACKAGE=ON` instead. The optional external-stream Python producer
also requires Python to match the target architecture (`32-bit` for Win32,
`64-bit` for x64); ordinary generation and unit tests do not.

## Recommended first build

For day-to-day debugging on Windows, use the Win32 debug preset:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
ctest --preset test-debug-win32
```

The Visual Studio presets target the `v142` toolset so local trees match the
delivery packaging layout. Use the x64 presets (`vs2022-x64`, `debug-x64`) for
64-bit binaries.

## Presets

| Configure | Build | Test |
| --- | --- | --- |
| `vs2022-win32` | `debug-win32`, `release-win32` | `test-debug-win32` |
| `vs2022-win32-no-tests` | `debug-win32-no-tests`, `release-win32-no-tests` | — |
| `vs2022-x64` | `debug-x64`, `release-x64` | `test-debug-x64` |
| `vs2022-x64-no-tests` | `debug-x64-no-tests`, `release-x64-no-tests` | — |

## Common options

| Option | Default | Effect |
| --- | --- | --- |
| `MFD_BUILD_DEMO` | `ON` | Builds `examples/`; does not gate `mfd_editor`. |
| `MFD_BUILD_TESTS` | `ON` | Builds and registers the test suite. |
| `MFD_ENABLE_WARNINGS` | `ON` | Stricter warning profile. |
| `USE_LOCAL_PACKAGE` | `OFF` | Use local third-party archives from `MFD_LOCAL_PACKAGE_ROOT` instead of `FetchContent`. |
| `MFD_LOCAL_PACKAGE_ROOT` | `third_party/archives` | Root of local source archives by dependency. |
| `MFD_ENABLE_POSITION_INDEPENDENT_CODE` | `OFF` Windows / `ON` else | PIC only where needed. |
| `MFD_MSVC_RUNTIME` | `default` | `default`/`dll` (`/MD`,`/MDd`) / `static` (`/MT`,`/MTd`). |

The deprecated `USE_LOCALE_PACKAGE` cache entry is still accepted as an alias.

## Fast onboarding

Build just enough to validate the main workflow:

```powershell
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin demo_client hud_client
```

Build the editor:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

That target includes the API LEVEL 1 container runtime and host-owned editor UI
extension. Their DLLs are copied beside `mfd_editor.exe`; a separate extension
build or ImGui installation is not required. Close a running copy from the same
staging directory before rebuilding, otherwise Windows can reject the final
`copy_if_different` step while the executable is locked.

The editor can launch without `examples/tutorial/assets`, but the complete
39-step guided authoring flow and its asset-dependent runtime tests only become
available after those tutorial assets have been authored. CMake reports this
condition during configure; editor tutorial unit tests remain available.

## Tests

```powershell
ctest --preset test-debug-win32
ctest --preset test-debug-win32 -R "CommandProcessorTests|LatestBatchPublisherTests"
```

The suite covers three layers:

- `mfd_api_tests` — runtime logic, command protocol, transport-map resolution, feedback, unit-space projection
- `client_api_tests` — high-level client helpers plus a compiled generated-API integration fixture
- `client_api_generator_tests` — Python generator validation (including C++ name-collision rejection)

## Staging and deliveries

`stage_exec` refreshes the local `_Exec/<toolset>/<platform>/<config>` runtime
staging area. `_Deliveries` is rebuilt only by `Scripts\BuildDeliveries.bat`:

```powershell
Scripts\BuildDeliveries.bat --version <version>
```

It configures the no-tests presets, builds Debug and Release for both
architectures, installs components, and verifies the delivery layout. Generated
package families include `MFDStudioClientApi`, `MFDStudioWindowLauncherPlugin`,
and the `*.Install` runtime payloads.

## GitHub automation

- Pull requests run the `PR Build and Tests` workflow on the Win32 delivery
  lane: `debug-win32` with the CI preset `test-debug-win32-ci`, plus
  `release-win32` as a build-only check. That CI preset excludes the
  render/offscreen suite subset that currently fails only on the hosted
  `windows-2022` runner. It also excludes tests requiring locally authored
  `examples/tutorial/assets/`: `tutorial_client_runtime_layout_smoke` and the
  three `OffscreenSurfaceTests.Tutorial*` tests (covered by the offscreen filter).
  Editor tutorial unit tests remain enabled. The local `test-debug-win32` and
  `test-debug-x64` presets retain the asset-dependent tests when registered.
- Tagged releases rerun that validation scope before publishing the GitHub
  release, then prepend the release page with a validation summary and links to
  the documentation assets.
- A successful release triggers the `Publish API Docs` workflow, which deploys
  the combined mdBook and Doxygen site to
  `https://benoitfragit.github.io/MFDStudio`.
