# Use The Mockup As A Client API Reference

This tutorial explains how `client_mockup` maps its visible controls to the public
client API in `mfd_api/include/mfd/control/CommandClient.h`.

The goal is simple:

- understand exactly what the mockup sends
- reuse the same patterns in your own real-time client

## Why The Mockup Matters

`client_mockup` is intentionally useful as both:

- a manual validation tool
- an executable reference implementation of a UDP client

That means:

- when one action works in the mockup, you already know the window-side command
  contract is healthy
- when you want to write your own client, the mockup already shows the normal
  public way to do it

There is no hidden backdoor API used by the mockup.

## Mental Model

\startuml
left to right direction
rectangle "Window JSON loaded by mockup" as WindowJson
rectangle "Discover pages, ids, blink types,\nUDP endpoints" as Discovery
rectangle "CommandClient" as CommandClient
rectangle "Running MFD window" as RunningWindow
rectangle "Feedback receiver in mockup" as FeedbackReceiver

WindowJson --> Discovery
Discovery --> CommandClient
CommandClient --> RunningWindow : UDP protobuf commands
RunningWindow --> FeedbackReceiver : optional UDP strobe feedback
\enduml

Keep these roles separate:

- the target window owns rendering and runtime state
- the mockup owns user input, drafts, and command emission
- the JSON loaded by the mockup is only local discovery data

## What The Mockup Loads And Why

The mockup parses the same root window JSON as the target window in order to
discover:

- `commands.udp`
- optional feedback transport
- page names
- static reticle ids
- strobe presence
- page blink types
- reticle library template ids

This gives it a very good operator UI:

- combo boxes instead of typing ids blind
- blink pickers constrained by the selected page
- dynamic template selection from the real library
- strobe editors only when a page actually exposes one

But this is still only convenience.

Your production client does not need to parse the window JSON if your
application already knows:

- the UDP endpoint
- the transport IDs it wants to control
- the matching `mappingHash`

If you still want to use raw helper methods such as
`UpdateReticle("Radar", "fixed_track_alpha", patch)`, load the companion
generated transport map locally and pass it to `CommandClient`.

## The Public API Layers Used By The Mockup

The mockup uses four increasingly powerful levels of the client API.

### 1. High-Level One-Shot Helpers

Examples:

- `client.ActivatePage(ui.Radar())` for generated pages
- `client.SetPageView(ui.Radar(), center, zoom)` for generated pages
- `client.SetWindowColorInverted(enabled)`
- `client.SetWindowBrightness(brightness)`
- `client.SetWindowDisabled(enabled)`
- `client.ResetWindow()`

Use these when one UI action maps to one semantic command.
The mockup inspector can still use selected page names because it is a generic
tool, but generated clients should pass the generated page wrapper so the
command is sent by transport id.

### 2. Direct Typed Commands

Example:

```cpp
client.Send(mfd::UpdateStrobeCommand {
    "Radar",
    0U,
    true,
    mfd::Vec2 {0.15f, -0.10f}
});
```

Use this when the public helper you want does not exist or when the typed
command is already the most readable shape.

The mockup does this for the strobe because the command naturally carries:

- target page
- optional active flag
- optional position

### 3. Partial Patches

Example:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.position = mfd::Vec2 {0.20f, -0.15f};
patch.rotationDegrees = 35.0f;
patch.color = mfd::ColorRgba {77, 224, 255, 255};

