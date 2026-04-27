# Tutorials

This folder contains step-by-step tutorials for the MFD project.

## Read This Before The Tutorials

If you are new to the project, start with:

1. [Documentation Guide](../README.md)
2. [Quick Start](../QUICKSTART.md)
3. [Core Concepts](../CONCEPTS.md)
4. [JSON Reference](../reference/README.md)

These pages give you the mental model and vocabulary used by the tutorials.

## Recommended Full Order

1. [Create Reticles From Primitives](./01_create_reticles_from_primitives.md)
2. [Create Pages And Windows](./02_create_pages_and_windows.md)
3. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)
4. [Drive A Window From A Live Client Over UDP](./04_drive_a_window_from_a_live_client.md)
5. [Add And Remove Dynamic Reticles](./05_dynamic_reticles.md)
6. [Control The Strobe And Receive Feedback](./06_strobe_control_and_feedback.md)
7. [Capture The Window As RGBA32](./07_framebuffer_rgba32_capture.md)
8. [Project User Space To Page Space](./08_project_user_space_to_page_space.md)
9. [Manage Page-Local Blink](./09_page_managed_blink.md)
10. [Drive The Cockpit Demo](./10_cockpit_demo.md)
11. [Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
12. [Run The Automated Runtime Tests](./12_run_the_automated_runtime_tests.md)
13. [Create A Window From Scratch In The Editor](./13_create_window_from_editor.md)
14. [Use The Integrated Runtime Debug Overlay](./14_use_the_runtime_debug_overlay.md)

## Fast Reading Paths

If you mainly want to author JSON and use the editor:

1. [Create Reticles From Primitives](./01_create_reticles_from_primitives.md)
2. [Create Pages And Windows](./02_create_pages_and_windows.md)
3. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)

If you mainly want to integrate an external runtime client:

1. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)
2. [Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
3. [Drive A Window From A Live Client Over UDP](./04_drive_a_window_from_a_live_client.md)
4. [Add And Remove Dynamic Reticles](./05_dynamic_reticles.md)
5. [Manage Page-Local Blink](./09_page_managed_blink.md)
6. [Control The Strobe And Receive Feedback](./06_strobe_control_and_feedback.md)
7. [Project User Space To Page Space](./08_project_user_space_to_page_space.md)

If you mainly want to understand the mockup as a real client:

1. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)
2. [Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
3. [Drive A Window From A Live Client Over UDP](./04_drive_a_window_from_a_live_client.md)
4. [Drive The Cockpit Demo](./10_cockpit_demo.md)

If you mainly want framebuffer export:

1. [Capture The Window As RGBA32](./07_framebuffer_rgba32_capture.md)

If you mainly want an end-to-end showcase:

1. [Test A Window With The Mockup](./03_test_with_mfd_mockup.md)
2. [Drive The Cockpit Demo](./10_cockpit_demo.md)
3. [Use The Mockup As A Client API Reference](./11_use_the_mockup_as_a_client_api_reference.md)
4. [Use The Integrated Runtime Debug Overlay](./14_use_the_runtime_debug_overlay.md)

If you mainly want automated safety nets around the runtime:

1. [Run The Automated Runtime Tests](./12_run_the_automated_runtime_tests.md)
2. [Manage Page-Local Blink](./09_page_managed_blink.md)
3. [Drive A Window From A Live Client Over UDP](./04_drive_a_window_from_a_live_client.md)

All client-side runtime tutorials now assume the generated API is the normal
surface exposed to application code, with `CommandClient` kept as the final
send boundary.
