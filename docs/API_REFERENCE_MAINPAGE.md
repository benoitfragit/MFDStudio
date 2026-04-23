# MFDStudio Documentation Portal

@tableofcontents

<div class="mfd-hero">
<div class="mfd-eyebrow">Release-Synchronized Docs</div>
<h1>MFDStudio documentation, tutorials, and C++ API reference</h1>
<p>Use one published site to onboard quickly, author JSON assets, integrate live
clients, and inspect the public headers shipped in the current release.</p>
<div class="mfd-pill-row">
<span class="mfd-pill">C++17 runtime API</span>
<span class="mfd-pill">Tutorial-driven onboarding</span>
<span class="mfd-pill">JSON authoring reference</span>
<span class="mfd-pill">Release-published on GitHub Pages</span>
</div>
<div class="mfd-stage-grid">
<div class="mfd-stage"><strong>Start</strong><span>README, quick start, and guided reading paths</span></div>
<div class="mfd-stage"><strong>Build</strong><span>Development workflow, presets, tests, and release process</span></div>
<div class="mfd-stage"><strong>Operate</strong><span>Tutorials for JSON assets, UDP control, and live client flows</span></div>
<div class="mfd-stage"><strong>Inspect</strong><span>Versioned header reference with Graphviz and PlantUML diagrams</span></div>
</div>
</div>

## Overview

This site is the generated Doxygen portal for the public `MFDStudio`
documentation and shipped C++ API.

\startuml
left to right direction
rectangle "Project README" as Readme
rectangle "Documentation Guide" as Guide
rectangle "Quick Start + Tutorials" as Tutorials
rectangle "Reference + Architecture Notes" as Reference
rectangle "Public C++ Headers" as Api

Readme --> Guide
Guide --> Tutorials
Guide --> Reference
Tutorials --> Api
Reference --> Api
\enduml

It now brings together:

- newcomer and operator guides from the repository markdown pages
- tutorial flows and exact JSON authoring reference pages
- architecture notes for generated APIs and transport maps
- the versioned C++ header surface exposed from `mfd_api/include`,
  `client_api/include`, and `mfd_window/include`

## Start Here

| If you need... | Open |
| --- | --- |
| the product overview and shipped entry points | [Project README](../README.md) |
| the curated reading paths by goal | [Documentation Guide](../docs/README.md) |
| the fastest first visible result | [Quick Start](./QUICKSTART.md) |
| build, test, CI, and release details | [Development Guide](./DEVELOPMENT.md) |
| the full tutorial ladder | [Tutorial Index](./tutorials/README.md) |
| exact authoring syntax and JSON fields | [JSON Reference](./reference/README.md) |
| deeper API and transport rationale | [Architecture Notes](./architecture/README.md) |

## Documentation Lanes

| Lane | Best first page | What you get next |
| --- | --- | --- |
| Asset author | [Core Concepts](./CONCEPTS.md) | JSON tutorials, authoring reference, and editor workflow pages |
| Client integrator | [Quick Start](./QUICKSTART.md) | UDP control tutorials, dynamic reticles, strobe feedback, and projection helpers |
| Contributor | [Development Guide](./DEVELOPMENT.md) | presets, tests, repository layout, release automation, and the published API surface |
| API reviewer | [Architecture Notes](./architecture/README.md) | generated client API notes, transport mapping rules, and the header reference itself |

## What Is Published Here

- release-aligned markdown guides rendered inside the same published site
- namespace, class, struct, function, and type alias reference for the public
  headers
- Graphviz and PlantUML diagrams for runtime flows, structure, and API context
- source browsing for the published headers

## Release Publishing

The repository publishes this documentation portal automatically to GitHub Pages each
time a GitHub release is published. The published site therefore tracks the
latest released API and documentation set, not the latest unpublished commit on
`master`.
