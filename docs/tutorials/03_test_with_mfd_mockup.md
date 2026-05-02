# Test A Window With The Mockup

This tutorial shows how to validate a window without writing any custom client
code.

It also introduces the most important idea behind `client_mockup`:

- it is not a special internal tool
- it is a normal UDP client built on the same public API as any external
  application
- it is a standalone Win32 + Dear ImGui + DX11 executable and does not link
  the raylib renderer used by the repository hosts

If you want the deep client-side reference after this hands-on walkthrough, go
to
[11 Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md).

## At A Glance

\startuml
actor User as U
participant client_mockup as M
participant Window as W

U -> M : Select window / page / reticle / strobe
M -> W : UDP protobuf command
W -> M : Optional runtime feedback
\enduml

`client_mockup` is the fastest way to:

- test whole-window inversion and brightness
- test whole-window blackout
- activate pages
- change page view
- edit reticles
- test page-managed blink types
- send strobe commands
- inspect live runtime feedback
- create and remove dynamic reticles
- stress-test a radar page with 100 dynamic tracks
- drive the integrated cockpit demo at one 20 ms update cycle

## What The Mockup Does Locally

The mockup loads the target window JSON locally in order to discover:

- the UDP command endpoint
- the optional UDP feedback endpoint
- the list of pages
- the list of static reticles with public ids
- whether a page exposes a strobe
- the page-local blink types
- the reticle template ids available for dynamic creation

This local load is for convenience only.

Important:

- the mockup does not draw inside the target window
- the mockup does not share memory with the target window
- the runtime control still happens only through UDP commands

That is why the mockup is such a good reference for your own live client.

## Step 1 - Build the project

