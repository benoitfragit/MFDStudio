# Generated Transport Map Specification

## Scope

This note defines the exact content of the runtime companion mapping file
generated next to a window asset:

- input entry point: `assets/windows/<window>.json`
- generated runtime mapping: `assets/windows/<window>.generated.map`

The same generation step must also emit the client header consumed by
applications. Both artifacts are derived from the exact same parsed window
model.

## Design Goals

- deterministic output for unchanged authored content
- stable transport IDs for unchanged authored objects
- human-inspectable debug format
- no manual synchronization between runtime and client artifacts
- explicit versioning and mismatch detection

## Transport Scope

The mapping file covers fixed authored identifiers only:

- page
- static reticle
- authored strobe reticle
- exposed primitive
- dynamic reticle template
- blink type
- page-local strobe catalog entry

The mapping file does not assign IDs to:

- dynamic runtime reticle instance IDs
- dynamic text payloads
- positions
- colors
- booleans
- thickness
- arbitrary metadata

### Strobe Selection Model

The strobe remains a page capability in the generated user-facing API, but the
transport map now carries one row per authored strobe entry so low-level
commands can select the active strobe without relying on raw strings.

- strobe commands remain page-scoped
- generated page APIs still expose a generic `strobe` handle with validity metadata
- generated pages may also expose named `StrobeType` entries such as
  `defaultStrobe`, `designatorStrobe`, or `strobe1`
- generated pages may also expose one authored strobe-reticle wrapper such as
  `defaultReticle` or `strobe1Reticle` so exposed primitives of the currently
  selected strobe can be patched without raw ids

## File Format

The `.generated.map` file is UTF-8 JSON with stable ordering and no generation
timestamp.

Top-level schema:

```json
{
  "schemaVersion": 1,
  "mappingHash": "4f5d1b...",
  "window": {
    "name": "mfd_tutorial",
    "title": "MFD Tutorial",
    "source": "mfd_tutorial.json"
  },
  "pages": [],
  "reticles": [],
  "primitives": [],
  "templates": [],
  "blinkTypes": [],
  "strobes": []
}
```

## Top-Level Fields

- `schemaVersion`
  - integer schema revision of the `.generated.map` format
  - initial value: `1`
- `mappingHash`
  - lower-case hexadecimal SHA-256 over the canonical semantic payload
  - identical between generated header and generated map
- `window`
  - descriptive window metadata used for validation and diagnostics

`window` fields:

- `name`
  - stable window logical identifier derived from the window file stem
- `title`
  - authored title when available
- `source`
  - source window file name relative to `assets/windows`

## Stable ID Strategy

IDs are deterministic 64-bit unsigned integers derived from canonical logical
keys, not from generation order counters.

Canonical key examples:

- page: `page/radar`
- reticle: `page/radar/reticle/heading_box`
- primitive: `page/radar/reticle/heading_box/primitive/heading_value`
- template: `template/radar_track`
- template primitive: `template/radar_track/primitive/contact_label`
- blink type: `page/radar/blink/attention`
- strobe entry: `page/radar/strobe/designator`

Rules:

- normalize authored names using the same normalization rules as runtime lookup
- hash the canonical key with SHA-256
- use the low 64 bits as the transport ID
- reject generation on collision
- IDs are unique within the window mapping payload

This avoids the instability of insertion-order counters while keeping the wire
format compact.

## Page Table

Each page entry contains:

```json
{
  "id": 1284925256178123456,
  "name": "Radar",
  "normalizedName": "radar",
  "hasStrobe": true,
  "defaultPage": false
}
```

Field meaning:

- `id`
  - stable page transport ID
- `name`
  - authored page name exposed publicly
- `normalizedName`
  - runtime lookup key
- `hasStrobe`
  - descriptive flag only, not a transport ID
- `defaultPage`
  - identifies the default active page

Generated page classes expose this stable id through `GeneratedId()` and the
matching map hash through `MappingHash()`. Client code should normally pass the
generated page object to page-level helpers:

```cpp
client.ActivatePage(ui.Radar());
client.SetPageView(ui.Radar(), center, zoom);
```

`CommandClient` extracts the page id and mapping hash from the generated page
wrapper and sends the id-only command with the matching `mappingHash`. Authored
page names remain available for tooling and compatibility helpers, but are
resolved locally before serialization.

## Reticle Table

Each static reticle entry contains:

```json
{
  "id": 7219032945529234141,
  "pageId": 1284925256178123456,
  "reticleId": "heading_box",
  "normalizedReticleId": "headingbox",
  "source": "static"
}
```

Field meaning:

- `id`
  - stable reticle transport ID
- `pageId`
  - owning page transport ID
- `reticleId`
  - authored reticle identifier
- `normalizedReticleId`
  - runtime lookup key
- `source`
  - `static` for page static reticles
  - `strobe` for authored strobe reticles

Dynamic runtime reticles are not part of this table because their instances are
not fixed authored objects.

Template-based strobe rows use the authored strobe `id` when it is present;
otherwise they reuse the referenced template id. Inline strobe rows require an
explicit authored `id`.

## Primitive Table

