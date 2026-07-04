# Quick Start

The shortest path from zero to a live `mfd_window` with a client driving it.

## 1. Build what you need

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin demo_client
```

Add the editor if you want it:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

## 2. Launch the runtime

Use a shipped launcher for a first run:

```powershell
.\Scripts\Start-MfdDemo.bat
```

Or point the generic launcher at any window JSON:

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json
```

The launcher resolves `mfd_window.exe` and passes `--window` (plus
`--framebuffer-plugin` when requested).

## 3. Launch the client

Start `demo_client`. From here:

- `mfd_window` owns rendering and runtime state
- `demo_client` sends live UDP commands
- both target the same authored window model

## 4. Watch the automatic demo

`demo_client` cycles through the simplified demo pages. It updates one moving
radar track, the active strobe, blink states, and the clipped attitude-ball page
without any control panel.

## 5. Inspect the runtime

Inside `mfd_window`, press `F1` to open the debug overlay. It is the fastest way
to tell an authored-asset issue from a client-side or runtime-side issue.

## Next

| Goal | Read next |
| --- | --- |
| Follow the same loop end to end, with C++ | [Getting Started Tutorial](./getting-started.md) |
| Drive the runtime from a generated C++ API | [Generated Client API](./handbook/generated_api.md) |
| Capture raw pixels | [Framebuffer Capture](./handbook/framebuffer.md) |
| Author assets visually | [Editor](./handbook/editor.md) |
| Learn the JSON model | [Concepts](./concepts.md) |
