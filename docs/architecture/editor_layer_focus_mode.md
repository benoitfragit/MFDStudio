# Editor Layer Focus Mode

The page-preview `Layer Inspector` strip is an editor-only focusing workflow
built on top of the existing page editor-layer metadata.

This feature does not add a new runtime layer model, does not change the page
schema, and does not serialize any new JSON fields.

## Goal

Large pages often keep several authoring layers visible at once. That is useful
for context, but it makes direct manipulation noisy when the user only wants to
edit one subset.

Layer focus solves that by separating two concerns:

- visibility: existing editor layers can already be shown or hidden
- edit focus: one visible layer can temporarily become the only editable layer

## Current Behavior

When `View > Layer Inspector` is enabled, the page preview shows a vertical
strip containing:

- `Full View`
- every existing editor layer in page order

Selecting `Full View` restores the normal page-preview behavior.

Selecting one layer activates focus mode:

- only reticles assigned to that layer remain selectable
- other visible layers stay rendered but are dimmed
- `Esc` exits focus mode before it falls back to the normal reticle-selection
  clear shortcut

If the focused layer is renamed or removed, the editor sanitizes the focus state
instead of keeping a dangling id.

## Controller Boundary

`LayerFocusController` owns the read-only decisions used by the page preview:

- strip entries
- whether focus is active
- whether one reticle is still selectable
- whether one reticle should be dimmed
- how one multi-selection is filtered when focus changes

This keeps selection/render rules out of the ImGui layer and gives the workflow
its own unit tests.

## Integration Rules

`EditorApplication` remains responsible for:

- wiring strip clicks to the editor state
- clearing focus when the strip is hidden
- clearing focus on page changes
- sanitizing the current page-reticle selection after layer edits
- rendering dimmed preview copies for non-focused layers

The result is intentionally conservative:

- full view remains the default
- hiding the strip does not mutate authored content
- newly inserted page reticles follow the focused layer when focus mode is
  active
