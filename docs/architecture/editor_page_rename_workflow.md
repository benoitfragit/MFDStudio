# Editor Page Rename Workflow

`mfd_editor` now exposes one dedicated `PageRenameService` for the safe
rename workflow used when one authored page asset is shared by several window
JSON files.

The service keeps the business rules out of ImGui and follows the same
`BuildPlan()` / `Execute()` split already used by page removal and page import.

## Why A Dedicated Service

Renaming a page safely is not just one local text edit:

- the page JSON itself must be rewritten
- every window under the scanned asset root must be inspected
- `defaultPage` must be rewritten when it points to the renamed page
- name collisions must be rejected before any disk mutation happens
- the workflow must not special-case `_Exec` when it is part of the scanned tree

Putting that logic behind one focused service makes the workflow testable and
reusable without embedding filesystem code in the UI layer.

## Service Contract

`PageRenameService` owns three public editor-facing types:

- `RenamePageRequest`
- `RenamePagePlan`
- `RenamePageResult`

The request stays deliberately narrow:

- current page index in the live editor document
- requested new page name
- scanned asset root

The plan exposes:

- `oldPageName`
- `newPageName`
- target page JSON file
- every exact `window -> page` reference found
- every blocking collision
- the concrete files that would be rewritten
- warnings shown before execution
- `canExecute`

## BuildPlan

`BuildPlan()` is read-only and performs all validation before the user can
confirm the rename.

It:

- validates the selected page and the requested new name
- refuses page files outside the protected scanned asset root
- reloads the target page JSON from disk and checks that the saved name still
  matches the in-memory editor state
- reuses `AssetReferenceIndexService` to discover every window that
  references the target page
- refuses the workflow when another window JSON under the scanned root is
  invalid, because the editor can no longer guarantee full reference coverage
- replaces the current open window reference set with the live in-memory
  document state so unsaved local page-list collisions are still detected
- checks every referenced window for collisions against the requested page name
- marks only the files that truly need rewriting:
  the page JSON itself and the window JSON files whose `defaultPage` still
  points to the old page name

No file content is changed during this phase.

## Execute

`Execute()` writes the rename directly to the scanned JSON assets.

It:

- rewrites the page JSON `name` and legacy `id` fields, including
  `{"page": {...}}` wrapper documents
- rewrites each impacted window `defaultPage`
- includes staged `_Exec` assets whenever they are part of the scanned root
- updates the current in-memory page name and normalized page id so the editor
  shell stays consistent immediately after the operation

This workflow is intentionally direct-to-disk. It does not wait for the next
`File > Save` because it may affect several windows outside the current editor
document.

## Current UI Flow

The editor exposes the workflow from three entry points:

- page tree right click: `Rename page globally...`
- page inspector button: `Rename page globally...`
- top menu: `Page > Rename current page globally...`

The popup shows:

- old and new page names
- every scanned window reference found
- the files that will be rewritten
- blocking collisions
- the generated API / `mappingHash` warning

## Limitations And Intentional Tradeoffs

- the rename stays scoped to the current scanned asset root; it does not jump across unrelated asset trees automatically
- the workflow updates only `defaultPage` in window JSON files because page file
  paths remain unchanged
- because several source files may be rewritten at once, this workflow should be
  considered a source-control level operation; use Git if you need a global
  rollback
