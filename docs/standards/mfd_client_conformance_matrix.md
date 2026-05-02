# MFDStudio Client Conformance Matrix

Status: Draft 0.1  
Aligned release baseline: MFDStudio current documentation set

## Purpose

This matrix links the interoperability specification to concrete automated
evidence already present in the repository.

Its role is to answer three questions quickly:

- which requirements are already covered by automated tests
- which requirements are covered only partially
- which requirements still need explicit conformance tests

## Reading Guide

Coverage status used in this document:

- `Covered`: a direct automated test already verifies the requirement
- `Partial`: the requirement is exercised indirectly or only at generator level
- `Gap`: no dedicated automated check exists yet

## Matrix

| ID | Requirement summary | Spec anchor | Automated evidence | Status |
| --- | --- | --- | --- | --- |
| `CLI-001` | a generated UI layer exposes typed page, reticle, primitive, strobe, dynamic-set, runtime-feedback, `BuildBatch`, `BuildCommandBatch`, and `SubmitLatest` surfaces | `6.4`, `7.3`, `7.4` | `tests/client_api_generator/test_generate_ui.py::test_generates_header_and_source_covering_full_ui_api` | Covered |
| `CLI-002` | generated code embeds the generated `mappingHash` into generated batch helpers | `7.4`, `11` | `tests/client_api_generator/test_generate_ui.py::test_generates_header_and_source_covering_full_ui_api` | Covered |
| `CLI-003` | generated static reticle and primitive handles carry generated IDs while still exposing typed author-facing accessors | `7.4`, `12.4` | `tests/client_api/AnimationTests.cpp::GeneratedStaticHandlesCarryTransportIdsAlongsideLegacyFields` | Covered |
| `CLI-004` | generated strobe access remains page-scoped even when generated page IDs are present | `7.4`, `12.5` | `tests/client_api/AnimationTests.cpp::StrobeHandleUsesOnlyThePageScopeEvenWhenGeneratedPageIdsExist` | Covered |
| `CLI-005` | generated dynamic sets allocate and own hidden runtime dynamic identities | `7.4`, `13.1` | `tests/client_api/AnimationTests.cpp::GeneratedDynamicReticleSetCreatesPersistentEntriesWithoutUserIds` | Covered |
| `CLI-006` | generated dynamic sets can emit removal commands and clear published entries correctly | `13.1` | `tests/client_api/AnimationTests.cpp::GeneratedDynamicReticleSetAppendRemovalCommandsClearsPublishedEntries` | Covered |
| `CLI-007` | dynamic reticle set visibility emits a page/template-scoped command instead of per-object hacks | `12.8`, `13` | `tests/client_api/AnimationTests.cpp::DynamicReticleSetVisibilityEmitsDedicatedTemplateCommand` | Covered |
| `CLI-008` | name-based static and dynamic helpers resolve through the configured generated transport map | `7.2`, `10.2`, `12.4`, `12.7` | `tests/mfd_api/CommandClientTests.cpp::NameBasedStaticHelpersResolveThroughConfiguredTransportMap`; `tests/mfd_api/CommandClientTests.cpp::NameBasedBulkDynamicReticlesResolveThroughConfiguredTransportMap` | Covered |
| `CLI-009` | generated-ID dynamic helper paths derive or preserve stable runtime identities | `10.3`, `12.6`, `12.7`, `13` | `tests/mfd_api/CommandClientTests.cpp::DynamicHelpersDeriveStableHiddenRuntimeIdWhenTransportMapIsConfigured`; `tests/client_api/LatestBatchPublisherTests.cpp::PreservesGeneratedIdentifiersWhenFlatteningBulkDynamicUpdates` | Covered |
| `CLI-010` | generated-ID batches require a non-empty `mappingHash` and mismatches are rejected | `10.2`, `11`, `15` | `tests/mfd_api/CommandProcessorTests.cpp::RejectsSerializedGeneratedTransportIdsWithoutMappingHash`; `tests/mfd_api/CommandProcessorTests.cpp::RejectsIdBasedBatchWhenMappingHashDoesNotMatchLoadedTransportMap` | Covered |
| `CLI-011` | `CommandBatch` splitting preserves `sequence` and `mappingHash` semantics | `8.1`, `11` | `tests/mfd_api/CommandClientTests.cpp::SplitBulkDynamicReticlesPreservesGeneratedIdentifiers`; `tests/client_api/LatestBatchPublisherTests.cpp::SubmitLatestVectorOverloadPreservesSequenceAndCommands` | Covered |
| `CLI-012` | `LatestBatchPublisher` preserves dynamic lifecycle operations while coalescing stale pending state | `8.3`, `13` | `tests/client_api/LatestBatchPublisherTests.cpp::PreservesPendingDynamicReticleLifecycleCommands`; `NewDynamicReticleLifecycleStateOverridesPendingState`; `DoesNotCarryPendingDynamicLifecycleAcrossDifferentMappingHashes` | Covered |
| `CLI-013` | the runtime accepts one large bulk dynamic radar update cycle of 100 tracks | `12.7`, `13`, `17` | `tests/mfd_api/CommandProcessorTests.cpp::BulkDynamicRadarBatchSupportsOneHundredTracks` | Covered |
| `CLI-014` | runtime feedback is decoded and applied as authoritative resolved runtime state | `14` | `tests/mfd_api/StrobeFeedbackTests.cpp::GenericDecoderReturnsActivePageVariant`; `tests/client_api/AnimationTests.cpp::RuntimeFeedbackStateTracksActivePageAndCapturedDynamicReticle`; `RuntimeFeedbackStateIgnoresOlderOutOfOrderFeedback`; `tests/client_api/GeneratedUiRuntimeTests.cpp::GeneratedPagesAndDynamicReticlesReflectRuntimeFeedbackState` | Covered |
| `CLI-015` | generated `Reset()` helpers are explicitly local staging resets and not runtime resets | `7.4` | `tests/client_api/AnimationTests.cpp::ReticleResetSuppressesEmissionUntilANewMutation`; `WindowDisplayResetSuppressesEmissionUntilNextMutation` | Partial |
| `CLI-016` | generated root `BuildCommandBatch(sequence)` behavior is verified end-to-end against one generated-style runtime command flow | `7.4`, `11`, `17` | `tests/client_api/GeneratedUiRuntimeTests.cpp::BuildCommandBatchCarriesMappingHashAndGeneratedIdentifiers` | Covered |
| `CLI-017` | generated `SubmitLatest(publisher, sequence)` is verified end-to-end for one real generated UI root | `7.4` | `tests/client_api/GeneratedUiRuntimeTests.cpp::SubmitLatestForwardsGeneratedUiBatchSemantics` | Covered |
| `CLI-018` | feedback UDP transport readiness, payload reception, and consumer-side conformance are validated end-to-end | `8.2`, `14`, `17` | runtime bridge feedback queue tests exist, but external-client conformance is not fully automated | Partial |
| `CLI-019` | generated UI code emits primitive-kind-specific accessors for exposed static and dynamic primitives instead of flattening everything to one generic handle | `6.4`, `7.3`, `7.5` | `tests/client_api_generator/test_generate_ui.py::test_generates_primitive_specialized_accessors_for_static_and_dynamic_handles` | Covered |
| `CLI-020` | generated primitive-level geometry mutations are serialized as primitive patches keyed by generated primitive IDs with the expected type-specific fields preserved | `7.5`, `12.4`, `13.1` | `tests/client_api/AnimationTests.cpp::GeneratedPrimitiveLevelGeometryHandlesEmitTypeSpecificPatchesById`; `tests/client_api/AnimationTests.cpp::GeneratedDynamicReticleSetCreatesPersistentEntriesWithoutUserIds` | Covered |
| `CLI-021` | generated page helpers carry both generated page IDs and the generated `mappingHash`, and reject stale hashes when a transport map is configured | `10.2`, `11`, `12.1` | `tests/mfd_api/CommandClientTests.cpp::GeneratedPageHelpersCarryMappingHashWithoutTransportMap`; `tests/mfd_api/CommandClientTests.cpp::GeneratedPageHelpersRejectStaleMappingHashWhenTransportMapIsConfigured`; `tests/client_api/GeneratedUiCompiledApiTests.cpp::GeneratedFixtureBuildsIdBasedCommandsFromRealGeneratedClasses` | Covered |
| `CLI-022` | generated feedback-capable pages and dynamic reticles expose authoritative `IsActive()` and `IsStrobeCaptured()` queries without manual id bookkeeping | `6.4`, `7.4`, `14` | `tests/client_api/GeneratedUiCompiledApiTests.cpp::GeneratedFixtureExposesRuntimeFeedbackQueries`; `tests/client_api/GeneratedUiRuntimeTests.cpp::GeneratedPagesAndDynamicReticlesReflectRuntimeFeedbackState` | Covered |

