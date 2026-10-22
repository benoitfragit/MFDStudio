# API LEVEL 1 widgets

Widgets and Containers belong to **API LEVEL 1**. They extend the existing
SDK, renderer and command/feedback envelopes; there is no LEVEL 2 or second
transport. The host owns the window, graphics context, ImGui context and scene.

## Assets and independent instances

A reusable `widgets/*.json` definition has `schemaVersion: 1`, an `id`, a
semantic `type`, `preferredSize`, `hitRegion`, `range`, `defaults` and `visuals`.
Supported types are `button`, `toggle`, `checkbox`, `radio`, `slider`, `knob`,
`gauge`, `progressBar`, `indicator`, `numericDisplay` and `label`.

Page `widgets` contain independent instances. `definition` is a relative path
from the Page file. An instance has its own `id`, position, optional size,
visibility, enabled state, exposure and optional `overrides` for `checked`,
`warning`, `value` and `text`. Editing a shared definition updates all its
instances while preserving their explicit overrides. Duplicating an instance
shares its definition; duplicating a definition makes a new asset.

```json
{
  "id": "ModeA",
  "definition": "../widgets/toggle.json",
  "exposed": true,
  "overrides": {"checked": false}
}
```

Definitions use booleans for checked/warning, a floating-point value with finite
minimum/maximum/step, integer decimal places (0–9), text (4096 UTF-8 bytes), and
enumerations for type/state/binding. They do not execute scripts or contain
application business logic. Radio group exclusivity is decided by the client.

## Shared layout

Root instances use Page coordinates. Parent ownership is explicit:

```json
{"id": "Panel", "children": [{"kind": "widget", "id": "ModeA"}]}
```

An omitted child `kind` still means container. Widget leaves use the same
Canvas, Linear, Grid, Overlay, fit-content, resize, scrolling and clipping
kernel as containers. Their preferred size participates in fitting and scroll
bounds. Children inherit ancestor visibility and enabled state. Parent placement
owns resize mode, alignment, grid row/column/span and render order.

Limits are 4096 containers and 4096 widgets per Page, 8192 ownership edges and
64 container levels. Widget leaves do not consume another container nesting
level. IDs must match references exactly; normalized duplicates are rejected.

## Visuals and hit region

Each definition has a required `normal` visual and optional `hover`, `pressed`,
`checked`, `disabled`, `warning` and `pending` variants. Priority is disabled,
pressed, pending, warning, checked, hover, normal; absent variants use normal.
Visuals reuse the existing reticle/primitives model, including image resources.
Numeric bindings target an existing primitive's text, rotation, scale, position
or visibility. Numeric text uses fixed decimal formatting; other text bindings
use confirmed text. The logical hit rectangle is independent of painted shapes.

The editor's library edits definitions without inserting them. Select a visual
state, choose **Edit visual in Reticle studio**, edit the existing primitive
controls, then **Apply edited visual** from Widgets or the library. The working
reticle is retained in the reticle library. Value bindings are edited beside the
shared definition controls. The Page inspector edits only instance overrides
unless **Edit shared definition** is enabled.

Widget visuals are drawn within their ancestor scissor. Legacy Page reticles
remain above the container/widget layer, and layer-local background restoration
redraws the mixed layer. Page-erasure reticle clipping inside a widget definition
is rejected: use container viewport clipping for widget-local clipping.
Pointer occlusion skips invisible/fully transparent primitives, samples cached
image alpha (including staged imports) and respects rectangle/circle/ring outline
interiors. Image masks share the bounded texture cache; masks above 4096 × 4096
or unavailable CPU data conservatively block. Text, complex curves and external
streams currently use transformed bounds, rather than glyph/stream pixel masks.

## Client authority and bounded recovery

Input reports `Pressed`, `Released`, `Activated`, `CheckedChangeRequested`,
`SelectionChangeRequested`, `ValueChangeRequested`, `DragBegin`, `DragEnd` and
`Cancelled`. A request changes transient pressed/pending visuals, never confirmed
checked/value state. Confirm by setting the typed handle's property and calling
`Acknowledge(requestId)` before sending the generated batch. Acknowledge without
a property change rejects the proposal. A stale request ID fails atomically.
Setters cache outgoing intent; they are not remotely synchronized getters.

