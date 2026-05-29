# Create A Window From Scratch In `mfd_editor`

This tutorial shows how to create a brand-new window directly from the editor UI, without starting from an existing preset JSON file.

![Editor screenshot](../images/mfd_editor_capture.png)

You will learn how to:
- launch the integrated guided tutorial from the editor
- create a window file from scratch
- set size and screen position
- configure a window font
- add an initial page
- define the default page
- import an existing page asset with its reticle dependencies
- rename one shared page asset safely across windows
- rename one shared reticle template safely across pages
- highlight the pages using one selected shared reticle template
- focus one page layer without changing authored JSON
- remove a page from the current window or delete its asset safely
- configure incoming command UDP and outgoing feedback UDP
- use the page-preview View menu without modifying authored JSON assets
- understand how the generated C++17 client API mirrors the authored window
- discover that the page title is a dedicated chrome element that can be framed, moved, scaled, hidden, and recolored
- create `RadarTrackLayer` on `Page1` and bind `mfd_tutorial_radar_track` without changing the existing steering cue
- continue with the right generated-API and documentation path once the editor tour is done

## Guided Tour Overview

The integrated editor tutorial is available from:

- **Help > Tutorial**
- the empty-state **Launch the tutorial** button

That guided flow is intentionally split into four phases:

1. **Author in editor**: create the tutorial window, pages, the framed `Page1` title chrome, shared reticles including the triangle-based `mfd_tutorial_aircraft`, two distinct Page1 strobes, `RadarTrackLayer`, and the exposed primitives used by the generated client.
2. **Explore editor tools**: use the integrated coach panel in the page preview, inspect the helper overlays, and open the import / rename / export workflows without mutating the tutorial assets.
3. **Review saved outputs**: open the dedicated follow-up guide and inspect the saved assets, generated map, and runtime entry points outside the editor.
4. **Continue in docs**: follow the mockup, generated-client, and architecture reading path.

The coach is integrated at the top of the page preview as one scrollable panel. It shows the current stage, the global progress, and the exact action still expected on each blocked UI step.

## Step 1 - Open the window wizard

1. Launch `mfd_editor`.
2. The editor starts empty, without auto-loading any staged `_Exec` asset copy.
3. Use **Open window asset...** if you want to browse to one existing window JSON from the file explorer.
4. For this tutorial, open the top menu: **File > New window from scratch**.

The popup creates an in-memory window draft. Nothing is written on disk until you click **File > Save**.

By default, the editor seeds creation paths from the repository source
`assets/` tree. If your authored assets live elsewhere, launch:

```powershell
mfd_editor.exe --asset-directory C:\Path\To\assets
```

## Step 2 - Fill the window fields

In **Create new window**, set:
- **Window file** (for example `assets/windows/my_window.json`)
- **Window title**
- **Size (px)** (`width`, `height`)
- **Position (px)** (`x`, `y`)
- **Font file (optional)** (for example `assets/fonts/ShareTechMono-Regular.ttf`)
- **Reticle library folder** (usually `assets/reticles`)

These fields map directly to the root window JSON.

Use the **Browse window file...**, **Browse page file...**, and
**Browse font file...** buttons to open native Windows file pickers. Use
**Browse reticle folder...** to open the native Windows folder picker. Returning
from those native pickers keeps the **Create new window** popup open so you can
finish the draft without re-opening the wizard.

The editor still guides you toward the source `assets/` tree because that is
the safest place to keep authored JSON, fonts, and images under version control.

The editor no longer treats `_Exec` as one blocked special case during authoring or reference scans. You can still open or scan staged runtime copies when needed, but the source `assets/` tree remains the recommended location for long-term editing and generated-client workflows.

This also matters for the integrated tutorial: `client_tutorial` is already
registered in `examples/CMakeLists.txt`, but its own
`examples/client_tutorial/CMakeLists.txt` returns immediately while the
tutorial assets are missing. The walkthrough now writes only the tutorial
assets under `assets/`, then hands off to one follow-up doc that tells you
which runtime files to inspect next. `Scripts/Start-MfdTutorial.bat` becomes
usable as soon as the assets exist and the target has been configured.

That source-tree discipline also feeds the editor asset reference index used by upcoming safe import, rename, highlight, and diagnostics workflows. Keeping authored windows, pages, reticles, fonts, and images under the real repository asset tree is what lets those tools resolve dependencies reliably.

## Step 3 - Configure UDP communication

### Commands UDP (incoming to the window runtime)
- **Enable command UDP**
- **Command address**
- **Command port**
- **Command max packet**