client.UpdateReticle("Radar", "fixed_track_alpha", patch);
```

Use patches when:

- one reticle already exists
- you only want to override some fields
- the untouched fields must remain unchanged

The mockup uses `ReticlePatch` heavily for:

- static reticle edits
- dynamic reticle creation
- cockpit frame updates

### 4. Batches

Examples:

- `client.SendBatch(commands, sequence)`
- `client.UpsertDynamicReticles(page, templateId, states)`

Use batches when several updates belong to the same external cycle.

This is the correct pattern for:

- sensor sweeps
- synchronized multi-instrument updates
- 20 ms simulation loops
- many track updates at once

## Panel To API Mapping

This is the most important section if you want to reproduce mockup behavior in
your own client.

| Mockup area | Public API used | Payload shape |
| --- | --- | --- |
| `Window target` | `CommandClient(WindowUdpCommandTransport, GeneratedTransportMap)` | connection creation |
| `Send window display` | `UpdateWindowDisplay` or convenience helpers such as `SetWindowDisabled` | `WindowDisplayPatch` |
| `Reset window` | `ResetWindow()` | `ResetWindowCommand` |
| `Activate selected page` | `ActivatePage(page)` in the generic mockup, `ActivatePage(ui.Radar())` in generated clients | `ActivatePageCommand` |
| `Send page view` | `SetPageView(page, center, zoom)` in the generic mockup, `SetPageView(ui.Radar(), center, zoom)` in generated clients | `SetPageViewCommand` |
| `Send reticle update` | `UpdateReticle(page, reticle, patch)` | `UpdateReticleCommand` |
| blink editor | `patch.blinkEnabled`, `patch.blinkType` | part of `ReticlePatch` |
| `Send strobe` | `Send(UpdateStrobeCommand { ... })` | `UpdateStrobeCommand` |
| `Upsert dynamic reticle` | `UpsertDynamicReticle(page, id, template, patch)` | `UpsertDynamicReticleCommand` |
| `Remove dynamic reticle` | `RemoveDynamicReticle(page, id)` | `RemoveDynamicReticleCommand` |
| `Send one radar batch` | `UpsertDynamicReticles(page, template, states)` | `UpsertDynamicReticlesCommand` |
| `Declutter one dynamic set` | `SetDynamicReticleSetVisible(page, template, visible)` | `SetDynamicReticleSetVisibilityCommand` |
| `Send one cockpit frame` | `SendBatch(commands, sequence)` | `CommandBatch` |

## Generated Client UI In 2 Minutes

If you prefer typed client-side accessors (instead of string ids everywhere),
generate a UI wrapper from your window JSON.

### Step A - Generate code from CMake

```cmake
client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages_cockpit.json"
    OUTPUT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/generated/MockupUi.h"
    OUTPUT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/generated/MockupUi.cpp"
    OUTPUT_MAP "${MFD_ROOT_DIR}/assets/windows/demo_pages_cockpit.generated.map"
    NAMESPACE "mockup_ui"
    UI_CLASS_NAME "CockpitMockupUi"
    HEADER_INCLUDE "MockupUi.h")
```

This is exactly how `examples/client_mockup` and
`examples/client_mockup_minimal` are wired.

### Step B - Use the generated types in your runtime client

```cpp
#include "MockupUi.h"
#include "mfd/control/CommandClient.h"
#include "mfd/io/JsonLoader.h"

mfd::JsonLoader loader;
const auto loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages_cockpit.json");
if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
{
    return;
}

mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
if (!client.IsReady())
{
    // inspect client.LastError()
}

mockup_ui::CockpitMockupUi ui;

auto& cockpit = ui.Cockpit();
client.ActivatePage(cockpit);

auto& contacts = cockpit.DynamicCockpitRadarContact();
contacts.SetVisible(true);

auto& contact = contacts.Create();
contact.SetVisible(true);
contact.SetPosition({0.15f, -0.10f});
contact.ContactLabel().SetText("B21");

if (cockpit.strobe.IsValid())
{
    cockpit.strobe.SetActive(true);
    cockpit.strobe.SetPosition({0.15f, -0.10f});
}

client.SendBatch(ui.BuildBatch());
```

### Step C - Know when to use generated UI vs low-level API

- Use generated accessors for discoverability and safer page/reticle/primitive
  navigation.
- Let generated dynamic sets own the hidden runtime ids through `Create()` and
  `Remove(...)`.
- Keep `CommandClient` for the final send path, with the generated transport
  map whenever raw helper methods still address objects by authored names.
- For high-rate loops, prefer `BuildBatch()` or `BuildCommandBatch(sequence)`
  after mutating the generated handles.

## Pattern 1 - Recreate The Targeted Connection

The mockup reads the target UDP settings from the selected window JSON, then
recreates a `CommandClient` when the operator changes target.

Minimal equivalent:

```cpp
#include "mfd/control/CommandClient.h"
#include "mfd/io/JsonLoader.h"

mfd::JsonLoader loader;
const auto loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages_cockpit.json");
if (!loaded.window.commandTransports.udp.has_value() || !loaded.generatedTransportMap.has_value())
{
    return;
}

mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);

