# LHLD

The LHLD example is a standalone asset/client pair under `examples/lhld`.
It renders a tactical Multi-Function Display with four pages:
**Radar/FCR** (fire-control radar B-scope), **SMS** (stores management),
**NAV/HSD** (horizontal situation display) and **A-G** (air-to-ground bomb
profile). The pages are shaped from the local Falcon BMS / Dash-34 datapackage
references and driven by a deterministic fake radar/airspace simulation.

- Assets: `examples/lhld/assets`
- Client: `examples/lhld/client`
- Runtime window: `examples/lhld/assets/windows/lhld_window.json`
- Client target: `lhld_client`
- Visual references: `examples/lhld/visual_ref`

The MFD pages use the bundled `ShareTechMono-Regular.ttf` font under the SIL
Open Font License 1.1. The `.ttf` and `OFL-ShareTechMono.txt` files live under
`examples/lhld/assets/fonts` and are staged with the runtime assets. Text
primitives define explicit letter spacing to avoid the overly tight raylib
default-font look.

Unlike the HUD, which drives a separate `mfd_window` process over UDP, the
MFD client **hosts the runtime in-process**: it embeds `mfd_runtime_api`
(`RuntimeSession` + `OffscreenSurface`), renders the active page offscreen, and
composites the resulting image into a DX11 texture drawn at the center of a
20-button MFD bezel painted with Dear ImGui. The generated UI still publishes
command batches over loopback UDP, so the integration boundary is identical to
every other MFD client.

Build the client (Win32 only):

```powershell
cmake --build --preset debug-win32 --target lhld_client
```

Run it from the repo or staged build tree:

```powershell
.\Scripts\Start-LHLD.bat
```

No separate runtime window is launched - the MFD screen is rendered offscreen
inside the client and shown in the bezel.

## Layering

The client mirrors the HUD layering so an external avionics producer can
replace the mini-simulation while keeping the projection, controller and
generated UI:

- `MfdTypes.h` - semantic sample (`MfdInputSample`) plus projected page frames.
- `MfdInputSource.h` - replaceable semantic producer contract. A real simulator
  can implement this interface and inject it into `MfdApplication` without
  changing the generated UI or page assets.
- `MfdProjection.h/.cpp` - stateless projection: airspace to FCR B-scope, and
  polar navigation to the heading-up HSD compass rose, including visible route
  legs between consecutive steerpoints.
- `MfdRadarSimulation.h/.cpp` - deterministic fake airspace/radar producer. It
  owns track motion, closure/aspect derivation and the animated azimuth sweep.
  **Operator controls are pushed in from the panel and never mutated here.**
- `MfdController.h/.cpp` - generated-UI adapter that writes the pages every
  frame, owns text formatting and lazily retains capturable dynamic-track
  handles. It owns no simulated radar state.
- `OffscreenMfdView` / `Dx11TextureUploader` - host the runtime offscreen and
  upload its RGBA8 frame into a DX11 texture. They are split into separate
  translation units so raylib and `windows.h`/Direct3D headers never mix.
- `MfdApplication.h/.cpp` - the ImGui bezel, MFD image and control panel.

## Control is separate from the simulation

Exactly like the HUD, the control surface (the 20 OSB bezel and the
collapsible control panel) only collects operator intent into
`RadarSettings`, `StoresState`, the shared `MasterMode` and the navigation
selection. That intent is pushed into the simulation once per frame; the
simulation owns physics and airspace state. The bezel buttons carry no
business logic beyond selecting the page or toggling a control.

## Pages

- **Radar/FCR:** Falcon-style B-scope with the scan volume between cyan azimuth
  limits and avionics data around the complete MFD perimeter. It includes the
  horizon line, antenna-elevation caret, acquisition cursor and altitude limits,
  range/bars/scan/PRF/bullseye data, assignment/weapon cues and bottom FCR
  legends. RWS search tracks use white bricks; TWS trackfiles and bugged/STT
  tracks use their distinct yellow symbols; extrapolated tracks turn red. A
  bugged track adds target data (altitude, aspect, closure, heading and speed),
  while STT uses the open square-in-circle presentation. Centered `FCR OFF` and
  `NO RAD` messages replace the live presentation for power-off and RF-silent
  states. Tracks are filtered by azimuth, slant range and antenna elevation;
  increasing the bar count expands vertical coverage and slows the sweep.
