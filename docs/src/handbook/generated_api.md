# Generated Client API

For a client dedicated to one authored window, the generated API is the
preferred surface. It gives typed access to pages, reticles, exposed primitives,
strobes, and dynamic sets, while `CommandClient` remains the transport sender.

Use the generated API first whenever your application targets one known window.
Use raw name-based `CommandClient` helpers only for generic tooling, migration
code, or low-level debugging.

## The generated contract

Treat the generated header, the generated source, and the companion
`<window>.generated.map` as **one contract**:

- generation expects an explicit `OUTPUT_MAP`
- the client uses the map for raw-name fallback helpers
- `mfd_window` must load the matching `.generated.map` to accept generated
  id-based batches

If the generated C++ and the generated map drift apart, or the runtime did not
load the matching sidecar, generated batches are rejected.

## Minimal usage

A generated client links `mfd_client_api`, builds the transport with
`CommandClient`, then drives the generated UI class through page handles and
batches:

```cpp
#include "mfd/client/ClientSdk.h"
#include "tutorial_ui/TutorialUi.h"

mfd::CommandClient client(udpCommandTransport, generatedTransportMap);
if (!client.IsReady())
{
    // handle client.LastError()
}

tutorial_ui::TutorialUi generatedUi;
auto& page1 = generatedUi.Page1();

client.ActivatePage(page1);
generatedUi.Initialize();

// each cycle: mutate generated handles, then publish one batch
page1.mfdTutorialCircle.SetVisible(true);
const std::vector<mfd::UserCommand> commands = generatedUi.BuildBatch();
client.SendBatch(commands);
```

## `Run()` vs `Initialize()`

- `ui.Run()` starts one new local client cycle. It drops staged dirty state
  without asking the runtime to return to the authored baseline. This is the
  normal per-cycle path.
- `ui.Initialize()` is the full authored-state reset: it restores the local UI
  baseline, invalidates previously created dynamic-reticle handles, clears
  cached feedback, and makes the next built batch prepend a runtime
  `ResetWindowCommand`. Call it only when you want that full reinitialization.

## What the generated surface covers

- whole-window display control, page activation, and page view
- static reticle mutation and primitive-level text, geometry, time, image updates
- exposed-primitive patterns such as progress bars
- dynamic reticle creation, bulk update, and removal
- page-scoped strobe control and feedback
- publication flows: `BuildBatch()`, `BuildResetBatch()`,
  `BuildCommandBatch(sequence)`, `BuildResetCommandBatch(sequence)`,
  `SubmitLatest(...)`, and `SubmitReset(...)`

For time primitives the generated API exposes structured runtime controls
(numeric time bypass, bypass clear, UTC/local selection, field visibility)
instead of requiring raw runtime format strings.

When a window exposes a feedback transport, a client can detect window shutdown
over the connectionless UDP stream with `mfd::client::WindowLivenessMonitor` and
reset its connections cleanly.

## Linking

Generated code and shipped examples link only `mfd_client_api`, even when they
reach helpers such as `CommandClient`, `JsonLoader`, or `UserSpaceProjector`
through the packaged SDK surface. External CMake consumers use
`find_package(MFDStudioClientApi)` and `MFDStudio::ClientApi`. The reference
consumer is `examples/client_test_package`.

Two umbrella entry points matter most:

- `mfd/client/ClientSdk.h` for standalone applications and shipped examples
- `mfd/client/GeneratedUiSupport.h` for generated source files

## C++ API reference

For the exact signatures of `CommandClient`, `LatestBatchPublisher`,
`WindowLivenessMonitor`, and the client SDK helpers, see the
[C++ API Reference](../api.md), generated from `mfd_client_api/include`.