if (!client.IsReady())
{
    // inspect client.LastError()
}
```

Takeaway:

- one target window usually means one configured `CommandClient`
- reconnect only when the endpoint changes or when transport setup changes

## Pattern 2 - Patch Only What You Want To Change

The mockup almost never rebuilds a reticle from scratch.

Instead, it sends partial patches such as:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.blinkEnabled = true;
patch.blinkType = std::string {"fast"};
patch.position = mfd::Vec2 {0.30f, 0.18f};
patch.rotationDegrees = -15.0f;
patch.text = std::string {"MOCK"};

client.UpdateReticle("Radar", "fixed_track_alpha", patch);
```

This is a very healthy runtime pattern because:

- you only send the fields you care about
- the window keeps the authored reticle structure
- the template remains owned by JSON, not by the client

## Pattern 3 - Use Page-Defined Blink Types

The mockup exposes the blink types declared by the selected page and lets the
operator choose:

- no blink
- page default blink
- one explicit blink type

Equivalent client patterns:

```cpp
client.SetReticleBlinkEnabled("Hud", "speed_box", true);
client.SetReticleBlinkType("Hud", "speed_box", "overspeed");
```

Or with a patch:

```cpp
mfd::ReticlePatch patch;
patch.blinkEnabled = true;
patch.blinkType = std::string {"overspeed"};
client.UpdateReticle("Hud", "speed_box", patch);
```

Important rule to remember:

- blink names are resolved on the page
- phase grouping is based on effective duration

So if two page-defined blink types share the same duration:

- they blink in phase
- even if their names differ

## Pattern 4 - Use The Window As The Source Of Truth For The Strobe

The mockup sends strobe commands, then listens to feedback.

Equivalent command:

```cpp
client.Send(mfd::UpdateStrobeCommand {
    "Radar",
    0U,
    true,
    mfd::Vec2 {0.05f, 0.22f}
});
```

Important operational rule:

- the sent position is a request
- the returned position is the actual resolved state

This matters when:

- magnetization snaps the strobe to a nearby target
- capture logic updates what the operator should treat as current

So if your own client exposes a strobe UI, copy the mockup behavior:

- keep a draft locally
- send the request
- accept feedback from the window as authoritative

## Pattern 5 - Use Dynamic Reticles For Runtime-Owned Objects

The manual `Dynamic reticle` panel in the mockup deliberately exposes the raw
low-level lifecycle:

1. choose a JSON-authored template
2. choose a runtime id
3. upsert with a patch
4. remove later by id

That is useful for debugging because it lets an operator inspect the raw public
commands directly.

For normal generated client code, prefer the typed workflow instead:

```cpp
auto& tracks = ui.Radar().DynamicRadarTrack();
auto& track = tracks.Create();

track.SetVisible(true);
track.SetPosition({-0.12f, 0.44f});
track.SetColor({120, 255, 154, 255});
track.TrackLabel().SetText("T42");

client.SendBatch(ui.BuildBatch());

tracks.Remove(track);
client.SendBatch(ui.BuildBatch());
```

The generated set owns the hidden runtime identifier, so the application never
has to invent or remember one.

If you intentionally stay on the raw API, the equivalent low-level flow remains
available:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.position = mfd::Vec2 {-0.12f, 0.44f};
patch.color = mfd::ColorRgba {120, 255, 154, 255};
patch.text = std::string {"T42"};

client.UpsertDynamicReticle("Radar", "track_042", "radar_track", patch);
client.RemoveDynamicReticle("Radar", "track_042");
```

Use this for:

- radar tracks
- markers created from external detections
- temporary cues
- runtime-only symbols that are not part of the static page JSON

## Pattern 6 - Bulk Publish Many Tracks During One Sensor Cycle

The radar simulator in the mockup is important because it demonstrates the most
efficient public shape for many similar runtime objects:

```cpp
std::vector<mfd::DynamicReticleState> states;

for (const Track& track : tracks)
{
    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {track.x, track.y};
    patch.rotationDegrees = track.headingDegrees;
    patch.color = track.color;
    patch.text = track.label;
    patch.blinkEnabled = track.threat;
    patch.blinkType = track.threat ? std::string {"caution"} : std::string {};

    mfd::DynamicReticleState state;
    state.reticleId = track.id;
    state.patch = std::move(patch);
    states.push_back(std::move(state));
}

client.UpsertDynamicReticles("Radar", "radar_track", states);
```

Why this pattern is good:

- one template id shared by many objects
- one public runtime id per object
- one client call per cycle instead of one call per object
- automatic packet splitting when the UDP payload would be too large

## Pattern 7 - Batch A Whole Cockpit Frame

The cockpit simulator is the best example of a coordinated multi-reticle client
frame.

It computes one aircraft state, then publishes:

- ADI reticle patches
- HUD reticle patches
- radar reticle patches
- optional dynamic contact updates

All of that is sent through one `SendBatch(commands, sequence)`.

Skeleton:

```cpp
std::vector<mfd::UserCommand> commands;

