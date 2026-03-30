# Manage Page-Local Blink

This tutorial shows how to declare blink types inside one page and switch
reticles between synchronized blink groups at runtime.

## At A Glance

```mermaid
flowchart LR
    A[Page JSON] --> B[Blink types]
    B --> C[Reticle instance]
    C --> D[Runtime patch]
    D --> E[Same duration = same phase]
```

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

Use the high-level helpers when you only need to toggle or change the type:

```cpp
client.SetReticleBlinkEnabled("Radar", "fixed_track_alpha", true);
client.SetReticleBlinkType("Radar", "fixed_track_alpha", "fast");
```

## Step 3 - Use a full patch when needed

`ReticlePatch` can carry blink together with position, color, text, and the
other normal reticle fields:

```cpp
mfd::ReticlePatch patch;
patch.visible = true;
patch.blinkEnabled = true;
patch.blinkType = std::string {"caution"};
patch.position = mfd::Vec2 {0.25f, 0.10f};
patch.rotationDegrees = 12.0f;
patch.color = mfd::ColorRgba {255, 214, 102, 255};

client.UpdateReticle("Radar", "fixed_track_alpha", patch);
```

## Step 4 - Fall back to the page default

To keep blinking enabled but remove the explicit type, clear the type string:

```cpp
mfd::ReticlePatch patch;
patch.blinkEnabled = true;
patch.blinkType = std::string {};

client.UpdateReticle("Radar", "fixed_track_alpha", patch);
```

This makes the reticle use `defaultBlink`.

## Step 5 - Change speed at runtime

You can switch from one group to another at any time:

```cpp
client.SetReticleBlinkType("Radar", "fixed_track_alpha", "slow");
client.SetReticleBlinkType("Radar", "fixed_track_alpha", "fast");
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

## Step 7 - Validate it with `mfd_mockup`

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
