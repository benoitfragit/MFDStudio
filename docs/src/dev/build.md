# Build

## Toolchain

- Visual Studio 2022
- CMake 3.25 or newer
- Python 3
- C++17

The first configure downloads third-party dependencies automatically, so the
first build is slower than incremental builds.

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
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

Build the editor:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

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
  lane: `debug-win32` with `test-debug-win32`, plus `release-win32` as a
  build-only check.
- Tagged releases rerun that validation scope before publishing the GitHub
  release, then prepend the release page with a validation summary and links to
  the documentation assets.
- A successful release triggers the `Publish API Docs` workflow, which deploys
  the combined mdBook and Doxygen site to
  `https://benoitfragit.github.io/MFDStudio`.
