# Editor Asset Reference Index

`mfd_editor` now owns one dedicated `AssetReferenceIndexService` that scans the
source asset tree and builds the dependency graph consumed by editor-side
rename, import, highlight, and diagnostics workflows.

## Why This Service Exists

Several editor features need the same answers:

- which windows reference one page file
- which pages reference one reticle template
- which templates or inline reticles reference one image file
- which dependencies are missing
- which asset files are invalid JSON

Those workflows must not each re-implement their own filesystem traversal in
ImGui code. The index service is the shared, testable source of truth.

## Current Scope

The service performs one read-only recursive scan under the requested asset
root and returns:

- `PageReference` entries for `window -> page`
- `ReticleReference` entries for:
  - `page -> reticle template`
  - `page strobe -> reticle template`
  - `reticle -> image asset`
  - `window -> font/icon asset`
- `MissingAssetReference` entries for missing pages, templates, images, fonts,
  and icons
- `AssetScanError` entries for invalid JSON or ambiguous template metadata

## Design Rules

- `BuildIndex()` is read-only and never modifies the document or the disk.
- `_Exec` trees are ignored by default, but callers can opt into scanning them
  when a workflow must operate on the currently opened staged asset tree.
- The scan is resilient: invalid JSON is reported through `AssetScanError`
  instead of crashing the editor workflow.
- Paths are normalized before they are returned so later services do not need to
  guess how to compare them.

## Intended Consumers

The roadmap features that should consume this service instead of adding their
own ad hoc scanners are:

- page import with dependency copy
- global page rename
- global reticle-template rename
- highlight pages using the selected reticle
- problems panel and quick fixes

If a future editor feature needs authored asset usages, add the logic here or
build on top of this index instead of scanning from UI widgets.
