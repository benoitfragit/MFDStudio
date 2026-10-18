# Targeted corrections, 2026-10-18

Baseline: `cf6eed08af85adcb17f115e64c8f178e4b7d314c` (`master`, 2.11.0),
verified against `origin/master` after `git fetch origin`. Per the user's
instruction, implementation is on `master`, with one correction commit followed
by a separate 2.11.1 version commit; no branch, PR, push or release is created.
The pre-existing untracked `AUDIT_CODE_STABILITE_UX_PERFORMANCES.md` is excluded.

## Verification matrix

Locations below refer to the baseline, so they remain reproducible after edits.
All seven findings were confirmed by reading their implementations and callers.

| ID / severity | Baseline location and minimal scenario | Cause and impact | Correction and regression coverage |
| --- | --- | --- | --- |
| F01 / P1 | `mfd_window/src/WindowLauncher.cpp:419`, `IsFenceReady`; `:715`, `TryConsume`. Driver returns `GL_WAIT_FAILED` after a successful capture. | Boolean status treats failure like timeout; cached pixels can be published as a new capture. | Explicit ready/pending/failed status; warning and permanent synchronous fallback for the affected instance. Private backend tests cover all statuses, repeated failure, frame freshness, pending-slot retention, mapping failures, resize and teardown. |
| R01 / P1 | `mfd_client_api/src/LatestBatchPublisher.cpp:950`, `Stop`. Send callback calls `Stop`, or two external threads stop together. | Self-join throws; concurrent joins access the same thread unsafely. | Callback requests stop only; external joins use a separate mutex without holding the worker-state mutex. Deadline-guarded subprocess tests cover self-stop, concurrent stop, exception after self-stop and destruction; existing retry/drop tests remain. |
| S01 / P1 | `mfd_editor/src/EditorPageRenameService.cpp:654`, `PageRenameService::Execute`; `EditorReticleRenameService.cpp:996`, `ReticleRenameService::Execute`. A later file cannot be written. | Sequential truncating writes leave mixed asset references and stale editor state. | Stage/verify all bytes, back up originals, promote then delete; reverse rollback and retained-backup diagnostics. State/result allocations precede disk commit. Injected stage/write/flush/close/verify/backup/promotion/deletion/rollback failures; Windows read-only integration tests for both services; existing success/collision tests. |
| S02 / P1 | `mfd_editor/src/EditorApplication.cpp:1707`, `WriteRecoverySnapshot`, and autosave call at `:1602`. Write fails after opening the recovery file. | Truncation loses the previous snapshot; scheduler records success regardless. | Exclusive adjacent temporary file, verified write/flush/close, same-filesystem replacement (`MoveFileExW` on Windows); success result drives bookkeeping, failed attempts defer 30 seconds. Tests cover staging failures, truncation verification, replacement failure, interrupted staging, serialization failure, old reader and retry timing. |
| S03 / P2 | `mfd_api/src/io/JsonLoader.cpp:288`, `ParseChannelNumber`; `:304`, `ParseOpacityNumber`; `:489`, `ParseChannelToken`. Channel is `1e308` or `-1e308`. | Rounding/conversion exceeds the integer domain before clamping. | Finite check and floating-point clamp before rounding; percentage clamp before scaling. Tests cover extremes in numeric/string/percent/opacity/alpha paths, ordinary boundaries, halfway values, NaN and infinity tokens. |
| S07 / P3 | `mfd_api/src/io/JsonLoader.cpp:508`, `TryParseRgbFunction`. Input is `rgb(255,0,0)garbage`. | Closing parenthesis need not terminate the trimmed token. | Require last non-whitespace character to close the function; existing numeric-token/count validation rejects nested/mismatched syntax. Tests cover suffix, extra/missing parentheses, empty/wrong channel counts and valid whitespace. |
| P01 / P2 | `mfd_editor/src/EditorApplication.cpp:1809`, `Undo`, and `:1822`, `Redo`. User invokes either operation with an empty stack. | Snapshot is copied before history discovers it cannot step. | Early `CanUndo`/`CanRedo` guard, preserving status messages. Existing history tests cover boundaries, successful steps and redo invalidation. Direct code review verifies the unavailable path returns before capture; the UI application itself has no snapshot-copy counter seam. |

