# Control The Strobe And Receive Feedback

This tutorial shows how to:

- define one or more strobes in a page
- control it from a client
- receive authoritative runtime state back over UDP

## At A Glance

\startuml
actor Client as C
participant "UDP I/O worker" as U
participant "Render thread" as W
participant "Strobe logic" as S

C -> U : SetStrobeActive / SetStrobePosition
U -> W : Queue typed command
W -> S : Resolve capture and magnetization
W -> U : Queue runtime feedback snapshot
U -> C : Strobe status + active-page feedback
\enduml

## Step 1 - Define the strobes in the page JSON

Example:

```json
{
  "name": "Radar",
  "activeStrobe": "Default",
  "strobes": [
    {
      "name": "Default",
      "id": "radar_strobe_default",
      "template": "strobe_cursor",
      "position": { "x": 0.0, "y": 0.0 },
      "capture": {
        "shape": "circle",
        "radius": 0.10
      },
      "magnet": {
        "enabled": true,
        "radius": 0.075,
        "strength": 1.0
      }
    },
    {
      "name": "Designator",
      "id": "radar_strobe_designator",
      "template": "designator_cursor",
      "position": { "x": 0.0, "y": 0.0 },
      "capture": {
        "shape": "rectangle",
        "size": [0.28, 0.18]
      }
    }
  ]
}
```

The legacy singular `strobe` object is still accepted for compatibility. The
loader normalizes it as one `strobes` catalog containing a single `Default`
entry.

## Step 2 - Expose the feedback UDP transport in the window JSON

```json
{
  "feedback": {
    "udp": {
      "enabled": true,
      "address": "127.0.0.1",
      "port": 47221,
      "maxPacketSize": 4096
    },
    "fastIntervalMs": 20,
    "heartbeatIntervalMs": 350
  }
}
```

The window will send one runtime feedback stream.

That stream can multiplex:

- `StrobeStatusFeedback` for the resolved strobe state
- `ActivePageFeedback` for the page currently rendered as active

Feedback payloads keep the same shape, but unchanged state is heartbeat-throttled
instead of being emitted at the fast cadence indefinitely. Meaningful runtime
changes can still be emitted on the fast interval; unchanged snapshots use the
slower heartbeat interval. If the interval fields are omitted, the defaults are
20 ms for changed state and 350 ms for unchanged-state heartbeat.

Only the strobe of the page currently rendered as active emits live
`StrobeStatusFeedback`. Inactive pages keep their authored strobe definition,
but they do not publish live strobe position, magnetization, or capture state
until they become active. `ActivePageFeedback` is therefore the authoritative
context used to interpret strobe feedback.

The generated client API consumes both so it can expose `Page::IsActive()` and
`DynamicReticle::IsStrobeCaptured()` without forcing the user to decode packets
and correlate ids manually.

Keep the feedback endpoint on `127.0.0.1` for the default trusted-local setup.
If you expose it on `0.0.0.0`, any reachable host can receive the stream.
`maxPacketSize` must stay in the supported `[64, 65507]` range.

If you author the page in `mfd_editor`, use:

- `Page inspector > Strobe > Strobe template` to assign one of the library
  reticles as the page strobe
- `Window > Window settings` to tune the feedback UDP endpoint plus the fast
  and heartbeat cadence after the window already exists

The same cadence fields are also available in the initial `Create new window`
popup before the first save.

In the recommended runtime model:

- the UDP worker thread receives strobe commands
- the render thread resolves the final strobe state
- the UDP worker thread sends the runtime feedback packet back to the client

## Step 3 - Control the strobe from the client

With generated client bindings, the preferred API is the generic page-scoped
handle:

```cpp
auto& radar = ui.Radar();
if (radar.strobe.IsValid())
{
    radar.strobe = radar.designatorStrobe;
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.15f, -0.08f});
    client.SendBatch(ui.BuildBatch());
}
```

If you intentionally stay on raw `CommandClient`, the low-level equivalent is:

```cpp
client.SelectStrobe("Radar", "Designator");
client.SetStrobeActive("Radar", true);
client.SetStrobePosition("Radar", {0.15f, -0.08f});
```

That raw helper path now assumes `CommandClient` was constructed with the
companion generated transport map so `Radar` and `Designator` can be resolved
locally to generated transport IDs before serialization.

