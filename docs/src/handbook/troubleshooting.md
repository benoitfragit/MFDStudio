# Troubleshooting

Short answers to the questions that come up in the first sessions.

## Nothing renders / placement looks wrong

Page coordinates are **not** pixels. The authored and runtime coordinate system
is normalized in `[-1, 1]` (see [Concepts](../concepts.md)). Check positions
there before debugging anything else.

## The client cannot drive the runtime

Verify the transport first: in `client_mockup`, use `Window display` →
`Send window display`. If the runtime reacts, the UDP path is alive and the
problem is in your command content. Press `F1` in `mfd_window` to inspect the
active page, reticle tree, and transport state.

## Generated batches are rejected

Treat `Ui.h`, `Ui.cpp`, and `<window>.generated.map` as one contract. Batches
are rejected if the generated C++ and the generated map drift apart, or if
`mfd_window` did not load the matching `.generated.map` sidecar. See
[Generated Client API](./generated_api.md).

## A reticle update hits the wrong thing

A **reticle template** is the reusable library definition; a **page reticle** is
one instance placed on a page. Runtime updates address the instance id on a
page, not the template.

## The editor edits the wrong files

The editor starts empty on purpose and never auto-loads staged `_Exec` copies.
Open the real source window JSON under `assets/`, not a staged runtime copy.

## Do I need the editor at all?

No. The core workflow is: author assets, load them in `mfd_window`, drive them
from a client. The editor is an optional convenience.

## Where are diagrams and screenshots?

Under `docs/images/`. The repository versions PlantUML sources, rendered SVGs,
and runtime/editor/client screenshots.