The primitive table contains only explicitly exposed primitives. Decorative or
internal primitives are omitted.

Exposed primitives authored inside strobe reticles are emitted the same way as
page-static reticle primitives: `ownerKind` stays `reticle`, and `ownerId`
points to the strobe-reticle row from the reticle table.

Each primitive entry contains:

```json
{
  "id": 510322874641882001,
  "ownerKind": "reticle",
  "ownerId": 7219032945529234141,
  "primitiveId": "heading_value",
  "normalizedPrimitiveId": "headingvalue",
  "primitiveType": "text",
  "exposed": true
}
```

Field meaning:

- `id`
  - stable primitive transport ID
- `ownerKind`
  - `reticle` or `template`
- `ownerId`
  - owning static reticle, strobe reticle, or template transport ID
- `primitiveId`
  - authored primitive identifier
- `normalizedPrimitiveId`
  - runtime lookup key
- `primitiveType`
  - authored primitive kind
- `exposed`
  - always `true` in emitted rows, kept for readability

## Template Table

Each authored reticle template usable for dynamic reticles contains:

```json
{
  "id": 2519234481201120044,
  "templateId": "radar_track",
  "normalizedTemplateId": "radartrack"
}
```

Template-owned exposed primitives appear in the primitive table with
`ownerKind: "template"`.

## Blink Type Table

Blink types are page-local authored identifiers resolved through IDs on the
wire. Each row contains:

```json
{
  "id": 2033118840123412200,
  "pageId": 1284925256178123456,
  "blinkType": "attention",
  "normalizedBlinkType": "attention",
  "durationMs": 200
}
```

`durationMs` is redundant with the page definition but kept in the map for fast
runtime validation and debugging.

## Strobe Table

Each authored page-local strobe entry contains:

```json
{
  "id": 14809254125336950102,
  "pageId": 9637022363458486384,
  "strobeName": "Designator",
  "normalizedStrobeName": "designator",
  "reticleId": "radar_strobe_designator",
  "defaultActive": false
}
```

Field meaning:

- `id`
  - stable strobe transport ID
- `pageId`
  - owning page transport ID
- `strobeName`
  - authored strobe catalog name exposed publicly
- `normalizedStrobeName`
  - runtime lookup key
- `reticleId`
  - public reticle id of the cursor reticle used by that strobe entry
- `defaultActive`
  - identifies the authored startup selection on that page

This table does not turn the strobe into one standalone transport object. It
only gives low-level command and runtime code one stable ID per authored
page-local strobe variant.

## Canonical Ordering

Every emitted array must be sorted lexicographically by canonical key before
serialization:

- pages by page canonical key
- reticles by reticle canonical key
- primitives by primitive canonical key
- templates by template canonical key
- blink types by blink canonical key
- strobes by strobe canonical key

Object fields must always be emitted in the same property order.

## Mapping Hash Calculation

`mappingHash` is computed from the canonical semantic mapping payload:

- exclude whitespace-only formatting differences
- exclude comments
- exclude file timestamps
- include schema version
- include every emitted table row and field value

The generated header embeds the same hash as a compile-time constant. Runtime
protocol messages also carry that hash so the runtime can reject mismatched
clients.

## Generator Responsibilities

The generator must:

- parse the window JSON and all referenced assets once
- decide primitive exposure once
- build one shared in-memory mapping model
- emit the `.generated.map` from that model
- emit the generated client header from that same model
- fail generation on duplicate canonical keys
- fail generation on transport ID collisions

## Runtime Responsibilities

The runtime must:

- accept the window JSON path as the only entry point
- resolve `<window>.generated.map` automatically next to the window JSON
- load and validate `schemaVersion`
- load and validate `mappingHash`
- build fast lookup tables keyed by transport IDs
- reject missing or mismatched mapping files with a clear runtime error

## Client Responsibilities

The generated client code must:

- embed transport IDs as internal implementation details only
- embed the mapping hash as an internal compatibility constant
- never parse or load `.generated.map` at runtime
- expose only high-level generated navigation to user code

## Command Transport Rules

When a command targets one fixed authored object covered by the mapping, the
generated client path must send only generated IDs on the wire for:

- page selection and page view updates
- static reticle targeting
- dynamic template targeting
- blink type targeting
- strobe selection targeting
- exposed primitive targeting

The normal generated transport path no longer duplicates authored names in
`mfd_commands.proto`.

Raw low-level `CommandClient` helpers may still accept authored names for:

- page selection
- static reticle targeting
- dynamic template targeting
- blink type targeting
- strobe selection targeting
- exposed primitive targeting

That compatibility path is local only:

- the client must load the companion `.generated.map`
- the client resolves authored names to transport IDs before serialization
- the serialized Protocol Buffers payload still carries only IDs plus
  `mappingHash`

Dynamic runtime reticle instances are not fixed authored objects, so they do
not appear in the generated map. They travel on the wire through runtime-scoped
integer ids allocated by the client layer and hidden from generated user code.

## Explicit Non-Goals

- no manual editing of `.generated.map`
- no standalone runtime mapping authoring workflow
- no user-visible raw transport ID API
- no user-facing standalone strobe transport object outside the page scope
