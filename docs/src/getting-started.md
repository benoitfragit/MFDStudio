# Getting Started Tutorial

A complete, minimal path for a new developer: author a window, run it, send one
command, and verify the result. It expands [Quick Start](./quickstart.md) into a
single end-to-end story you can follow once and reuse.

Everything below uses files that ship with the repository, so you can complete
the loop without writing any new asset first.

## What you will build

```text
window JSON  ->  mfd_window (runtime)  ->  a client sends one command  ->  visible change
```

You will do it twice:

1. the fast path, with the shipped GUI client `client_mockup` (no C++ to write)
2. the typed path, with a generated C++ client driving the same runtime

## 1. Build the pieces

From a Visual Studio 2022 developer prompt at the repository root:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32 --target mfd_window client_mockup mfd_framebuffer_stdout_plugin
```

The first configure downloads third-party dependencies, so it is slower than
later builds. See [Build](./dev/build.md) for presets and options.

## 2. The window JSON

A window file is the runtime contract: window size, UDP transports, and the
pages to load. This tutorial uses the shipped minimal window
`assets/windows/demo_pages_minimal.json`:

```json
{
  "title": "MFD Minimal Radar",
  "size": [480, 480],
  "commands": { "udp": { "enabled": true, "address": "127.0.0.1", "port": 47230, "maxPacketSize": 16384 } },
  "feedback": { "udp": { "enabled": true, "address": "127.0.0.1", "port": 47231, "maxPacketSize": 4096 } },
  "defaultPage": "Radar",
  "reticleLibraryFolder": "../reticles",
  "pages": ["../pages/pfd.json", "../pages/navigation.json", "../pages/radar.json", "../pages/tactical.json"]
}
```

The two ports matter: `47230` receives commands, `47231` emits feedback. For the
exact field meanings see [Pages And Windows](./reference/pages_and_windows.md).

## 3. Launch the runtime

```powershell
.\Scripts\Start-MfdWindow.bat assets/windows/demo_pages_minimal.json
```

`mfd_window` opens, loads the window, and shows the `Radar` page. It now listens
for UDP commands on `127.0.0.1:47230`. Press `F1` for the runtime debug overlay
(active page, reticle tree, transport state).

## 4. Send one command (fast path)

Start `client_mockup`. It loads the same window JSON locally for discovery, then
talks to the runtime over UDP — it shares no memory with `mfd_window`.

1. select the preset that matches the launched window
2. choose a page and click **Activate selected page**
3. open a static reticle, change its **Position**, click **Send reticle update**

**Expected result:** the runtime window reacts immediately — the page switches
or the reticle moves. If it does, your command path is alive end to end.

A quick transport sanity check: in `client_mockup`, use **Window display** →
toggle **Invert colors** → **Send window display**. A reaction confirms the UDP
link before you debug command content.

## 5. The typed path: a generated C++ client

For an application that targets one known window, the generated API is the
preferred surface. It gives typed handles for pages, reticles, exposed
primitives, strobes, and dynamic sets, while `CommandClient` stays the transport
sender. See [Generated Client API](./handbook/generated_api.md).

### 5a. Generate the API at configure time

Generation is a CMake step, not a manual command. A consumer target calls
`client_api_generate_ui(...)`, which runs the Python generator and emits three
files that form **one contract**:

```cmake
client_api_generate_ui(
    WINDOW_JSON  "assets/windows/demo_pages_cockpit.json"
    OUTPUT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/generated/MockupUi.h"
    OUTPUT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/generated/MockupUi.cpp"
    OUTPUT_MAP   "${MFD_ROOT_DIR}/assets/windows/demo_pages_cockpit.generated.map"
    NAMESPACE    "mockup_ui"
    UI_CLASS_NAME "CockpitMockupUi")
```

- the generated **header + source** give the typed `*_Ui` class
- the `<window>.generated.map` sidecar lets the runtime accept id-based batches
- `mfd_window` must load the **matching** `.generated.map`, or generated batches
  are rejected

The map is a build artifact; it is regenerated from the window JSON and is not
meant to be hand-edited. See [Public API Contract](./reference/public_contract.md).

### 5b. Drive the runtime from C++

A generated client builds the transport with `CommandClient`, then mutates typed
handles and publishes one batch per cycle:

```cpp
#include "mfd/client/ClientSdk.h"
#include "MockupUi.h"

mfd::CommandClient client(udpCommandTransport, generatedTransportMap);
if (!client.IsReady())
{
    // inspect client.LastError()
}

mockup_ui::CockpitMockupUi ui;
auto& page = ui.Cockpit();

client.ActivatePage(page);   // send one command
ui.Initialize();             // full authored-state reset on the next batch

// each cycle: mutate handles, then publish the changed state
page.radarStatusBox.SetValue("SEARCH");
const std::vector<mfd::UserCommand> commands = ui.BuildBatch();
client.SendBatch(commands);
```

The shipped, buildable reference for this exact pattern is
`examples/client_mockup_minimal` (a headless client that drives the cockpit demo
from one plain `main` loop). Build and run it alongside `Start-MfdCockpit.bat`:

```powershell
cmake --build --preset debug-win32 --target client_mockup_minimal
.\Scripts\Start-MfdCockpit.bat
```

**Expected result:** the cockpit runtime animates from the client — ADI,
HUD, and radar values update every cycle, driven entirely over UDP.

## 6. Verify it worked

| Signal | Means |
| --- | --- |
| The runtime window reacts to a command | command path is alive end to end |
| `F1` overlay shows the expected active page and reticle tree | the runtime loaded what you authored |
| **Window display** toggles take effect | the UDP transport itself is healthy |
| Generated client runs without "batch rejected" logs | the runtime loaded the matching `.generated.map` |

## Common first errors

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Nothing reacts to commands | client and runtime on different ports | match the `commands.udp.port` in the window JSON (`47230` here) |
| Placement looks wrong | coordinates treated as pixels | authoring space is normalized `[-1, 1]`; see [Concepts](./concepts.md) |
| "Generated batches are rejected" | runtime loaded no/old `.generated.map` | regenerate and load the sidecar matching your client |
| Reticle update hits the wrong symbol | addressed a template, not the instance | runtime updates target the page **instance** id |
| Port already in use | a previous runtime is still bound | close the old `mfd_window`, or change the port |

## Where to go next

| Goal | Page |
| --- | --- |
| Short task-focused snippets | [Cookbook](./cookbook.md) |
| Typed client surface in depth | [Generated Client API](./handbook/generated_api.md) |
| Embed the runtime without a window | [Offscreen Embedding](./handbook/offscreen.md) |
| Exact JSON fields | [Pages And Windows](./reference/pages_and_windows.md) |
| What is stable to depend on | [Public API Contract](./reference/public_contract.md) |
