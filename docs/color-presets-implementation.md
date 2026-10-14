# Built-in color presets: implementation and validation

Implemented on `master` in C++17, with the selector layout revised on 2026-10-14.
Validation uses Visual Studio Win32 Debug; results are recorded below.
The user-facing specification is [Color presets](src/reference/colors.md).

## Delivered behavior

Eight exact built-in colors and Custom RGBA are available throughout authoring,
client staging, generated wrappers, Protobuf and Window runtime application.
The renderer continues consuming the unchanged `ColorRgba` structure.

All eleven former color-edit call sites use one `DrawColorPicker` implementation:
primitive stroke/fill, library default stroke/fill, page reticle stroke, strobe
stroke, page title, page background, external-stream replacement color, and the
new-page/new-window background drafts. Each control displays its current color
as a button opening the standard RGBA selector, with exactly eight shortcuts on
the right. Tooltips expose names, RGBA and hex; contrasting outlines mark the
matching preset. RGBA editing and preset selection apply immediately. Closing
with Escape or an outside click retains edits. Named mutation helpers capture
one undo snapshot per continuous gesture or changed preset selection. Merely
opening or closing does not create an override. Existing inherited fill and
disabled-control behavior is preserved.

## Main files by module

| Module | Main modified or added files |
| --- | --- |
| Shared model | `mfd_common_api/include/mfd/model/Types.h` |
| Command contract | `mfd_common_api/include/mfd/control/CommandTypes.h`, `mfd_common_api/proto/mfd_commands.proto`, `mfd_common_api/src/control/CommandTypes.cpp` |
| Atomic color semantics | `mfd_common_api/include/mfd/control/internal/ColorUpdate.h` |
| Client handles and deltas | `mfd_client_api/include/mfd/client/Animation.h`, `mfd_client_api/src/Animation.cpp` |
| Transport and coalescing | `mfd_common_api/src/control/CommandClient.cpp`, `mfd_client_api/src/LatestBatchPublisher.cpp`, `mfd_api/src/control/UdpRuntimeBridge.cpp` |
| Runtime and JSON | `mfd_api/src/runtime/SceneRegistry.cpp`, `mfd_api/src/io/JsonLoader.cpp` |
| Generation | `mfd_client_api/generator/scripts/generate_ui.py`, regenerated `examples/lhld/client/generated/LhldUi.h` |
| Shared editor control | `mfd_editor/include/internal/application/EditorColorPicker.h`, `mfd_editor/src/application/EditorColorPicker.cpp` |
| Editor integration | `mfd_editor/src/application/EditorApplicationInspectors.cpp`, `mfd_editor/src/application/EditorExternalStreamInspector.cpp`, `mfd_editor/src/EditorApplication.cpp`, corresponding private declarations and CMake source lists |
| Guided tutorial | `mfd_editor/src/EditorTutorialData.cpp`, `mfd_editor/src/EditorTutorialController.cpp`, corresponding headers |
| Tests | `CommandTypesTests`, `CommandProcessorTests`, `JsonLoaderTests`, `UdpRuntimeBridgeTests`, `AnimationTests`, `GeneratedUiCompiledApiTests`, `LatestBatchPublisherTests`, `EditorColorPickerTests`, `EditorDocumentSerializerTests`, `EditorTutorialDataTests` |

Identifier detection, transport normalization, numeric-to-runtime identifier
resolution, dynamic creation preflight, dispatch and reset paths were inspected.
They retain complete patches and therefore need no separate preset dispatch.
The two coalescers and delta generation use shared color helpers rather than
merging enum and RGBA as independent properties.

## Final C++ API

```cpp
namespace mfd {
enum class ColorPreset {
    Custom = 0, Black = 1, White = 2, Red = 3, Green = 4,
    Blue = 5, Yellow = 6, Cyan = 7, Amber = 8
};
constexpr std::optional<ColorRgba> TryColorPreset(ColorPreset preset) noexcept;
}

// Static/dynamic reticles and primitive handles:
void SetColor(mfd::ColorPreset preset);
void SetColor(mfd::ColorRgba color);

// Fill-capable primitive handles only:
void SetFillColor(mfd::ColorPreset preset);
void SetFillColor(mfd::ColorRgba color);
```

`TryColorPreset` returns no value for Custom or unknown enum values. The preset
setter overloads reject those values with `std::invalid_argument` before mutation.
Existing RGBA signatures remain available. Generated wrappers retain their `using`
declarations, exposing both overloads, and explicitly include the shared types.
Circle, ring, rectangle, ellipse, square, diamond, triangle, closed polyline and
arc handles expose fill overloads; unrelated handles gain no fill API.

`ReticlePatch` appends `std::optional<ColorPreset> colorPreset`.
`PrimitivePatch` appends `colorPreset` and `fillColorPreset`; existing optional
RGBA members remain unchanged. SDK consumers must rebuild for the struct additions.

## Final Protobuf additions

```protobuf
enum ColorPreset {
  COLOR_PRESET_CUSTOM = 0;
  COLOR_PRESET_BLACK = 1;
  COLOR_PRESET_WHITE = 2;
  COLOR_PRESET_RED = 3;
  COLOR_PRESET_GREEN = 4;
  COLOR_PRESET_BLUE = 5;
  COLOR_PRESET_YELLOW = 6;
  COLOR_PRESET_CYAN = 7;
  COLOR_PRESET_AMBER = 8;
}
// ReticlePatch:
optional ColorPreset color_preset = 14;
// PrimitivePatch:
optional ColorPreset color_preset = 31;
optional ColorPreset fill_color_preset = 32;
```

