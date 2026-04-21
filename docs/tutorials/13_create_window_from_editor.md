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

This also matters for the integrated tutorial: `client_tutorial` is only configured once the authored tutorial window, page, and reticle JSON files exist under the repository `assets/` tree.

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

## Result

You now have a full window created from scratch directly in `mfd_editor`, including geometry, typography, pages, default page selection, and UDP runtime transport configuration.
