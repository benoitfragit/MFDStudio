# Targeted corrections — 22 October 2026

## Scope and starting evidence

Starting master: `dde15d48991bfb143e37992fabcb55a9cad896f8`.
The current master matched the audit reference. Root `AGENTS.md` was read; no
nested instruction files were present. All eleven reported issues were confirmed
in their named paths. The original Windows run
[39648425602](https://github.com/benoitfragit/MFDStudio/actions/runs/39648425602)
failed in both Win32 Debug and Release while copying the runtime LEVEL 1 DLL to
an as-yet absent launcher output directory.

## Finding-to-correction matrix

| Finding | Confirmed cause and correction | Regression coverage |
| --- | --- | --- |
| IND-001 | Producer post-build copies assumed consumer folders existed. Each DLL creates its destination folders; the runtime producer refreshes both hosts directly. | `apilevel1_host_deployment` compares source/host DLL hashes; existing staged-layout and extension availability tests remain. |
| SEC-001 | Finite logical coordinates could overflow float transforms and integer scissor casts. Double transforms, viewport intersection, and clamped rounded endpoints precede integer conversion. Unrepresentable drawing geometry is skipped. | Headless real-renderer command sink: extreme positions/sizes/zoom, NaN/Inf, empty clips, fractional clipping and INT_MAX viewport. UBSan + float-cast-overflow checks locally. |
| IO-002 | Document saving/restoration still used an older rollback implementation that discarded secondary errors. They now use the existing verified asset-commit service. Cleanup failures are reported, and cleanup never triggers rollback after commit. | Existing staging/backup/promotion/deletion/recovery tests; new restore/removal failure and post-commit cleanup tests assert retained originals and retry behavior. |
| IO-001 | RGBA parsed integers into `int` before checking range. Range checks now use the original signed/unsigned 64-bit domain. | Boundary colors, negative/out-of-range values, UINT64_MAX, 2^32, floats, bool, string, null and round-trip. |
| FUN-002 | Generator normalized child references but runtime/editor use exact authored identity. Generator now rejects mismatched references; normalization for collisions and transport keys is unchanged. | Python and C++ tests for case/space mismatches, collisions, exact references and round-trip; existing legacy generator suite. |
| FUN-003 | Hidden grandchildren compared their unopened parent against the visible foreground stack and popped ancestors early. Hidden nodes are skipped before scope closing. | Deterministic background/foreground draw order with hidden child + grandchild, a later visible child, and a following root; balanced scissors. |
| FUN-004 | Inspector recorded each changed frame independently. Host UI lifecycle now assigns a gesture identity, with one pre-edit snapshot on the first valid change. | Gesture activation, held unchanged frames, release/keyboard commit, separate clicks, control changes, canceled activation and missing inspector frames. Interactive integration remains a manual check. |
| PERF-002 | Full snapshots were captured even with no edit. The DLL receives local scalar values; a changed draft is created only on edit, validated with scoped rollback, then the snapshot is captured only for the first valid change. | Lifecycle tests and source review: idle/rejected frames reach no `CaptureCurrentSnapshot`; continuous accepted frames after the first reuse the gesture identity. |
| PERF-001 | Ancestor checks repeatedly scanned all layout nodes. A bounded per-query source index is reused for wheel and thumb hit candidates. | Existing clipping/disabled/bubbling tests plus 100/1000/4096-node reverse-authored hierarchies and malformed parent/index/cycle tests. Measurement below. |
| PERF-003 | Idle input processing and drawing both evaluated layout. Idle input now returns before resolution. Actual interaction frames still evaluate before input and again for drawing after state changes. | Source-path count: ordinary frame 2 → 1 evaluations, wheel/drag frame 2 → 2. No persistent cache, generation token, resize/page/reload invalidation or client-thread assumption. Same-frame interactive rendering requires manual graphics verification. |
| CPP-001 | Two public `noexcept` state updates allocate normalized keys/map entries. Narrow `std::bad_alloc` handlers preserve the signatures and return failure before patch writes. | Public noexcept static assertion, allocated long IDs, valid updates and invalid-input state preservation. Isolated statically linked allocation-fault tests exercise every insertion allocation, failed updates, state preservation and successful retry; Windows execution is part of the PR CI. |

## Local validation

Linux x86_64, GCC 13.3, C++17. The full product configure was attempted with:

```sh
cmake -S . -B build/linux-audit -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMFD_BUILD_DEMO=ON -DMFD_BUILD_TESTS=ON
```

It stopped at raylib's X11 dependency; this is **not** a successful product build.
A temporary headless CMake harness compiled the repository's real layout, JSON,
input, decoration, file-commit and gesture sources with their corresponding tests:

```sh
cmake --build ../headless/build -j 4
ctest --test-dir ../headless/build --output-on-failure
python tests/client_api_generator/test_generate_ui.py --generator mfd_client_api/generator/scripts/generate_ui.py
```

Headless flags: `-std=c++17 -Wall -Wextra -Wpedantic
-fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all`.
The expanded headless suite runs 89 tests: **88 pass and one is skipped** by its
existing Windows-only cross-drive path condition. It includes the existing
serializer/commit and LEVEL 1 ABI suites, plus real allocation-failure injection.
The portable ABI target uses `-include cstdint` to compensate an existing
MSVC-dependent transitive include in the unchanged extension source; no repository
source was patched for that local harness. The existing generator suite, including
the added identity case, passes 31 tests. Syntax-only C++17 compilation also
covers the edited host inspector, application inspector, serializer, renderer,
SceneRegistry and its test translation unit. This is not a substitute for linking
the full Windows editor or interactive testing.

## Pointer-routing measurement

GCC 13.3 `-O2 -std=c++17`, same machine, 30 wheel queries per sample; layout
construction excluded. Every container covers the pointer, chains are bounded
to 32 levels and every candidate is unable to scroll. This deliberately measures
the expensive full-search case, not average application FPS.

| Containers | Original mean/query | Corrected mean/query |
| ---: | ---: | ---: |
| 100 | 32.05 µs | 5.61 µs |
| 1,000 | 4,056.53 µs | 54.96 µs |
| 4,096 | 78,048.40 µs | 227.69 µs |

The index is fixed-capacity, allocation-free and scoped to one query. Its maximum
storage is 4096 pointers (16 KiB on Win32, 32 KiB on x64). No index survives a
layout mutation. A persistent layout cache was deliberately unnecessary: skipping
idle input resolution removes the ordinary duplicate evaluation with no stale
state risk.

## Windows validation and remaining manual checks

The PR runs the existing VS2022/v142 Win32 Debug job and now runs its same CI test
selection in Release too. `test-debug-win32-ci` retains its pre-existing hosted
runner GPU/local-asset exclusions; Release inherits exactly those exclusions.
See the PR checks for the current commit's results. A green CI job therefore
means that selected suite passed, **not** that the full graphical/local suite or
x64 was executed.

On a workstation with the project's GPU/local tutorial prerequisites:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
ctest --preset test-debug-win32
cmake --build --preset release-win32
ctest --preset test-release-win32
```

Manually verify continuous inspector dragging and keyboard Escape/commit,
Undo/Redo, changing selection/page, hiding the inspector, same-frame wheel/thumb
rendering, resize/zoom, reload/reset and optional extension loading. Source review
and headless tests do not constitute a completed visual smoke test.

## Compatibility and documentation

No changes to either versioned C DLL ABI, Protobuf definitions, JSON fields or
defaults, generated transport key/hash algorithms, CMake generator entry points,
LEVEL 0 behavior, or window sizing. Existing generated LEVEL 0 byte-equality tests
remain enabled. The host-only editor interface gains an additive draft method;
the existing `DrawInspector` signature remains available. The container handbook,
editor persistence guidance and step-by-step editor tutorial describe the changed
behavior. Multi-file saves provide handled-failure rollback, not crash/power-loss
atomicity, and still assume no concurrent external writer.
