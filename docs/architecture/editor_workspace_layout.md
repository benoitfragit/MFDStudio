# Editor Workspace Layout

The editor resolves its layout through small, pure, testable helpers instead of
hard-coding panel rules in the ImGui drawing path:

- `EditorResponsiveLayout` resolves the **root shell** (sidebar, workspace,
  inspector) into one responsive arrangement.
- `EditorWorkspaceLayout` resolves the **page-preview docks** and the
  **page-context / reticle-studio split** inside the workspace.

## Responsive Shell Layout

The root shell no longer enforces a rigid stack of cumulative minimum widths.
`EditorResponsiveLayout::ComputeShellLayout()` takes the available width, the
splitter width, the workspace floor and each side panel's preference
(`wantVisible`, `preferredWidth`, `minWidth`) and returns:

- one `ShellLayoutMode`: `Wide` (sidebar + workspace + inspector), `Compact`
  (workspace + one auxiliary panel) or `Focus` (workspace only)
- effective, clamped widths for the sidebar, the workspace and the inspector
- per-panel `autoCollapsed` flags

### Degradation policy

- the editable **workspace keeps priority** and never drops below its floor
- side panels are **preferences, not hard constraints**: they compress toward
  their minimum width before anything else gives way
- when width is still insufficient, panels collapse along a strict priority
  ladder: the **sidebar collapses before the inspector**, so resizing walks a
  monotonic `Wide -> Compact -> Focus` sequence with no visual swapping
- auto-collapse is **transient**: it is reported through `autoCollapsed` and
  never written back into the user's persisted width or visibility preferences,
  so widening the window restores the panel at its untouched preferred width
- a narrow window only lowers the native minimum to a sane technical floor
  (`720x480`); the full three-column layout is no longer required to resize

The shell surfaces the current mode in the menu bar and lets the user toggle
panel visibility from the **Panels** section of the page-preview **View** button,
so an auto-collapsed or user-hidden panel always has a visible way back. The
visibility preferences are persisted across sessions, while the transient
auto-collapse state is shown as a disabled, unchecked entry.

## Page-Preview Docks

The page-preview workspace routes docked editor panels through the pure
`EditorWorkspaceLayout` helper instead of hard-coding every panel directly in
the ImGui drawing path.

## Goal

The editor preview needs optional panels without letting them cover the actual
page content:

- the page preview itself must remain the primary surface
- docked tools must reserve real space instead of drawing on top of the canvas
- small workspaces must preserve one minimum editable preview area

## Current Usage

The helper now drives two page-preview docks:

- the left **Layer Inspector** panel
- the bottom **Problems** panel

When those view options are enabled:

- the layer inspector is docked beside the preview instead of overlaying it
- the problems panel is docked under the preview instead of overlaying it
- the problems panel spans the full width of the preview column
- the diagnostic text lives inside one scrollable child region

This keeps the minimap, reticle handles and preview interactions unobstructed.

## Helper Contract

`EditorWorkspaceLayout` exposes one pure `ComputeWorkspaceLayout()` function.

The request describes:

- the available workspace width and height
- the spacing between docked regions
- one optional leading dock
- one optional bottom dock
- the minimum preview width and height that must survive

The result returns three clamped regions:

- `leadingPanel`
- `previewPanel`
- `bottomPanel`

The same module also exposes `ComputeStudioSplitLayout()` for the
library-studio workspace. The reticle studio is the primary surface there, so
the page-context pane compresses toward its minimum and then auto-collapses
(reported through `secondaryAutoCollapsed`) before the studio loses its own
minimum width. Like the shell helper, this never overwrites the persisted
page-context width.

Both helpers are pure, do not depend on ImGui state, sanitize non-finite,
negative or too-small inputs, and are covered by dedicated unit tests.

## Integration Boundary

`EditorApplication` stays responsible for:

- deciding which editor panels are visible for the current mode
- building validation diagnostics once per frame
- rendering layer-preview thumbnails inside the leading dock
- rendering the preview inside the computed `previewPanel`
- rendering the docked diagnostics panel in the computed `bottomPanel`

Overlay-only features such as gizmos, minimap labels and reticle annotations
still stay in `DrawPreviewOverlays()`. Only the persistent diagnostics panel
and the layer-inspector dock have moved out of that overlay path.

## Editor Shell Split

The editor shell is still one class, but its implementation is now split by
responsibility:

- `mfd_editor/src/EditorApplication.cpp` keeps the application loop,
  document-level editing primitives, shared preview interactions, and the
  cross-cutting geometry helpers still shared by the shell and inspectors
- `mfd_editor/src/application/EditorApplicationShell.cpp` owns the root ImGui
  shell, menu bar, sidebar, workspace split, empty state, and inspector-host
  routing
- `mfd_editor/src/application/EditorApplicationInspectors.cpp` owns the
  window, page, strobe, title, reticle, and primitive inspectors together with
  their editor-only drafting state
- `mfd_editor/src/EditorApplicationWorkflow.cpp` owns modal workflows, native
  file-dialog orchestration, safe asset-management popups, and tutorial-driven
  authoring actions
- `mfd_editor/include/internal/application/EditorApplicationState.h` groups
  private state by responsibility instead of keeping one flat tail of members
- `mfd_editor/include/internal/application/EditorApplicationAuthoringSupport.h`
  centralizes editor-only rules shared by preview code and inspectors
- `mfd_editor/include/internal/application/EditorApplicationInternal.h`
  centralizes small private helpers reused by multiple implementation units
  without widening the public API

This keeps workspace-layout behavior close to the preview code while moving the
long authoring and popup workflows out of the main shell compilation unit. The
internal headers now live under `include/internal/application/` so the split is
visible in the tree instead of being hidden inside `src/`.
