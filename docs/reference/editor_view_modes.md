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

The menu-bar **View** menu exposes a **Panels** section to show or hide the
sidebar and inspector manually:

- **Panels > Sidebar**
- **Panels > Inspector**

These visibility preferences are persisted across editor sessions. When a wanted
panel is temporarily auto-collapsed on a narrow window, the matching entry is
shown unchecked and disabled so it never claims a hidden panel is visible; widen
the window to bring it back.

## View Menu

The **View** menu lives in the top menu bar and stays available at all times. Its
content adapts to the active workspace, so the same menu drives the page-preview
overlays and the reticle-studio display options without a separate per-panel
button. Even before a window is open it still exposes the **Panels** (sidebar and
inspector) toggles, since those panels render regardless of the loaded document.
When pending validation issues exist while the **Problems** panel is hidden, the
menu label shows `View !`.

The page and reticle viewports also expose two compact toolbar buttons in the
top-left corner:

- `?`: open the interaction help for the current viewport
- `R`: recenter the current editor camera without changing the authored JSON

In the page-preview and fullscreen workspaces, the **View** menu exposes these
editor-only controls, which never touch authored assets:

- **Layer Inspector**
- **Minimap**
- **Problems**
- **Highlight reticle usages**
- **Reticle names**
- **Gizmos**
- **Grid**
- **Snap to grid**
- **Grid step**
- **Fullscreen preview** (`F11`)

In the reticle-studio workspace, the same menu instead exposes the studio
display options:

- **Show page context**
- **Show primitive names**
- **Show gizmos**
- **Grid**
- **Snap to grid**
- **Grid step**

The visible grid, the snap toggle, and the shared `Grid step` are reused by
both the page preview and the reticle studio so placement reads the same way in
both workspaces.

## Fullscreen Preview

Fullscreen preview is toggled from the **View** menu (**Fullscreen preview**) or
with the `F11` key.

It is a pure editor layout mode that depends on no document state, so the toggle
is available in every workspace and from the moment the editor opens, even before
a window is loaded.

Behavior:

1. enable **Fullscreen preview** in the **View** menu, or press `F11`, to enter
2. the sidebar, inspector, page-context split, and docked helper panels are hidden
3. the page canvas stays interactive
4. disable the same entry, press `F11`, or press `Esc` to restore the previous layout

Fullscreen preview is an editor workspace mode, not an operating-system
fullscreen window mode.

The editor restores the previous panel visibility and pane sizes when
fullscreen preview exits.

## Shortcuts

- `F11`: toggle fullscreen preview from any workspace, at any time
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
