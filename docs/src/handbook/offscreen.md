# Offscreen Embedding

When your application must render one MFD without launching `mfd_window`, use
the dedicated `mfd_runtime_api` package. It keeps the authored UDP in/out
contract, preserves clipping offscreen, and lets the host application resize each
offscreen surface explicitly.

## Build the example

```powershell
cmake --build --preset debug-win32 --target offscreen_viewer
```

Run `offscreen_viewer` from the staged build tree. The example loads one window
JSON through `mfd_runtime_api`, renders two independent offscreen surfaces, and
displays the uploaded images in a resizable host window — without using
`WindowLauncher` scripts.

## When to use it

Choose offscreen embedding when:

- you own the host window and frame loop
- you need more than one independent MFD surface in the same process
- you want explicit control over each surface's size

The runtime keeps the same authored command and feedback contract as
`mfd_window`, so a client written against the generated API behaves identically
whether it targets the standalone host or your embedded surfaces.