- **SMS:** stores diagram with the nine weapon stations, the shared master mode,
  selected weapon and submode, the selected-station box, delivery parameters and
  a panel-side jettison action that clears the carried-store inventory.
- **NAV/HSD:** heading-up compass rose, ownship symbol, numbered steerpoints
  (current one highlighted), flight-plan route legs, the bullseye and a
  steerpoint data block (bearing/range/ETA, ground speed and Mach).
- **A-G:** air-to-ground delivery profile page based on the local SMS A-G
  references. It shows selected station, quantity, config, delivery mode,
  fuzing, dispense mode, ready weapon count, time-to-go, pair/single, nose/tail,
  ripple, interval, target steerpoint, profile and release status.

## Interaction

- The top selector uses **RADAR / SMS / NAV / A-G** for clarity.
- **OSB 12/13/14/15** select the FCR, SMS, HSD and A-G pages on any page.
- Keyboard **1 / 2 / 3 / 4** selects RADAR, SMS, NAV and A-G respectively.
- On **FCR**, OSB 2 toggles RWS/TWS, OSB 6 cycles range, OSB 17 toggles the
  azimuth scan width, OSB 18 cycles bars and OSB 19 toggles PRF. The panel antenna
  elevation slider changes the search volume, so targets can enter or leave the
  scope instead of only moving the elevation caret. OPER/SILENT/OFF exercise
  the corresponding radar presentation states.
- On **SMS**, OSB 1/2/3 select the master mode, OSB 6 steps the station and
  OSB 7 cycles the profile. The **Stores (SMS)** control-panel section exposes
  **JETTISON**; it clears all stations in the demo inventory, hides the station
  select box and drives the SMS status to `JETT`.
- On **HSD**, OSB 1 toggles centered/decentered presentation, OSB 2 toggles
  declutter for route legs and bullseye, OSB 6 cycles the range scale, OSB 7
  steps the current steerpoint and OSB 8 inserts the draft waypoint after the
  selected steerpoint, up to the authored HSD slot limit. The panel exposes the
  same HSD toggles, bearing/range sliders and an overwrite action for the
  selected steerpoint.
- On **A-G**, OSB 1/2 select CCIP/CCRP, OSB 6 steps only through loaded A-G
  release stations, OSB 7 cycles the profile, OSB 8 toggles single/pair, OSB 9
  cycles quantity, OSB 10 cycles interval and OSB 11 performs a simulated
  release. MASTER ARM removes the store from the demo inventory; SIM leaves
  stores loaded but still drives release timing.
- Click on the FCR screen, or use Cursor X/Y, to slew the normalized acquisition
  cursor. **BUG** designates exactly the dynamic track captured by the runtime
  strobe, **STT** promotes that bug to single-target track and **UNBUG** returns
  to the previous search mode. No nearest-track calculation is used.

## FCR integration API

An external radar publishes one `RadarTrack` per sensor track using slant range
in nautical miles and azimuth/elevation in degrees. It does not publish target
altitude or authored B-scope coordinates. The stateless projection derives
altitude and display placement from ownship state and current radar settings.
The UI cursor remains a two-axis `[-1, 1]` input; conversion to azimuth, display
range and cursor altitude limits remains inside the projection/controller
boundary.

The panel and bundled simulation are demonstrators only. A real simulator should
replace `MfdInputSource` and publish `MfdInputSample` directly; generated page
handles, OSB reticles and runtime transport details do not need to leak into the
external integration.

Visual reference captures used for the MFD page work are kept in
`examples/lhld/visual_ref`. The local Falcon BMS datapackage is the preferred
source for page crops and notes. The reference folder intentionally preserves the
SMS A-G, SMS S-J, HSD and planform crops used for this pass. Reference files are
documentation-only and must not be added to the runtime asset graph.
