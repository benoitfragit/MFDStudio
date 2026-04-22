# Drive A Window From A Live Client

This tutorial shows how an external application can animate reticles in real
time without knowing anything about the window UI implementation.

If you already explored `client_mockup`, this tutorial is the natural next step:

- the mockup showed the API from an operator UI
- this page shows the same API from a standalone application loop

For the detailed mapping between mockup controls and public client calls, also
read
[11 Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md).

## At A Glance

```mermaid
flowchart LR
    A[External application] -->|UDP protobuf commands| B[MFD window]
    B --> C[Active page]
    C --> D[Reticles updated in real time]
```

The external client always needs:

- the UDP address and port
- one transport-compatible way to address authored objects

That can be either:

- generated bindings
- raw transport IDs already known by your application
- or the companion `.generated.map` loaded locally so raw name-based
  `CommandClient` helpers can resolve authored names before serialization

This is the key idea:

- the window owns the UI
- the client owns the live data
- they only share a command contract

On the window side, the recommended runtime model is:

- one render thread at the window frame rate
- one background UDP I/O thread receiving commands and sending strobe feedback

## Step 1 - Know the UDP endpoint

Assume the target window exposes:

```json
"commands": {
  "udp": {
    "enabled": true,
    "address": "127.0.0.1",
    "port": 47220,
    "maxPacketSize": 16384
  }
}
```

Your external client can use those values directly.

## Step 2 - Create a command client

```cpp
#include "mfd/control/CommandClient.h"
#include "mfd/io/JsonLoader.h"

mfd::JsonLoader loader;
const auto loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages.json");

if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
{
    return 1;
}

mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
```

Recommended first check:

```cpp
if (!client.IsReady())
{
    // Log client.LastError() and abort or retry.
}
```

If your application already knows transport IDs and `mappingHash`, it can skip
JSON loading entirely and send typed id-based commands directly.

## Step 3 - Activate a page once

```cpp
client.ActivatePage("Radar");
```

Do this once when the operator changes context, not every frame.

This is exactly the same semantic action as `Activate page now` in the mockup.

## Step 4 - Adjust whole-window display when needed

Window-level display controls are also available through the same client:

```cpp
client.SetWindowColorInverted(true);
client.SetWindowBrightness(0.55f);
client.SetWindowDisabled(false);
```

This is useful for:

- host-driven night mode
- brightness dimming
- hardware-driven blanking or maintenance black screens
- hardware display states that affect the whole rendered window at once

This matches the mockup `Window display` panel.

## Step 5 - Animate one reticle every cycle

This example updates one reticle every 20 ms:

```cpp
#include "mfd/control/CommandClient.h"

#include <chrono>
#include <cmath>
#include <thread>

int main()
{
    mfd::WindowUdpCommandTransport transport;
    transport.enabled = true;
    transport.address = "127.0.0.1";
    transport.port = 47220;
    transport.maxPacketSize = 16384;

    mfd::CommandClient client(transport);
    client.ActivatePage("Radar");

    float angle = 0.0f;

    using namespace std::chrono_literals;

    while (true)
    {
        angle += 0.03f;

        mfd::ReticlePatch patch;
        patch.visible = true;
        patch.blinkEnabled = true;
        patch.blinkType = std::string {"fast"};
        patch.position = mfd::Vec2 {
            std::cos(angle) * 0.35f,
            std::sin(angle) * 0.35f
        };
        patch.color = mfd::ColorRgba {77, 224, 255, 255};

        client.UpdateReticle("Radar", "fixed_track_alpha", patch);

        std::this_thread::sleep_for(20ms);
    }
}
```

This matches the mockup `Reticle` inspector, but without the operator UI.

## Step 6 - Send several updates during the same cycle

If several reticles must update together, prefer a batch:

