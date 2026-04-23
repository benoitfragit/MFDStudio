# Control The Strobe And Receive Feedback

This tutorial shows how to:

- define a strobe in a page
- control it from a client
- receive its live state back over UDP

## At A Glance

\startuml
actor Client as C
participant "UDP I/O worker" as U
participant "Render thread" as W
participant "Strobe logic" as S

C -> U : SetStrobeActive / SetStrobePosition
U -> W : Queue typed command
W -> S : Resolve capture and magnetization
W -> U : Queue strobe feedback snapshot
U -> C : Strobe status feedback
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
    }
  }
}
```

The window will send one feedback stream for the strobe state.

If you author the page in `mfd_editor`, select the page and use
`Page inspector > Strobe > Strobe template` to assign one of the library
reticles as the page strobe before saving.

In the recommended runtime model:

- the UDP worker thread receives strobe commands
- the render thread resolves the final strobe state
- the UDP worker thread sends the feedback packet back to the client

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

## Step 5 - Create a feedback receiver

```cpp
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"

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

    while (true)
    {
        const auto payload = feedbackChannel->TryReceive();
        if (!payload.has_value())
        {
            continue;
        }

        const auto* raw = reinterpret_cast<const char*>(payload->data());
        std::string error;
        const auto feedback =
            mfd::DeserializeStrobeStatusFeedback(std::string_view(raw, payload->size()), &error);

        if (!feedback.has_value())
        {
            continue;
        }

        // Use feedback->pageName
        // Use feedback->active
        // Use feedback->position
        // Use feedback->magnet
        // Use feedback->captureResult
    }
}
```

## Step 6 - Read the feedback fields

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
client bindings, control remains page-scoped through `page.strobe`.

## Step 7 - Test quickly with the mockup

You do not need to write the client first.

Use `client_mockup`:

1. select a page with a strobe
2. send a strobe update
3. inspect the `Live return from window` section

## Step 8 - Understand requested vs resolved state

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
- a feedback path from window to client
- support for strobe magnetization and capture feedback
- a clean split between UDP I/O and render-thread scene ownership