Example with Visual Studio 2022 presets:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
```

## Step 2 - Start a window application

Launch one of:

- `.\Scripts\Start-MfdDemo.bat`
- `.\Scripts\Start-MfdCockpit.bat`
- `.\Scripts\Start-MfdMinimal.bat`
- `.\Scripts\Start-MfdTutorial.bat` after the editor tutorial has generated `assets/windows/mfd_tutorial.json`
- your own `mfd_window --window <window.json>` command

The window must load a root JSON file that contains a valid `commands.udp`
configuration.

`Start-MfdMinimal.bat` and, once the tutorial assets exist, `Start-MfdTutorial.bat` also pass the sample
framebuffer plugin DLL so you can see the callback wiring without compiling a
custom host executable.

## Step 3 - Start `client_mockup`

Launch `client_mockup.exe`.

This executable is intentionally separate from the runtime host render stack:
it loads the window JSON locally for discovery, then sends normal UDP commands
through the public client API.

At startup it can target:

- `Full Demo`
- `Cockpit Demo`
- `Minimal Radar`

Each target maps to one root window JSON file and one UDP endpoint.

In the header bar you can immediately see:

- the loaded window title
- the UDP command address and port
- the optional feedback address and port
- whether the client transport is currently ready
- UI FPS and simulator cycle timing

## Step 4 - Test whole-window display first

Inside the `Window display` section:

1. toggle `Invert colors`
2. change `Brightness`
3. optionally toggle `Disable output`
4. click `Send window display`

This is intentionally the first check because it validates:

- that the mockup can send commands to the right port
- that the target window receives and applies them
- that you are looking at the correct target window

Under the hood this panel sends one `UpdateWindowDisplayCommand` through
`mfd::CommandClient`.

When `Disable output` is enabled, the target window keeps its runtime state but
renders a black screen.

The same section also exposes `Reset window`, which sends
`client.ResetWindow()`. Use it when you want to restore the full runtime state
to the JSON-defined initial state (default page, page views, strobe state,
window display state, and dynamic reticles).

## Step 5 - Activate the target page

In the tree on the left:

1. select a page
2. click `Activate page now`

Or use the header shortcut `Activate selected page`.

The mockup is a generic inspection tool, so this sends
`client.ActivatePage(pageName)`. In generated clients, prefer passing the
generated page wrapper directly, for example `client.ActivatePage(ui.Radar())`;
`CommandClient` then sends the generated page id and mapping hash.

Typical usage:

- activate once when changing operator context
- do not spam page activation every frame

## Step 6 - Change the page view

Inside the page inspector:

1. edit `Center`
2. edit `Zoom`
3. click `Send page view`

The mockup sends `client.SetPageView(page, center, zoom)` from the selected
page name. In generated clients, prefer
`client.SetPageView(ui.Radar(), center, zoom)` so the command is addressed by
generated page id and mapping hash.

Use it to confirm that:

- your page behaves correctly in normalized coordinates
- your content remains stable when zoom changes
- your client-side projection logic is coherent

## Step 7 - Edit one static reticle

Inside the tree:

1. open the page
2. select a static reticle
3. change visibility, blink, position, rotation, color, thickness, and text
4. click `Send reticle update`

This sends one `UpdateReticleCommand` with a `ReticlePatch`.

The patch fields used by the mockup are:

- `visible`
- `blinkEnabled`
- `blinkType`
- `position`
- `rotationDegrees`
- `color`
- `thickness`
- `text`
- `letterSpacing`

Good things to validate here:

- the reticle has a public id
- you are on the correct page
- the coordinate frame matches what you think `[-1, 1]` means
- the blink type really exists on that page

## Step 8 - Validate page-managed blink

If the selected page declares blink types, the page inspector shows:

- the page default blink
- every named blink type with its duration in milliseconds

In the reticle inspector and dynamic-reticle panel, the blink editor lets you:

- disable blinking
- enable blinking with the page default type
- enable blinking with an explicit named type

Important runtime rule:

- synchronization is done by effective duration, not by type name

So on one page:

- two different names with the same duration blink in phase
- switching a reticle from one duration to another attaches it immediately to
  the new phase group

For the dedicated conceptual explanation, see
[09 Manage Page-Local Blink](./09_page_managed_blink.md).

## Step 9 - Test the strobe

If the selected page has a strobe, you can use it from two places:

- `Quick strobe` in the page inspector
- the full `Strobe` inspector from the tree

From there you can:

1. toggle `Strobe active`
2. move `Strobe position`
3. click `Send strobe update`

This uses one `UpdateStrobeCommand`.

The full strobe inspector also shows the live feedback returned by the window:

- sequence
- actual position
- capture shape and size
- magnetization state
- captured reticle metadata

Important:

- if magnetization is enabled, the returned position may differ from the
  requested one
- this is expected and useful
- the window remains authoritative for the actual strobe state

For the full feedback model, see
[06 Control The Strobe And Receive Feedback](./06_strobe_control_and_feedback.md).

## Step 10 - Create and remove a dynamic reticle

Use the `Dynamic reticle` panel:

1. select the target page
2. choose one reticle template id from the library
3. enter a public dynamic id
4. set visibility, blink, position, rotation, color, thickness, text
5. click `Upsert dynamic reticle`

Then click `Remove dynamic reticle` to validate deletion.

This panel demonstrates two important public client calls:

- `client.UpsertDynamicReticle(page, dynamicId, templateId, patch)`
- `client.RemoveDynamicReticle(page, dynamicId)`

This is the pattern to use when:

- the symbol may not exist yet
- the symbol must be addressable later by a stable runtime id
- you want JSON-authored graphics but live application-owned instances

For the dedicated runtime model, see
[05 Add And Remove Dynamic Reticles](./05_dynamic_reticles.md).

## Step 11 - Use the radar batch simulator

For the radar page, the mockup can send 100 simulated tracks every 20 ms.

This panel is not only a demo. It is a reference for a real client pattern:

- compute many dynamic states during one external cycle
- pack them into one bulk command
- send them through the batch API

The simulator demonstrates:

- one template id shared by many dynamic instances
- one public runtime id per track
- per-track position, rotation, color, text, and blink patching
- cycle timing and observed send duration

This is close to what a tactical sensor client would do in production.

## Step 12 - Use the cockpit simulator

For the cockpit demo window, the mockup can also drive one coordinated
composite page:

- `Cockpit`

Use the `Pilot Controls` panel to:

- enable a simple 20 ms flight loop
- control pitch with the Up and Down arrow keys
- control roll with the Left and Right arrow keys
- change throttle with the slider and watch speed plus Mach react
- toggle the radar on and off
- activate the composite cockpit page directly

The same cockpit state is sent to one page that contains the three instruments,
so:

- heading updates change the HUD heading readout, the ADI heading box, and the
  radar presentation together
- pitch updates move the ADI ball and the HUD ladder
- overspeed above `700 kts` makes the HUD speed and Mach reticles blink in sync
- radar contact publication is added or removed from the same simulation cycle

Internally this is a good example of `client.SendBatch(commands, sequence)`:

- many `UpdateReticleCommand`
- one optional `UpsertDynamicReticlesCommand`
- several optional `RemoveDynamicReticleCommand`
- one sequence id representing one flight update cycle

For the demo walkthrough itself, see
[10 Drive The Cockpit Demo](./10_cockpit_demo.md).

## Panel To API Mapping

Use this table as a quick reference when you want to reproduce a mockup action
in your own application.

| Mockup control | Public client call or command | Typical usage |
| --- | --- | --- |
| `Window target` + `Reload` | create a `CommandClient` from the target UDP config | retarget one running window |
| `Send window display` | `UpdateWindowDisplay`, `SetWindowColorInverted`, `SetWindowBrightness`, `SetWindowDisabled` | host-driven inversion, dimming, and blackout |
| `Reset window` | `ResetWindow()` | restore runtime to authored initial state |
| `Activate page now` | `ActivatePage(page)` in the generic mockup, `ActivatePage(ui.Radar())` in generated clients | page selection |
| `Send page view` | `SetPageView(page, center, zoom)` in the generic mockup, `SetPageView(ui.Radar(), center, zoom)` in generated clients | panning and zoom |
| `Send reticle update` | `UpdateReticle(page, reticle, patch)` | patch one static reticle |
| blink combo | `ReticlePatch.blinkEnabled`, `ReticlePatch.blinkType` | page-local blinking |
| `Send strobe` | `Send(UpdateStrobeCommand { ... })` | cursor or probe control |
| `Upsert dynamic reticle` | `UpsertDynamicReticle(page, id, template, patch)` | create or update one runtime symbol |
| `Remove dynamic reticle` | `RemoveDynamicReticle(page, id)` | remove one runtime symbol |
| `Send one radar batch` | `UpsertDynamicReticles(page, template, states)` | publish many tracks per cycle |
| `Send one cockpit frame` | `SendBatch(commands, sequence)` | coordinated multi-reticle frame update |

## What You Should See

When everything is wired correctly:

- whole-window inversion and brightness changes apply immediately
- activating a page changes the visible page in the window immediately
- a reticle update moves or recolors the symbol immediately
- dynamic reticles appear and disappear on command
- runtime feedback updates live in the mockup
- the radar simulator sustains many track updates at one cycle
- the cockpit simulator keeps the ADI, HUD, and radar coherent inside the same
  page frame after frame

## Common First Checks When Nothing Changes

Check these first:

- the target UDP port
- the selected window target preset
- the page name
- the reticle id
- the dynamic template id
- the blink type name on that page
- that the target window is actually running

Also remember:

- coordinates are normalized in `[-1, 1]`
- page names and reticle ids are case-sensitive runtime identifiers
- a static reticle without a public id cannot be patched at runtime

## Result

You now have a practical command console for:

- whole-window display control
- page activation
- page view control
- static reticle patching
- dynamic reticle lifecycle
- page-local blink validation
- strobe control and feedback
- radar batch publishing
- integrated cockpit driving

If you now want to translate what you just did into your own C++ client, read:

- [04 Drive A Window From A Live Client](./04_drive_a_window_from_a_live_client.md)
- [11 Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
