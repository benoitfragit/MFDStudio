# Architecture Notes

This section gathers the deeper design notes that sit behind the onboarding
guides, tutorials, and JSON reference.

Use these pages when you already understand the normal runtime workflow and
need the lower-level design constraints that shaped the generated client API or
the transport mapping model.

If you need the client-facing rules rather than the internal rationale, read
[Generated Client API Standardization](../standards/mfd_generated_client_api_standardization.md)
first, then come back here for the design detail behind those rules.

## Read This Section When

- you are changing the generated client API surface
- you need the exact authored-to-transport mapping rules
- you are reviewing compatibility boundaries between generated helpers and the
  low-level runtime commands
- you want the architectural rationale behind a public API choice

## Pages

| Page | Use it when |
| --- | --- |
| [Generated Client API Architecture](./generated_client_api.md) | you are shaping the typed generated client layer exposed to applications |
| [Generated Transport Map Specification](./generated_transport_map.md) | you need the stable rules mapping authored names to runtime transport IDs |
| [Editor Asset Reference Index](./editor_asset_reference_index.md) | you are changing editor workflows that need one shared source of truth for `window -> page -> reticle -> asset` dependencies |

If you are new to the project, start with:

- [Project README](../../README.md)
- [Documentation Guide](../README.md)
- [Quick Start](../QUICKSTART.md)
- [Core Concepts](../CONCEPTS.md)
