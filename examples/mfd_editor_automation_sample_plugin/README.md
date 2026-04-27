# `mfd_editor_automation_sample_plugin`

Minimal in-process automation plugin example for `mfd_editor`.

## Purpose

This sample shows the smallest valid plugin built against:

- [mfd_editor_plugin_api](../../mfd_editor_plugin_api)

It intentionally stays non-destructive:

- `start` reads one document summary through the host ABI
- `tick` does not mutate the editor
- `stop` clears local state
- `destroy` releases the plugin context

Use it as the smallest reference for:

- the exported symbol `MfdGetEditorAutomationPluginApi`
- the construction of `MfdEditorAutomationPluginApiV1`
- storing one `plugin_context`
- the `start / tick / stop / destroy` lifecycle
- calling stable host callbacks

## Build

The sample is built when `MFD_BUILD_DEMO=ON`.

Target:

- `mfd_editor_automation_sample_plugin`

Output:

- `mfd_editor_automation_sample_plugin.dll`

## Source

- [src/EditorAutomationSamplePlugin.cpp](./src/EditorAutomationSamplePlugin.cpp)