```cpp
std::vector<mfd::UserCommand> commands;

commands.push_back(mfd::UpdateReticleCommand {
    mfd::StaticReticleHandle {"Radar", "track_a"},
    mfd::ReticlePatch {.position = mfd::Vec2 {-0.20f, 0.15f},
                       .blinkEnabled = true,
                       .blinkType = std::string {"caution"},
                       .color = mfd::ColorRgba {120, 255, 154, 255},
                       .visible = true}});

commands.push_back(mfd::UpdateReticleCommand {
    mfd::StaticReticleHandle {"Radar", "track_b"},
    mfd::ReticlePatch {.position = mfd::Vec2 {0.32f, -0.10f},
                       .blinkEnabled = true,
                       .blinkType = std::string {"fast"},
                       .color = mfd::ColorRgba {255, 144, 112, 255},
                       .visible = true}});

commands.push_back(mfd::UpdateWindowDisplayCommand {
    mfd::WindowDisplayPatch {.invertColors = false, .brightness = 0.65f, .disabled = false}});

client.SendBatch(commands, 42);
```

Use one `sequence` value per external cycle if you want a stable cycle id in the
transport stream.

This is the same batching principle used by the cockpit simulator in the
mockup.

## Step 7 - Use dynamic reticles for runtime-owned symbols

If the object may appear or disappear at runtime, use dynamic reticles instead
of trying to patch static JSON content into existence.

One symbol:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.position = mfd::Vec2 {0.18f, -0.24f};
patch.rotationDegrees = 55.0f;
patch.text = std::string {"B21"};

client.UpsertDynamicReticle("Radar", "track_021", "radar_track", patch);
client.RemoveDynamicReticle("Radar", "track_021");
```

Many symbols in one cycle:

```cpp
std::vector<mfd::DynamicReticleState> reticles;

for (const Track& track : tracks)
{
    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {track.x, track.y};
    patch.rotationDegrees = track.headingDegrees;
    patch.color = track.color;
    patch.text = track.label;

    mfd::DynamicReticleState state;
    state.reticleId = track.id;
    state.patch = std::move(patch);
    reticles.push_back(std::move(state));
}

client.UpsertDynamicReticles("Radar", "radar_track", reticles);
```

This is the same public pattern as the mockup radar simulator.

On the wire, those dynamic instance ids are serialized as runtime-scoped
integers. Raw helper methods still let you call them by name because
`CommandClient` resolves the local names before serialization when it owns the
generated transport map.

## Step 8 - Know what the client API controls

The client API can update:

- whole-window color inversion
- whole-window brightness
- whole-window blackout
- page activation
- page center and zoom
- reticle visibility
- reticle blink enable and blink type
- reticle position
- reticle rotation
- reticle color
- reticle thickness
- reticle text
- reticle letter spacing
- strobe active state
- strobe position
- dynamic reticles

## Step 9 - Apply the same practical rules as the mockup

The mockup is a good model here. Copy these habits:

- check `client.IsReady()` before entering the live loop
- surface `client.LastError()` when a send fails
- clamp normalized coordinates to `[-1, 1]`
- clamp brightness to `[0, 1]`
- sanitize zoom before sending
- send page activation on context changes, not every cycle
- group same-cycle updates in one batch when coherence matters

## Step 10 - Know what happens inside the window

The example windows use a background `UdpRuntimeBridge`.

That means:

- the UDP thread receives and decodes protobuf command packets
- commands are queued safely
- the render thread drains that queue at the beginning of the frame
- the render thread applies the commands through `CommandProcessor` and `EnTT`
- strobe feedback snapshots are queued back to the UDP thread for sending

## What You Should See

If the loop is correct:

- the page remains active
- the target reticle moves every cycle
- color and visibility changes apply immediately
- the client never needs to create or manage the window UI itself

Typical mistake:

- sending commands to the right port but to the wrong page name

## Result

You now have a simple live client model:

- one UDP endpoint
- one `CommandClient`
- one loop at 20 ms
- one reticle patch or one command batch per cycle when needed

If your external application works in a physical frame such as nautical miles
and radians, continue with
[08 Project User Space To Page Space](./08_project_user_space_to_page_space.md).
