# Built-in color presets: implementation and validation

Implemented on `master`, validated on 2026-10-13 with Visual Studio Win32 Debug,
C++17. The user-facing specification is [Color presets](src/reference/colors.md).

## Delivered behavior

Eight exact built-in colors and Custom RGBA are available throughout authoring,
client staging, generated wrappers, Protobuf and Window runtime application.
The renderer continues consuming the unchanged `ColorRgba` structure.

All eleven former color-edit call sites use one `DrawColorPicker` implementation:
primitive stroke/fill, library default stroke/fill, page reticle stroke, strobe
stroke, page title, page background, external-stream replacement color, and the
new-page/new-window background drafts. The grid contains eight swatches and a
Custom (+) entry. Tooltips expose names, RGBA and hex; contrasting outlines mark
the matching color. Custom edits a popup draft, preserving alpha, with Apply and
Cancel. Committed inspector changes call named mutation helpers and capture one
undo snapshot. Rendering or cancelling does not create an override. Existing
inherited fill and disabled-control behavior is preserved.

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
- The existing progress-fill exposure step now teaches selecting Yellow,
  observing the preview, then opening and closing Custom. Existing tutorial step
  IDs and stages remain unchanged. Completion waits until the popup closes.

## Executed validation

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

## Validation limits

The tutorial asset set is absent under `examples/tutorial/assets/`, so CMake
cannot enable `tutorial_client`. Its three existing offscreen tutorial tests
skip for the same reason. The optional packaged-client example is also disabled
because `_Deliveries/` is incomplete. Other representative generated clients
and the compile/runtime fixture were rebuilt and tested.

Editor interaction was exercised with headless ImGui frames, including actual
swatch/popup clicks. No manual desktop visual walkthrough was performed.
The unrelated untracked `AUDIT_CODE_STABILITE_UX_PERFORMANCES.md` was preserved.
The CI preset explicitly excludes `tutorial_client_runtime_layout_smoke`; its
existing offscreen filter also excludes the three asset-dependent tutorial render
tests. Editor tutorial unit tests remain enabled. Final preset validation uses
CTest test enumeration only, without another build or test execution.
Enumeration confirmed 1,001 local tests and 959 CI tests: all three tutorial
render tests are excluded, all seven editor tutorial unit tests remain selected,
and the filter matches the conditional tutorial runtime smoke test.
