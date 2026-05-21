# mfd_common_api

`mfd_common_api` is the internal low-level API module shared by the repository
targets.

It owns the static components that are reused by `mfd_api`,
`mfd_client_api`, and the tests:

- `mfd_model`
- `mfd_transport`

This module now owns its own `include/`, `src/`, and `proto/` trees.

It is intentionally not packaged as a user-facing SDK. The staged client SDK
republishes only the header subset needed by `MFDStudioClientApi`, including
the standalone client examples and generated helpers, while the rest of the
module stays internal to the build graph.
