# Editor View Modes

This page documents the editor-only page preview modes available in
`mfd_editor`.

## View Menu

The page preview header exposes one **View** menu.

These toggles are session-only editor preferences:

- **Layer Inspector**
- **Minimap**
- **Problems**
- **Highlight reticle usages**
- **Reticle names**
- **Gizmos**
- **Page context**

They do not rewrite the authored window, page, or reticle JSON files.

## Fullscreen Preview

The page preview header also exposes one small fullscreen button immediately
next to **View**.

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

- `F11`: toggle fullscreen preview
- `Esc`: exit fullscreen preview first, then fall back to the normal selection clear workflow

## Persistence

View toggles and fullscreen preview state are not serialized into authored
assets.
