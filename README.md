# MFDStudio

`MFDStudio` is a C++17 / CMake toolkit for authoring and running 2D
multi-function display windows from JSON.

The historical technical prefix remains `mfd` in namespaces, targets, folders,
and APIs.

![Cockpit runtime screenshot](./docs/images/mfd_window_cockpit_capture.png)

## Start Here

Do not read the documentation tree linearly. Most users only need one of these
paths first:

| Goal | Open first | Then |
| --- | --- | --- |
| Launch a window and see something live | [Quick Start](./docs/QUICKSTART.md) | [Test A Window With The Mockup](./docs/tutorials/03_test_with_mfd_mockup.md) |
| Drive a window from a generated C++ client API | [Use The Mockup As A Client API Reference](./docs/tutorials/11_use_the_mockup_as_a_client_api_reference.md) | [Drive A Window From A Live Client](./docs/tutorials/04_drive_a_window_from_a_live_client.md) |
| Embed the runtime offscreen into your own app | [Detailed User Guide (.docx)](./docs/user_guide/MFDStudio_End_To_End_User_Guide.docx) | Build `offscreen_viewer` and review section `6.19` |
| Capture the runtime framebuffer | [Capture The Window As Raw Pixels](./docs/tutorials/07_framebuffer_rgba32_capture.md) | [Quick Start](./docs/QUICKSTART.md) |
| Build or edit assets visually | [Create A Window From Scratch In `mfd_editor`](./docs/tutorials/13_create_window_from_editor.md) | [Review The Integrated Editor Tutorial Outputs](./docs/tutorials/15_review_integrated_editor_tutorial_outputs.md) |
| Need a compact map of the docs | [Documentation Guide](./docs/README.md) | [Tutorial Index](./docs/tutorials/README.md) |

You do not need the architecture notes or contributor docs to get a window running.

## Fast First Run

Build the minimum useful set on Windows:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup mfd_editor
```

Then:

1. launch `.\Scripts\Start-MfdDemo.bat`
2. launch `client_mockup`
3. activate one page
4. edit one reticle or page control
5. press `F1` in `mfd_window`

If you want the sample framebuffer path immediately, launch
`.\Scripts\Start-MfdMinimal.bat` instead. It starts `mfd_window` with the
sample framebuffer plugin enabled.

## Practical Workflows

### Launch `mfd_window`

The simplest path is still one of the shipped launchers:

- `.\Scripts\Start-MfdDemo.bat`
- `.\Scripts\Start-MfdCockpit.bat`
- `.\Scripts\Start-MfdMinimal.bat`

If you want one explicit entry point for your own window JSON, use:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages.json
```