All old field numbers and packed RGBA format remain unchanged. Built-ins carry
only the enum; new custom setters carry explicit CUSTOM plus RGBA. Legacy
RGBA-only patches remain accepted. Neither field means no update. Unknown enums,
CUSTOM without RGBA, and built-ins with RGBA are rejected before runtime mutation.
Each stroke/fill pair follows last-write-wins, clearing stale fields in both
transition directions. Unchanged selections produce no delta. Runtime resolution
produces RGBA and reset continues restoring authored values.

## Examples and documentation

- Tutorial: generated static/dynamic handles switch between preset and custom
  colors; the exposed progress fill uses Yellow.
- Demo: Cyan preset alternates with the existing custom animated pulse.
- HUD: the downward energy chevron uses Yellow, exactly matching its old RGBA.
- LHLD: non-extrapolated RWS tracks use White, exactly matching their old RGBA;
  extrapolation and other visual effects retain custom colors.
- Added the shared color reference, linked from JSON/public contract, editor and
  generated API guides, tutorials 01/04/05/11/13/15, and the focused README notice.
- The existing progress-fill exposure step teaches opening the current-color selector,
  selecting Yellow, observing the preview and closing it. Existing tutorial step
  IDs and stages remain unchanged. Completion waits until the popup closes.

## Initial validation (2026-10-13, before the selector layout revision)

1. `cmake --preset vs2022-win32`: successful configuration.
2. `cmake --build --preset debug-win32 --parallel 4`: final global build succeeded
   (exit 0), including model/transport, client API, editor, Window, regenerated
   client fixture, demo, HUD, LHLD and GPU test targets.
3. `ctest --preset test-debug-win32 --parallel 4 --timeout 90`: 1,001 tests
   discovered; 996 passed, 3 skipped, 2 deployment smoke tests failed because
   concurrent MSBuild invocations locked shared tracking files.
4. `ctest --preset test-debug-win32 --rerun-failed --parallel 1 --timeout 120`:
   both deployment tests passed. Combined outcome: **998 passed, 3 skipped**.
5. Final targeted run after the final build: **25/25 passed**, covering colors,
   protocol presence/invalid combinations, generated BuildBatch/SendBatch,
   runtime static and bulk dynamic updates, coalescing, JSON, tutorial phases,
   ImGui clicks, disabled controls, Apply/Cancel and alpha preservation.
6. `git diff --check`: clean. Repository readability scan executed; added-line
   matches are five generated Protobuf `mutable_*` accessors in tests, not new
   C++ mutable members. No new forbidden casts, generic dispatch, broad captures,
   manual allocation or unused-variable suppression was introduced.
7. Editor search confirms all direct ColorButton/ColorPicker calls are confined
   to the shared helper, and no old ColorEdit3/ColorEdit4 call remains.

Local logs are retained under `build/color-presets-final-build.log`,
`build/color-presets-full-tests.log`, `build/color-presets-retry-tests.log`,
`build/color-presets-final-tests.log`, and `build/color-presets-readability-existing.log`.
Build warnings originate from Protobuf/Abseil and generated Protobuf code.

## Selector layout validation (2026-10-14)

- `cmake --build --preset debug-win32 --parallel 1`: successful global build.
  Sequential staging resolved a transient failure removing the staged tests directory.
- `ctest --preset test-debug-win32-ci --parallel 1 --timeout 120`: **950/950 passed**,
  including all seven revised picker interaction tests, seven tutorial unit tests
  and the runtime deployment smoke tests.
- The generated external-stream test directory had been deleted locally. Running
  `cmake --preset vs2022-win32` restored its configuration; rebuilding
  `external_stream_tests` and `external_stream_gpu_test` succeeded.
- `ctest --preset test-debug-win32 -R '^ExternalStream' --parallel 1 --timeout 60`:
  **24/24 passed**, including all seven local GPU integration/failure-mode tests.
- Comparing the two successful runs with the regenerated CI inventory confirms
  **969 distinct passing tests: all 962 CI tests plus seven local GPU tests**.
  The restored local inventory contains 1,004 tests. Tutorial asset-dependent tests
  remain excluded from CI; editor tutorial unit tests remain included.
- `git diff --check` is clean. The readability scan found no newly introduced
  forbidden C++ constructs. All eleven authored color controls still delegate to
  the shared selector, including creation dialogs and external-stream fallback.

Logs: `build/color-picker-layout-final-build.log`,
`build/color-picker-layout-ci-tests.log`, `build/color-picker-layout-configure.log`,
`build/color-picker-layout-stream-build.log`, `build/color-picker-layout-stream-tests.log`
and `build/color-picker-layout-readability.log`.

## Validation limits and selector layout revision

The tutorial asset set is absent under `examples/tutorial/assets/`, so CMake
cannot enable `tutorial_client`. Its three existing offscreen tutorial tests
skip for the same reason. The optional packaged-client example is also disabled
because `_Deliveries/` is incomplete. Other representative generated clients
and the compile/runtime fixture were rebuilt and tested.

The initial editor interaction was exercised with headless ImGui frames, including
actual swatch/popup clicks. The 2026-10-14 layout revision replaces the transactional
Custom grid with the current-color button, immediate RGBA edits and eight in-popup
shortcuts; its revised tests cover all eight presets, alpha, disabled controls,
independent controls and undo grouping. No manual desktop visual walkthrough was performed.
The unrelated untracked `AUDIT_CODE_STABILITE_UX_PERFORMANCES.md` was preserved.
The CI preset explicitly excludes `tutorial_client_runtime_layout_smoke`; its
existing offscreen filter also excludes the three asset-dependent tutorial render
tests. Editor tutorial unit tests remain enabled. Preset validation on 2026-10-13 used CTest test enumeration only.
Enumeration confirmed 1,001 local tests and 959 CI tests: all three tutorial
render tests are excluded, all seven editor tutorial unit tests remain selected,
and the filter matches the conditional tutorial runtime smoke test.
