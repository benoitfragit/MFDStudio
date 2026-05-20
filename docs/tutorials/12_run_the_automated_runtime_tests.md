# Run The Automated Runtime Tests

This tutorial explains the automated test suite that validates the runtime API
and the JSON-loading layer.

## Goal

At the end of this page you will know:

- where the automated tests live
- what they cover
- how to build them
- how to run them locally on Windows

## Why This Suite Exists

The project now contains enough runtime rules that manual validation alone is
not enough.

The automated suite focuses on the parts that are the easiest to regress:

- JSON loading through `JsonLoader`
- page-local blink resolution
- active-page state changes
- whole-window display patches such as inversion and brightness

## Test Framework

The project uses `GoogleTest`.

Important scope choice:

- the suite is intentionally focused on runtime logic and loading
- it does not try to pixel-compare rendered frames
- it does not depend on the mockup or the editor UI

This keeps the tests fast, deterministic, and useful for day-to-day work.

## Where The Tests Live

The root test aggregation happens in:

- `tests/CMakeLists.txt`

The root `tests/` tree mirrors the tested modules with dedicated bridge
`CMakeLists.txt` files, for example:

- `tests/mfd_api/CMakeLists.txt`

Each module keeps its own test target definitions under its local test
directory, for example:

- `tests/mfd_api/CMakeLists.txt`

The first test files are:

- `tests/mfd_api/JsonLoaderTests.cpp`
- `tests/mfd_api/RuntimeValidationTests.cpp`
- `tests/mfd_api/SceneRegistryTests.cpp`
- `tests/mfd_api/StrobeFeedbackTests.cpp`
- `tests/mfd_api/CommandTransportTests.cpp`

The low-level authored-data and transport sources exercised by those tests now
live physically under `mfd_common_api/include`, `mfd_common_api/src`, and
`mfd_common_api/proto`, while the runtime and JSON-loading layer stays under
`mfd_api/`.

## What Is Covered Today

`JsonLoaderTests.cpp` covers:

- loading a root window JSON with relative page and reticle-library paths
- loading page-local blink definitions and default blink resolution
- rejecting an unknown page default blink type
- rejecting an unknown reticle blink type
- a smoke test on the repository cockpit window configuration

`RuntimeValidationTests.cpp` covers:

- the shared numeric input budgets now reused by `JsonLoader` and `SceneRegistry`
- rejection of non-finite or out-of-budget coordinates
- rejection of oversized text payloads and authored point lists

`SceneRegistryTests.cpp` covers:

- initial active-page selection
- ignoring activation of an unknown page
- changing a reticle from one blink type to another
- clearing a reticle blink type back to the page default
- rejecting invalid blink type changes without mutating runtime state
- synchronization of reticles that share the same effective blink duration
- clamping and validating whole-window brightness patches

`StrobeFeedbackTests.cpp` covers:

- round-tripping full strobe and active-page feedback envelopes including capture metadata
- preserving optional and defaulted strobe feedback fields
- decoding mixed runtime feedback payload kinds correctly
- rejecting malformed or wrong-envelope Protocol Buffers payloads

`client_api_tests` also cover the higher-level generated feedback path:

- authoritative active-page tracking
- authoritative dynamic-reticle strobe-capture tracking
- generated `IsActive()` and `IsStrobeCaptured()` queries

`CommandTransportTests.cpp` covers:

- rejecting disabled or incomplete command and feedback UDP configs
- loopback delivery through the real UDP transport factories
- transport-level validation such as missing remote endpoints or invalid bind addresses

## Build The Tests

The default project configuration already builds the tests.

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

The controlling CMake option is:

```text
MFD_BUILD_TESTS=ON
```

If needed, you can disable the suite in a local configure step, but the default
repository behavior is to compile it. The repository also ships no-tests
configure and build presets when you explicitly want to skip test targets:

```powershell
cmake --preset vs2022-win32-no-tests
cmake --build --preset debug-win32-no-tests
```

## Run The Tests

You can run the executable directly:

```powershell
.\build\vs2022-win32\tests\Debug\mfd_api_tests.exe
```

Or target only the shared validation and runtime-loading suites while iterating:

```powershell
.\build\vs2022-win32\tests\Debug\mfd_api_tests.exe --gtest_filter="JsonLoaderTests.*:SceneRegistryTests.*:RuntimeValidationTests.*"
```

All GoogleTest executables are grouped under `build/<preset>/tests/<config>/`.
The clean staged `_Exec/<toolset>/<platform>/<config>/tests/` bucket is kept
for launcher-oriented smoke executables such as `client_api_tests`,
`mfd_window_tests`, and `mfd_editor_tests`. The heavier low-level
`mfd_api_tests` executable stays in the build tree and is launched from there.

If you want to regenerate the staged `_Exec` layout cleanly before launching
tests or runtime hosts from it, use:

```powershell
cmake --build --preset debug-win32 --target stage_exec
```

Or use the convenience target:

```powershell
cmake --build --preset debug-win32 --target mfd_api_tests_run
```

Or run the registered suite through the shipped CTest preset:

```powershell
ctest --preset test-debug-win32
```

## How To Extend The Suite

Prefer adding tests when one of these situations appears:

- a runtime rule becomes subtle enough to be forgotten easily
- a JSON authoring rule should fail loudly on invalid input
- a recent bug fix must stay fixed

Good candidates for future tests:

- bulk dynamic-reticle update behavior
- additional asset smoke tests for demo windows

## Recommended Workflow

When you touch the runtime or loading code:

1. build the project
2. refresh `_Exec` with `stage_exec` if you plan to launch from the staged tree
3. run `mfd_api_tests` from `build/<preset>/tests/<config>/`
4. launch the mockup only after the automated suite is green

This gives a faster and more reliable feedback loop than manual checking alone.
