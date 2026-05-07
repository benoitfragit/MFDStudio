# Documentation Guide

This page answers one question: which document should you read next?

If this is your first contact with the project, use this order:

1. [Quick Start](./QUICKSTART.md)
2. [Core Concepts](./CONCEPTS.md)
3. the track matching your job below

![Documentation map](./images/mfd_doc_paths.svg)

## Choose Your Track

| I want to... | Read first | Then continue with |
| --- | --- | --- |
| see one shipped demo running | [Quick Start](./QUICKSTART.md) | [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md) |
| author assets in JSON | [Core Concepts](./CONCEPTS.md) | [Create Reticles From Primitives](./tutorials/01_create_reticles_from_primitives.md), [Create Pages And Windows](./tutorials/02_create_pages_and_windows.md), [JSON Reference](./reference/README.md) |
| drive a window from code | [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md) | [Drive A Window From A Live Client](./tutorials/04_drive_a_window_from_a_live_client.md), [Dynamic Reticles](./tutorials/05_dynamic_reticles.md), [Strobe And Feedback](./tutorials/06_strobe_control_and_feedback.md) |
| work mainly with `mfd_editor` | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) | [Review The Integrated Editor Tutorial Outputs](./tutorials/15_review_integrated_editor_tutorial_outputs.md), [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) |
| understand the generated client path | [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) | [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md), [Generated Client API Architecture](./architecture/generated_client_api.md) |
| read one end-to-end user manual in Word | [Detailed User Guide (.docx)](./user_guide/MFDStudio_End_To_End_User_Guide.docx) | [Detailed User Guide (.pdf)](./user_guide/MFDStudio_End_To_End_User_Guide.pdf), [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) |
| build, test, or modify the repository | [Development Guide](./DEVELOPMENT.md) | [MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md), [Run The Automated Runtime Tests](./tutorials/12_run_the_automated_runtime_tests.md) |

## Read By Shelf

| Shelf | Purpose |
| --- | --- |
| [Quick Start](./QUICKSTART.md) | first visible result in one short session |
| [Core Concepts](./CONCEPTS.md) | minimum shared vocabulary |
| [Tutorials](./tutorials/README.md) | guided workflows by objective |
| [Reference](./reference/README.md) | exact JSON syntax and authoring rules |
| [Development Guide](./DEVELOPMENT.md) | build, tests, presets, staging, and packaging |
| [Standards](./standards/README.md) | normative engineering and interoperability rules |
| [Architecture](./architecture/README.md) | internal design notes and rationale |

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
4. [Dynamic Reticles](./tutorials/05_dynamic_reticles.md)
5. [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)

### Contributor

1. [Development Guide](./DEVELOPMENT.md)
2. [MFDStudio C++ Repository Maintenance Standard](./standards/mfd_cpp_repository_maintenance_standard.md)
3. [Core Concepts](./CONCEPTS.md)
4. the specific tutorial, reference, or architecture page related to the code you touch

## What To Ignore At First

You do not need to start with the standards or architecture pages to launch a
window or edit assets. Those pages are useful when you need:

- contributor rules and engineering constraints
- a normative generated-client contract
- replacement-client interoperability details

## Quick Utility Pages

- [FAQ](./FAQ.md): practical questions and short answers
- [What's New](./WHATS_NEW.md): recent documentation and repository highlights
