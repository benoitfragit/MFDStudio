# `mfd_editor_automation_sample_plugin`

Sample in-process automation plugin for `mfd_editor`, updated for the current
stable **ABI v3**.

## Purpose

This example is a reference implementation for the current stable editor
automation ABI. It is intentionally written as a readable walkthrough, not as a
minimal stub, and it stays on a pure native C ABI with no COM object model and
no COM link dependency at the plugin boundary.

The sample illustrates one high-value end-to-end workflow over
`MfdEditorAutomationHostApi`.

Exhaustive callback coverage lives in the loader test plugin:

- [../../mfd_editor/tests/plugins/ExtendedAutomationPlugin.cpp](../../mfd_editor/tests/plugins/ExtendedAutomationPlugin.cpp)

The sample is split into two phases:

- one mutation walkthrough inside a preview session followed by `rollback_session`
- one lightweight `commit_session` plus host-owned persistence calls

This keeps the example useful without committing the mutation-heavy demo content
to the live document.

## Covered Callbacks

Read-only queries:

- `get_snapshot_summary`
- `get_page_info`
- `get_reticle_asset_info`
- `get_layer_info`
- `get_page_reticle_info`

Session and diagnostics:

- `begin_session`
- `validate_session`
- `get_session_validation_diagnostic`
- `commit_session`
- `rollback_session`

Semantic edits:

- `create_page_asset`
- `instantiate_reticle_on_page`
- `move_page_reticle`
- `set_page_reticle_visibility`
- `set_page_reticle_draw_on_top`
- `set_page_reticle_transform`
- `set_page_reticle_layer`
- `create_layer`
- `set_layer_visibility`

Persistence and events:

- `save_asset`
- `save_all`
- `consume_pending_event`

## Walkthrough

The sample code:

- reads the current document summary
- caches one baseline page and one baseline reticle asset
- creates one temporary preview page
- creates one temporary layer on that page
- instantiates two reticles so reorder operations can be demonstrated
- moves one reticle, assigns its layer, changes visibility, draw order, and transform
- reads the resulting stable ids back through the query callbacks
- validates the session and probes the diagnostic accessor
- rolls the preview session back
- opens a second session just to illustrate `commit_session`
- saves one page, one reticle asset, then all authored assets
- drains the automation event queue

## Build

The sample is built when `MFD_BUILD_DEMO=ON`.

Target:

- `mfd_editor_automation_sample_plugin`

Output:

- `mfd_editor_automation_sample_plugin.dll`

## What To Read

- Public ABI contract: [../../mfd_editor_plugin_api/README.md](../../mfd_editor_plugin_api/README.md)
- Sample source: [src/EditorAutomationSamplePlugin.cpp](./src/EditorAutomationSamplePlugin.cpp)

## Why This Example Exists

It gives one compact reference for:

- the exported symbol `MfdGetEditorAutomationPluginApi`
- the `start / tick / stop / destroy` lifecycle
- the caller-owned UTF-8 buffer pattern used by the ABI
- session-oriented editor automation
- page, layer, and reticle manipulation through the stable ABI
- host-owned save flows
- event consumption through the host callback table

