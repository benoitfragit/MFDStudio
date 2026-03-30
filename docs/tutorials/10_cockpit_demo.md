# Cockpit Demo

This tutorial walks through the integrated cockpit demonstration composed of:

- one composite page containing:
  - an ADI on the left
  - a HUD in the center
  - a radar on the right
- a mockup-side 20 ms flight loop that updates the whole cockpit page together

The cockpit demo is useful for two reasons:

- it is a visual showcase
- it is also a concrete example of one client batching many related reticle
  updates during one simulation tick

For the low-level mapping between the mockup cockpit controls and the public
client API, see
[11 Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md).

## Step 1 - Build the project

Example with Visual Studio 2022 presets:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

## Step 2 - Start the cockpit window

Launch `examples/mfd_demo_cockpit`.

This window loads `assets/windows/demo_pages_cockpit.json`, which references:

- `cockpit.json`

## Step 3 - Start the mockup

Launch `mfd_mockup.exe`.

In the `Window target` combo box, select `Cockpit Demo`.

The mockup now switches to a simplified pilot-station style UI oriented around
live control of one composite page.

If you want the smallest possible client example instead of the full operator
UI, launch `mfd_mockup_minimal.exe`.

This headless sample loads `assets/windows/demo_pages_cockpit.json`, creates a
`CommandClient`, activates page `Cockpit`, then publishes one dummy cockpit
batch every `20 ms` from a single `main` loop until you stop it with `Ctrl+C`.

## Step 4 - Enable the cockpit simulator

Open the `Pilot Controls` panel and:

1. enable the simulation
2. optionally click `Show cockpit page`

The mockup now sends one cockpit batch every 20 ms.

## Step 5 - Fly the simple aircraft model

Use:

- Up and Down arrow keys for pitch
- Left and Right arrow keys for roll
- the `Throttle` slider for speed
- the `Radar enabled` checkbox for radar standby

The same state is propagated to one page that contains all three instruments.

## What Changes On The Cockpit Page

On the ADI area:

- the ball shifts toward sky or ground with pitch
- the ball rotates with roll
- heading, pitch, and roll numeric boxes update live
- the heading card and heading bug move around the outer crown

On the HUD area:

- heading, flight-path angle, throttle, and radar state update live
- the HUD ladder reacts to pitch and bank
- the flight-path marker drifts with the simple flight model
- speed and Mach blink together when the aircraft exceeds `700 kts`

On the radar area:

- the sweep rotates continuously
- the heading and speed boxes update live
- contacts move as ownship heading and speed evolve
- the radar can switch between search and standby

## Why This Demo Matters For Client Integration

The cockpit demo is the clean example of a coordinated runtime frame:

- one simulation state
- one vector of typed commands
- one batch send
- one sequence number for that frame

That is the right model when several instruments must remain coherent from the
same external update cycle.

Examples:

- heading affects ADI, HUD, and radar together
- overspeed affects speed and Mach together
- radar standby hides radar elements and removes contacts together

## Result

You now have a representative cockpit showcase that demonstrates:

- page-authored instruments
- synchronized page-local blink
- dynamic radar contacts
- one-client / one-composite-page runtime control
- one batched cockpit frame every 20 ms
