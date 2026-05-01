# MFDStudio Documentation Portal

@tableofcontents

<div class="mfd-hero"><span class="mfd-eyebrow">Release-Synchronized Docs</span><span class="mfd-hero-title">MFDStudio documentation, generated client API guidance, and C++ reference</span><span class="mfd-hero-copy">Use one published portal to onboard quickly, author JSON assets, integrate live clients through the generated API, and inspect the public C++ headers shipped in the current release.</span><div class="mfd-pill-row"><span class="mfd-pill">Generated API first</span><span class="mfd-pill">C++17 runtime API</span><span class="mfd-pill">Tutorial-driven onboarding</span><span class="mfd-pill">JSON + standards + architecture</span></div><div class="mfd-stage-grid"><div class="mfd-stage"><strong>Start</strong><span>README, quick start, and curated reading paths</span></div><div class="mfd-stage"><strong>Integrate</strong><span>Generated client API, transport map, dynamic reticles, and strobe flows</span></div><div class="mfd-stage"><strong>Author</strong><span>Editor workflow plus exact page, reticle, and primitive JSON reference</span></div><div class="mfd-stage"><strong>Inspect</strong><span>Versioned header reference with Graphviz and PlantUML diagrams</span></div></div></div>

## Overview

This portal is the generated Doxygen site for the public MFDStudio
documentation and the shipped public C++ headers.

\startuml
left to right direction
rectangle "Project README" as Readme
rectangle "Documentation Guide" as Guide
rectangle "Quick Start + Tutorials" as Tutorials
rectangle "Generated API + Standards" as Standards
rectangle "Reference + Architecture Notes" as Reference
rectangle "Public C++ Headers" as Api

Readme --> Guide
Guide --> Tutorials
Guide --> Standards
Guide --> Reference
Tutorials --> Api
Standards --> Api
Reference --> Api
\enduml

It brings together:

- newcomer and operator guides from the repository markdown pages
- generated-client onboarding and standardization pages
- tutorial flows and exact JSON authoring reference pages
- architecture notes for generated APIs and transport maps
- the versioned C++ header surface exposed from `mfd_api/include`,
  `mfd_client_api/include`, `mfd_editor_plugin_api/include`,
  `mfd_window/include`, and `mfd_window_plugin_api/include`

Internally, the repository implementation is now split into `mfd_model`,
`mfd_transport`, `mfd_io_json`, `mfd_runtime`, and the private
`mfd_render_raylib` layer. The published client-facing documentation therefore
stays render-agnostic: the public API focuses on authored data, command flow,
runtime control, and generated clients rather than exposing one renderer as a
required dependency.

<div class="mfd-callout"><strong>Preferred client integration:</strong> for one C++ client specific to one authored window, start from the generated UI emitted by the generator, mutate generated page / reticle / primitive handles, then let <code>CommandClient</code> publish the resulting batch.</div>

## Start Here

| If you need... | Open |
| --- | --- |
| the product overview and shipped entry points | [Project README](../README.md) |
| the curated reading paths by goal | [Documentation Guide](../docs/README.md) |
| the fastest first visible result | [Quick Start](./QUICKSTART.md) |
| build, test, CI, and release details | [Development Guide](./DEVELOPMENT.md) |
| the full tutorial ladder | [Tutorial Index](./tutorials/README.md) |
| exact authoring syntax and JSON fields | [JSON Reference](./reference/README.md) |
| the generated client API as a standard client surface | [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md) |
| deeper API and transport rationale | [Architecture Notes](./architecture/README.md) |
| the broader replacement-client contract | [Interoperability Standards](./standards/README.md) |

## Documentation Lanes

| Lane | Best first page | What you get next |
| --- | --- | --- |
| Asset author | [Core Concepts](./CONCEPTS.md) | JSON tutorials, editor workflow pages, and exact authoring reference |
| Generated API integrator | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) | live-client loop, dynamic reticles, strobe, generated API architecture, and standardization |
| Third-party replacement client implementer | [Interoperability Standards](./standards/README.md) | generated transport rules, command semantics, conformance checks, and fallback helper boundaries |
| Contributor | [Development Guide](./DEVELOPMENT.md) | presets, tests, repository layout, release automation, and the published API surface |
| API reviewer | [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md) | generated surface rules, architecture notes, transport mapping, and header reference |

## What Is Published Here

| Area | What you get |
| --- | --- |
| Onboarding | README, quick start, concepts, and tutorials for new users or integrators |
| Generated Client API | How to use the generated page, reticle, primitive, strobe, and dynamic-set handles as the normal client-facing workflow |
| Reference | Exact JSON fields, architecture notes, and formal standards pages linked from the same portal |
| C++ Headers | Namespace, class, function, and type reference for the public C++ surface, with source browsing and include graphs |

## Feature Coverage At A Glance

The generated client-facing documentation now covers:

- whole-window display control
- page activation and page view
- static reticle mutation
- primitive-level text, geometry, time, and image updates
- exposed-primitive patterns such as progress bars
- dynamic reticle creation, bulk update, and removal
- page-scoped strobe control and feedback
- generated `BuildBatch()`, `BuildCommandBatch(sequence)`, and
  `SubmitLatest(...)` publication flows
- authored `drawOnTop` and `exposed` rules as they affect generation and
  runtime behavior

## Release Publishing

The repository publishes this documentation portal automatically to GitHub
Pages each time a GitHub release is published. The published site therefore
tracks the latest released API and documentation set, not the latest
unpublished commit on `master`.