If you do not switch the selected strobe first, the command updates whichever
strobe entry is currently active on the page.

When the authored page exposes primitives inside its strobe reticles, the
generated page also exposes one reticle wrapper per strobe entry. Those
wrappers patch only the currently selected authored strobe:

```cpp
auto& page1 = ui.Page1();

page1.strobe = page1.defaultStrobe;
page1.defaultReticle.CursorLine().SetThickness(0.010f);

page1.strobe = page1.strobe1;
page1.strobe1Reticle.SetRotationDegrees(18.0f);
page1.strobe1Reticle.SetScale({1.10f, 1.10f});
page1.strobe1Reticle.StrobeLabel().SetText("ALT");
```

This is the supported way to mutate one exposed primitive on the active strobe
without addressing raw reticle ids. When the selected strobe changes, the
generated page API automatically redirects runtime reticle commands to the new
active strobe wrapper.

## Step 4 - Understand magnetization

If magnetization is enabled:

- the requested position is the input command
- the returned position is the actual resolved position
- nearby dynamic reticles remain valid magnet targets even though their
  runtime instance ids are hidden behind generated handles in user code

Dynamic reticles are not magnet targets automatically anymore. One generated
dynamic set must opt in explicitly:

```cpp
auto& page1 = ui.Page1();
auto& tracks = page1.DynamicMfdTutorialRadarTrack();
auto& cues = page1.DynamicInspiredSteeringCue();

tracks.SetStrobeMagnetEnabled(true);
cues.SetStrobeMagnetEnabled(false);
```

That opt-in is evaluated when the strobe searches a magnet target:

- `SetStrobeMagnetEnabled(true)` makes dynamic instances from that set eligible
  for strobe snap/follow behavior
- `SetStrobeMagnetEnabled(false)` keeps them non-magnetized even if the strobe
  is positioned on top of them
- capture is still independent, so `DynamicReticle::IsStrobeCaptured()` can
  become `true` for a non-magnetized set when the strobe is over it

So the feedback position may differ from the command position.

That is expected.

Magnetization does not have to change the authored strobe shape. If you want a
visual cue while the strobe is locked to a target, enable it explicitly:

```json
"magnet": {
  "enabled": true,
  "radius": 0.075,
  "visual": {
    "enabled": true,
    "shape": "square",
    "size": 0.12
  }
}
```

With `visual.enabled` omitted or `false`, the strobe keeps its authored reticle
while still snapping to nearby dynamic reticles.

## Step 5 - Bind the feedback stream to the generated Page1 API

```cpp
#include "mfd/control/FeedbackTransport.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "TutorialUi.h"

int main()
{
    mfd::WindowUdpFeedbackTransport feedbackUdp;
    feedbackUdp.enabled = true;
    feedbackUdp.address = "127.0.0.1";
    feedbackUdp.port = 47221;
    feedbackUdp.maxPacketSize = 4096;

    auto feedbackChannel = mfd::CreateFeedbackReceiverChannel(feedbackUdp);
    if (feedbackChannel == nullptr || !feedbackChannel->IsReady())
    {
        return 1;
    }

    tutorial_ui::TutorialUi ui;
    auto& page1 = ui.Page1();
    auto& tracks = page1.DynamicMfdTutorialRadarTrack();
    auto& cues = page1.DynamicInspiredSteeringCue();
    auto& track = tracks.Create();

    tracks.SetStrobeMagnetEnabled(true);
    cues.SetStrobeMagnetEnabled(false);
    track.SetVisible(true);
    track.SetPosition({0.22f, -0.10f});

    if (page1.strobe.IsValid())
    {
        page1.strobe.SetActive(true);
        page1.strobe.SetPosition({0.22f, -0.10f});
    }

    // Publish the Page1 batch through your existing CommandClient here.

    while (true)
    {
        std::string error;
        if (ui.PollFeedback(*feedbackChannel, 8U, &error) == 0U)
        {
            continue;
        }

        const bool page1Active = page1.IsActive();
        const bool trackCaptured = track.IsStrobeCaptured();

        // page1Active is true only while Page1 is the active rendered page.
        // trackCaptured is true only while the Page1 strobe captures this track.
    }
}
```

This is the preferred high-level flow for generated clients:

