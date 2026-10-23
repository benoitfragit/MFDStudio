# API LEVEL 1 widgets example

This sibling preserves the original Containers example. It uses the same SDK,
UDP commands and feedback, with `mfd_widgets_api1.dll` at API LEVEL 1.

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
.\Scripts\Start-APILevel1Widgets.bat
```

The window listens on command port 47330 and feedback port 47331. RootAction
is a root button; RootToggle shares the nested ToggleA definition. Panel owns
Grid, whose CellCanvas contains a clipped, scrollable Linear with ToggleA and
ScrollLabel. Resize these containers in the editor to inspect placement. The
catalog includes all eleven semantic types with deliberately distinct visuals:
button, switch, checkbox, radio, slider, knob, gauge, progress bar, indicator,
numeric display and label. ToggleA and ToggleB share one definition and start
with different independent values. PrivateButton is interactive with
`exposed: false`: its numeric event identity remains available.

The example assets have one deterministic source. Regenerate them after an
intentional showcase change, or check that committed JSON still matches it:

```powershell
py -3 .\examples\apilevel1\widgets\generate_showcase_assets.py
py -3 .\examples\apilevel1\widgets\generate_showcase_assets.py --check
```

The script targets Python 3.7 or newer. It preserves the instance identifiers
used by the generated client and tests. Combo boxes and trees are not emulated:
API LEVEL 1 currently has no corresponding semantic widget types.

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

Slider proposals above 90 are deliberately rejected; accepted values also drive
Gauge, ProgressBar and NumericDisplay. This illustrates client authority without
putting policy in the widget framework.
