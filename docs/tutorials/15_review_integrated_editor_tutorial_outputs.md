# Review The Integrated Editor Tutorial Outputs

The integrated editor tutorial now stops after the authoring flow and the
editor workflow discovery steps.

This page is the follow-up hand-off: open the real files on disk, confirm what
the tutorial saved, then continue with the runtime and generated-client docs.

## What The Tutorial Changes

- only the tutorial assets under `examples/tutorial/assets/`
- no repository source rewrite during the walkthrough
- `examples/CMakeLists.txt` already contains `add_subdirectory(tutorial/client)` in Git
- `examples/tutorial/client/CMakeLists.txt` still returns immediately when the tutorial assets are missing

## Step 1 - Inspect `Page1`

Open:

- `examples/tutorial/assets/pages/mfd_tutorial_page1.json`

Focus on these authored sections:

- `titleDisplay`
- `layers`
- `dynamicReticleBindings`
- `strobes`
- `staticReticles`

The important Page1 rule is:

- the tutorial frames the generated page title through `titleDisplay`
- `mfd_tutorial_aircraft` is instantiated once as the static ownship reference at `x = 0.0`, `y = -0.7`
- that static ownship instance is then renamed locally to `page1_ownship`, while its `template` still points to `mfd_tutorial_aircraft`
- `inspired_steering_cue` is seeded in the tutorial reticle folder and stays bound on the overlay layer
- the tutorial adds `mfd_tutorial_radar_track` on `RadarTrackLayer`
- `Default` and `Strobe1` now use two distinct templates
- the default strobe cursor exposes its line primitives for generated-client mutation
- `aircraft_label` is exposed, aligned to the right, and opts out of parent reticle rotation and scale so it stays upright on the alternative strobe

Minimal before / after for `dynamicReticleBindings`:

Before:

```json
[
  { "templateId": "inspired_steering_cue", "layerId": "overlay", "orderInLayer": 0 }
]
```

After:

```json
[
  { "templateId": "inspired_steering_cue", "layerId": "overlay", "orderInLayer": 0 },
  { "templateId": "mfd_tutorial_radar_track", "layerId": "RadarTrackLayer", "orderInLayer": 0 }
]
```

The tutorial should not ask the user to reconfigure the cue.

You should also see one explicit title chrome block in `Page1`, for example:

```json
"titleDisplay": {
  "decoration": "frame"
}
```

The exact transform, line style, or color can differ if you adjusted them while
following the guided title step.

## Step 2 - Inspect The Window Asset

Open:

- `examples/tutorial/assets/windows/mfd_tutorial.json`

Confirm that the window points to:

- the tutorial page files
- the tutorial reticle library folder
- the UDP command / feedback setup used by `tutorial_client`
- the explicit `fastIntervalMs` / `heartbeatIntervalMs` cadence fields used for
  active-page strobe feedback

## Step 3 - Inspect The Tutorial Reticles

Open these files as needed:

- `examples/tutorial/assets/reticles/mfd_tutorial_radar_track.json`
- `examples/tutorial/assets/reticles/mfd_tutorial_aircraft.json`
- `examples/tutorial/assets/reticles/mfd_tutorial_circle.json`
- `examples/tutorial/assets/reticles/mfd_tutorial_progress_bar.json`
- `examples/tutorial/assets/reticles/mfd_tutorial_strobe_cursor.json`
- `examples/tutorial/assets/reticles/inspired_steering_cue.json`

These are the authored reticle templates created or seeded by the integrated
tutorial flow.

Focus especially on:

- `mfd_tutorial_strobe_cursor.json`: the crosshair lines are marked `exposed`
- `mfd_tutorial_aircraft.json`: `aircraft_label` is marked `exposed`, sets `align: "right"`, and keeps `reticleRotationSensitive: false` plus `reticleScaleSensitive: false`

## Step 4 - Inspect The Generated Runtime Map

Open:

- `examples/tutorial/assets/windows/mfd_tutorial.generated.map`

Use it to confirm that:

- `Page1` and `Page2` are exported
- authored static reticles are mapped
- authored strobe reticles are mapped with `source: "strobe"`
- dynamic templates are registered for generated client usage
- exposed primitives from both page reticles and strobe reticles receive generated ids

## Step 5 - Inspect The Runtime Entry Point

Open:

- `examples/tutorial/client/src/main.cpp`

This file shows how the saved assets are consumed:

- `Page1()` and `Page2()` typed handles
- `defaultReticle` and `strobe1Reticle` patching the currently selected Page1 strobe reticle
- `DynamicMfdTutorialRadarTrack()` for both the persistent linked pair and the spawned transient contacts
- `DynamicInspiredSteeringCue()` linking the persistent pair while their source state stays in business coordinates
- `mfd::UserSpaceProjector` converting nautical miles and radians into page-space commands
- `strobe` feedback changing captured dynamic tracks to a different color and thickness
- the alternative strobe label staying upright unless the client explicitly rotates or scales that primitive
- the Page2 progress bar animation

## Step 6 - Inspect The Build Gate

Open:

- `examples/tutorial/client/CMakeLists.txt`

The tutorial client is part of the examples tree by default, but the target
still self-skips until these assets exist:

- `examples/tutorial/assets/windows/mfd_tutorial.json`
- `examples/tutorial/assets/pages/mfd_tutorial_page1.json`
- `examples/tutorial/assets/pages/mfd_tutorial_page2.json`
- the tutorial reticle JSON files
- `examples/tutorial/assets/reticles/inspired_steering_cue.json`

That keeps a fresh clone buildable while avoiding any tutorial-time rewrite of
repository source files.

## Next Reading Order

Continue with:

1. [Create A Window From Scratch In `mfd_editor`](./13_create_window_from_editor.md)
2. [Test A Window With The Demo Client](./03_test_with_mfd_demo.md)
3. [Use The Demo Client As A Client API Reference](./11_use_the_demo_client_as_a_client_api_reference.md)
4. [Generated Client API](../src/handbook/generated_api.md)
5. [Capture The Window As Raw Pixels](./07_framebuffer_rgba32_capture.md)
