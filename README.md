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

Start here:

- [Use The Mockup As A Client API Reference](./docs/tutorials/11_use_the_mockup_as_a_client_api_reference.md)
- [Drive A Window From A Live Client](./docs/tutorials/04_drive_a_window_from_a_live_client.md)

### Capture The Framebuffer

There are two practical capture paths:

- host-side `RGBA32` capture from the runtime layer
- plugin-based capture from `mfd_window`, with plugin-selected `RGBA32` or `BGRA32`

Start here:

- [Capture The Window As Raw Pixels](./docs/tutorials/07_framebuffer_rgba32_capture.md)

### Work In The Editor

If your main entry point is the visual tool, start here:

- [Create A Window From Scratch In `mfd_editor`](./docs/tutorials/13_create_window_from_editor.md)

## Main Applications

| Entry point | Purpose |
| --- | --- |
| `mfd_window` | Runtime host that loads one window JSON file |
| `client_mockup` | Live UDP client used to validate pages, reticles, blink, strobe, and feedback |
| `mfd_editor` | Visual authoring tool for windows, pages, and reticles |
| `mfd_framebuffer_stdout_plugin` | Sample framebuffer plugin for the `mfd_window` capture ABI |

## Advanced Docs

Keep these for later, when you actually need them:

- [Core Concepts](./docs/CONCEPTS.md)
- [Tutorials](./docs/tutorials/README.md)
- [Reference](./docs/reference/README.md)
- [Development Guide](./docs/DEVELOPMENT.md)
- [Detailed User Guide (.docx)](./docs/user_guide/MFDStudio_End_To_End_User_Guide.docx)
- [Detailed User Guide (.pdf)](./docs/user_guide/MFDStudio_End_To_End_User_Guide.pdf)
- [Architecture Notes](./docs/architecture/README.md)

## Licenses

- project license: [LICENSE](./LICENSE)
- third-party notices: [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md)