commands.push_back(mfd::UpdateReticleCommand {
    mfd::StaticReticleHandle {"Cockpit", "adi_heading_box"},
    mfd::ReticlePatch {.texts = {{"heading_value", "275"}}}});

commands.push_back(mfd::UpdateReticleCommand {
    mfd::StaticReticleHandle {"Cockpit", "hud_speed_box"},
    mfd::ReticlePatch {.texts = {{"speed_value", "742"}},
                       .blinkEnabled = true,
                       .blinkType = std::string {"overspeed"}}});

mfd::UpsertDynamicReticlesCommand dynamicContacts;
dynamicContacts.page = "Cockpit";
dynamicContacts.templateId = "cockpit_radar_contact";
dynamicContacts.reticles = std::move(contactStates);
commands.push_back(std::move(dynamicContacts));

client.SendBatch(commands, sequence);
```

Use this model when one external cycle must remain visually coherent across
several instruments.

Typical examples:

- one avionics frame
- one tactical sensor sweep
- one 20 ms simulation tick

## How The Mockup Handles Sanitization

Before sending commands, the mockup sanitizes user input. This is worth copying
in your own client.

Examples:

- page and reticle coordinates are clamped to `[-1, 1]`
- brightness is clamped to `[0, 1]`
- zoom is sanitized before being sent
- thickness is clamped to a small positive minimum
- empty dynamic ids are rejected before send

This keeps the runtime command stream predictable and prevents noisy invalid
states from reaching the window.

## How The Mockup Handles Errors

Every send path in the mockup follows the same pattern:

1. verify `client != nullptr && client->IsReady()`
2. send the command
3. if the send failed, expose `client->LastError()`
4. keep a human-readable status line visible in the UI

That is a good baseline for your own application too.

A reliable client should always surface:

- transport setup errors
- serialization failures
- operator mistakes such as missing ids
- current targeted endpoint

## How To Decide Which Public API Shape To Use

Use this decision guide.

If one action means one semantic change:

- use a high-level helper such as `ActivatePage` or `SetPageView`

If you need to update one existing reticle:

- use `UpdateReticle(page, reticle, patch)`

If you use generated UI wrappers for runtime-owned reticles:

- keep the typed handle returned by `Create()`
- mutate that handle directly
- call `Remove(...)` when it disappears from your own domain model

If you intentionally stay on the raw API:

- use `UpsertDynamicReticle`
- or `UpsertDynamicReticles` when there are many

If several commands belong to the same external cycle:

- build a `std::vector<UserCommand>`
- send it through `SendBatch(commands, sequence)`

If the window should report back a resolved state:

- design around a feedback loop like the strobe flow

## Mockup Source Tour

When reading the code, these functions are the most useful entry points:

- `ReloadConfiguration`
- `RecreateClient`
- `RecreateFeedbackReceiver`
- `SendWindowDisplayUpdate`
- `SendActivatePage`
- `SendPageView`
- `SendReticleUpdate`
- `SendStrobeUpdate`
- `SendDynamicUpsert`
- `SendDynamicRemove`
- `SendRadarSimulationBatch`
- `SendCockpitSimulationBatch`

And these UI sections show how one operator interaction maps to one command
path:

- `DrawWindowDisplayInspector`
- `DrawPageInspector`
- `DrawReticleInspector`
- `DrawStrobeInspector`
- `DrawDynamicComposer`
- `DrawRadarSimulationPanel`
- `DrawCockpitSimulationPanel`

## Recommended Reading Order After This

If you want to keep going from the mockup toward production integration, read:

1. [04 Drive A Window From A Live Client](./04_drive_a_window_from_a_live_client.md)
2. [05 Add And Remove Dynamic Reticles](./05_dynamic_reticles.md)
3. [06 Control The Strobe And Receive Feedback](./06_strobe_control_and_feedback.md)
4. [09 Manage Page-Local Blink](./09_page_managed_blink.md)
5. [10 Drive The Cockpit Demo](./10_cockpit_demo.md)

## Result

You now have a precise mapping between:

- the visible mockup controls
- the public client API
- the command payloads sent at runtime

That makes `client_mockup` a practical reference implementation for your own
real-time client.
