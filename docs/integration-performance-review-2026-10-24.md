# Integration, safety and performance review - 2026-10-24

## Findings

No P0 finding was confirmed in the inspected scope. Two P1 defects and four P2
defects were confirmed and corrected.

### P1 - Public widget projection could dereference a missing definition

- **File/function:** `extensions/apilevel1/widgets/runtime/src/WidgetModel.cpp`,
  `ProjectWidgetLayoutNodes`.
- **Minimal scenario:** call the public projection function with a widget instance
  whose `definition` pointer is null.
- **Exact cause:** the projection path dereferenced every definition although only
  its higher-level caller had performed validation; the public header declared no
  precondition requiring validated input.
- **Impact:** deterministic access violation for a valid C++ call shape at a public
  extension boundary.
- **Correction:** reject excessive counts and missing definitions before projection;
  document the defensive empty result and keep `EvaluateWidgetLayout` as the
  preferred external-boundary entry point.
- **Regression test:**
  `WidgetModel.ProjectionRejectsMissingDefinitionWithoutDereferencingIt`.

### P1 - Container DLL output crossed the host boundary without full validation

- **File/function:** `extensions/apilevel1/runtime/src/ContainerRuntimeHost.cpp`,
  `ContainerRuntimeHost::Evaluate`; private validation in
  `ContainerRuntimeHostValidation.h`.
- **Minimal scenario:** a compatible or corrupted extension returns one layout
  record containing NaN geometry, an invalid flag, an out-of-range source index or
  an inconsistent parent/depth pair.
- **Exact cause:** the host checked only result count and source indices before
  converting DLL-owned ABI records into renderer/runtime objects.
- **Impact:** non-finite geometry or invalid hierarchy could reach rendering/input,
  with crash, state corruption or unbounded downstream behavior depending on the
  malformed field.
- **Correction:** validate bounded counts, indices, uniqueness, hierarchy, depth,
  flags, inherited state, and finite/non-negative rectangle, offset and scrollbar
  geometry components before conversion. Scroll offsets must also fit the
  returned content/viewport extent.
  Validation uses bounded fixed storage and does not enlarge the public ABI.
- **Regression tests:**
  `ContainerRuntimeHostValidationTests.RejectsMalformedExtensionOutput` and
  `ContainerRuntimeHostValidationTests.RejectsInconsistentHierarchy`.

### P2 - Editor widget preview retained stale state after authored changes

- **File/function:** `mfd_editor/src/application/EditorWidgetPreviewController.cpp`,
  `EditorWidgetPreviewController::Synchronize`.
- **Minimal scenario:** drag a slider above 50, replace its definition with a
  0..10 range, or change its authored override, while keeping the same instance ID.
- **Exact cause:** synchronization compared only the ordered instance IDs. It did
  not compare definition identity or resolved authored state.
- **Impact:** an out-of-range transient value could remain in preview; later
  evaluation rejected it and the affected preview stopped reflecting the document.
- **Correction:** retain definition/authored snapshots per entry, rebuild only the
  changed entry, and release capture if that entry was active.
- **Regression test:**
  `EditorWidgetPreviewController.DefinitionAndOverrideChangesResetStalePreviewValues`.

### P2 - Feedback overflow shifted the complete queue

- **File/function:** `mfd_api/src/runtime/SceneRegistry.cpp`,
  `SceneRegistry::QueueWidgetFeedback` and `TakeWidgetFeedback`.
- **Minimal scenario:** enqueue more than 1024 widget events before the client
  drains them.
- **Exact cause:** overflow called `vector::erase(begin())`, moving every retained
  event for each additional event.
- **Impact:** O(queue size) work on every overflow at the feedback input boundary.
- **Correction:** use a private `std::deque`, evict with `pop_front()` and move into
  the public vector only when events are taken. The public API is unchanged.
- **Regression test:**
  `WidgetFeedbackQueue.OverflowDropsOldestEventsWithoutChangingFifoOrder`.

### P2 - Hidden streams caused an unnecessary renderer batch flush

- **File/function:** `mfd_api/src/render/ExternalStreamRegistry.cpp`,
  `ExternalStreamRegistry::EndFrame`.
- **Minimal scenario:** one or more endpoints remain registered while every stream
  primitive is hidden or has no drawable texture.
- **Exact cause:** the renderer flushed its current raylib batch whenever the
  registry was nonempty, even though no external texture was sampled.
- **Impact:** avoidable synchronization/state work on otherwise pixel-idle frames.
- **Correction:** flush only when at least one visible single-reference endpoint
  owns a nonzero texture. Metadata and resource housekeeping remain active.
- **Regression coverage:** existing external-stream protocol/budget/model tests;
  the condition is graphics-context-specific and remains part of local GPU
  validation.

### P2 - Numeric widget text formatted through stream allocation

- **File/function:**
  `extensions/apilevel1/widgets/runtime/src/WidgetVisuals.cpp`,
  numeric binding formatting.
- **Minimal scenario:** draw many visible widgets with numeric text bindings on
  every frame.
- **Exact cause:** each binding built an `ostringstream` and locale state.
- **Impact:** repeated formatting setup and potential heap work in the render path.
- **Correction:** format with `std::to_chars` into a fixed 512-byte stack buffer;
  the fixed-decimal behavior and 0..9 precision contract are unchanged.
- **Regression coverage:** `WidgetVisualBinding` and widget example tests cover the
  formatted visual behavior.

## Residual risks and measurements still needed

These are explicit residual risks, not confirmed runtime bugs:

- Widget visual projection currently materializes current draw data and applies
  bindings each frame. Complex reusable reticle compositions may make this cost
  visible. Measure representative pages before adding a cache; invalidation must
  include definition, instance override, binding and ancestor state.
- A container/widget interaction frame may evaluate layout for input and again for
  drawing so a scroll change is visible immediately. Idle input frames already skip
  the input-side evaluation. Any reuse must prove same-frame correctness first.
- Container host/extension vectors are rebuilt during evaluation. Their maximum
  bounds guarantee termination, not a constant frame budget. Profile large trees
  on target Win32 hardware before changing allocation ownership across the ABI.
- Direct GPU interop and CPU fallback behavior still depends on the deployment GPU,
  driver and OpenGL/D3D11 interoperability. Protocol tests cannot replace local
  hardware tests.

## Inspected scope

The review covered the integration and hot-path code for:

- external stream model, registry, native producer/receiver, protocol, resource
  budget, CPU fallback and example;
- API LEVEL 1 container model/layout, runtime ABI host, render/input routing,
  generator/deployment example and tests;
- API LEVEL 1 widget model/layout, visual projection, input/feedback, generated
  command path, editor preview, example and tests;
- related `README.md`, handbook pages, troubleshooting and `AGENTS.md` readability,
  performance, boundary-validation and Definition of Done rules.

This was not a line-by-line audit of unrelated legacy primitives, every editor
panel, all transport commands or third-party dependencies.

## Validation evidence

- Visual Studio 2022 Win32 Debug build passed for `mfd_api`,
  `mfd_apilevel1_tests`, `mfd_widgets_tests`, `mfd_editor_tests` and
  `external_stream_tests`.
- 93 targeted container, widget, editor-preview and external-stream tests passed.
- The `AGENTS.md` readability audit found only localized low-level casts at Win32
  DLL/procedure, OpenGL procedure and byte-wire boundaries in the reviewed paths;
  no broad rewrite was performed.
- Documentation static validation and final diff hygiene are part of the same
  change validation.
