# MFDStudio C++ API Reference

This site is the generated C++ header reference for the public MFDStudio
surface. It is scoped to the API only.

For onboarding, authoring, integration, and architecture guidance, use the main
MFDStudio documentation portal (built with mdBook). The published portal links
back to this reference at its `/api/` path.

## Public Modules

The reference covers the public headers of:

- `mfd_common_api/include` — shared low-level model and transport types
- `mfd_api/include` — core runtime, JSON I/O, and the low-level command API
- `mfd_client_api/include` — client-side helpers and the generated-client support layer
- `mfd_runtime_api/include` — offscreen embedding of the runtime
- `mfd_window/include` — runtime host integration points
- `mfd_window_plugin_api/include` — public framebuffer-plugin SDK

The reference also includes the HUD integration headers:

- `examples/hud/client/src/HudSimulation.h`
- `examples/hud/client/src/HudController.h`

Those two files document the semantic SI-unit contract used to hand the shipped
HUD example to an external aircraft adapter.

## Client Integration Entry Points

For customer-facing client code, two umbrella headers matter most:

- `mfd/client/ClientSdk.h` for standalone applications and shipped examples
- `mfd/client/GeneratedUiSupport.h` for generated source files emitted by the
  client API generator
- `hud::HudInputSample` plus `hud::HudController::Populate()` for
  handing the HUD example to an external aircraft adapter

Generated code and shipped examples should link only `mfd_client_api`, even when
they reach lower-level helpers such as `CommandClient`, `JsonLoader`, or
`UserSpaceProjector` through the packaged SDK surface. External CMake consumers
use `find_package(MFDStudioClientApi)` and `MFDStudio::ClientApi`.

## Where To Go Next

The main user and integrator documentation lives in the mdBook portal, not here.
Start there for the generated client API workflow, offscreen embedding,
framebuffer capture, the JSON reference, and the build and architecture notes.
