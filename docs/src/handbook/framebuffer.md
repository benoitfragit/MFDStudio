# Framebuffer Capture

There are two practical paths to capture the runtime image as raw pixels.

## Host-side capture

Capture `RGBA32` directly from the runtime layer. Use this when you embed the
runtime (see [Offscreen Embedding](./offscreen.md)) and already own the frame
loop: the host reads the rendered surface back as raw pixels each frame.

## Plugin-based capture

Capture from `mfd_window` through the stable framebuffer plugin ABI defined in
`mfd_window_plugin_api`. The plugin selects `RGBA32` or `BGRA32` output.

The repository ships a sample plugin, `mfd_framebuffer_stdout_plugin` (which
requests `BGRA32`). Launch it through the generic launcher:

```powershell
.\Scripts\Start-MfdWindow.bat examples/demo/assets/windows/demo_window.json --framebuffer-plugin mfd_framebuffer_stdout_plugin.dll
```

Or use the shipped preset:

```powershell
.\Scripts\Start-MfdDemo.bat
```

## Choosing a path

| Use | When |
| --- | --- |
| Host-side `RGBA32` | You embed `mfd_runtime_api` and control the loop |
| Plugin-based | You run the standalone `mfd_window` host and want a pluggable sink |

To build your own sink, implement the plugin ABI from `mfd_window_plugin_api`
and model it on the shipped sample.

## C++ API reference

For the framebuffer plugin ABI (`mfd/window/WindowLauncherPlugin.h`) and its
pixel-format options, see the [C++ API Reference](../api.md), generated from
`mfd_window_plugin_api/include`.
