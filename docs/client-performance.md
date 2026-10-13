# Realtime client implementation optimizations

Baseline: `3fd0098b8632962036f9a0ab271af0fc846ca566` (master, 2.10.1).
The public SDK signatures, generated helpers, object data layout, `.proto` files,
mapping hash calculation, runtime ids and UDP contract are unchanged. The only
header change is to private `CommandClient` implementation methods; no new public
entry point is exposed. No `mfd_window` behavior is changed.

## Confirmed costs and retained changes

- `CommandClient` copied all input commands before normalizing, including resolved
  patches, maps and dynamic states. `NormalizeBatchForTransport` now borrows fully
  resolved static/dynamic update commands. Eligibility is conservative: every
  identity must already exist and no named patch field may need resolution.
  Identity validation reuses the existing normalizer/resolvers on small identity
  values, including conflicting names, ids and mapping hashes. Other command types
  or unresolved fields take the complete normalization path. The serialized view
  owns no caller data and never escapes the synchronous send operation.
- `Send` and `SendBatch(ArrayView)` no longer create an intermediate owning vector.
  The existing atomic work check runs before normalization and any send.
- `CommandTypes` uses a local Protobuf arena with a 4 KiB stack initial block for
  the envelope and nested messages. Known repeated-field sizes are reserved once.
  Storage is destroyed at the end of each operation; returned strings remain owned
  by the caller. There is no global/thread-local production cache or additional
  dependency. Input validation and the public serializer's error messages remain
  the same. Oversized candidate batches are measured without writing output bytes.
- Packet assembly previously serialized each growing candidate chunk. It now adds
  exact independent command wire contributions to the conservative fragment
  envelope overhead. Protobuf itself computes tags and length-prefix sizes, so
  varint boundaries remain exact. The existing dynamic-state size helper and
  bounded greedy dynamic splitter remain in use. The splitter measures candidates
  without serialized buffers. Owned temporary commands and final payloads are
  moved where their source is no longer needed.
- Both dynamic-set `AppendCommands` implementations reserve their update vector
  only on the first effective update. Unchanged/removal-only cycles do not allocate
  this vector. Dense cycles retain the existing one-reservation policy.

All final fragment payloads are built and checked before the first send. The 512
atomic work units, dynamic limits, packet ceiling, command order, sequence,
session identity and fragment metadata are retained. Fragment count/partitioning
uses the existing conservative metadata budget.

## Deliberately retained copies and rejected directions

`desiredPatch_` and `lastSentPatch_` are independent snapshots; moving one into the
other would destroy subsequent delta computation. Their map copies remain intact.
Adding a second map comparison/update scheme was not justified by a measured gain
and would duplicate patch-field semantics. Returned generated batches must own
their data across subsequent cycles. `BuildBatch`/`BuildCommandBatch` already
transfer their vectors by move; no generator or generated signature is changed.

`LatestBatchPublisher` already moves pending batches into the worker and through
the common submit path. The copies used when collecting/merging pending deltas
have state-preservation responsibilities. No safe measured local replacement was
demonstrated; retry, latest-wins and merge code are unchanged.

Persistent Protobuf/output caches were not introduced: they would retain peak
memory and need ownership/reentrancy invariants across calls. Local arenas remove
many small allocations without that lifetime change. `SerializeToArray` was not
selected: `SerializeToString` already owns the exact output string and avoids an
additional manual output-size/cast contract. No change to serialization format is
needed to eliminate candidate serialization.

## Reproduction and validation

The measurement harness is local-only, as requested, and is not part of the SDK
or this commit. Release Win32 measurements use the same compiler, dependencies
and harness for baseline and modified source, with 120 warm-up cycles and 1,200
measured cycles per workload, repeated three times in alternating before/after
order. Workloads contain 10, 100 or 400 dynamic reticles, with simulated time
`cycle / 60` seconds, for bulk updates, unchanged cycles and individual commands.
No sleeps or actual 60 Hz scheduler are measured.

