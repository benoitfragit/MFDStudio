# Editor Workspace Layout

The page-preview workspace now routes docked editor panels through one pure
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

The helper itself does not depend on ImGui state and is covered by dedicated
unit tests.

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
