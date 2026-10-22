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
ScrollLabel. Resize these containers in the editor to inspect placement. The catalog includes
all eleven semantic types. ToggleA and ToggleB share one definition and start
with different independent values. PrivateButton is interactive with
`exposed: false`: its numeric event identity remains available.

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
