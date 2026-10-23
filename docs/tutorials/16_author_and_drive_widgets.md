# Author and drive API LEVEL 1 widgets

1. Build `mfd_editor`, `mfd_window`, the container and widget extensions, and
   `apilevel1_widgets_client` with `cmake --build --preset debug-win32`.
2. Open `examples/apilevel1/widgets/assets/windows/widgets_window.json` in the
   editor. The left sidebar shows **Pages → Reticle library → Containers → Widgets**.
   LEVEL 0 retains its two sections; LEVEL 1 keeps both extension sections even
   when the current Page has no instances. The showcase uses visibly different
   controls for every supported semantic type. It does not fake combo boxes or
   trees because API LEVEL 1 has no such types.
3. Open **Widgets → Library**. Select the shared toggle definition. Edit its ID,
   default text or preferred size directly in the library. **Save library and
   document** writes the asset with the existing save transaction.
4. Select `checked` under **Visual state / preview**, click **Edit visual in
   Reticle studio**, change a primitive color and click **Apply edited visual**
   in Widgets. Both ToggleA and ToggleB use the changed shared visual.
5. Select ToggleA on the Page. Change only **Override checked**. ToggleB keeps
   its own checked value. **Duplicate** adds another independent instance;
   **Duplicate definition** in the library instead makes a new reusable asset.
6. Edit the instance's **Parent**, grid row/column/span and alignment. An empty
   parent moves it to Page root. Drag the selected widget's corner to resize;
   parent Grid/Linear placement still governs its resolved position. Select a
   parent container and resize/scroll it to inspect the shared layout and clip.
7. Use **Import** in the widget library to bring in a definition package. Use
   **Export definition and resources** to create one. Page import preserves
   typed widget ownership edges and remaps colliding resources. Import into a
   LEVEL 0 Window is rejected before changing the document; explicitly upgrade
   the destination first. Undo/Redo applies to successful edits/imports.
8. Save and build `apilevel1_widgets_client` again to regenerate the C++ API/map.
   Launch `.\Scripts\Start-APILevel1Widgets.bat` from the repository root.
   Click ToggleA, then ToggleB; each feedback request is confirmed independently
   by the client. Drag Slider/Knob and inspect their confirmed values. Stop the
   client to see pending proposals time out without changing confirmed state.
9. Read `examples/apilevel1/widgets/client/main.cpp`: `PollFeedback` and
   `TakeWidgetEvents` receive typed requests, `Matches` selects typed handles,
   and setters plus `Acknowledge` confirm them in the next generated batch.
   PrivateButton deliberately has no public generated handle; the example uses
   its numeric event identity with the same SDK command envelope.
10. In a copy of the assets, try downgrading the Window. Review the counts,
    cancel once, then confirm and Undo. Shared definition assets are retained.
    Never resave widget-bearing assets with a previously distributed editor:
    that binary cannot be made aware of fields added in this release.

The checked-in showcase JSON is generated deterministically. Run
`py -3 .\examples\apilevel1\widgets\generate_showcase_assets.py` to restore its
baseline, or append `--check` in validation jobs. Regeneration intentionally
overwrites edits made directly to these example JSON files.

For DLL requirements, bounded UDP recovery and current picking/clipping limits,
see [the widget handbook](../src/handbook/widgets.md).
