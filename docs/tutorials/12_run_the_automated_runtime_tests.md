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

The test target is defined in:

- `mfd_api/tests/CMakeLists.txt`

The first test files are:

- `mfd_api/tests/JsonLoaderTests.cpp`
- `mfd_api/tests/SceneRegistryTests.cpp`

## What Is Covered Today

`JsonLoaderTests.cpp` covers:

- loading a root window JSON with relative page and reticle-library paths
- loading page-local blink definitions and default blink resolution
- rejecting an unknown page default blink type
- rejecting an unknown reticle blink type
- a smoke test on the repository cockpit window configuration

`SceneRegistryTests.cpp` covers:

- initial active-page selection
- ignoring activation of an unknown page
- changing a reticle from one blink type to another
- clearing a reticle blink type back to the page default
- rejecting invalid blink type changes without mutating runtime state
- synchronization of reticles that share the same effective blink duration
- clamping and validating whole-window brightness patches

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
repository behavior is to compile it.

## Run The Tests

You can run the executable directly:

```powershell
.\build\vs2022-win32\mfd_api\tests\Debug\mfd_api_tests.exe
```

Or use the convenience target:

```powershell
cmake --build --preset debug-win32 --target mfd_api_tests_run
```

## How To Extend The Suite

Prefer adding tests when one of these situations appears:

- a runtime rule becomes subtle enough to be forgotten easily
- a JSON authoring rule should fail loudly on invalid input
- a recent bug fix must stay fixed

Good candidates for future tests:

- command serialization edge cases
- bulk dynamic-reticle update behavior
- additional asset smoke tests for demo windows

## Recommended Workflow

When you touch the runtime or loading code:

1. build the project
2. run `mfd_api_tests`
3. launch the mockup only after the automated suite is green

This gives a faster and more reliable feedback loop than manual checking alone.