- let the generated root decode the mixed runtime feedback stream
- query the current authoritative page state through `Page::IsActive()`
- query the current authoritative capture state through
  `DynamicReticle::IsStrobeCaptured()`

## Step 6 - Know what the generated queries guarantee

`page1.IsActive()`:

- is `true` only when the render thread reported `Page1` as the active page
- becomes `false` again as soon as another page becomes active
- stays `false` until the first authoritative `ActivePageFeedback` was received

`track.IsStrobeCaptured()`:

- is `true` only when the latest authoritative strobe feedback for the
  currently active page points to that exact dynamic reticle
- becomes `false` again when capture is lost
- becomes `false` again when another page becomes active until a fresh strobe
  snapshot arrives for the newly active page
- becomes `false` again when the strobe captures another dynamic reticle

## Step 6b - Detect window shutdown from the same feedback stream

The same feedback channel also carries a window-level lifecycle signal that is
**not** tied to the strobe: a periodic `Alive` heartbeat and a final `Closing`
payload at graceful shutdown. Feed `mfd::client::WindowLivenessMonitor` from the
generated counters to detect a closed or crashed window and reset the client:

```cpp
liveness.Observe(ui.TotalDecodedFeedbackPackets(), ui.WindowReportedClosing(), nowSeconds);
if (liveness.ConsumeDisconnect())
{
    // rebuild transports, ui.Initialize(), liveness.Reset()
}
```

See [04 Drive A Window From A Live Client](./04_drive_a_window_from_a_live_client.md)
Step 12 and end to end user guide section 6.20.

## Step 7 - Use the raw decoder only when you intentionally stay low-level

If you are not using generated client bindings, decode the runtime feedback
envelope generically:

```cpp
#include "mfd/control/StrobeFeedback.h"

const auto payload = feedbackChannel->TryReceive();
if (payload.has_value())
{
    std::string error;
    const auto feedback = mfd::DeserializeFeedbackPayload(
        std::string_view(reinterpret_cast<const char*>(payload->data()), payload->size()),
        &error);

    if (feedback.has_value() &&
        std::holds_alternative<mfd::StrobeStatusFeedback>(*feedback))
    {
        const auto& strobe = std::get<mfd::StrobeStatusFeedback>(*feedback);
        // Use strobe.pageName / strobe.active / strobe.position / strobe.captureResult
    }
    else if (feedback.has_value())
    {
        const auto& activePage = std::get<mfd::ActivePageFeedback>(*feedback);
        // Use activePage.pageName
    }
}
```

## Step 8 - Read the low-level strobe feedback fields

Important fields are:

- `pageName`
- `strobeId`
- `active`
- `position`
- `capture`
- `magnet`
- optional `captureResult`

`captureResult` gives you:

- captured reticle id
- source template id
- label
- category
- position
- distance
- metadata

`strobeId` is the public reticle id of the currently active strobe cursor.
When you use generated client bindings, control still remains page-scoped
through `page.strobe`, while authored strobe variants stay available through
generated `StrobeType` members such as `defaultStrobe` or `designatorStrobe`.
Always pair `StrobeStatusFeedback` with the latest `ActivePageFeedback`,
because only the active page strobe is live.

## Step 9 - Test quickly with the mockup

You do not need to write the client first.

Use `client_mockup`:

1. select a page with a strobe
2. send a strobe update
3. inspect the `Live return from window` section

## Step 10 - Understand requested vs resolved state

The client commands only express intent:

- active or inactive
- requested position

The window feedback reports the resolved result after runtime processing:

- final active state
- final position after optional magnetization
- capture state
- optional captured target

## What You Should See

With a strobe-enabled page:

- enabling the strobe makes the cursor appear
- moving the strobe changes its location
- if magnetization is enabled, the reported position may snap to a nearby target
- feedback tells you what the window resolved, not only what the client asked for

This distinction is important:

- command position = requested input
- feedback position = actual resolved state

## Result

You now have:

- a command path from client to window
- one generated page-scoped `strobe` accessor that can also switch between
  authored strobe variants without exposing raw transport ids
- one runtime feedback path from window to client
- generated `Page::IsActive()` and `DynamicReticle::IsStrobeCaptured()`
  queries backed by authoritative feedback
- support for strobe magnetization and capture feedback
- a clean split between UDP I/O and render-thread scene ownership
