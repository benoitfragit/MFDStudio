# Editor Reticle Extraction Workflow

The `Extract as reticle...` workflow turns one selected page-reticle block
into one reusable library template, then replaces that block on the page with a
single instance of the new template.

The goal is to factor repeated authored content without visually changing the
page.

## Goal

This workflow exists for two common authoring situations:

- a page contains several reticles that now form one logical symbol
- a one-off page reticle has become reusable and should move into the shared
  reticle library

The extraction must preserve the page rendering closely enough that the user
can refactor layout without having to rebuild the symbol by hand.

## Service Boundary

`ReticleExtractionService` owns the non-UI logic.

Its responsibilities are:

- validate the current page-reticle selection
- choose one target template id and JSON file, including collision-safe renames
- flatten the selected reticle primitives into one new template-local
  coordinate space
- build the replacement page-reticle instance
- apply the mutation through `Execute()`

Its responsibilities explicitly do not include:

- drawing ImGui controls
- pushing undo snapshots
- saving JSON files to disk immediately

`BuildPlan()` is read-only. `Execute()` mutates the in-memory document and the
tracked file layout.

## Current MVP

The current editor entry points operate on selected page reticles, not on a
separate page-primitive selection mode.

That still covers the intended MVP because each selected page reticle may
already contain:

- simple geometric primitives
- text primitives
- image primitives whose file dependencies stay under the current `assets`
  root

During planning, every primitive is flattened into world space, translated back
into the new template-local anchor space, and copied into the extracted
template.

The replacement instance is then inserted back at the first selected reticle
index so page ordering remains stable.

## Validation Rules

The current implementation accepts only conservative cases:

- at least one selected page reticle
- one contiguous page-reticle block
- visible reticles only
- no clipping override
- no page-reticle blink binding
- one shared editor layer across the selection
- one shared `drawOnTop` mode across the selection
- image dependencies kept under the current `assets` root

The workflow currently rejects:

- non-contiguous selections
- clipped reticles
- mixed regular / draw-on-top selections
- mixed editor-layer selections
- external image files outside the current `assets` root

Those cases fail during planning with one clear blocking error instead of
partially mutating the page.

## UI Contract

`EditorApplication` exposes the workflow from the current page-reticle
selection through:

- the page-reticle inspector
- the multi-selection inspector
- the page-preview context menu

The popup shows:

- the selected source reticle ids
- the requested template id and optional file path
- the final collision-adjusted target id and file path
- the number of flattened primitives
- the replacement reticle id and retained layer / draw-order mode

Executing the plan:

- pushes one undo snapshot
- stages the new template in memory
- stages the tracked template JSON file path
- replaces the selected page-reticle block with one instance
- keeps the replacement instance selected

The new template is persisted later through `File > Save`, matching the other
editor staging workflows.