### Feedback UDP (outgoing from runtime to clients)
- **Enable feedback UDP**
- **Feedback address**
- **Feedback port**
- **Feedback max packet**
- **Fast interval**
- **Heartbeat interval**

Use loopback (`127.0.0.1`) when the client and runtime are on the same machine.
The cadence fields define how quickly active-page feedback changes are emitted
versus unchanged heartbeat snapshots.

Once the window already exists, reopen the same window-level inspector from
**Window > Window settings**. That menu sits before **Page** in the menu bar
and lets you retune transports, title, size, and feedback cadence without
switching page context.

## Step 4 - Create the initial page

Enable **Create one initial page**, then fill:
- **First page name**
- **First page title**
- **First page file**
- **First page background**

The editor marks this first page as the default startup page.

## Step 5 - Create and save

1. Click **Create window**.
2. Check the status bar message: the draft exists in memory.
3. Click **File > Save** to write:
   - the root window JSON
   - the first page JSON (if enabled)
   - reticle template files when relevant

## Step 6 - Add more pages and choose another default page

After creation:
1. Use **Page > New page** to add more pages.
2. Select a page in the tree.
3. In the page inspector, toggle **Default page for this window** on the page that should open at startup.

### Dynamic reticles on a page

In the selected page inspector, the **Dynamic reticles** section now keeps the
workflow explicit and local to the page:

1. choose one reticle template in the **Reticle template** combo
2. choose one runtime page layer in the **Layer** combo
3. click **Add**

Each added entry then stays in the page list with:

- its template id
- one **Layer** combo to move it to another page layer
- **Move earlier** / **Move later** to adjust `orderInLayer` inside that layer
- **Remove** to delete the binding

Important: these are runtime bindings only. Adding `mfd_tutorial_radar_track`
or `inspired_steering_cue` here does not create authored static reticles on the
canvas by itself. They remain stored in `dynamicReticleBindings`, and the live
client later creates the actual runtime instances.

In the integrated tutorial, `Page1` already keeps `inspired_steering_cue` on
its authored layer. The guided flow first creates `RadarTrackLayer` in
**Page layers**, then asks you to add only `mfd_tutorial_radar_track` on that
new layer. The cue stays unchanged during this step.

The same guided flow also stops on the dedicated page-title inspector: on
`Page1`, it asks you to select the generated title chrome, then switch its
`Decoration` to `Frame`. That walkthrough exists to make it clear that the page
title is no longer one hardcoded overlay. It is an authored chrome element with
its own visibility, color, line style, and transform.

Later steps return to the primitive inspector on `mfd_tutorial_aircraft`: the
tutorial asks you to expose `aircraft_label`, then disable its parent reticle
rotation and scale inheritance. That is the authored way to keep one text
primitive upright while the alternative Page1 strobe reticle itself can still
rotate or scale at runtime.

The generated client API keeps the same boundary: it validates that these page
bindings are coherent, but it does not expose any client-side control to move a
dynamic reticle to another layer at runtime.

## Step 7 - Use the page-preview selection workflow

Once a page contains several reticles, the page preview supports direct authoring gestures:

