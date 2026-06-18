# Editor View Modes

This page documents the editor-only page preview modes available in
`mfd_editor`.

## Responsive Shell Layout

The editor shell arranges the left **sidebar**, the central **workspace** and
the right **inspector** responsively. The workspace always keeps priority:

- when the window is wide, all three columns are shown (`Wide` layout)
- when it narrows, auxiliary panels first compress toward their minimum width
- when space is still tight, the sidebar auto-collapses (`Compact` layout),
  then the inspector auto-collapses (`Focus` layout)

Auto-collapse is temporary: it never changes your saved panel widths, and
widening the window restores the hidden panel at its previous width. The menu
bar shows a `[Compact layout]` or `[Focus layout]` marker while panels are
auto-hidden.

The page preview **View** button (next to the fullscreen button) exposes a
**Panels** section to show or hide the sidebar and inspector manually:

- **Panels > Sidebar**
- **Panels > Inspector**

These visibility preferences are persisted across editor sessions. When a wanted
panel is temporarily auto-collapsed on a narrow window, the matching entry is
shown unchecked and disabled so it never claims a hidden panel is visible; widen
the window to bring it back.

## Page Preview View Menu

The page preview header exposes its **View** button for preview-only overlays
and the shell **Panels** toggles described above.

The page and reticle viewports also expose two compact toolbar buttons in the
top-left corner:

- `?`: open the interaction help for the current viewport
- `R`: recenter the current editor camera without changing the authored JSON

These editor-only view controls never touch authored assets:

- **Layer Inspector**
- **Minimap**
- **Problems**
- **Highlight reticle usages**
- **Reticle names**
- **Gizmos**
- **Grid**
- **Snap to grid**
- **Grid step**
- **Page context**

The visible grid, the snap toggle, and the shared `Grid step` are reused by
both the page preview and the reticle studio so placement reads the same way in
both workspaces.

## Fullscreen Preview

The page-preview editing header exposes one small fullscreen button immediately
next to **View**.

The button is intentionally hidden from the reticle-studio **Page context**
panel so fullscreen stays scoped to the page-editing workspace.

Behavior:

1. click the button to enter fullscreen preview
2. the sidebar, inspector, page-context split, and docked helper panels are hidden
3. the page canvas stays interactive
4. click the same button again, press `F11`, or press `Esc` to restore the previous layout

Fullscreen preview is an editor workspace mode, not an operating-system
fullscreen window mode.

The editor restores the previous panel visibility and pane sizes when
fullscreen preview exits.

## Shortcuts

- `F11`: toggle fullscreen preview while the page-editing workspace is active
- `Esc`: exit fullscreen preview first, then fall back to the normal selection clear workflow

## Persistence

View toggles and fullscreen preview state are not serialized into authored
assets.

The shared visible-grid preferences are persisted in the editor UI state file:

- **Grid**
- **Snap to grid**
- **Grid step**

That persistence remains editor-only and does not modify any window, page, or
reticle JSON file.
