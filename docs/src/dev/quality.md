# Quality

Heavyweight analysis is kept outside the nominal Visual Studio build. Each entry
point writes a report under `outputs/quality/`.

| Script | Role |
| --- | --- |
| `.\Scripts\Run-ClangTidy.ps1` | Repository `clang-tidy` policy → `outputs/quality/clang-tidy/latest`. |
| `.\Scripts\Run-Cppcheck.ps1` | One exhaustive `cppcheck` pass → `outputs/quality/cppcheck/latest`. |
| `.\Scripts\Run-Fuzzing.ps1` | Dedicated Clang/Ninja `libFuzzer` + ASan build → `outputs/quality/fuzz/latest`. |
| `.\Scripts\Run-QualitySuite.ps1` | GoogleTest + clang-tidy + cppcheck + fuzzing → `outputs/quality/runs/<timestamp>`. |

By default the scope is limited to production modules; `tests` and `examples`
are opt-in through `-Paths`:

```powershell
.\Scripts\Run-ClangTidy.ps1 -Paths mfd_api,mfd_common_api,mfd_window,mfd_editor
.\Scripts\Run-ClangTidy.ps1 -Paths tests
.\Scripts\Run-ClangTidy.ps1 -AuditOnly
.\Scripts\Run-Cppcheck.ps1
.\Scripts\Run-Fuzzing.ps1 -DurationSeconds 120
.\Scripts\Run-QualitySuite.ps1
```

Details:

- **clang-tidy** also audits the `AGENTS.md` forbidden patterns, reuses
  `build/clang-tidy-win32/compile_commands.json`, and finds `clang-tidy.exe` from
  the Visual Studio LLVM tools when it is not on `PATH`.
- **cppcheck** runs in exhaustive mode and filters out diagnostics emitted only
  from `build/` or vendored dependency trees.
- **fuzzing** requires LLVM (`clang`, `clang++`, `llvm-symbolizer`) and uses
  `MFD_ENABLE_FUZZING=ON`. That option must stay `OFF` in the normal build; the
  fuzz build lives under `build/clang-fuzz-x64` and never touches the Win32 tree.

## Review expectations

The repository review bar (see [`AGENTS.md`](https://github.com/benoitfragit/MFDStudio/blob/master/AGENTS.md))
prioritizes runtime bugs: freezes, crashes, state corruption, NaN/Inf,
desynchronization, and memory risks. Every loop must have proven progression and
iteration guards; every input boundary (JSON, UDP, Protobuf, plugins, editor,
API) must validate bounds, types, ids, enums, and non-finite values. Bug fixes
ship with a non-regression test when reasonable.