Instrumentation is inserted into build-directory copies of the implementation.
Standard C++ allocation replacements count allocation calls and requested bytes
in statically linked code, including aligned allocations, but exclude CRT malloc,
DLL heaps and virtual memory. This measures allocation traffic, not peak memory.
The synchronous counting channel records packet sizes without UDP/kernel noise.
Construction, normalization, Protobuf preparation/size/encoding, and fragmentation
are charged separately to the innermost active scope. Total percentiles use actual
per-cycle totals. CPU cycles use Windows `QueryThreadCycleTime`; elapsed time uses
`steady_clock`. Instrumentation overhead remains present in both executables.
Reported values are medians of the three run statistics, not pooled percentiles.
Random session identities can slightly change packet sizes between runs.

Functional tests include named/generated identities, mapping failures, caller
immutability, fragment ordering and reassembly, atomic/UDP limits, invalid trailing
NaN input with zero sends, exact wire sizes at varint boundaries, independent
Protobuf semantic comparison of maps/composite patches, concurrent encoding with
failures, idle/removal/recreation cycles, and generated batch snapshot lifetimes.
Existing async publisher tests protect retry and undelivered deltas.

Validation on 2026-10-13:

- Debug Win32 build succeeded with the repository's Visual Studio 2022 preset
  (MSVC v142, compiler 19.29.30159.0, strict C++17).
- `ctest --preset test-debug-win32 -j 1 --timeout 180`: 984 registered tests,
  **981 passed, 3 skipped, 0 failures**, 241.02 seconds. The three existing
  offscreen tutorial tests skip because the tutorial asset set is incomplete.
- The initial parallel run passed all functional tests; two packaging tests
  collided while rebuilding shared MSBuild outputs. Both passed in the complete
  serial rerun. No unrelated packaging behavior was changed.
- Public `CommandClient` declarations and all object fields were compared with
  the baseline and are identical. No `.proto` or generator file was modified.
- Relevant readability checks and a second diff review found no new template
  dispatch, unsafe cast, broad lambda capture, manual production allocation,
  persistent cache or public interface expansion.
- `docs/CheckDocs.ps1 -SkipBuild` found one pre-existing orphan page,
  `docs/src/handbook/external_stream_review.md`, absent from `SUMMARY.md`.
  The new README link resolves; no documentation link failure was introduced.

The modified production functions are in `CommandClient.cpp` (normalization,
`Send`, `SendBatch`, `SendBatchedPayloads`, single-command sizing and dynamic
splitting), `CommandTypes.cpp` (validation view, envelope construction,
`SerializeCommandBatch`, repeated-field reservation and private size helpers),
and the two dynamic-set `AppendCommands` methods in `Animation.cpp`.
`CommandSerialization.h` is a private, non-installed implementation header.

## Release measurements

Host: Intel Core i5-8265U, 4 cores / 8 logical processors, 8 GiB RAM, Windows;
Release Win32 using MSVC 19.29.30159.0. All compilations and tests finish before
the alternating benchmark runs start.

The measurement parent was pinned to logical CPU 7 (affinity `0x80`); child
executables inherited that affinity and ran at above-normal process priority.
Every paired cell below is **baseline -> optimized**, per simulated cycle.

### Complete-cycle allocation and timing traffic

