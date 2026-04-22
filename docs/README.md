# Documentation Guide

This page helps you choose the shortest documentation path for your goal.

## Start Here If You Are New

Read these pages in order:

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. one path from the goal table below

That sequence gives you:

- one visible result quickly
- the project vocabulary
- the right deep-dive pages without reading the whole documentation tree first

## Choose Your Goal

| Goal | Read first | Then continue with |
| --- | --- | --- |
| See a shipped demo working quickly | [Quick Start](./QUICKSTART.md) | [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md) |
| Understand the project structure before coding | [Core Concepts](./CONCEPTS.md) | [Development Guide](./DEVELOPMENT.md) |
| Author reticles, pages, and windows in JSON | [Core Concepts](./CONCEPTS.md) | [Tutorials 01-03](./tutorials/README.md), then [JSON Reference](./reference/README.md) |
| Use `mfd_editor` to create or edit assets | [Quick Start](./QUICKSTART.md) | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) |
| Integrate a live external client over UDP | [Quick Start](./QUICKSTART.md) | [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md), [Dynamic Reticles](./tutorials/05_dynamic_reticles.md), [Strobe And Feedback](./tutorials/06_strobe_control_and_feedback.md) |
| Use the generated client-side API | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) | [Generated Client API Architecture](./architecture/generated_client_api.md) |
| Work on runtime safety or contributor workflows | [Development Guide](./DEVELOPMENT.md) | [Run The Automated Runtime Tests](./tutorials/12_run_the_automated_runtime_tests.md) |
| Need exact JSON field syntax | [JSON Reference](./reference/README.md) | [Page And Window Reference](./reference/page_and_window_reference.md) and [Primitive Reference](./reference/primitive_reference.md) |
| Need the low-level generated transport rules | [Generated Transport Map Specification](./architecture/generated_transport_map.md) | then relevant runtime and client tutorials |

## Recommended Reading Paths

### Asset Author

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. [Create Reticles From Primitives](./tutorials/01_create_reticles_from_primitives.md)
4. [Create Pages And Windows](./tutorials/02_create_pages_and_windows.md)
5. [JSON Reference](./reference/README.md)

### Client Integrator

1. [Quick Start](./QUICKSTART.md)
2. [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)
3. [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md)
4. [Add And Remove Dynamic Reticles](./tutorials/05_dynamic_reticles.md)
5. [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)

### Editor-Centric User

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md)
4. [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)

### Contributor

1. [Development Guide](./DEVELOPMENT.md)
2. [Core Concepts](./CONCEPTS.md)
3. [Run The Automated Runtime Tests](./tutorials/12_run_the_automated_runtime_tests.md)
4. the specific tutorial or reference page related to the feature you touch

## Documentation Map

- [Quick Start](./QUICKSTART.md): first visible result in one short session
- [Core Concepts](./CONCEPTS.md): project vocabulary and runtime model
- [Tutorial Index](./tutorials/README.md): step-by-step workflows by topic
- [JSON Reference](./reference/README.md): exact fields, aliases, and JSON rules
- [Development Guide](./DEVELOPMENT.md): build, test, repository layout, and release workflow
- [Architecture](./architecture): advanced design notes for generated client code and transport maps

## Common First-Time Mistakes

- reading the architecture specs before the quick start and concepts pages
- treating page coordinates as pixels instead of logical `[-1, 1]` space
- confusing reticle templates with page reticle instances
- trying to use UDP commands before validating the target window and port
- using the low-level API first when the mockup or generated client path would explain the workflow faster
