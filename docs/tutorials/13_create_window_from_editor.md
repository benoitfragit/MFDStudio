# Create A Window From Scratch In `mfd_editor`

This tutorial shows how to create a brand-new window directly from the editor UI, without starting from an existing preset JSON file.

You will learn how to:
- create a window file from scratch
- set size and screen position
- configure a window font
- add an initial page
- define the default page
- configure incoming command UDP and outgoing feedback UDP

## Step 1 - Open the window wizard

1. Launch `mfd_editor`.
2. The editor starts empty, without auto-loading any staged `_Exec` asset copy.
3. Use **Open window asset...** if you want to browse to one existing window JSON from the file explorer.
4. For this tutorial, open the top menu: **File > New window from scratch**.

The popup creates an in-memory window draft. Nothing is written on disk until you click **File > Save**.

## Step 2 - Fill the window fields

In **Create new window**, set:
- **Window file** (for example `assets/windows/my_window.json`)
- **Window title**
- **Size (px)** (`width`, `height`)
- **Position (px)** (`x`, `y`)
- **Font file (optional)** (for example `assets/fonts/ShareTechMono-Regular.ttf`)
- **Reticle library folder** (usually `assets/reticles`)

These fields map directly to the root window JSON.

Use the **Browse ... folder** buttons when creating new assets. The editor now guides you toward the source `assets/` tree and blocks `_Exec` staging folders, because `_Exec/assets` is only a runtime copy and would break later CMake-generated client API steps.

This also matters for the integrated tutorial: `client_tutorial` is only configured once the authored tutorial window, page, and reticle JSON files exist under the repository `assets/` tree. `Start-MfdTutorial.bat` becomes usable at the same point.

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

Use loopback (`127.0.0.1`) when the client and runtime are on the same machine.

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

## Undo behavior

The editor keeps one undo snapshot per page-modifying action:

- one paste action
- one cut action
- one delete action
- one completed drag gesture, including grouped reticle moves
- one clipping change

`Copy` alone does not create an undo step because it only updates the internal clipboard and does not modify the authored page.

## Result

You now have a full window created from scratch directly in `mfd_editor`, including geometry, typography, pages, default page selection, and UDP runtime transport configuration.
