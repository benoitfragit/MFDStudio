# Editor

`mfd_editor` is the visual authoring tool for windows, pages, and reticles. It
is optional: JSON files remain the source of truth, and you can author by hand,
in the editor, or mix both.

![Editor showcase](../images/editor/editor-showcase.png)

This overview uses one loaded workspace because it keeps the main authoring
surfaces visible at the same time:

- menu bar: global entry points for file, page, reticle, and help workflows
- navigation sidebar: page tree plus reticle library, with one shared filter
- layer inspector and helper panels: layer focus, minimap, and validation state
- page preview: the main authoring canvas for selection, zoom, and layout work
- right inspector: the editable properties of the current page, reticle, or primitive

The shell uses one Material-inspired dark design system throughout the executable
and the API LEVEL 1 extension: Segoe UI on Windows (with a readable fallback),
tonal elevation instead of stacked outlines, aligned label/value rows, turquoise
state accents and blue interaction accents. Rounded project chips, circular ghost
icon buttons, spacing and focus states are shared by the app bar, sidebars,
viewport tools and inspectors; extension code never creates a second visual theme.

## Start

```powershell
cmake --build --preset debug-win32 --target mfd_editor
```

The editor starts with a branded but intentionally empty resume hub. The
monochrome MFDStudio mark is drawn from compiled geometry and does not depend
on the optional `branding/` directory. Open a source window JSON or create a
new window from scratch: it never auto-loads staged `_Exec` copies, so you do
not accidentally edit runtime artifacts instead of the repository assets.

![Editor start page](../images/editor/editor-startpage.png)

The start page keeps the first actions explicit:

- open one existing authored window asset
- create one new window from scratch
- launch the integrated tutorial
- reopen one recent source window quickly

## Open existing assets

Use **Open window** from the start page or **File > Open window asset...** from
the menu bar to browse one authored window JSON under one repository example
asset tree, such as `examples/demo/assets/windows`.

The editor opens source assets under `examples/*/assets` and never auto-loads
staged `_Exec` copies, so you stay on the authored files that belong under
version control.

![Loaded editor workspace](../images/editor/editor-asset-loaded.png)

Once one window is loaded, the normal authoring workspace takes over:

- the page tree mirrors the authored window hierarchy
- the page preview stays centered on the active page
- the right inspector follows the current selection
- the shared filter helps you jump between pages and reticles without leaving the workspace

## Authoring model

The editor writes text and time alignment (`left`, `center`, `right`) directly
into the asset model, matching what both the preview gizmos and `mfd_window`
render at runtime. It keeps one shared editor-only visible grid, snap toggle,
and logical grid step across the page preview and reticle studio **without**
serializing those preferences into the authored JSON.

The **API LEVEL** selector is available both in **Create new window** and in the
Window inspector. It exposes `0 — Legacy` and `1 — Containers`. Older Window files with no
`apiLevel` load identically; saving writes `apiLevel: 0` explicitly. Unsupported
or malformed levels are rejected before the document can be edited. The level
is part of the Window snapshot used by the existing Undo/Redo workflow.

At LEVEL 1, **Containers** is the third collapsible left-sidebar section, below
Pages and Reticles. If the editor extension is missing or incompatible, the
section is read-only and explains the problem. Downgrading a Window that owns
containers requires explicit confirmation; deletion and the level change form
one undoable operation. See [API LEVEL 1 containers](./containers.md).

## Responsive shell

The central workspace keeps priority. Narrowing the window first compresses,
then auto-collapses the sidebar and inspector instead of blocking the resize.
Auto-collapse is temporary and never changes saved panel widths; use the **View**
menu or widen the window to bring panels back.

Inspector fields keep labels in a stable left column and controls in a flexible
right column. Collapsible groups use quiet rounded tonal headers, while selected
content and active tools use brighter state layers. The preview toolbar floats as
an elevated borderless pill above the canvas and uses a filled circular state for
armed one-shot tools. Sidebar, workspace and inspector panes use surface tone and
spacing rather than nested outlines to establish hierarchy.

In the fully loaded workspace, the left sidebar exposes pages and reticles, the
center keeps the preview tools visible, and the right inspector follows the
current selection. This is the recommended layout for authoring and for the
focused screenshots used below.

## Navigation sidebar

![Page sidebar controls](../images/editor/editor-sidebarpagecontrols.png)

The top half of the sidebar is where page authoring stays anchored:

- the shared filter narrows the visible page and reticle lists from one query
- the page tree keeps the authored window, page file, and page-local instances visible
- the selected page stays highlighted in the tree and mirrored in the inspector
- the quick-action row keeps the most common page actions close to the tree

