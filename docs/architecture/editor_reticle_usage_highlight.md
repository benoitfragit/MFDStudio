# Editor Reticle Usage Highlight Workflow

The page-preview `Highlight reticle usages` option is an editor-only workflow
that answers one question quickly: "which pages currently use the selected
library reticle template?"

It is deliberately read-only. Enabling the overlay must not mutate the window,
page, or reticle JSON assets.

## Why This Exists

The same reticle template can be reused by several authored pages. Once the
library grows, manually hunting through every page becomes error-prone and slow.

The highlight workflow gives three pieces of feedback at once:

- the page tree marks every current-window page that uses the selected template
- the active page preview outlines matching instances directly on the canvas
- a small overlay reports how many pages currently reference that template

That feedback is driven by the same asset graph used by safe rename and import
workflows, so the editor does not maintain one separate scanner per feature.

## Service Boundary

`ReticleUsageHighlightService` owns the read-only computation.

Its responsibilities are:

- merge the in-memory editor document with the scanned asset index
- identify current-page reticle indices that reference the selected template
- identify page-level strobe references that also point to that template
- list additional page JSON files found under the current asset root
- ignore `_Exec` trees by default, unless the opened asset root is itself under
  `_Exec/.../assets`

Its responsibilities explicitly do not include:

- drawing ImGui widgets
- mutating selection
- mutating any JSON asset
- rescanning every frame

## Cache Rules

`EditorApplication` keeps one small cache keyed by:

- selected template id
- resolved asset root
- whether `_Exec` assets are explicitly included

The cache is invalidated whenever the editor document or tracked asset layout
changes, including:

- undo restore
- page import
- page remove/delete
- page rename
- reticle rename
- loading another window

This keeps the preview responsive while still reflecting the current authoring
state after each structural edit.

## Current UI Contract

When `View > Highlight reticle usages` is enabled:

- no selected library reticle: no overlay is drawn
- selected but unused reticle: one discreet "no page currently uses ..."
  message is shown
- selected and used reticle: matching pages are highlighted in the tree and the
  current page draws accent rectangles around matching instances

The overlay stays intentionally subtle so it does not interfere with normal page
selection, drag, copy/paste, clipping, or gizmo interaction.