## Inspected scope and design decisions

Read the root `AGENTS.md`, build/architecture/quality documentation, affected
headers and source, publisher worker/destructor/flush/callers, capture ring and
publication/teardown callers, rename planning/collisions/reference rewriting,
serializer save/recovery transactions, autosave scheduling/restore/retention,
history implementation and relevant tests. This is a targeted review, not a
whole-repository audit; unrelated runtime, transport and renderer paths were
not exhaustively reread.

The existing serializer transaction cannot be reused unchanged: rollback errors
are discarded and its sequential promotions do not give single-file recovery
replacement the required visibility guarantee. A private `EditorFileCommit`
utility serves only rename and recovery operations. The existing normal save
and recovery-restore implementation is outside this change's scope.

`AsyncFramebufferCapture` was moved out of the launcher into a private source
pair with a borrowed backend interface. Production retains the same dynamically
loaded GL implementation. Deterministic tests exercise the actual capture ring,
including resource ownership, without mocking a public SDK or requiring a GPU.

No command type changed, so serialization, transport normalization, identifier
resolution and batching rules remain untouched. The publisher retains its
coalescing, failed-send retry and stop/drop semantics.

## Guarantees and residual limits

- Public SDK signatures, JSON/protobuf schemas, generated APIs, authored asset
  paths, color semantics for normal inputs, recovery filename/format, history
  semantics and plugin pixel layouts are preserved. Malformed RGB suffixes are
  intentionally rejected; finite out-of-range channels saturate predictably.
- Publisher callbacks may stop but must not destroy their own publisher or
  call `Flush()`. Destruction requires other callers to have finished; violating
  callback destruction is explicitly fail-fast. External stop still depends on
  user callbacks returning; it cannot cancel a callback blocked in external I/O.
- Multi-file rename rollback handles ordinary exceptions/I/O errors, not process
  termination, power loss or concurrent external writers. An unrecoverable
  rollback reports retained backup paths; it never reports successful rename.
  Failed cleanup can leave owned temporary files; they are not JSON assets.
- Recovery replacement is atomic for readers on the same filesystem. No disk
  flush/barrier protocol promises power-loss durability. Serialization and disk
  I/O remain on the existing editor thread; status reporting is non-modal and
  persistent failure is throttled rather than retried each rendered frame.
- Fault injection tests exercise error propagation at I/O boundaries and real
  Windows read-only failures. They do not emulate storage-controller crashes or
  every possible kernel short-write/flush/close failure. GPU failure injection
  validates capture control flow, not arbitrary vendor-driver faults.
- Empty-history allocation avoidance is verified by the direct early return;
  existing history tests validate the predicates and transition semantics.

## Validation

