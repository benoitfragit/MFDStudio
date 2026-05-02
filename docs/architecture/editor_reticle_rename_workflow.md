# Editor Reticle Rename Workflow

`mfd_editor` now exposes one dedicated `ReticleRenameService` for the safe
rename workflow used when one authored reticle template is referenced by
several page JSON files.

The service keeps the business rules out of ImGui and follows the same
`BuildPlan()` / `Execute()` split already used by page removal, page import,
and page rename.

## Page Vs Reticle

The editor now treats these two rename domains explicitly:

- pages are authored assets referenced by window JSON files
- reticle templates are authored assets referenced by page JSON files

That distinction matters because renaming one page rewrites window metadata,
while renaming one reticle template rewrites page-level `template` references.

## Why A Dedicated Service

Renaming one reticle template safely is not just one local text edit:

- the template JSON `id` must be rewritten
- every source page under the scanned `assets/` root must be inspected
- static reticle references and page strobe references must be rewritten
- optional template-file renames must update relative image paths
- page-level collisions must be rejected before any disk mutation happens
- `_Exec` must stay ignored by default

Putting that logic behind one focused service makes the workflow testable and
reusable without embedding filesystem code in the UI layer.

## Service Contract

`ReticleRenameService` owns three public editor-facing types:

- `RenameReticleRequest`
- `RenameReticlePlan`
- `RenameReticleResult`

The request stays deliberately narrow:

- current template id in the live editor document
- requested new template id
- source `assets/` root
- `_Exec` inclusion flag, off by default
- optional template-file rename flag

The plan exposes:

- `oldReticleName`
- `newReticleName`
- current and target template JSON files
- every exact page-level reference found
- blocking page-level collisions
- concrete files that would be rewritten or deleted
- warnings shown before execution
- `canExecute`

## BuildPlan

`BuildPlan()` is read-only and performs all validation before the user can
confirm the rename.

It:

- validates the selected template and the requested new id
- refuses template files outside the protected source `assets/` root
- reuses `AssetReferenceIndexService` to discover every page or strobe
  reference under source assets
- overlays the current open pages from the live in-memory editor document so
  unsaved page edits are still planned correctly
- refuses the workflow when another source page or reticle JSON under the
  scanned root is invalid, because the editor can no longer guarantee full
  reference coverage
- blocks any global template-id collision
- blocks page-level collisions where one page already references the requested
  target template id
- computes the optional template-file move target and rejects filesystem
  collisions before execution

No file content is changed during this phase.

## Execute

`Execute()` writes the rename directly to source JSON assets.

It:

- rewrites the template JSON `id`
- rewrites every impacted page `staticReticles[*].template`
- rewrites every impacted page `strobe.template`
- optionally writes the template JSON to a new file name and deletes the old
  source file
- serializes current in-memory pages and the current in-memory template so
  local unsaved editor changes are preserved
- rewrites relative image paths automatically when the template file moves
- updates the current in-memory reticle library and current page references so
  the editor shell stays consistent immediately after the operation

This workflow is intentionally direct-to-disk. It does not wait for the next
`File > Save` because it may affect several page assets outside the current
window.

## Current UI Flow

The editor exposes the workflow from three entry points:

- reticle-library tree right click: `Rename reticle globally...`
- library reticle inspector button: `Rename reticle globally...`
- top menu: `Reticle > Rename selected library reticle globally...`

The popup shows:

- old and new template ids
- current and target template JSON files
- every exact page or strobe reference found
- the files that will be rewritten or deleted
- blocking page-level collisions
- the generated API / `mappingHash` warning

## Generated API Impact

Template ids feed generated transport maps and generated client API names.

After a successful rename, regenerate the generated client API before
rebuilding or shipping the authored asset set that exposes this template.

## Limitations And Intentional Tradeoffs

- only source assets are renamed by default; `_Exec` stays ignored
- current pages must already map to tracked source JSON files under the
  protected source asset root
- the workflow updates page template references only; there is no separate
  authored dynamic-template asset in the current editor model
- because several source files may be rewritten at once, this workflow should
  be considered a source-control level operation; use Git if you need a global
  rollback
