# API LEVEL 1 widgets example

This sibling preserves the original Containers example. It uses the same SDK,
UDP commands and feedback, with `mfd_widgets_api1.dll` at API LEVEL 1.

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
.\Scripts\Start-APILevel1Widgets.bat
```

The launcher starts both the visible Window and its generated client; the client
executable by itself is intentionally headless. The Window listens on command
port 47330 and feedback port 47331. The Page is a fuel-management cockpit rather
than a control catalog: one root Canvas owns an interactive transfer synoptic,
left/right feed panels, a Grid of fuel instruments, a lower Overlay annunciator
rail, and a clipped scrollable pump-status region. Resize the containers in the
editor to inspect the same hierarchy used at runtime.

All eleven semantic types remain present: button, switch, checkbox, radio,
slider, knob, gauge, progress bar, indicator, numeric display and label. ToggleA
and ToggleB share one definition but keep independent values. PrivateButton is
interactive with `exposed: false`; its numeric event identity remains available.

The central schematic follows an aircraft fuel page: two tapered wing tanks join
the center tank along one continuous white silhouette. Quantities and thin
orthogonal feed lines use green; units and interactive accents use cyan. The
bundled Share Tech Mono font keeps small labels readable (SIL Open Font License
included in assets/fonts). Three circular valves remain independent widgets.
Seven short classic Page reticles connect the ports, manifold and engine feeds;
none crosses a valve's central click target. Buttons share one compact rectangular
profile around the perimeter, and switches use squircle tracks. The authored
schematic has no generic widget cards or page-title overlay.

Each tank is a progress-bar widget. Its level rises from the fixed lower edge and
is cropped by two local convex polygons to follow the inclined wing walls, including
at empty and full capacity. The masks belong to the widget visual, not to a
page-sized reticle. Quantities and the BINGO threshold use kilograms (2000 kg per
tank, 6000 kg total); transfer flow uses kg/min. Values are illustrative.

The generated client makes the panel live immediately. Main tanks consume fuel;
an open S1/S2 valve transfers fuel from the center tank to its associated main tank
when CTR PUMPS and the central S3 valve are enabled.
The transfer-rate knob changes the flow and the **BINGO** slider sets the warning
threshold. **REFUEL** fills all three tanks continuously; press **STOP FILL** to
stop early, or let it stop automatically at capacity. Normal consumption resumes
afterwards. **XFER TEST** runs a short annunciator sequence. Updates publish at a
bounded 10 Hz cadence and are batched so the animation does not become one UDP
packet or allocation per widget and render frame.

The example assets have one deterministic source. Regenerate them after an
intentional showcase change, or check that committed JSON still matches it:

```powershell
py -3 .\examples\apilevel1\widgets\generate_showcase_assets.py
py -3 .\examples\apilevel1\widgets\generate_showcase_assets.py --check
```

The script targets Python 3.7 or newer. It preserves the original instance
identifiers used by the generated client and adds named tank/valve handles.
Combo boxes, list boxes and trees are not emulated: API LEVEL 1 currently has no
corresponding semantic widget types.

The generated C++ client activates the Page, receives typed input intent and
confirms state through the existing command channel. It exits after ten minutes
or a Window-close event. Closing the client leaves the runtime active; pending
requests expire and checked/value remain at their last confirmed state.

Both container and widget runtime DLLs are required, even for a Page with only
root widgets. The editor additionally requires both editor DLLs. The runtime
never needs the editor widget DLL. Run the launcher from the staged build tree
or the repository; keep the generated map aligned with the client and assets.

Follow [the authoring tutorial](../../../docs/tutorials/16_author_and_drive_widgets.md)
for shared visual editing, instance overrides, parent layout, import/export and
loss-aware downgrade. See [the handbook](../../../docs/src/handbook/widgets.md)
for bounded feedback semantics and compatibility limits.

In the editor, enable the pointer-and-response-ring tool in the Page preview to
test buttons, toggles, checkboxes, radio controls, sliders and knobs directly.
This session-only mode uses the runtime state machine but never mutates JSON or
undo history; disable it to restore selection and geometry gestures.
