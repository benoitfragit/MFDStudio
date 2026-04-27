# Manage Page-Local Blink

This tutorial shows how to declare blink types inside one page and switch
reticles between synchronized blink groups at runtime.

## At A Glance

\startuml
left to right direction
rectangle "Page JSON" as PageJson
rectangle "Blink types" as BlinkTypes
rectangle "Reticle instance" as ReticleInstance
rectangle "Runtime patch" as RuntimePatch
rectangle "Same duration = same phase" as SamePhase

PageJson --> BlinkTypes
BlinkTypes --> ReticleInstance
ReticleInstance --> RuntimePatch
RuntimePatch --> SamePhase
\enduml

## The Core Rule

Blink is managed by the page.

- the page declares named blink types
- each type has one effective duration in milliseconds
- a reticle can opt in to blinking and optionally choose one type
- synchronization is done by effective duration, not by type name

So if `slow` and `caution` both use `1000 ms`, they blink together.

## Step 1 - Declare blink types in the page JSON

```json
{
  "name": "Radar",
  "blinkTypes": [
    { "name": "slow", "durationMs": 1000 },
    { "name": "fast", "durationMs": 320 },
    { "name": "caution", "durationMs": 1000 }
  ],
  "defaultBlink": "slow",
  "staticReticles": [
    { "id": "fixed_track_alpha", "template": "radar_track", "blink": "slow" },
    { "id": "fixed_track_bravo", "template": "radar_track", "blink": "caution" },
    { "id": "radar_caption", "template": "status_clock", "blink": "fast" }
  ]
}
```

Notes:

- `blink` belongs to the page reticle instance, not to the reticle template
- `defaultBlink` is used when a reticle enables blinking without naming a type

## Step 2 - Enable blink from the client API

Use the generated reticle wrapper when the client targets one authored window:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetBlinkEnabled(true);
radar.fixedTrackAlpha.SetBlinkType(radar.fast);
client.SendBatch(ui.BuildBatch());
```

## Step 3 - Use a full patch when needed

The generated API still emits one `ReticlePatch` internally when you change
several fields together:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetVisible(true);
radar.fixedTrackAlpha.SetBlinkType(radar.caution);
radar.fixedTrackAlpha.SetPosition({0.25f, 0.10f});
radar.fixedTrackAlpha.SetRotationDegrees(12.0f);
radar.fixedTrackAlpha.SetColor({255, 214, 102, 255});
client.SendBatch(ui.BuildBatch());
```

## Step 4 - Fall back to the page default

To keep blinking enabled but remove the explicit type, clear the type:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetBlinkEnabled(true);
radar.fixedTrackAlpha.ClearBlinkType();
client.SendBatch(ui.BuildBatch());
```

This makes the reticle use `defaultBlink`.

## Step 5 - Change speed at runtime

You can switch from one group to another at any time:

```cpp
full_demo_ui::FullDemoMockupUi ui;
auto& radar = ui.Radar();

radar.fixedTrackAlpha.SetBlinkType(radar.slow);
client.SendBatch(ui.BuildBatch());

radar.fixedTrackAlpha.SetBlinkType(radar.fast);
client.SendBatch(ui.BuildBatch());
```

The reticle immediately joins the phase already running for the new effective
duration.

## Step 6 - Apply blink to dynamic reticles too

Dynamic reticles use the same patch structure:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.blinkEnabled = true;
patch.blinkType = std::string {"fast"};
patch.position = mfd::Vec2 {-0.30f, 0.22f};
patch.text = std::string {"T42"};

client.UpsertDynamicReticle("Radar", "track_42", "radar_track", patch);
```

If you intentionally stay on raw `CommandClient`, the name-based helpers shown
above remain available, but they are the low-level fallback path. The
recommended client-facing path stays the generated wrapper plus
`client.SendBatch(ui.BuildBatch())`.

## Step 7 - Validate it with `client_mockup`

The mockup now shows:

- the blink types declared by the selected page
- a blink enable toggle for static reticles
- a blink type selector for static and dynamic reticles

This is the easiest way to test:

- default blink
- explicit type selection
- slow to fast transitions
- same-duration synchronization

## Step 8 - Author it directly in `mfd_editor`

The page editor now exposes the same page-managed blink model:

- in the page inspector, define page-local blink types
- choose the page default blink
- rename one blink type or change its duration
- remove one blink type from the page

Then, in the page reticle inspector:

- enable or disable blink for the selected page reticle
- assign the reticle to one explicit page blink type
- switch the reticle back to `<page default>`

Important:

- blink is still authored at page level
- reticle templates stay blink-free
- removing one blink type clears the explicit bindings that pointed to it

## Result

You now have page-managed blinking where:

- the page owns the blink catalog
- the page editor can author that catalog visually
- the API selects a type by name
- the runtime synchronizes by effective duration
- static and dynamic reticles follow the same rule
