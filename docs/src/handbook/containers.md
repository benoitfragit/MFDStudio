# API LEVEL 1 containers

API LEVEL 1 adds an optional recursive layout layer while preserving the complete
LEVEL 0 page, reticle, primitive, transport, and generated-client contract. Set
`"apiLevel": 1` on the Window to enable it. A LEVEL 0 Window never loads either
LEVEL 1 DLL.

## Compatibility and transitions

The Window level is a capability ceiling. Pages do not store an `apiLevel`; their
minimum requirement is inferred from actual content:

| Page content | LEVEL 0 Window | LEVEL 1 Window |
| --- | --- | --- |
| Legacy reticles/primitives only | Supported | Supported |
| One or more containers | Rejected before mutation | Supported |

Import therefore ignores the source Window's declared level. A legacy-only Page
exported from a LEVEL 1 Window can be imported into LEVEL 0. A Page containing
containers cannot: planning and execution both re-check compatibility, and a
failure leaves the document and files unchanged.

Changing a Window from LEVEL 1 to LEVEL 0 is immediate when all Pages are legacy.
If any Page contains containers, the editor opens a destructive confirmation:
**Cancel** preserves everything; **Delete containers and downgrade** removes all
containers and changes the level as one undoable operation. Containers are never
kept as silently ignored data.

## Document model

Each Page may contain a flat `containers` catalog. Stable `children[].id` edges
form the hierarchy and have exactly one owner. Cycles, duplicate ownership,
unknown IDs, non-finite geometry, more than 4096 definitions, and depth beyond 64
are rejected. Grid row and column counts are bounded to 1–4096.

Common container fields are:

- `id`, `name`, `type`, `position`, `size`, `minimumSize`, and `sizeMode`;
- `visible`, `enabled`, and `exposed`;
- `padding`, `spacing`, `orientation`, `alignment`, and `grid`;
- `appearance` for optional background and border;
- `scrolling` for independent axes, clipping, offset, and scrollbar style;
- ordered `children`, whose placement stores resize mode, grid cell/span,
  alignment, and `renderOrder`.

For a selected child container, the extension-provided inspector edits these
parent-owned placement fields directly; manual JSON editing is not required.

The supported `type` values are `canvas`, `linear`, `grid`, and `overlay`.
Canvas and Overlay children use parent-local authored geometry; `fixed`,
`proportional`, and `stretch` define resize behavior. Linear preserves each
child's preferred main-axis size, places children from the leading edge, and
applies alignment on the cross axis. Overflow remains scrollable. Grid uses
uniform rows/columns, spacing and spans. Overlay alone sorts children by
`renderOrder`; the other types keep authored child order.

`sizeMode` controls the container's preferred size:

- `fixed` keeps both authored dimensions;
- `fitContent` fits both dimensions to nested content plus padding;
- `fitHorizontal` fits width and keeps authored height;
- `fitVertical` fits height and keeps authored width.

Resolution is bottom-up, respects `minimumSize`, and is shared by editor and
runtime. An empty fitted axis resolves to padding or minimum size. Parent-owned
stretch placement can override a child's preferred size. Canvas/Overlay fitting
measures authored child offsets and preferred sizes before proportional/stretch
placement, avoiding circular parent/child size dependencies. Scrolling starts at
the authored viewport and proceeds right/down; negative-side overflow should be
repositioned or included with a fit mode.

The ownership edge is intentionally separate from the container definition so
future graphical nodes can use the same transform, clipping, and input contract.
In LEVEL 1, only child containers participate in container hierarchy ownership.
Legacy reticles and primitives keep their existing Page/layer ownership and APIs.

## Rendering and input

The runtime DLL resolves bounds, viewport, effective visibility/enabled state,
scroll limits, and scrollbar geometry. The host renderer retains the graphics
context and draws container backgrounds, borders, and scrollbars before the
existing ordered legacy reticles. The editor preview calls this same host-side
decoration renderer, so appearance changes are visible while authoring. There is
no second drawing API and runtime scrollbars do not use ImGui.

