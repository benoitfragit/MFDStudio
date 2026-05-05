# Documentation Guide

This page is the main entry point for the repository documentation.

The goal is simple:

- get you to the right page quickly
- avoid making you read the whole tree first
- separate onboarding, usage, reference, and deep architecture work

![Documentation map](./images/mfd_doc_paths.svg)

## Start Here

If this is your first session with the project, read in this order:

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. the path matching your job below

## What The Product Looks Like

### Runtime Window

![Runtime window screenshot](./images/mfd_window_cockpit_capture.png)

### Live Client

![Client mockup screenshot](./images/client_mockup_demo.png)

### Visual Editor

![Editor screenshot](./images/mfd_editor_capture.png)

## Choose Your Path

| I want to... | Read first | Then continue with |
| --- | --- | --- |
| see a shipped demo working | [Quick Start](./QUICKSTART.md) | [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md) |
| understand the authored model | [Core Concepts](./CONCEPTS.md) | [JSON Reference](./reference/README.md) |
| create assets in JSON | [Core Concepts](./CONCEPTS.md) | [Tutorials 01 to 03](./tutorials/README.md) |
| use `mfd_editor` | [Quick Start](./QUICKSTART.md) | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md), [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md), [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md), [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md), [Generated Client API Architecture](./architecture/generated_client_api.md) |
| drive the runtime from a client | [Quick Start](./QUICKSTART.md) | [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md) |
| understand `client_mockup` as a reference implementation | [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md) | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) |
| use the generated client API | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) | [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md) |
| work on the repository itself | [Development Guide](./DEVELOPMENT.md) | [MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md), [Run The Automated Runtime Tests](./tutorials/12_run_the_automated_runtime_tests.md) |
| review formal contracts and compatibility | [Interoperability Standards](./standards/README.md) | [Architecture Notes](./architecture/README.md) |

## Quick Utility Pages

- [FAQ](./FAQ.md): practical questions and fast answers
- [What's New](./WHATS_NEW.md): the current highlights of the repository tree

## Documentation Shelves

### Get Started

- [Quick Start](./QUICKSTART.md): first visible result in one short session
- [Core Concepts](./CONCEPTS.md): the vocabulary you need before the deep dive
- [Tutorial Index](./tutorials/README.md): step-by-step workflows grouped by topic

### Build And Contribute

- [Development Guide](./DEVELOPMENT.md): build presets, tests, staging, and repository layout
- [MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md): repository expectations for C++17 architecture, API discipline, tests, performance, and Doxygen

### Authoring Reference

- [JSON Reference](./reference/README.md): exact supported JSON fields and syntax

### Advanced Design And Contracts

- [Architecture Notes](./architecture/README.md): internal design notes for generated APIs, transport mapping, and editor workflows
- [Interoperability Standards](./standards/README.md): generated client standardization and external client contract

## Recommended Reading By Profile

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
4. [Dynamic Reticles](./tutorials/05_dynamic_reticles.md)
5. [Strobe And Feedback](./tutorials/06_strobe_control_and_feedback.md)

### Editor-Centric User

1. [Quick Start](./QUICKSTART.md)
2. [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md)
3. [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)
4. [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)
5. [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md)
6. [Generated Client API Architecture](./architecture/generated_client_api.md)

### Contributor

1. [Development Guide](./DEVELOPMENT.md)
2. [MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md)
3. [Core Concepts](./CONCEPTS.md)
4. [Tutorial Index](./tutorials/README.md)
5. the specific reference or architecture page related to the feature you touch

## Common First-Time Mistakes

- reading architecture pages before `Quick Start` and `Core Concepts`
- confusing reticle templates with page reticle instances
- treating page coordinates as pixels instead of logical `[-1, 1]`
- trying UDP control before confirming the target window and port
- using low-level string-based helpers first when the generated client path is available