## Traceability View

![Conformance evidence flow](./conformance_evidence_flow.svg)

## Existing Test Families

### Generator-Level Evidence

These tests validate the generated API surface emitted from a window model:

- `tests/client_api_generator/test_generate_ui.py`

They are especially relevant for:

- generated accessor shape
- generated `mappingHash`
- generated strobe and dynamic-set API presence
- generated primitive-handle specialization across static and dynamic surfaces
- generated `BuildBatch` and `BuildCommandBatch` declarations and source content

### Client-Side Helper Evidence

These tests validate the shared client helper layer:

- `tests/client_api/AnimationTests.cpp`
- `tests/client_api/LatestBatchPublisherTests.cpp`

They are especially relevant for:

- generated-handle emission rules
- generated primitive-level runtime patch emission by generated primitive ID
- dynamic-set lifecycle behavior
- strobe handle behavior
- runtime feedback state tracking and feedback-backed convenience queries
- publisher coalescing semantics

### Runtime and Transport Evidence

These tests validate command normalization and runtime application:

- `tests/mfd_api/CommandClientTests.cpp`
- `tests/mfd_api/CommandProcessorTests.cpp`
- `tests/mfd_api/SceneRegistryTests.cpp`

They are especially relevant for:

- `mappingHash`
- generated ID normalization
- bulk dynamic updates
- runtime strobe semantics

## Priority Gaps

The next gaps worth closing are:

1. one external feedback-client conformance test that receives and checks a real
   runtime feedback payload from a running bridge
2. one stronger end-to-end test that drives a real generated source pair emitted
   by `mfd_client_api/generator`, not only one generated-style fixture built on the
   same public API

## Related Documents

- [MFDStudio Generated Client API Standardization](./mfd_generated_client_api_standardization.md)
- [MFDStudio External Client Interoperability Specification](./mfd_client_interoperability_specification.md)
- [Generated Client API Architecture](../architecture/generated_client_api.md)
- [Generated Transport Map Specification](../architecture/generated_transport_map.md)
