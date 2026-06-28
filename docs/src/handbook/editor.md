# Editor

`mfd_editor` is the visual authoring tool for windows, pages, and reticles. It
is optional: JSON files remain the source of truth, and you can author by hand,
in the editor, or mix both.

![Editor capture](../images/mfd_editor_capture.png)

## Start

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

The editor starts empty by design. Open a source window JSON or create a new
window from scratch — it never auto-loads staged `_Exec` copies, so you do not
accidentally edit runtime artifacts instead of the repository assets.

## Authoring model

The editor writes text and time alignment (`left`, `center`, `right`) directly
into the asset model, matching what both the preview gizmos and `mfd_window`
render at runtime. It keeps one shared editor-only visible grid, snap toggle,
and logical grid step across the page preview and reticle studio **without**
serializing those preferences into the authored JSON.

## Responsive shell

The central workspace keeps priority. Narrowing the window first compresses,
then auto-collapses the sidebar and inspector instead of blocking the resize.
Auto-collapse is temporary and never changes saved panel widths; use the **View**
menu or widen the window to bring panels back.

## Navigation sidebar

The **Pages** and **Reticle library** headers show live counts of entries passing
the current filter, and a line near the filter surfaces pending validation
problems (click it to open the Problems panel).

The filter box matches page and reticle names by default. Prefix the text to aim
one section:

- `page:` — pages only
- `reticle:` — reticles only
- `problem:` — only entries that still have validation issues

The branch holding the current selection stays expanded, and the first filtered
match opens automatically so results never hide behind a collapsed page.

## Page preview toolbar

The page-preview header carries a compact glyph toolbar to the left of the help
(`?`) button:

- **Zoom box** — arm it, then drag a rectangle over the preview to zoom the page
  camera onto that region (same result as the mouse wheel, framed precisely).
- **Smart select** — arm it, then drag a rectangle to add every reticle inside it
  to the selection. With a layer focused in the layer inspector the pick is limited
  to that layer; in full view it spans every visible layer.
- **Fit to page** — scale and recenter the drawn reticles so their bounding box
  fills the page border. It is a single undoable step (`Ctrl+Z`).

Zoom box and smart select are one-shot: after a drag (or `Esc`) they disarm and the
left button returns to normal selection and dragging.
