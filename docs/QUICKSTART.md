# Quick Start

This page is the shortest path from zero to a live `mfd_window`.

At the end of this session you should have:

- one runtime window on screen
- one live client talking to it
- one page activation working
- one visible reticle update working

If you want the full doc map first, open [Documentation Guide](./README.md).

## Step 1 - Build Only What You Need

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

If you also want the editor:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

## Step 2 - Launch `mfd_window`

For a first run, use the shipped launcher:

```powershell
.\Scripts\Start-MfdDemo.bat
```

If you want the explicit generic launcher instead of a preset script:

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages.json
```

If you want to exercise the sample framebuffer plugin immediately:

```powershell
.\Scripts\Start-MfdMinimal.bat
```

Under the hood, the launcher resolves `mfd_window.exe` and passes the
equivalent `--window` argument, plus `--framebuffer-plugin` when requested.

## Step 3 - Launch The Client

Start `client_mockup`.

At this point:

- `mfd_window` owns rendering and runtime state
- `client_mockup` sends live UDP commands
- both sides target the same authored window model

## Step 4 - Activate One Page

In `client_mockup`:

1. select the window preset matching the launched runtime
2. choose a page in the left tree
3. click `Activate selected page` or `Activate page now`

The runtime window should switch immediately.

## Step 5 - Move One Reticle

Still in `client_mockup`:

1. open the selected page
2. choose one static reticle
3. change `Position`
4. click `Send reticle update`

You should see the update live in `mfd_window`.

## Step 6 - Inspect The Runtime

Inside `mfd_window`, press `F1`.

The debug overlay is the fastest way to distinguish:

- authored asset issue
- client-side command issue
- runtime-side state issue

## Next Step By Goal

| Goal | Read next |
| --- | --- |
| drive the runtime from your own generated C++ API | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) |
| write a lower-level live client | [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md) |
| capture raw pixels or use the plugin path | [Capture The Window As Raw Pixels](./tutorials/07_framebuffer_rgba32_capture.md) |
| author or edit assets visually | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) |
| learn the JSON model | [Core Concepts](./CONCEPTS.md) |