The same launcher can pass a framebuffer plugin:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages_minimal.json --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll
```

### Use The Generated Client API

For a client dedicated to one authored window, the generated API is the
preferred surface. It gives typed access to pages, reticles, exposed
primitives, strobes, and dynamic sets, while `CommandClient` remains the final
transport sender.

For time primitives, the generated API exposes structured runtime controls:
numeric time bypass, bypass clear, UTC/local selection, and field visibility.
It does not require clients to send raw format strings at runtime.

When the target window exposes a feedback transport, a client can also detect
window shutdown over the connectionless UDP stream with
`mfd::client::WindowLivenessMonitor` and reset its connections cleanly. See the
[Detailed User Guide](./docs/user_guide/MFDStudio_End_To_End_User_Guide.docx)
section `6.20`.

Treat the generated header, generated source, and companion
`<window>.generated.map` as one contract. The generated runtime path is only
valid when `mfd_window` loads the matching `.generated.map`. Use generated
`ui.Run()` to start one new local client cycle, and generated `ui.Initialize()`
only when you want one full authored-state reinitialization on the next batch.

Start here:

- [Use The Mockup As A Client API Reference](./docs/tutorials/11_use_the_mockup_as_a_client_api_reference.md)
- [Drive A Window From A Live Client](./docs/tutorials/04_drive_a_window_from_a_live_client.md)

### Capture The Framebuffer

There are two practical capture paths:

- host-side `RGBA32` capture from the runtime layer
- plugin-based capture from `mfd_window`, with plugin-selected `RGBA32` or `BGRA32`

Start here:

- [Capture The Window As Raw Pixels](./docs/tutorials/07_framebuffer_rgba32_capture.md)

### Embed The Runtime Offscreen

When your application must render one MFD without launching `mfd_window`, use
the dedicated `mfd_runtime_api` package. It keeps the authored UDP in/out
contract, preserves clipping offscreen, and lets the host application resize
each offscreen surface explicitly.

Build the shipped example:

```powershell
cmake --build --preset debug-win32 --target offscreen_viewer
```

Then run `offscreen_viewer` from the staged build tree. The example loads one
window JSON through `mfd_runtime_api`, renders two independent offscreen
surfaces, and displays the uploaded images in a resizable host window without
using `WindowLauncher` scripts.

### Work In The Editor

If your main entry point is the visual tool, start here:

- [Create A Window From Scratch In `mfd_editor`](./docs/tutorials/13_create_window_from_editor.md)

The editor authors text and time alignment directly in the asset model with the
same `left`, `center`, and `right` behavior used by both the preview gizmos and
`mfd_window` at runtime. It also keeps one shared editor-only visible grid,
shared snap toggle, and shared logical grid step across the page preview and
reticle studio without serializing those preferences into the authored JSON.

The editor shell is responsive: the central workspace always keeps priority, so
narrowing the window first compresses and then auto-collapses the sidebar and
inspector instead of blocking the resize. Auto-collapse is temporary and never
changes your saved panel widths; use the menu-bar **View** menu or widen the
window to bring panels back. See
[Editor View Modes](./docs/reference/editor_view_modes.md) for details.

The navigation sidebar keeps long projects scannable. The **Pages** and **Reticle
library** headers show live counts of the entries currently passing the filter,
and a single line near the filter surfaces the number of pending validation
problems (click it to open the Problems panel). The filter box matches page and
reticle names by default; start the text with `page:`, `reticle:`, or `problem:`
to aim the same filter at one section (the `problem:` token keeps only entries
that still have validation issues). The branch holding the current selection
stays expanded, and the first filtered match opens automatically so search
results never hide behind a collapsed page.

## Developer Checks

Use the repository clang-tidy entry point when you want one industrial C++ review pass without rebuilding the Visual Studio tree:

```powershell
.\Scripts\Run-ClangTidy.ps1
```

Useful variants:

```powershell
.\Scripts\Run-ClangTidy.ps1 -Paths mfd_api,mfd_common_api,mfd_window,mfd_editor
.\Scripts\Run-ClangTidy.ps1 -Paths tests
.\Scripts\Run-ClangTidy.ps1 -AuditOnly
```

The script:

- audits the `AGENTS.md` forbidden patterns before running clang-tidy
- targets production modules by default; `tests` and `examples` are opt-in through `-Paths`
- generates or reuses `build/clang-tidy-win32/compile_commands.json`
- reuses dependency source trees from `build/vs2022-win32/_deps/*-src` when they already exist
- locates `clang-tidy.exe` from the Visual Studio LLVM tools when it is not on `PATH`
- writes a consumable report under `outputs/quality/clang-tidy/latest`

Use the repository `cppcheck` entry point when you want one deeper external static-analysis pass:

```powershell
.\Scripts\Run-Cppcheck.ps1
```

The script:

- generates or reuses a dedicated compilation database
- targets production modules by default; `tests` and `examples` are opt-in through `-Paths`
- runs `cppcheck` in exhaustive mode on the repository paths you requested
- filters out diagnostics emitted only from `build/` and vendored dependency trees from the actionable summary
- writes XML, logs, and a summary under `outputs/quality/cppcheck/latest`

Use the dedicated fuzzing entry point when you want real `libFuzzer` coverage outside the nominal build:

```powershell
.\Scripts\Run-Fuzzing.ps1 -DurationSeconds 120
```

The script:

- configures one external Clang/Ninja build under `build/clang-fuzz-x64`
- enables dedicated `libFuzzer` + AddressSanitizer targets only in that build
- keeps the nominal Visual Studio Win32 build unchanged
- writes crash artifacts, logs, and a summary under `outputs/quality/fuzz/latest`

Use the full quality gate when you want one consolidated report including GoogleTest, clang-tidy, cppcheck, and fuzzing:

```powershell
.\Scripts\Run-QualitySuite.ps1
```

The suite:

- builds and runs the Win32 GoogleTest matrix with JUnit output
- runs external clang-tidy and cppcheck reports
- runs the dedicated fuzzing build and archives artifacts
- writes a consolidated report under `outputs/quality/runs/<timestamp>`

## Main Applications

| Entry point | Purpose |
| --- | --- |
| `mfd_window` | Runtime host that loads one window JSON file |
| `client_mockup` | Live UDP client used to validate pages, reticles, blink, strobe, and feedback |
| `mfd_editor` | Visual authoring tool for windows, pages, and reticles |
| `offscreen_viewer` | Resizable example embedding `mfd_runtime_api` and displaying two offscreen images |
| `mfd_framebuffer_stdout_plugin` | Sample framebuffer plugin for the `mfd_window` capture ABI |

## Advanced Docs

Keep these for later, when you actually need them:

- [Core Concepts](./docs/CONCEPTS.md)
- [Tutorials](./docs/tutorials/README.md)
- [Reference](./docs/reference/README.md)
- [Development Guide](./docs/DEVELOPMENT.md)
- [Detailed User Guide (.docx)](./docs/user_guide/MFDStudio_End_To_End_User_Guide.docx)
- [Architecture Notes](./docs/architecture/README.md)

## Licenses

- project license: [LICENSE](./LICENSE)
- third-party notices: [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)