Commands executed in the repository root:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
cmake --build --preset debug-win32 --target client_api_tests
ctest --preset test-debug-win32 --timeout 30
ctest --preset test-debug-win32 -R 'LatestBatchPublisherShutdownDeathTest|LatestBatchPublisherTests' --output-on-failure
ctest --preset test-debug-win32 -R 'EditorFileCommitTests|PageRenameServiceTests|ReticleRenameServiceTests|EditorAutosaveSchedulerTests|EditorUndoHistoryTests|AsyncFramebufferCaptureTests' --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File docs/CheckDocs.ps1 -SkipBuild
git diff --check
```

The editor/capture/history/rename selection passed **52/52** tests. The initial
publisher selection passed 44/45: its added callback-destruction test assumed
the executable's terminate handler also applied to the DLL's separate CRT.
The process correctly terminated, but with the CRT's exit code. The assertion
was corrected to require unsuccessful termination without that false assumption.
The subsequent incremental build hit Windows linker error `LNK1168` on
`build/vs2022-win32/tests/Debug/client_api_tests.exe`. No matching process remained
when inspected. Following the user's instruction, that exact generated executable
was deleted with `Remove-Item -LiteralPath`, then `client_api_tests` was rebuilt
successfully. All other build targets had completed. Final rebuilt-suite results
follow below.

Cppcheck was executed on the six affected implementation files for color parsing,
publisher shutdown, page/reticle rename, file commit and asynchronous capture:

```powershell
& 'C:\Program Files\Cppcheck\cppcheck.exe' --std=c++17 --platform=win32W --enable=warning,style,performance,portability --inline-suppr --template=gcc --suppress=missingIncludeSystem --suppress=missingInclude -D_WIN32 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -Imfd_editor/include -Imfd_window/src -Imfd_window_plugin_api/include -Imfd_common_api/include -Imfd_api/include -Imfd_client_api/include mfd_editor/src/EditorFileCommit.cpp mfd_window/src/AsyncFramebufferCapture.cpp mfd_client_api/src/LatestBatchPublisher.cpp mfd_editor/src/EditorPageRenameService.cpp mfd_editor/src/EditorReticleRenameService.cpp mfd_api/src/io/JsonLoader.cpp
```

Cppcheck completed with no errors. Suggestions were reviewed: the new utility's
explicit deletion-entry loop is retained for readability; the remaining
constructor/STL/scope suggestions concern existing code. This is source-level
analysis with unresolved external headers suppressed, not a complete MSVC
compilation-database analysis. Compiler warnings were also inspected; new macro
redefinition warnings were corrected, while existing dependency/generated-code
warnings and an existing unreachable test return remain outside scope.

All `AGENTS.md` readability patterns were searched using `git grep -n -F -e`
for each listed pattern. The tracked scan produced 717 hits, with 87 in affected
C++ files including new files. Those affected-file hits were reviewed: existing
Windows GL/ABI casts and platform-only `[[maybe_unused]]` declarations are
localized; old templates/visitors and synchronization mutex remain unchanged;
other matches are comments, UI strings or test strings. No new raw allocation,
broad captures, local classes, artificial unused suppression or public SDK
exposure were introduced.

Documentation static checks found one **pre-existing** orphan:
`docs/src/handbook/external_stream_review.md`, already tracked in the baseline
and absent from its `SUMMARY.md`. No new documentation-link issue was reported.
The check therefore exits 1; that unrelated page was left intact.

Validation is limited to the preferred Win32 Debug lane. Release, x64,
clang-tidy and sanitizer builds were not executed; the user's final instruction
was to finish the requested corrections and commits. Local logs are under
`build/corrections-*.log` (not committed).

### Final results

- Configure and full Win32 Debug build succeeded. The final incremental build's
  locked test executable was deleted and its target rebuilt successfully, as
  described above; no source compilation or link error remains.
- `ctest --preset test-debug-win32 --timeout 30`: **1,058 registered tests**,
  **1,054 passed**, **3 skipped**, **1 failed**, 238.15 seconds.
- The three skipped cases are `OffscreenSurfaceTests.Tutorial*`; their locally
  authored tutorial assets are absent, as reported during configuration.
- The one failed test, `ExternalStreamInteropLockFailure`, reported
  `Disconnect image/status mismatch` at `tests/external_stream/GpuIntegration.cpp:355`
  after killing its producer and ticking ten times. This existing test and the
  receiver implementation were not modified. An isolated rerun using
  `ctest --preset test-debug-win32 -R '^ExternalStreamInteropLockFailure$' --timeout 30 --output-on-failure`
  **passed (1/1, 4.09 seconds)**. The observed intermittent failure remains a
  residual integration-test risk; its root cause was not established here.
- All tests directly covering the seven corrections passed, including the
  corrected callback-destruction death test in the final full run. Across the
  full run and isolated retry, **1,055 distinct tests passed; 3 were skipped**.
- `git diff --check` passed. Manual diff review found no SDK signature/schema,
  generated-code or unrelated asset changes. The existing untracked audit file
  remains untouched and excluded from both commits.
