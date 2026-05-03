# Quick Start

This page is the fastest path from zero to a visible live window.

In one short session you will:

- launch one runtime window
- launch the live mock client
- activate a page
- edit one reticle
- inspect the runtime overlay

If you want the full map first, open [Documentation Guide](./README.md).

## What You Will Be Looking At

### Runtime Window

![Runtime window](./images/mfd_window_cockpit_capture.png)

### Live Client

![Client mockup](./images/client_mockup_demo.png)

## The Only Mental Model You Need

![Runtime loop](./images/mfd_runtime_roundtrip.svg)

Keep this in mind:

- JSON assets define what exists
- `mfd_window` renders the active page
- `client_mockup` sends commands over UDP
- the runtime can return feedback

## Step 1 - Build Only What You Need

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window mfd_framebuffer_stdout_plugin client_mockup
```

If you want the editor too:

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

For a clean staged runtime refresh:

```powershell
cmake --build --preset debug-win32 --target stage_exec
```

## Step 2 - Start A Runtime Window

Use one of the shipped launchers:

- `.\Scripts\Start-MfdDemo.bat`
- `.\Scripts\Start-MfdCockpit.bat`
- `.\Scripts\Start-MfdMinimal.bat`

For a first run, `.\Scripts\Start-MfdDemo.bat` is the simplest choice.

## Step 3 - Start The Client

Launch `client_mockup`.

At this point you have the complete live loop:

- one authored window running in `mfd_window`
- one UDP client discovering the same JSON locally
- one control surface for pages, reticles, blink, strobe, and feedback

## Step 4 - Activate One Page

In `client_mockup`:

1. select the window preset matching the launched runtime
2. choose a page in the left tree
3. click `Activate selected page` or `Activate page now`

If the command loop is correct, the runtime window changes immediately.

## Step 5 - Move One Reticle

Still in `client_mockup`:

1. open the selected page
2. pick one static reticle
3. edit `Position`
4. click `Send reticle update`

You should see the change live in the runtime window.

## Step 6 - Check Something More Dynamic

Useful quick checks:

- enable the radar simulation to flood a page with dynamic tracks
- change `Brightness` or `Invert colors` in `Window display`
- activate the cockpit preset to see a denser page in motion

## Step 7 - Inspect The Runtime Itself

Inside `mfd_window`:

1. press `F1`
2. inspect the transport state and active page
3. select one reticle in the runtime tree
4. apply a temporary local bypass
5. clear the bypass to return to the live client-driven state

This is the fastest way to distinguish:

- an authored asset problem
- a client-side command problem
- a runtime-state problem

## After 10 Minutes, You Should Understand

- where the assets live
- how the runtime is launched
- how the client talks to the runtime
- how to validate a page without writing new code

## Where To Go Next

| Goal | Next page |
| --- | --- |
| create your own assets | [Create Reticles From Primitives](./tutorials/01_create_reticles_from_primitives.md) |
| build a window in JSON | [Create Pages And Windows](./tutorials/02_create_pages_and_windows.md) |
| use the mockup as a real client reference | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) |
| drive a runtime from your own code | [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md) |
| work visually in the editor | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) |
| understand the vocabulary better | [Core Concepts](./CONCEPTS.md) |
