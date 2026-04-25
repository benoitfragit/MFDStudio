# Core Concepts

This page explains the project vocabulary before you dive into the tutorials.

## The Object Model

\startuml
top to bottom direction
rectangle "Window" as Window
rectangle "Pages" as Pages
rectangle "Static reticles" as StaticReticles
rectangle "Optional strobe" as OptionalStrobe
rectangle "Dynamic reticles\nat runtime" as DynamicReticles
rectangle "Primitives" as Primitives

Window --> Pages
Pages --> StaticReticles
Pages --> OptionalStrobe
Pages --> DynamicReticles
StaticReticles --> Primitives
OptionalStrobe --> Primitives
DynamicReticles --> Primitives
\enduml

## Window

A window is the top-level runtime unit.

It defines:

- title
- pixel size
- screen position
- reticle library folder
- UDP command transport
- optional UDP feedback transport
- the list of page JSON files to load

Think of the window as the runtime container.

## Page

A page is one named view inside the window.

A page defines:

- `name`
- optional title
- background color
- page view center
- page zoom
- optional page-local blink types
- static reticles
- optional strobe

Only one page is active and drawn at a time.

## Reticle

A reticle is a reusable symbol or widget.

Examples:

- aircraft symbol
- compass rose
- radar track
- clock widget
- strobe cursor

A reticle can be:

- static
  defined in page JSON and always present
- dynamic
  created and removed at runtime through the client API

A page reticle can also opt in to page-managed blinking.

## Primitive

A primitive is the smallest drawable element.

Supported primitives are:

- `text`
- `time`
- `line`
- `circle`
- `ring`
- `rectangle`
- `ellipse`
- `square`
- `diamond`
- `triangle`
- `polyline`
- `bezier`
- `arc`

A reticle is made of one or more primitives.

## Strobe

A strobe is an optional page-specific cursor-like reticle used to probe or
capture nearby dynamic elements.

It can:

- be enabled or disabled
- be moved by the client
- capture nearby targets
- magnetize to the nearest target if configured
- send its live state back to the client

## Blink

Blink is managed by the page, not by the reticle template library.

Each page can declare:

- several named blink types
- one optional default blink type

Each blink type is defined by:

- a name
- an effective duration in milliseconds

Each page reticle can then:

- stay steady
- blink with the page default type
- blink with one explicit named type

Important rule:

- synchronization is done by effective duration, not by type name

So inside one page:

- two type names with the same duration blink in phase
- they appear and disappear at the same time
- moving one reticle from `slow` to `fast` immediately attaches it to the new phase group

## Coordinate System

Everything that users author in JSON or drive through the client API uses the
same normalized coordinate space.

```text
          y = +1
            ^
            |
 x = -1 <---+---> x = +1
            |
            v
          y = -1
```

Examples:

- `(0.0, 0.0)` means center of the page
- `(0.5, 0.0)` means right side, halfway to the edge
- `(-0.25, 0.75)` means upper-left quadrant

This is why page content remains valid when the window size changes.

## Client-Side User Space Projection

Some external applications already work in their own physical frame:

- positions in nautical miles
- rotations in radians
- an origin tied to the aircraft or to another moving object

The recommended approach is to keep the window itself unchanged in `[-1, 1]`
and to project the physical frame on the client side.

The helper used for this is `mfd::UserSpaceProjector`.

Typical use cases:

- aircraft-centered tactical pages
- offset aircraft anchors such as `(0.0, -0.3)`
- swapped or inverted axes between the physical frame and the page frame

Important note:

- this helper is optional
- if your client already works directly in page coordinates, you do not use it
- in that case everything stays in the normal page space `[-1, 1]`

## Runtime Identity

At runtime, the most important identifiers are:

- page name
- reticle id
- optional primitive id

Examples:

- page: `Radar`
- static reticle: `fixed-track`
- dynamic reticle: `track_042`
- text primitive inside a reticle: `track_label`

For runtime control, think in terms of:

- `page`
- `reticle`
- sometimes `primitive`
- optionally one page-defined `blink type`

You do not need to know the internal project structure.

## Command Flow

\startuml
actor "External client" as C
participant "UDP I/O worker" as U
participant "Render thread" as M
participant "CommandProcessor / EnTT" as P
participant "SceneRegistry" as S
participant "Renderer" as R

C -> U : protobuf command
U -> M : queue decoded commands
M -> P : submit commands
P -> S : apply page / reticle / strobe update
S -> R : expose active page state
R -> R : draw active page
\enduml

## Feedback Flow

\startuml
actor "External client" as C
participant "UDP I/O worker" as U
participant "Render thread" as M
participant "Strobe logic" as S

C -> U : command packet
U -> M : queued strobe command
M -> S : resolve magnetization and capture
M -> U : queue feedback snapshot
U -> C : UDP protobuf feedback
\enduml

## Threading Model

The recommended window-side runtime model uses two threads:

- render thread
  owns `SceneRegistry`, `CommandProcessor`, `EnTT`, `raylib`, and OpenGL rendering
- UDP I/O worker thread
  owns the UDP sockets and performs network receive/send work

Why this split is healthy:

- rendering stays isolated from network latency
- `raylib` and OpenGL remain on the main thread
- scene mutation stays on the same thread as rendering
- strobe feedback can be sent asynchronously

The bridge used by the examples is `mfd::UdpRuntimeBridge`.

## Recommended Reading Paths

If you are mainly authoring graphics:

1. [Quick Start](./QUICKSTART.md)
2. [01 Create Reticles From Primitives](./tutorials/01_create_reticles_from_primitives.md)
3. [02 Create Pages And Windows](./tutorials/02_create_pages_and_windows.md)
4. [03 Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)

If you are mainly integrating a live external application:

1. [Quick Start](./QUICKSTART.md)
2. [03 Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)
3. [11 Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)
4. [04 Drive A Window From A Live Client Over UDP](./tutorials/04_drive_a_window_from_a_live_client.md)
5. [05 Dynamic Reticles](./tutorials/05_dynamic_reticles.md)
6. [09 Page-Managed Blink](./tutorials/09_page_managed_blink.md)
7. [06 Strobe Control And Feedback](./tutorials/06_strobe_control_and_feedback.md)
8. [08 Project User Space To Page Space](./tutorials/08_project_user_space_to_page_space.md)

If you want one complete showcase to study:

1. [03 Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)
2. [10 Drive The Cockpit Demo](./tutorials/10_cockpit_demo.md)
3. [11 Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)
