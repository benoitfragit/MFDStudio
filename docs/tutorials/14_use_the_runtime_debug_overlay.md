# Use The Integrated Runtime Debug Overlay

This tutorial explains the debug mode built directly into `mfd_window`.

It is not a client API and it does not change the UDP protocol. It is a local
runtime inspection and bypass tool available from the window host itself.

## Goal

At the end of this tutorial you will know how to:

- open the overlay with `F1`
- inspect the transport state and the active page
- force the preview to another page without touching the live client
- inspect one reticle tree and locally bypass one reticle
- leave debug mode and return every reticle to the live UDP-driven state

## Prerequisites

Build the runtime and a client:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window client_mockup mfd_window_tests
```

Then launch:

1. `.\Start-MfdDemo.bat`
2. `client_mockup`

Use the mockup to activate `Radar` and send at least one reticle update.

## Step 1 - Open the overlay

Focus the `mfd_window` window and press `F1`.

The overlay appears inside the same native window. It does not create a second
desktop window.

Press `F1` again to close it. Closing the ImGui window itself has the same
effect.

When the overlay closes:

- debug mode stops immediately
- the preview scene is discarded
- every page and reticle follows the normal live runtime state again

## Step 2 - Read the transport panel

The first panel shows runtime transport health.

The checkboxes are read-only. They are not controls.

They tell you:

- whether the UDP command receiver is configured
- whether the command receiver is ready
- whether command traffic was observed recently
- whether the UDP feedback sender is configured
- whether the feedback sender is ready

UDP is connectionless, so the overlay intentionally does not claim a true
"connected" state. The useful signal is whether traffic is configured, ready,
and actually observed.

## Step 3 - Inspect and bypass the active page

The page section shows:

- the live active page
- the preview active page
- a `Bypass active page` checkbox
- a combo box listing authored pages

When page bypass is enabled, the preview scene keeps the selected page active
even if new UDP page-activation commands arrive.

When page bypass is cleared, the preview returns to the last live runtime page
state immediately.

## Step 4 - Inspect the reticle tree

The left pane lists every page and every currently visible runtime reticle
instance known by `mfd_window`.

For each reticle you can inspect:

- its page
- its runtime kind
  `Static`, `Dynamic`, or `Strobe`
- whether it is currently hidden
- whether it is locally bypassed by the overlay

Selecting one reticle opens the inspector on the right.

## Step 5 - Create one reticle bypass

Select one reticle, for example `Ownship`, then change one field such as:

- `Visible`
- `Blink enabled`
- `Blink type`
- `Position`
- `Rotation`
- `Color override`
- `Thickness override`
- one editable text primitive value

The first local modification automatically creates a reticle bypass.

That means:

- the live runtime scene keeps following UDP
- the overlay preview owns only that reticle locally
- new UDP updates for that reticle are ignored in the preview while bypass is active
- every other non-bypassed reticle keeps following live updates normally

The `Bypassed` checkbox reflects that local ownership explicitly.

## Step 6 - Release the bypass correctly

Clear the `Bypassed` checkbox on the selected reticle.

The overlay immediately restores the reticle from the last live UDP-driven
state already known by the runtime. No extra UDP packet is required.

This is important:

- the overlay does not wait for a future update
- it restores the last state that the live runtime already holds
- this keeps the debug view deterministic even if the client is temporarily idle

## Step 7 - Use the manual test panel

The bottom panel exists to validate the feature quickly without rewriting a
client. It exposes actions such as:

- resync the preview from the live UDP state
- release all reticle and page bypasses
- select the first reticle on the active page
- toggle the selected reticle visibility
- nudge the selected reticle on the X axis
- force the preview to the current live page

This panel is intended for manual validation of the runtime debug mode itself.

## What This Feature Is For

Use the overlay when you need to:

- inspect what `mfd_window` currently renders
- verify that UDP commands are arriving
- compare live state and local test edits
- validate a reticle layout quickly during runtime
- debug page routing or reticle ownership problems

Do not use it as a replacement for the external client API. The live client and
the normal UDP command path remain the source of truth for runtime control.