- `Ctrl+click` on one page reticle to add it to the current selection
- `Ctrl+click` again to remove it from the current selection
- press `Esc` to clear the current page-reticle selection entirely
- drag one selected reticle to move the full selected group with one gesture
- use `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, or `Del` on the current page-reticle selection

The inspector and the **Edit** menu mirror the same actions with explicit buttons.

## Step 8 - Use the right-click menu on overlapping reticles

When several reticles overlap in the page preview:

1. right-click on the stacked area
2. the popup lists every hovered reticle instead of forcing only the nearest one
3. open the submenu of the reticle you actually want
4. use the clipping submenu for the hovered convex primitive you want to mask through

The same popup also exposes **Copy selection**, **Cut selection**, **Paste copies**, and **Delete selection** for the current page-reticle group.

## Step 9 - Use the Page Preview View menu

The page preview now owns one editor-only **View** menu. Open it from the preview header to toggle:

- **Layer Inspector**
- **Minimap**
- **Problems**
- **Highlight reticle usages**
- **Reticle names**
- **Gizmos**
- **Page context**

Default startup behavior stays intentionally conservative:

- **Layer Inspector** starts off
- **Minimap** starts off
- **Problems** starts off
- **Highlight reticle usages** starts off
- **Reticle names** starts on
- **Gizmos** starts on

These switches are session preferences only. They help you inspect the current page, but they do not change the authored window/page/reticle JSON files.

If the **View** button shows `!`, validation diagnostics exist and you can enable **Problems** to inspect them directly from the preview.

When **Problems** is enabled, the diagnostics are now shown in one full-width
docked panel under the preview. The list is scrollable, so the minimap and the
actual page canvas stay visible and interactive while you review the issues.

### Fullscreen preview

The page-preview editing header now exposes one small fullscreen toggle
immediately next to **View**.

The toggle is only available in the page-editing workspace, not in the
reticle-studio **Page context** panel.

Use it when you want to edit the page with the editor chrome hidden:

1. click the `[]` button next to **View**
2. the sidebar, inspector, page-context split, and docked helper panels are hidden
3. keep using zoom, pan, selection, and gizmos directly on the page canvas
4. click the same button again, press `F11`, or press `Esc` to restore the previous layout

`Esc` exits fullscreen first, then falls back to the normal page-selection clear
behavior when fullscreen is not active.

## Step 10 - Import an existing page

When one page already exists in another asset tree, use **Page > Import page...**
or drop the page JSON file directly onto the editor window.

The import popup now previews:

- the selected source page JSON
- the target page file staged in the current window asset tree
- every reticle-template dependency referenced by the page
- the deterministic collision outcome for pages and templates

Current collision rules:

- missing target file: copy it into the current assets tree
- identical existing target file: reuse it
- different existing target file: create one renamed imported copy

If one imported reticle template contains image primitives, the editor also
rewrites relative file paths so the staged template stays valid once it is
saved into the current reticle library folder.

The imported page and any imported templates are added to the in-memory
document immediately, then written with the next **File > Save**.

## Step 11 - Remove or delete a page

When you no longer want one page in the current window, right-click the page inside the left tree or use the page inspector buttons.

- **Remove page from window** detaches the page from the current window but keeps its JSON file so it can be re-imported or reused later.
- **Delete page asset...** removes the page from the window and marks its JSON file for deletion on the next **Save**.

Safety rules now enforced by the editor:

- the window must always keep at least one page
- removing or deleting the default page requires choosing a replacement default page first
- deleting a page file outside the source `assets/` root is blocked unless you explicitly confirm the advanced override

Both actions create one undo step and leave the document dirty until you save or reload.

## Step 12 - Rename a page safely

When one page JSON is reused by several source windows, use the safe rename
workflow instead of editing only the page name field locally.

Open it from any of these entry points:

- right-click one page in the left tree, then choose **Rename page globally...**
- use the page inspector button **Rename page globally...**
- use **Page > Rename current page globally...**

The popup now shows:

- the current page name
- the requested new page name
- every scanned window reference found under the current asset root
- the exact JSON files that will be rewritten
- blocking name collisions, if any
- a warning that the generated client API and `mappingHash` may change

Current behavior:

- the page JSON itself is always rewritten
- window JSON files are rewritten only when their `defaultPage` still points to
  the old page name
- the scan includes staged `_Exec` assets too when they are part of the scanned asset tree
- collisions inside any referenced window block execution before any file is modified
- the rename writes directly to the scanned JSON files and does not wait for the next
  **File > Save**

After a successful rename, regenerate the generated client API if this page is
part of an exported window contract.

## Step 13 - Rename a reticle template safely

Pages live inside windows. Reticle templates live inside pages through page
`template` references.

Use the safe reticle rename workflow when one shared template is reused by
several authored pages.

Open it from any of these entry points:

- right-click one template in the reticle-library tree, then choose **Rename reticle globally...**
- use the library-reticle inspector button **Rename reticle globally...**
- use **Reticle > Rename selected library reticle globally...**

The popup now shows:

- the current template id
- the requested new template id
- the current template JSON file
- the optional target template JSON file when you also rename the file
- every scanned page or strobe reference found under the current asset root
- the exact JSON files that will be rewritten or deleted
- blocking collisions when one page already references the requested target template id
- a warning that the generated client API and `mappingHash` may change

Current behavior:

- the template JSON `id` is always rewritten
- page `staticReticles[*].template` references are rewritten
- page `strobe.template` references are rewritten
- page `dynamicReticleBindings[*].templateId` references are rewritten too
- you can choose between logical rename only and logical rename plus template-file rename
- when the template file moves, relative image paths are rewritten automatically
- the scan includes staged `_Exec` assets too when they are part of the scanned asset tree
- the rename writes directly to the scanned JSON files and does not wait for the next
  **File > Save**

After a successful rename, regenerate the generated client API if this template
is part of an exported runtime contract.

## Step 14 - Highlight pages using the selected reticle

The page preview `View` menu keeps **Highlight reticle usages** disabled by
default.

When you need to inspect template reuse:

1. select one reticle template in the left **Reticle library** tree
2. open the page-preview **View** menu
3. enable **Highlight reticle usages**

The editor then:

- highlights the pages in the current page tree that reference that template
- outlines matching instances on the active page preview
- shows a small usage summary directly in the preview overlay

If no page currently uses the selected template, the preview shows one discreet
message instead of forcing any selection change or JSON rewrite.

## Step 15 - Use Layer Focus Mode

The page-preview **Layer Inspector** is optional and starts disabled by
default.

When you enable **View > Layer Inspector**:

1. the preview shows a docked panel on the left with **Full View** plus the
   current runtime page layers
2. each entry now includes one small thumbnail preview and one reticle count
3. hidden layers keep a dimmed thumbnail so you can still identify them
4. click **Full View** to keep the normal selection behavior
5. click one layer to focus it

In focus mode:

- only reticles assigned to the active layer stay selectable and editable
- other visible layers stay rendered, but with a dimmed appearance
- `Esc` exits the focus layer first, then falls back to the normal selection
  clear shortcut

This workflow uses the authored runtime page-layer order together with one
editor-only preview visibility state per layer.

## Step 16 - Export design documentation

When you need one designer-facing package for the currently loaded window:

1. open **File > Export > Export design...**
2. choose one output folder
3. keep or adjust the export options for Markdown ICD files, exploded views, coordinates, snippets, strobe, blink, primitive ids, and mapping hash
4. click **Export**

The export creates:

- `README.md`
- `window_icd.md`
- `pages/*.md`
- `images/*_design_exploded.png` when exploded views are enabled and rendered successfully
- `data/design_manifest.json`

If the chosen folder already contains files, the editor creates one timestamped
subfolder automatically instead of overwriting unrelated content.

The Markdown output keeps relative links between pages and images so the export
can be opened directly in GitHub, VS Code preview, or any other Markdown
viewer.

## Step 17 - Use the integrated tutorial as the project overview

Once the base editor workflow feels clear, launch **Help > Tutorial** and let
the guided coach walk you through the repository-specific tutorial asset set.

This integrated flow does more than explain clicks:

- it seeds the editor popups with the tutorial window, page, and reticle values
- it highlights the exact control expected by the current step
- it explains why each authored asset matters for the runtime contract
- it now stays focused on authoring plus editor workflows, then hands off the saved-file review to one dedicated follow-up doc

By the end of the integrated flow, the user should understand the full chain:

- editor action
- saved authored JSON
- static ownship anchor through `mfd_tutorial_aircraft` on `Page1`
- exposed `aircraft_label` plus its disabled parent rotation/scale inheritance
- `RadarTrackLayer` plus the `mfd_tutorial_radar_track` binding on `Page1`
- generated transport map
- generated C++17 page and reticle wrappers
- runtime client loop
- documentation path for deeper study

## Step 18 - Open the follow-up guide, then continue with the generated API docs

`client_tutorial` is part of the examples tree by default, but it still
self-skips while the tutorial assets do not exist.

The integrated walkthrough now keeps the repository source tree stable:

- `examples/CMakeLists.txt` already registers `client_tutorial`
- `examples/client_tutorial/CMakeLists.txt` still self-skips when the tutorial asset set is incomplete
- the walkthrough itself only writes the tutorial assets under `assets/`
- the saved-file and runtime follow-up now lives in [Review The Integrated Editor Tutorial Outputs](./15_review_integrated_editor_tutorial_outputs.md)

After the integrated tour, the recommended follow-up order is:

1. [Review The Integrated Editor Tutorial Outputs](./15_review_integrated_editor_tutorial_outputs.md)
2. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)
3. [Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
4. [Generated Client API Architecture](../architecture/generated_client_api.md)
5. [Capture The Window As Raw Pixels](./07_framebuffer_rgba32_capture.md)

## Undo behavior

The editor keeps one undo snapshot per page-modifying action:

- one paste action
- one cut action
- one delete action
- one completed drag gesture, including grouped reticle moves
- one clipping change

`Copy` alone does not create an undo step because it only updates the internal clipboard and does not modify the authored page.

## Result

You now have a full window created from scratch directly in `mfd_editor`, including geometry, typography, page creation/import flows, fullscreen preview, safe shared-page and shared-reticle rename handling, design-document export, default page selection, and UDP runtime transport configuration.

With the integrated guided tour on top of that workflow, the editor now also
serves as a clean project overview: authored assets, editor workflows,
generated API, runtime client behavior, and the next documentation layers all
stay connected in one logical path.