| Reticles / workload | Allocations | Allocated KiB | Mean us | P95 us | P99 us | Mean thread CPU cycles |
|---|---:|---:|---:|---:|---:|---:|
| 10 / bulk | 544 -> 375 | 56.7 -> 38.7 | 77.1 -> 47.9 | 105.1 -> 69.0 | 123.4 -> 95.7 | 135632 -> 83314 |
| 10 / idle | 2 -> 1 | 3.5 -> 0.0 | 2.3 -> 2.2 | 2.4 -> 2.6 | 3.1 -> 3.4 | 2332 -> 2018 |
| 10 / individual | 1142 -> 966 | 125.4 -> 107.0 | 150.7 -> 121.9 | 191.7 -> 156.2 | 248.9 -> 187.4 | 267103 -> 215742 |
| 100 / bulk | 5317 -> 3707 | 556.7 -> 406.5 | 606.1 -> 410.3 | 862.8 -> 594.2 | 1159.6 -> 818.1 | 1082300 -> 732614 |
| 100 / idle | 2 -> 1 | 35.2 -> 0.0 | 5.6 -> 5.0 | 5.8 -> 5.2 | 12.2 -> 5.2 | 8235 -> 7333 |
| 100 / individual | 11718 -> 10011 | 1310.4 -> 1151.7 | 1281.1 -> 1199.8 | 1882.9 -> 1570.7 | 2294.2 -> 2010.1 | 2291590 -> 2147900 |
| 400 / bulk | 73354 -> 20872 | 8022.9 -> 2885.5 | 10282.3 -> 3015.1 | 15026.4 -> 4796.7 | 21332.4 -> 6568.2 | 18320300 -> 5336820 |
| 400 / idle | 2 -> 1 | 140.7 -> 0.0 | 18.5 -> 26.3 | 18.8 -> 33.2 | 37.8 -> 42.3 | 31259 -> 44659 |
| 400 / individual | 209529 -> 71215 | 17081.7 -> 8569.0 | 29063.7 -> 8896.5 | 40098.1 -> 10249.2 | 53683.5 -> 11556.8 | 51966700 -> 15836700 |

### Encoding and packet counts

Packet bytes are total bytes per cycle; maximum packet is the largest observed
packet in that run. Session-id varints explain small before/after byte differences.

| Reticles / workload | Encodings | Packets | Total payload bytes | Maximum packet bytes |
|---|---:|---:|---:|---:|
| 10 / bulk | 1 -> 1 | 1 -> 1 | 328 -> 328 | 328 -> 328 |
| 10 / idle | 0 -> 0 | 0 -> 0 | 0 -> 0 | 0 -> 0 |
| 10 / individual | 1 -> 1 | 1 -> 1 | 399 -> 398 | 399 -> 398 |
| 100 / bulk | 1 -> 1 | 1 -> 1 | 2758 -> 2758 | 2758 -> 2758 |
| 100 / idle | 0 -> 0 | 0 -> 0 | 0 -> 0 | 0 -> 0 |
| 100 / individual | 1 -> 1 | 1 -> 1 | 3548 -> 3549 | 3548 -> 3549 |
| 400 / bulk | 15 -> 3 | 3 -> 3 | 10988 -> 10985 | 3926 -> 3925 |
| 400 / idle | 0 -> 0 | 0 -> 0 | 0 -> 0 | 0 -> 0 |
| 400 / individual | 809 -> 4 | 4 -> 4 | 14209 -> 14213 | 4079 -> 4080 |

### Exclusive phases for the fragmented 400-reticle workloads

Construction includes state setters, delta construction and, for the individual
workload, conversion of bulk states into individual commands. Serialization
includes validation, Protobuf construction, exact sizing and final encoding.
Fragmentation includes remaining packet preparation. Phase medians need not sum
exactly to the median complete-cycle statistic.

