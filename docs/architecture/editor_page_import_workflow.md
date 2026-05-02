# Editor Page Import Workflow

This note documents the `mfd_editor` page-import workflow introduced for
external page JSON assets.

## Goals

The import flow must let the editor:

- stage one external page JSON inside the current window
- discover referenced reticle templates before mutating the live document
- keep the UI thin by pushing path, collision, and dependency logic into one
  dedicated service
- reuse the same logic for menu-driven imports and OS drag & drop

## Service Boundary

The workflow is centered on `PageImportService` and keeps the same
`BuildPlan()` / `Execute()` split already used by other editor asset workflows.

### `BuildPlan()`

`BuildPlan()` is read-only. It:

- validates the selected page JSON
- infers the source asset root
- reuses `AssetReferenceIndexService` to discover page-to-template
  dependencies
- resolves page-name and reticle-id collisions
- computes target page/template files

### `Execute()`

`Execute()` applies one validated plan to the live in-memory editor state. It:

- loads the source page through a temporary wrapper window
- remaps imported template ids when collisions required renaming
- appends the imported page to the current document
- stages new template files in the current reticle library
- leaves persistence to the normal editor `Save` flow

That last point matters: importing a page immediately changes the live editor
document, but authored JSON files are still written through `File > Save`, the
same as new-page creation and page deletion.

## Collision Policy

The import workflow currently uses one deterministic policy:

- target file missing: copy as-is
- target file already exists with identical content: keep existing
- target file already exists with different content: create one renamed copy

For page names and reticle ids, the first rename candidate uses the
`_Imported` suffix, then `_Imported_2`, `_Imported_3`, and so on.

## UI Integration

Two entry points reuse the same planning path:

- `Page > Import page...`
- dropping a page JSON file onto the editor window

Both open the same popup that previews the staged page file, the reticle
dependency plan, and the resolved collision policy before execution.
