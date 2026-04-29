# `mfd_editor_plugin_api`

Stable public contract for in-process automation plugins loaded by `mfd_editor`.

The current contract is **ABI v3**. The previous binary surface is no longer
accepted: plugins must be rebuilt against the updated header.

## Purpose

This project exposes one typed C ABI so external plugins can automate the
editor without linking against the editor's internal C++ classes.

The DLL boundary intentionally avoids:

- C++ classes
- COM/OLE automation objects
- STL containers
- exceptions across the boundary
- raw ImGui/UI widget access
- JSON commands at the ABI boundary

The editor keeps ownership of:

- the live document
- undo/redo
- validation
- JSON serialization
- disk persistence
- event production

Plugins receive one versioned host callback table and request semantic editor
operations through it.

## Public Files

- Public header: [include/mfd/editor/AutomationPlugin.h](./include/mfd/editor/AutomationPlugin.h)
- CMake target: [CMakeLists.txt](./CMakeLists.txt)
- Example plugin: [../examples/mfd_editor_automation_sample_plugin](../examples/mfd_editor_automation_sample_plugin)

## ABI Principles

The contract relies on:

- one `extern "C"` entry point
- POD structs only
- versioned tables plus `struct_size`
- opaque session handles
- caller-owned UTF-8 output buffers
- explicit result codes

The exported symbol remains:

- `MfdGetEditorAutomationPluginApi`

The header also defines:

- `MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION`
- `MFD_EDITOR_AUTOMATION_PLUGIN_ENTRY_POINT`
- `MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK`

## ABI Versioning

The current value of `MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION` is `3`.

Implications:

- old automation plugin DLLs compiled against the previous ABI are rejected
- the editor and plugins must agree on ABI version `3`
- future extensions may still append host callbacks at the end of the host
  table while preserving the leading layout

`MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK` is provided so plugins can probe one
callback safely from `struct_size` when a later editor adds more capabilities.

## Lifecycle

The host loads one DLL and then executes:

1. resolve `MfdGetEditorAutomationPluginApi`
2. retrieve `MfdEditorAutomationPluginApi`
3. validate `struct_size`, `abi_version`, and required callbacks
4. call `start(plugin_context, host, error)`
5. call `tick(plugin_context, error)` once per frame while loaded
6. call `stop(plugin_context)`
7. call `destroy(plugin_context)`

Rules:

- `start` validates prerequisites and caches only plugin-owned state
- `tick` must stay lightweight and non-blocking
- `stop` tolerates partial startup
- `destroy` releases every plugin-owned allocation
- `plugin_context` may be `nullptr` for stateless plugins
- host pointers must not outlive the plugin lifetime

## Host Surface

`MfdEditorAutomationHostApi` now exposes the following stable capabilities.

Read-only queries:

- `get_snapshot_summary`
- `get_window_info`
- `get_ui_state`
- `get_page_info`
- `get_page_blink_type_info`
- `get_reticle_asset_info`
- `get_reticle_asset_primitive_info`
- `get_page_reticle_info`
- `get_page_reticle_primitive_info`
- `get_layer_info`

Session and validation:

- `begin_session`
- `validate_session`
- `get_session_validation_diagnostic`
- `commit_session`
- `rollback_session`

Semantic mutations:

- `create_page_asset`
- `delete_page_asset`
- `create_reticle_asset`
- `delete_reticle_asset`
- `instantiate_reticle_on_page`
- `delete_page_reticle`
- `move_page_reticle`
- `set_page_reticle_visibility`
- `set_page_reticle_draw_on_top`
- `set_page_reticle_transform`
- `set_page_reticle_layer`
- `set_reticle_asset_visibility`
- `set_reticle_asset_draw_on_top`
- `set_reticle_transform`
- `set_reticle_clipping`
- `create_layer`
- `replace_layer`
- `delete_layer`
- `set_layer_visibility`
- `upsert_page_blink_type`
- `delete_page_blink_type`
- `set_page_default_blink_type`
- `set_page_reticle_blink`
- `assign_page_strobe_template`
- `delete_page_strobe`
- `select_entity`

Persistence and events:

- `save_all`
- `save_asset`
- `export_json_preview`
- `consume_pending_event`

## Key Public Types

Summary and query types:

- `MfdEditorAutomationSnapshotSummary`
- `MfdEditorAutomationWindowInfo`
- `MfdEditorAutomationUiState`
- `MfdEditorAutomationPageInfo`
- `MfdEditorAutomationPageBlinkTypeInfo`
- `MfdEditorAutomationReticleAssetInfo`
- `MfdEditorAutomationPageReticleInfo`
- `MfdEditorAutomationLayerInfo`
- `MfdEditorAutomationPrimitiveInfo`

Validation and events:

- `MfdEditorAutomationValidationSummary`
- `MfdEditorAutomationValidationDiagnostic`
- `MfdEditorAutomationEvent`

Mutation requests:

