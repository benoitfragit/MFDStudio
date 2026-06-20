# Quick Start

The shortest path from zero to a live `mfd_window` with a client driving it.

## 1. Build what you need

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
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
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages.json
```

The launcher resolves `mfd_window.exe` and passes `--window` (plus
`--framebuffer-plugin` when requested).

## 3. Launch the client

Start `client_mockup`. From here:

- `mfd_window` owns rendering and runtime state
- `client_mockup` sends live UDP commands
- both target the same authored window model

## 4. Activate a page and move a reticle

In `client_mockup`:

1. select the window preset matching the launched runtime
2. choose a page and click `Activate selected page`
3. open a static reticle, change its `Position`, click `Send reticle update`

The runtime window updates live.

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