Every instance, including `exposed: false`, has a Page-scoped numeric ID in the
transport map. Exposure controls only generation of a public typed handle.
`RuntimeFeedbackState::TakeWidgetEvents()` returns typed events; `Matches(event)`
selects an exposed handle. Generic clients route private instances by
`event.pageId`/`event.widgetId` and submit an `UpdateWidgetCommand` using these
numeric IDs and the generated mapping hash. No raw protobuf parsing is needed.

UDP remains best effort. Session/sequence checks reject stale and duplicate
events; queues keep at most 1024 events, identity history at most 4096 entries,
and periodic snapshots repair confirmed-state knowledge. Snapshot work is at
most 64 widgets per pass. An overflowing queue may lose transitions. The host
releases capture on pointer release, cancellation, focus/page/visibility changes
or its 30-second safety timeout. Pending proposals expire independently after
2 seconds. Clients should resend current authoritative state after reconnecting;
UDP send success is not an acknowledgement.

## Compatibility, imports and migration

| Content | Runtime requirement | Editor requirement |
| --- | --- | --- |
| LEVEL 0 | Core only | Core only |
| LEVEL 1 without widget instances | Existing container runtime DLL | Existing container editor DLL |
| LEVEL 1 with widgets, even with zero containers | Container runtime plus `mfd_widgets_api1.dll` | Container editor plus `mfd_editor_widgets_api1.dll` |

The widget runtime never depends on the widget editor DLL. Both new ABIs use
explicit versions, capabilities and structure sizes. The existing unsized
`MfdContainerChildV1` layout is unchanged. A missing/incompatible widget module
rejects activation or opening a widget-bearing document before replacing live
state. Container-only assets do not require a widget DLL. Rebuild the shared SDK
for widget C++ APIs; previously distributed editors cannot be patched to preserve
unknown widget fields, so do not save widget assets with old editors.

Import checks actual Page content both when planning and executing. Widget-only
Pages require LEVEL 1; missing/unknown/corrupt definitions, escaping paths,
changed source files and missing editor capability reject before mutation.
Definition and binary resource collisions are deduplicated when identical or
remapped deterministically when different. Relative dependency paths remain
inside the asset root (the enclosing `assets` directory, otherwise the Window
folder), including resolved symlinks. Save commits Pages, definitions and staged
resources using the existing recoverable transaction. Autosave preserves staged
binary resources; import does not partially write files.

Downgrade shows per-Page counts of containers, widget instances and edges.
Confirmation removes these instances/edges/visuals in one undo step and retains
shared definitions. There is no automatic static fallback. A legacy-only Page
from a LEVEL 1 Window can still be imported into LEVEL 0.

## Wire, generation and deployment

`extensions/apilevel1/widgets/proto/mfd_widgets.proto` adds
`UserCommand.update_widget = 13` and `FeedbackEnvelope.widget = 5`. Existing
container fields and other tags are unchanged. New commands follow the existing
validation, serialization, generated-ID resolution, dispatch and transactional
batch pipeline. Widget transitions/acknowledgements are retained in order by
`LatestBatchPublisher`, with a 4096-command budget including in-flight work.

The existing `client_api_generate_ui()` signature is unchanged. Its conditional
widget generator runs for nonempty widget catalogs independently of containers.
All widget routes contribute to the changed Page's map; unused definitions and
empty catalogs do not. Existing assets without widgets retain generated bytes,
IDs and hashes. `--print-inputs` includes referenced definitions and images.
The installed extension script is under `tools/extensions/apilevel1/widgets/generator`.
Both development SDKs include the command, feedback, container and widget source
schemas together in `share/mfd/proto`, preserving their relative imports.

See [the example](../../../examples/apilevel1/widgets/README.md) and
[the authoring walkthrough](../../tutorials/16_author_and_drive_widgets.md).
