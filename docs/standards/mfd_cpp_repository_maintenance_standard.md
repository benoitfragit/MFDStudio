# MFDStudio C++ Repository Maintenance Standard

Status: versioned contributor standard
Audience: contributors changing C++, CMake, tests, or contributor-facing
documentation

This document defines the default engineering bar for versioned contributions
touching C++, CMake, tests, or contributor-facing documentation in this
repository.

Use this page as the repository source of truth for maintainability
expectations. A local root `AGENTS.md` may exist for user- or tool-specific
instructions, but that file is intentionally ignored by Git and must not
replace this versioned standard.

If you are trying to launch the product, author JSON, or understand the user
workflow, start with [Documentation Guide](../README.md) and
[Quick Start](../QUICKSTART.md) instead of this page.

## Quick Checklist

Every contribution should preserve these properties:

- the code remains C++17-only
- modules stay small, focused, and composable
- public APIs stay minimal and intentional
- ownership, lifetime, and runtime cost stay explicit
- tests and documentation move with the code
- public headers stay documented with current Doxygen comments

## Scope

This standard applies to:

- public headers under `*/include/`
- private implementation code under `*/src/`
- repository `CMakeLists.txt` files and helper scripts
- automated tests under `tests/`
- contributor-facing documentation under `README.md` and `docs/`

## Core Engineering Rules

### C++17 Baseline

- Write repository C++ against the `C++17` language level only.
- Keep `CMAKE_CXX_EXTENSIONS` disabled and avoid compiler-specific APIs unless
  the code is isolated behind a narrow platform layer.
- Prefer standard-library facilities already available in C++17 before adding
  custom compatibility wrappers or third-party abstractions.

### Architecture And Object Design

- Do not introduce monolithic classes that mix transport, parsing, rendering,
  persistence, and UI concerns.
- Keep one primary responsibility per class, service, or helper module.
- Prefer composition over inheritance. Use inheritance only when the subtype
  relationship is real and improves clarity.
- Use explicit design patterns only when they make the structure easier to
  recognize and maintain. Do not introduce pattern-shaped complexity without a
  concrete payoff.
- Hide implementation details in `.cpp` files, internal helpers, or anonymous
  namespaces instead of widening public headers.

### Public API Discipline

- Expose only the operations the next layer truly needs.
- Keep public interfaces stable, typed, and intention-revealing.
- Avoid leaking third-party types through public headers unless that dependency
  is part of the intended contract.
- Prefer forward declarations and narrow headers when they materially reduce
  coupling without obscuring the code.
- Add new public API only when the same result cannot be achieved through an
  existing, coherent abstraction.

### Memory And CPU Discipline

- Default to RAII ownership.
- Prefer `std::unique_ptr` over `std::shared_ptr` unless shared ownership is
  required by the design itself.
- Avoid raw `new` and `delete` outside tightly scoped low-level integration.
- Avoid avoidable copies, temporary allocations, and repeated recomputation in
  hot paths.
- Reserve container capacity when the final size is known.
- Prefer simple data structures with predictable lifetime and iteration costs.
- Measure before adding complexity in the name of optimization.

### Error Handling And Invariants

- Encode invariants in types and constructors where possible.
- Keep invalid states hard to represent and easy to detect.
- Use `const`, `override`, `noexcept`, `enum class`, and `constexpr` when they
  express real guarantees.
- Document preconditions or side effects when they are not obvious from the
  type signature.

## Documentation Standard

### Doxygen For Public C++

Public headers are part of the published product surface. Document them with
Doxygen comments that stay synchronized with the code:

- use `@brief` on public classes, structs, enums, and functions
- use `@param` for non-obvious parameters
- use `@return` when the meaning of the returned value is not trivial
- use `@pre`, `@post`, or `@note` when callers need extra behavioral context

Private implementation comments should stay rare and focused on non-obvious
intent, invariants, or algorithmic constraints.

### Repository Documentation

When behavior changes, update the right documentation shelf in the same change:

- `README.md` for newcomer-facing entry points, targets, and prerequisites
- `docs/DEVELOPMENT.md` for build, test, packaging, or contributor workflow
- `docs/reference/` for exact authoring and data-shape rules
- `docs/tutorials/` for hands-on workflows
- `docs/architecture/` for internal structure and design rationale
- `docs/standards/` for normative repository or interoperability rules

## Test Standard

- Every bug fix should add or adjust a regression test when practical.
- Every new behavioral rule should be covered at the lowest useful test layer.
- Keep tests deterministic, readable, and focused on one responsibility.
- Prefer unit or service-level coverage before adding heavier end-to-end flows.
- When generator behavior changes, update both compiled and runtime validation
  where relevant.

The repository test tree should continue to mirror the production modules:

- `tests/mfd_api`
- `tests/client_api`
- `tests/mfd_window`
- `tests/mfd_editor`
- `tests/client_api_generator`

## Build And Validation Baseline

For local validation on Windows, prefer the Win32 debug flow unless the task
specifically requires another configuration:

```powershell
cmake --preset vs2022-win32
cmake --build --preset debug-win32
ctest --preset test-debug-win32
```

If a change intentionally affects only documentation or ignored local tooling
instructions, say so explicitly in the review or handoff note.

## Review Checklist

Before considering a change done, verify:

- the design is still modular and understandable
- the public API surface did not grow without a clear reason
- ownership and lifetime rules are obvious
- CPU and memory costs are reasonable for the touched path
- tests cover the new or corrected behavior
- `README.md` and `docs/` were updated when the change affected users or contributors
- public headers remain documented with current Doxygen comments

## Non-Goals

This standard does not replace:

- feature-specific architecture notes
- the generated client API standardization documents
- the external client interoperability specification
- local operator instructions kept outside version control