- `MfdEditorAutomationCreatePageAssetRequest`
- `MfdEditorAutomationDeletePageAssetRequest`
- `MfdEditorAutomationCreateReticleAssetRequest`
- `MfdEditorAutomationDeleteReticleAssetRequest`
- `MfdEditorAutomationInstantiateReticleOnPageRequest`
- `MfdEditorAutomationDeletePageReticleRequest`
- `MfdEditorAutomationMovePageReticleRequest`
- `MfdEditorAutomationSetPageReticleVisibilityRequest`
- `MfdEditorAutomationSetPageReticleDrawOnTopRequest`
- `MfdEditorAutomationSetPageReticleTransformRequest`
- `MfdEditorAutomationSetPageReticleLayerRequest`
- `MfdEditorAutomationSetReticleAssetVisibilityRequest`
- `MfdEditorAutomationSetReticleAssetDrawOnTopRequest`
- `MfdEditorAutomationSetReticleTransformRequest`
- `MfdEditorAutomationSetReticleClippingRequest`
- `MfdEditorAutomationCreateLayerRequest`
- `MfdEditorAutomationReplaceLayerRequest`
- `MfdEditorAutomationDeleteLayerRequest`
- `MfdEditorAutomationSetLayerVisibilityRequest`
- `MfdEditorAutomationUpsertPageBlinkTypeRequest`
- `MfdEditorAutomationDeletePageBlinkTypeRequest`
- `MfdEditorAutomationSetPageDefaultBlinkTypeRequest`
- `MfdEditorAutomationSetPageReticleBlinkRequest`
- `MfdEditorAutomationAssignPageStrobeTemplateRequest`
- `MfdEditorAutomationDeletePageStrobeRequest`
- `MfdEditorAutomationSelectEntityRequest`
- `MfdEditorAutomationExportJsonPreviewRequest`
- `MfdEditorAutomationSaveAssetRequest`

Shared utility types:

- `MfdEditorStringView`
- `MfdEditorUtf8Buffer`
- `MfdEditorAutomationTransform2D`
- `MfdEditorAutomationSessionHandle`
- `MfdEditorAutomationJsonPreviewResult`
- `MfdEditorAutomationSaveSummary`

## String Handling

Input strings use `MfdEditorStringView`.

Output strings use `MfdEditorUtf8Buffer`:

- `data` points to caller-owned storage
- `capacity` is the available byte count
- `size` receives the full logical byte length excluding `\0`

When the buffer is too small, the callee:

- writes as much as possible
- null-terminates when `capacity > 0`
- returns `MfdEditorAutomationResultCode_BufferTooSmall`

## Recommended Workflow

For a non-trivial mutation flow:

1. read the document through `get_snapshot_summary`, `get_window_info`,
   `get_ui_state`, `get_page_info`, `get_reticle_asset_info`,
   `get_page_reticle_info`, `get_layer_info`, and primitive-info queries
2. open a preview session with `begin_session`
3. reuse the stable ids returned by the queries when issuing semantic actions
4. call `validate_session`
5. inspect diagnostics with `get_session_validation_diagnostic` when needed
6. `commit_session` or `rollback_session`
7. persist with `save_asset` or `save_all`, or export one dry-run payload with `export_json_preview`
8. drain `consume_pending_event` if the plugin needs structured feedback

The editor always owns the final save implementation.

## Example Flow

The readable sample plugin demonstrates one non-destructive walkthrough:

1. read the first page and first reticle asset
2. start one preview session
3. create a temporary editor layer
4. instantiate one reticle on the page
5. assign the new reticle to the temporary layer
6. update visibility and transform
7. validate the preview session
8. roll everything back
9. consume the queued automation events

See:

- [../examples/mfd_editor_automation_sample_plugin/src/EditorAutomationSamplePlugin.cpp](../examples/mfd_editor_automation_sample_plugin/src/EditorAutomationSamplePlugin.cpp)

For exhaustive callback coverage, see:

- [../tests/mfd_editor/plugins/ExtendedAutomationPlugin.cpp](../tests/mfd_editor/plugins/ExtendedAutomationPlugin.cpp)

## Minimal Factory

```cpp
extern "C" MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode
MFD_EDITOR_AUTOMATION_CALL
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApi* outApi,
                                MfdEditorUtf8Buffer* error) noexcept
{
    if (outApi == nullptr)
    {
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = new (std::nothrow) MyPluginContext();
    if (context == nullptr)
    {
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    outApi->info.plugin_id = MfdEditorStringView {"demo.plugin", 11U};
    outApi->info.display_name = MfdEditorStringView {"Demo Plugin", 11U};
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}
```

## CMake

```cmake
add_library(my_editor_plugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MyEditorPlugin.cpp)

target_link_libraries(my_editor_plugin
    PRIVATE
        mfd::editor_plugin_api)

target_compile_features(my_editor_plugin PRIVATE cxx_std_17)
target_compile_definitions(my_editor_plugin PRIVATE MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)
```

Recommended integration:

- build one `SHARED` DLL per plugin
- compile as `C++17`
- link `mfd::editor_plugin_api`
- keep the repository warning policy
- set an explicit debugger working directory when needed

## Test Coverage

The repository covers the ABI with `GoogleTest`, including:

- nominal loading and mutation
- granular `save_asset`
- host-owned `save_all`
- validation summary and diagnostic retrieval
- structured event consumption
- rollback on unload
- runtime plugin failures
- startup refusal
- ABI mismatch rejection
- incomplete plugin-table rejection
- stateless plugin acceptance