A hidden ancestor removes its subtree from rendering and input. A disabled
ancestor only removes input; descendants keep their authored state, so enabling
the parent does not overwrite an independently disabled child.

Wheel input is routed to the deepest visible and enabled viewport under the
pointer. When that region cannot move farther in the requested direction, the
gesture bubbles to an eligible ancestor. Ancestor clipping is respected. Thumb
dragging maps directly to the shared clamped scroll offset; hover and active
colors come from the authored scrollbar appearance.

## Editor

For LEVEL 1 Windows, **Containers** appears in the existing left sidebar directly
below **Reticles**. The tree is the Page hierarchy itself and supports root/child
creation, rename, duplicate, delete, and reparenting. Tree and canvas selection
share one selection state. The inspector edits geometry, layout, appearance,
scrolling, visibility, enabled state, generated-API exposure, and all four size
modes. Both sidebar separators are draggable and their proportions persist.

Selecting a container displays the existing preview-style outline and corner gizmo. A resize
captures authored geometry once, creates one undo snapshot, and applies the final
parent size through the same layout engine used by runtime preview. Layout-driven
children are recomputed once; proportional children use `layoutReferenceSize`.
Content-fitted axes retain their authored values during a gizmo resize: only the
fixed axis can be resized in a single-axis fit mode, and `fitContent` shows the
selection outline without resize handles. Parent placement still takes precedence.

Extension-specific inspector controls are drawn by
`mfd_editor_containers_api1.dll` through a versioned host-owned UI facade. The EXE
owns the ImGui context, frame, fonts, theme, input, and rendering. The ABI passes
only fixed-layout C records and callbacks; no STL ownership or ImGui context
crosses the DLL boundary. Registration is tied to the loaded DLL and selection
lifetime, and unload removes callbacks before unloading code.

The facade also owns section and aligned-field presentation. Container type,
geometry, placement, layout, scrolling and appearance therefore use the same
neutral section cards, label column, typography, spacing and interaction states
as LEVEL 0 inspectors. The extension supplies domain labels and values only; it
cannot drift into a separate ImGui theme. Section support is an optional
`struct_size`-guarded tail callback: an older facade remains binary compatible
and receives the same controls expanded with text headings.

If a LEVEL 1 Window is opened without a compatible editor DLL, the sidebar shows
a read-only diagnostic and does not expose broken actions. LEVEL 0 editing does
not attempt to load this DLL.

## Generated C++ control

Only containers with `"exposed": true` receive a page-scoped transport ID and a
strongly typed `mfd::client::Container` handle. For example:

```cpp
ui.Containers().MainPanel().SetVisible(true);
ui.Containers().MainPanel().SetEnabled(false);
ui.Containers().MainPanel().SetScrollOffset({0.0f, 0.2f});
```

The existing generator entry point dynamically loads
`extensions/apilevel1/generator/generate_containers.py` only when container
content needs it. A LEVEL 1 Window containing only legacy content, or only
non-exposed containers, introduces no container handles or transport entries.
Equivalent LEVEL 0 inputs therefore keep byte-identical generated files, IDs,
and mapping hashes.

Container updates use the authoritative `UserCommand` protobuf envelope and send
partial visibility, enabled, or scroll-offset patches—not layout trees. Batching
coalesces independent fields for the same page/container identity.

## DLLs and deployment

- `mfd_containers_api1.dll`: versioned runtime layout/state ABI;
- `mfd_editor_containers_api1.dll`: versioned editor UI-facade ABI.

A Window configured for LEVEL 1 requires a compatible runtime DLL even while a
legacy-only Page is active. A missing or incompatible DLL is a clear load error,
not partial rendering. The runtime DLL never depends on the editor DLL. See
`examples/apilevel1` for a mixed Window and generated client. After building the
example, `Scripts/Start-APILevel1Containers.bat` launches both from the staged
runtime tree; the example opens directly on its Containers Page.