The **Pages** and **Reticle library** headers show live counts of entries passing
the current filter, and a line near the filter surfaces pending validation
problems (click it to open the Problems panel).

The filter box matches page and reticle names by default. Prefix the text to aim
one section:

- `page:` - pages only
- `reticle:` - reticles only
- `problem:` - only entries that still have validation issues

The branch holding the current selection stays expanded, and the first filtered
match opens automatically so results never hide behind a collapsed page.

The sidebar container itself never scrolls: the **Pages** and **Reticle library**
sections each own their scroll region so only the long list under the pointer moves.

![Reticle library controls](../images/editor/editor-sidebarreticlecontrols.png)

The lower half keeps the library reticles separate from page instances:

- the library list stays reusable across every page of the current window
- selecting one template opens its shared definition without mutating page-local ids
- the lower quick-action row keeps library-specific actions near the template list

## Page preview toolbar

The page-preview header carries a compact glyph toolbar to the left of the help
(`?`) button:

- **Zoom box** - arm it, then drag a rectangle over the preview to zoom the page
  camera onto that region (same result as the mouse wheel, framed precisely).
- **Smart select** - arm it, then drag a rectangle to add every reticle inside it
  to the selection. With a layer focused in the layer inspector the pick is limited
  to that layer; in full view it spans every visible layer.
- **Fit to page** - scale and recenter the drawn reticles so their bounding box
  fills the page border. It is a single undoable step (`Ctrl+Z`).

Zoom box and smart select are one-shot: after a drag (or `Esc`) they disarm and the
left button returns to normal selection and dragging.

## Integrated tutorial coach

The guided flow contains 39 ordered steps. Its coach is a bounded, scrollable
panel: it keeps the current step, stage progress and required action readable
without consuming the entire preview on a short window. Target halos use the
same editor palette, stay inside the viewport work area and move around the
target when the preferred side has insufficient room. Starting the tutorial
temporarily opens the inspector sections needed by the current target without
overwriting the user's persisted inspector preferences.

## Reticle studio

![Reticle studio](../images/editor/editor-reticlestudio.png)

Opening one library reticle replaces the page preview with the reticle studio:

- the center canvas focuses one shared template at primitive level
- primitive labels and handles make geometry edits explicit
- the right inspector edits default appearance plus the primitive list
- the left library remains available so you can jump to another shared template quickly

## Validation problems

A line near the sidebar filter surfaces pending validation problems; clicking it
opens the docked **Problems** panel. Opening an asset that still has validation
problems reveals that panel automatically so an invalid asset never loads silently.

## Fullscreen preview

![Fullscreen preview](../images/editor/editor-fullscreen.png)

Fullscreen preview hides the surrounding editor chrome so the page canvas keeps
priority. Use it when you want to inspect placement, labels, or the active page
composition without the sidebar, inspector, minimap, or docked helper panels
competing for space.

## Color controls

All authored colors use the same current-color button, including creation dialogs, page titles and external-stream replacement colors. Click it to open the standard RGBA selector with exactly eight preset shortcuts on the right. Both RGBA edits and presets apply immediately; click outside or press Escape to close. Each continuous edit can be undone in one step. See [Color presets](../reference/colors.md#editor). The guided progress-bar exposure step teaches opening the selector, choosing Yellow, observing the preview and closing the selector.

## Recovery snapshots and rename failures

Recovery snapshots keep their existing filename and JSON bundle format. A new
snapshot is serialized, written to an adjacent temporary file, flushed, closed
and verified before replacing the committed snapshot. Failed writes or
replacement leave the previous snapshot intact. The status bar reports the
failure and another attempt is deferred for 30 seconds; only a successful
replacement resets the autosave success timer. Restore and clean-exit retention
rules are unchanged. Atomic replacement gives reader-visible consistency, not
a guarantee against power loss.

Document Save, recovery-bundle restoration, and global page/reticle renames
share the same transactional writer. They stage and verify every changed file
before modifying live assets. Original files are backed up, new contents promoted, then obsolete
reticle files removed. The in-memory document and file layout change only after
that transaction succeeds. A handled failure rolls back the changed files.
If rollback itself fails, the error lists the affected target and retained
`.mfd_backup_*` file: preserve those files, close the editor, and restore the
listed originals before reopening the asset tree. These multi-file operations
are not crash-atomic and assume no concurrent external writer. Primary errors and
secondary restore/removal errors are reported together. A cleanup error after
all promotions explicitly says **Assets committed** and retains the affected
transaction paths; it never rolls back a transaction whose backups were already
partly cleaned. Preserve any reported backup until the document is verified.

Undo and Redo keep the same history and selection semantics. When their history
stack is empty, the editor reports that immediately without copying the document.