| Workload / phase | Allocations | Allocated KiB | Mean us | P99 us | Mean thread CPU cycles |
|---|---:|---:|---:|---:|---:|
| bulk / construction | 14803 -> 14803 | 1517.7 -> 1517.7 | 1833.5 -> 1537.9 | 3619.4 -> 3383.4 | 3280720 -> 2754500 |
| bulk / normalization | 5203 -> 0 | 608.3 -> 0.0 | 503.7 -> 6.7 | 1171.6 -> 23.7 | 902435 -> 11008 |
| bulk / serialization | 9838 -> 849 | 795.2 -> 753.4 | 2151.7 -> 840.1 | 4340.7 -> 1753.8 | 3450050 -> 1160960 |
| bulk / fragmentation | 43510 -> 5220 | 5101.7 -> 614.5 | 5793.4 -> 630.5 | 12435.5 -> 1450.7 | 10687100 -> 1410350 |
| individual / construction | 37194 -> 37194 | 4153.0 -> 4153.0 | 4381.2 -> 3932.3 | 8575.7 -> 5799.7 | 7856450 -> 7055090 |
| individual / normalization | 5202 -> 400 | 636.0 -> 12.5 | 477.9 -> 108.7 | 1155.5 -> 192.5 | 856026 -> 193938 |
| individual / serialization | 121902 -> 431 | 7190.0 -> 458.7 | 18214.0 -> 1317.8 | 33392.0 -> 1851.2 | 31814700 -> 1662040 |
| individual / fragmentation | 45231 -> 33190 | 5102.8 -> 3944.8 | 5990.5 -> 3537.7 | 10663.3 -> 4811.3 | 11439500 -> 6925630 |

### Interpretation and remaining limits

The fragmented bulk workload removes 71.5% of allocation calls and 64.0% of
allocated bytes; mean thread CPU cycles drop 70.9%. Individual fragmented commands
remove 66.0% of allocation calls and 49.8% of allocated bytes; mean thread CPU
cycles drop 69.5%. Encoding is now once per final fragment in these workloads.
Packet counts remain identical. Already resolved bulk normalization allocates
nothing in this fixture; identity-only copies can still allocate for individual
commands with strings. No claim of zero allocation for the whole client is made.

Idle allocation traffic consistently falls from two calls to one, with only the
fixture's 32-byte mapping-hash copy remaining. Timing is less stable: in the
complete runs, 400-reticle idle means vary from 17.8 to 34.3 us after optimization,
and the median is higher than baseline. The tiny 10-reticle idle P99 also rises
by 0.3 us. These observations are reported rather than hidden by the large-batch
gains. The isolated idle check below does not reproduce the apparent regression.

The harness does not measure UDP scheduling, receiver work, rendering, end-to-end
latency or peak resident memory. Laptop frequency changes and background activity
remain possible despite affinity; these statistics are not confidence intervals.
Named-field normalization intentionally retains its copying fallback. Publisher
merging and generated snapshot costs are not eliminated. Production throughput
should be confirmed on the target hardware and real workload.

### Isolated idle cross-check

To investigate the noisy idle result, the same instrumented sources were rebuilt
with an `--idle-only` harness selector. Nine alternating before/after pairs ran
the same 120 warm-up and 1,200 measured cycles, on the same logical CPU and priority,
without preceding heavy fragmented workloads. This is an additional measurement,
not a replacement for the complete-run figures above.

| Reticles | Mean us | P95 us | P99 us | Mean thread CPU cycles |
|---|---:|---:|---:|---:|
| 10 | 2.59 -> 2.12 | 3.20 -> 2.20 | 4.30 -> 2.40 | 2800 -> 2067 |
| 100 | 6.08 -> 5.73 | 6.10 -> 7.90 | 10.10 -> 9.40 | 8835 -> 8331 |
| 400 | 22.13 -> 21.41 | 32.60 -> 30.20 | 43.00 -> 36.30 | 37421 -> 35933 |

Idle mean CPU cycles and P99 improve at all three sizes in this isolated check.
The 100-reticle P95 still varies by 1.8 us. These results support removing the
unconditional reserve, while leaving small timing differences subject to host
noise. They do not establish a universal latency guarantee.

Local raw CSVs are retained in `build/client-perf-results-pinned/` and
`build/client-perf-idle-isolated/`. The uncommitted harness is retained in
`build/client-perf-harness/`; no benchmark target, script, executable or generated
measurement file is added to the repository.
