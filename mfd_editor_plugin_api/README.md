# `mfd_editor_plugin_api`

Public contract for the in-process automation plugin loaded by `mfd_editor`.

This project exposes one **stable typed C ABI**. The DLL boundary intentionally
avoids:

- C++ classes
- STL containers
- exceptions
- JSON payloads at the boundary

The editor keeps ownership of:

- the live document
- preview rendering
- validation
- undo/redo
- JSON serialization
- disk persistence

The plugin only receives one versioned function table and asks the host to
perform semantic editor actions.

## Scope

- No visible ImGui widget is exposed.
- No raw UI automation is exposed.
- No network transport is imposed.
- No assistant, agent, or AI concept exists in this contract.
- ABI `v1` is intentionally small and acts as the stable foundation.

## Public Files

- Public header: [include/mfd/editor/AutomationPlugin.h](./include/mfd/editor/AutomationPlugin.h)
- CMake target definition: [CMakeLists.txt](./CMakeLists.txt)
- Minimal example: [../examples/mfd_editor_automation_sample_plugin](../examples/mfd_editor_automation_sample_plugin)

## ABI Principles

The binary contract relies on:

- one `extern "C"` entry point
- versioned POD `struct` types
- one `struct_size` field on every versioned structure
- opaque handles
- caller-owned UTF-8 buffers
- host/plugin function tables

The exported plugin symbol is:

- `MfdGetEditorAutomationPluginApi`

The header also defines:

- `MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION`
- `MFD_EDITOR_AUTOMATION_PLUGIN_ENTRY_POINT`

## Compatibility

This ABI is meant to be more flexible than one exported C++ virtual interface.
It no longer requires:

- the same repository revision
- the same compiler family
- the same C++ runtime

The following constraints still apply:

- same binary platform
- same binary architecture
- plugin loaded in the same process as the editor

So one faulty plugin can still crash the editor. The host guards common
contract errors, not arbitrary memory corruption.

## Lifecycle

The host loads one DLL and then executes:

1. resolve `MfdGetEditorAutomationPluginApi`
2. retrieve `MfdEditorAutomationPluginApiV1`
3. validate `struct_size`, `abi_version`, and required callbacks
4. call `start(plugin_context, host, error)`
5. call `tick(plugin_context, error)` once per frame while loaded
6. call `stop(plugin_context)`
7. call `destroy(plugin_context)`

Rules:

- `start` must validate its prerequisites and return a clear error on failure
- `tick` must stay lightweight and non-blocking
- `stop` must tolerate partial startup
- `destroy` must release all plugin-owned state
- `plugin_context` may be `nullptr` for stateless plugins
- the plugin must not keep host pointers beyond its lifetime

## Function Tables

### Host Table

The host provides `MfdEditorAutomationHostApiV1`.

Currently exposed callbacks:

- `get_snapshot_summary`
- `get_page_info`
- `begin_session`
- `validate_session`
- `create_page_asset`
- `commit_session`
- `rollback_session`
- `save_all`

### Plugin Table

The plugin returns `MfdEditorAutomationPluginApiV1`.

It contains:

- `info`
- `plugin_context`
- `start`
- `tick`
- `stop`
- `destroy`

## Important Types

The public header defines:

- `MfdEditorAutomationResultCode`
- `MfdEditorStringView`
- `MfdEditorUtf8Buffer`
- `MfdEditorAutomationSessionHandle`
- `MfdEditorAutomationSnapshotSummaryV1`
- `MfdEditorAutomationPageInfoV1`
- `MfdEditorAutomationValidationSummaryV1`
- `MfdEditorAutomationSaveSummaryV1`
- `MfdEditorAutomationCreatePageAssetRequestV1`

General pattern:

- the caller prepares the structure
- sets `struct_size`
- provides output buffers when needed
- calls the host/plugin function
- reads the result code and outputs

## String Handling

Input strings use `MfdEditorStringView`.

Output strings use `MfdEditorUtf8Buffer`:

- `data` points to caller-owned storage
- `capacity` gives the available byte count
- `size` receives the full logical string length, excluding `\0`

If the buffer is too small:

- the callee writes as much as possible
- terminates with `\0` when `capacity > 0`
- returns `MfdEditorAutomationResultCode_BufferTooSmall`

## Recommended Workflow

For any non-trivial mutation:

1. read context with `get_snapshot_summary` and `get_page_info`
2. open a preview session with `begin_session`
3. send one or more semantic actions
4. validate with `validate_session`
5. `commit_session` or `rollback_session`
6. optionally call `save_all`

The save itself always remains editor-owned.

## Current ABI `v1` Surface

The stable surface exposed today covers:

- reading one summary of the current document
- reading one page by index
- opening one preview session
- validating one session
- creating one page asset
- committing one session
- rolling back one session
- saving all authored assets

The internal automation core inside the editor is richer. Other semantic
actions remain internal for now and can be promoted in a future ABI revision
without breaking `v1`.

## Minimal Example

The plugin exports `MfdGetEditorAutomationPluginApi`, allocates its context when
needed, then returns one callback table.

```cpp
extern "C" MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode
MFD_EDITOR_AUTOMATION_CALL
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApiV1* outApi,
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

Example session on the plugin side:

```cpp
MfdEditorAutomationSessionHandle session = 0U;
host->begin_session(host->host_context,
                    MfdEditorStringView {"demo-session", 12U},
                    &session,
                    error);

MfdEditorAutomationCreatePageAssetRequestV1 request {};
request.struct_size = sizeof(request);
request.session = session;
request.name = MfdEditorStringView {"PluginPage", 10U};
request.title = MfdEditorStringView {"Plugin Page", 11U};
request.relative_source_path = MfdEditorStringView {"plugin_page.json", 16U};

char createdPageIdStorage[128] {};
request.created_page_id =
    MfdEditorUtf8Buffer {createdPageIdStorage, sizeof(createdPageIdStorage), 0U};

MfdEditorAutomationSaveSummaryV1 saveSummary {};
saveSummary.struct_size = sizeof(saveSummary);

host->create_page_asset(host->host_context, &request, error);
host->commit_session(host->host_context, session, error);
host->save_all(host->host_context, &saveSummary, error);
```

## CMake

Declare one plugin directly with standard CMake:

```cmake
add_library(my_editor_plugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MyEditorPlugin.cpp)

target_link_libraries(my_editor_plugin
    PRIVATE
        mfd::editor_plugin_api)

target_compile_features(my_editor_plugin PRIVATE cxx_std_17)
target_compile_definitions(my_editor_plugin PRIVATE MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)
```

Recommended repository integration:

- build the plugin as one `SHARED` DLL
- link `mfd::editor_plugin_api`
- compile as `C++17`
- define `MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS` for the plugin target
- apply the same warning policy as the repository
- set an explicit debugger working directory when needed

## Tests

The repository already covers this ABI with `GoogleTest`:

- nominal loading
- plugin-driven mutation
- implicit rollback on unload
- plugin runtime errors
- startup refusal
- ABI mismatch
- incomplete callback table
- stateless plugin acceptance
- plugin-triggered save all

The sample plugin stays intentionally minimal and non-destructive.
