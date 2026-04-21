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
- exposed primitive
- dynamic reticle template
- blink type

The mapping file does not assign IDs to:

- strobe
- dynamic runtime reticle instance IDs
- dynamic text payloads
- positions
- colors
- booleans
- thickness
- arbitrary metadata

### Strobe Exception

The strobe is a page capability, not an addressable transport object.

- no strobe ID is generated
- no strobe table is emitted
- strobe commands remain page-scoped
- generated page APIs expose a generic `strobe` handle with validity metadata

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
  "blinkTypes": []
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
  - currently always `static`

Dynamic runtime reticles are not part of this table because their instances are
not fixed authored objects.

## Primitive Table

The primitive table contains only explicitly exposed primitives. Decorative or
internal primitives are omitted.

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
  - owning reticle or template transport ID
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

## Canonical Ordering

Every emitted array must be sorted lexicographically by canonical key before
serialization:

- pages by page canonical key
- reticles by reticle canonical key
- primitives by primitive canonical key
- templates by template canonical key
- blink types by blink canonical key

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

## Explicit Non-Goals

- no manual editing of `.generated.map`
- no standalone runtime mapping authoring workflow
- no user-visible raw transport ID API
- no strobe ID table
