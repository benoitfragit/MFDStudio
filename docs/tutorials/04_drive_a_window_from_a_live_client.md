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

\startuml
left to right direction
rectangle "External application" as ExternalApplication
rectangle "MFD window" as Window
rectangle "Active page" as ActivePage
rectangle "Reticles updated\nin real time" as UpdatedReticles

ExternalApplication --> Window : UDP protobuf commands
Window --> ActivePage
ActivePage --> UpdatedReticles
\enduml

The external client always needs:

- the UDP address and port
- one transport-compatible way to address authored objects

That can be either:

- generated bindings, which are the preferred client-facing API
- raw transport IDs already known by your application
- or the companion `.generated.map` loaded locally so raw name-based
  `CommandClient` helpers can resolve authored names before serialization

This is the key idea:

- the window owns the UI
- the client owns the live data
- they only share a command contract

On the window side, the recommended runtime model is:

- one render thread at the window frame rate
- one background UDP I/O thread receiving commands and sending runtime feedback

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

Keep the command endpoint on `127.0.0.1` for the default trusted-local setup.
If you bind the window to `0.0.0.0`, any host that can reach that port can send
runtime commands. `maxPacketSize` must stay in the supported `[64, 65507]`
range.

## Step 2 - Create a command client

```cpp
#include "mfd/client/ClientSdk.h"

mfd::JsonLoader loader;
const auto loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages.json");

if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
{
    return 1;
}

mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
```

`mfd/client/ClientSdk.h` is the supported umbrella include for standalone
client code and shipped examples. It keeps the integration on the
`mfd_client_api` SDK surface even when the code uses raw `CommandClient` or
`JsonLoader` helpers.

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

With generated bindings, pass the generated page wrapper:

```cpp
full_demo_ui::FullDemoMockupUi ui;
client.ActivatePage(ui.Radar());
```

`CommandClient` extracts the generated page id and mapping hash, then sends an
id-only transport command. Raw authored-name helpers such as
`client.ActivatePage("Radar")` remain available for tools, but only when the
client was constructed with the companion generated transport map.

The same pattern applies to page view changes:

```cpp
client.SetPageView(ui.Radar(), {0.0f, 0.0f}, 1.0f);
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

This example updates one reticle every 20 ms through the generated API:

```cpp
#include "FullDemoMockupUi.h"
#include "mfd/client/ClientSdk.h"

#include <chrono>
#include <cmath>
#include <thread>

int main()
{
    mfd::JsonLoader loader;
    const auto loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages.json");
    if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
    {
        return 1;
    }

    mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);

    full_demo_ui::FullDemoMockupUi ui;
    auto& radar = ui.Radar();
    auto& track = radar.fixedTrackAlpha;
    client.ActivatePage(radar);

    float angle = 0.0f;

    using namespace std::chrono_literals;

    while (true)
    {
        angle += 0.03f;

        track.SetVisible(true);
        track.SetBlinkType(radar.fast);
        track.SetPosition({
            std::cos(angle) * 0.35f,
            std::sin(angle) * 0.35f
        });
        track.SetColor({77, 224, 255, 255});

        client.SendBatch(ui.BuildBatch());

        std::this_thread::sleep_for(20ms);
    }
}
```

This matches the mockup `Reticle` inspector, but without the operator UI.
`CommandClient` stays on the send boundary while the generated wrapper owns the
page, reticle, and blink addressing details.

## Step 6 - Send several updates during the same cycle

If several reticles must update together, prefer a batch:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.SetBlinkType(radar.caution);
radar.fixedTrackAlpha.SetPosition({-0.20f, 0.15f});
radar.fixedTrackAlpha.SetColor({120, 255, 154, 255});

radar.fixedTrackBravo.SetVisible(true);
radar.fixedTrackBravo.SetBlinkType(radar.fast);
radar.fixedTrackBravo.SetPosition({0.32f, -0.10f});
radar.fixedTrackBravo.SetColor({255, 144, 112, 255});

ui.Window().SetColorInverted(false);
ui.Window().SetBrightness(0.65f);
ui.Window().SetDisabled(false);

client.SendBatch(ui.BuildCommandBatch(42U));
```

Use one `sequence` value per external cycle if you want a stable cycle id in the
transport stream.

This is the same batching principle used by the cockpit simulator in the
mockup.

## Step 7 - Use dynamic reticles for runtime-owned symbols

If the object may appear or disappear at runtime, use generated dynamic
reticles instead of trying to patch static JSON content into existence.

One symbol:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& tracks = ui.Radar().DynamicRadarTrack();
auto& track = tracks.Create();

track.SetVisible(true);
track.SetPosition({0.18f, -0.24f});
track.SetRotationDegrees(55.0f);
track.TrackLabel().SetText("B21");
client.SendBatch(ui.BuildBatch());

tracks.Remove(track);
client.SendBatch(ui.BuildBatch());
```

Many symbols in one cycle:

```cpp
for (const Track& track : tracks)
{
    auto& symbol = ui.Radar().DynamicRadarTrack().Create();
    symbol.SetPosition({track.x, track.y});
    symbol.SetRotationDegrees(track.headingDegrees);
    symbol.SetColor(track.color);
    symbol.TrackLabel().SetText(track.label);
}

client.SendBatch(ui.BuildBatch());
```

This is the same public pattern as the mockup radar simulator. If you
intentionally stay on raw `CommandClient`, the name-based
`UpsertDynamicReticle(...)` and `UpsertDynamicReticles(...)` helpers still
exist, but they are now the explicit low-level alternative.

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
- runtime feedback snapshots are queued back to the UDP thread for sending

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
