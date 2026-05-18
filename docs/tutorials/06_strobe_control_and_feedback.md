# Control The Strobe And Receive Feedback

This tutorial shows how to:

- define a strobe in a page
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

## Step 1 - Define the strobe in the page JSON

Example:

```json
{
  "name": "Radar",
  "strobe": {
    "id": "radar_strobe",
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
  }
}
```

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

The generated client API consumes both so it can expose `Page::IsActive()` and
`DynamicReticle::IsStrobeCaptured()` without forcing the user to decode packets
and correlate ids manually.

Keep the feedback endpoint on `127.0.0.1` for the default trusted-local setup.
If you expose it on `0.0.0.0`, any reachable host can receive the stream.
`maxPacketSize` must stay in the supported `[64, 65507]` range.

If you author the page in `mfd_editor`, select the page and use
`Page inspector > Strobe > Strobe template` to assign one of the library
reticles as the page strobe before saving.

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
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.15f, -0.08f});
    client.SendBatch(ui.BuildBatch());
}
```

If you intentionally stay on raw `CommandClient`, the low-level equivalent is:

```cpp
client.SetStrobeActive("Radar", true);
client.SetStrobePosition("Radar", {0.15f, -0.08f});
```

That raw helper path now assumes `CommandClient` was constructed with the
companion generated transport map so `Radar` can be resolved locally to its
transport ID before serialization.

## Step 4 - Understand magnetization

If magnetization is enabled:

- the requested position is the input command
- the returned position is the actual resolved position
- nearby dynamic reticles remain valid magnet targets even though their
  runtime instance ids are hidden behind generated handles in user code

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
    auto& track = tracks.Create();

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

- is `true` only when the latest authoritative strobe feedback points to that
  exact dynamic reticle
- becomes `false` again when capture is lost
- becomes `false` again when the strobe captures another dynamic reticle

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

`strobeId` is informational feedback from the runtime. When you use generated
client bindings, control remains page-scoped through `page.strobe` and the
authoritative runtime queries stay available directly on the generated page and
dynamic reticle handles.

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
- one generated page-scoped `strobe` accessor that does not require a user
  managed id
- one runtime feedback path from window to client
- generated `Page::IsActive()` and `DynamicReticle::IsStrobeCaptured()`
  queries backed by authoritative feedback
- support for strobe magnetization and capture feedback
- a clean split between UDP I/O and render-thread scene ownership
