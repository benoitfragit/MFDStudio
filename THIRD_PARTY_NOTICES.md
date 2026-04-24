# Third-Party Notices

This repository uses a small set of external open-source dependencies.

This file has two goals:

- list the dependencies effectively used by the current build
- summarize the license obligations that matter in practice

The corresponding license texts are copied under `third_party/licenses/`.

Important:

- this file is an engineering/compliance aid
- it is not formal legal advice
- the statements below are based on the current repository and build
  configuration

## Current Dependency Inventory

The current build pulls the following direct dependencies from
`cmake/Dependencies.cmake`:

| Dependency | Version source in build | License | Used for |
| --- | --- | --- | --- |
| `raylib` | `5.5` | zlib/libpng-style permissive license | rendering, windowing integration |
| `nlohmann_json` | `v3.11.3` | MIT | JSON loading |
| `entt` | `v3.13.2` | MIT | ECS/runtime scene registry |
| `protobuf` | `v29.4` | BSD-3-Clause | command and feedback serialization |
| `imgui` | `v1.92.1` | MIT | editor and demo UIs |
| `rlImGui` | `main` branch in current CMake setup | zlib/libpng-style permissive license | ImGui/raylib bridge |

When `MFD_BUILD_TESTS=ON` (default in this repository), the build also pulls
this direct test dependency:

| Dependency | Version source in build | License | Used for |
| --- | --- | --- | --- |
| `googletest` | `v1.15.2` | BSD-3-Clause | automated runtime and JSON-loading tests |

The current build also uses these important transitive dependencies:

| Dependency | Source | License | Why it matters |
| --- | --- | --- | --- |
| `GLFW` | bundled by `raylib` for desktop platforms | zlib/libpng-style permissive license | window/input backend used by raylib on desktop |
| `abseil-cpp` | bundled by `protobuf` | Apache-2.0 | linked by the fetched protobuf build |
| `utf8_range` | bundled by `protobuf` | MIT | linked by the fetched protobuf build |

The repository also vendors one documentation-only frontend dependency for the
generated HTML portal:

| Dependency | Source | License | Used for |
| --- | --- | --- | --- |
| `doxygen-awesome-css` | vendored under `docs/doxygen-awesome/` from `jothepro/doxygen-awesome-css` `v2.4.2` | MIT | Doxygen HTML theme and UI extensions |

## Dependencies Not Counted As Runtime/Linked Requirements

The fetched `protobuf` source tree also contains other third-party folders such
as test or tooling dependencies.

Based on the current build configuration:

- `protobuf_BUILD_TESTS` is forced to `OFF`
- `protobuf_BUILD_EXAMPLES` is forced to `OFF`

So this notice intentionally focuses on the dependencies that matter for the
project binaries and their normal redistribution, not on disabled test/tooling
trees.

## Enterprise Usage Assessment

From a license perspective, the currently used dependency set is generally
enterprise-friendly.

Why:

- the set is composed of permissive licenses only
- no strong copyleft license was identified in the linked/runtime dependency
  set
- commercial and internal enterprise use is allowed by these licenses
- redistribution is allowed, provided the notice obligations are respected

In practical terms, the dependency set is usually acceptable in enterprise
software programs that allow:

- MIT
- BSD-3-Clause
- Apache-2.0
- zlib/libpng-style permissive licenses

## Practical Obligations

For the dependency set currently used here, the main obligations are the usual
permissive-license ones:

- keep the copyright and license notices
- keep the disclaimer of warranty/liability
- when redistributing binaries, include the required notices in a
  `THIRD_PARTY_NOTICES`, `licenses`, installer, or documentation bundle
- if you modify a dependency and redistribute it, keep the original notices and
  make your changes identifiable when the license requires it

Additional practical note for Apache-2.0 (`abseil-cpp`):

- keep the Apache-2.0 license text with redistributions
- keep any required notices if a dependency ships a `NOTICE` file

## Main Compliance Caveats

I do not see a license blocker for enterprise usage in the current dependency
set, but there are still a few practical caveats worth calling out.

### 1. Internal policy still wins

Many companies automatically approve these licenses, but some organizations
still require:

- a legal review
- SBOM registration
- attribution file packaging
- explicit approval of transitive dependencies

So "usable in enterprise" is, in practice:

- yes from a permissive open-source-license point of view
- subject to your internal compliance workflow

### 2. `rlImGui` is not pinned to a release tag

The current CMake file fetches:

- `rlImGui` from `main`

This is not a license issue, but it is a supply-chain and reproducibility
issue.

For enterprise usage, I recommend pinning it to:

- a specific tag, if available
- or an exact commit

That will make audits, rebuilds and future compliance reviews easier.

## License File Copies Included In This Repository

The following copied license texts are available under `third_party/licenses/`:

- `third_party/licenses/raylib-LICENSE.txt`
- `third_party/licenses/glfw-LICENSE.txt`
- `third_party/licenses/imgui-LICENSE.txt`
- `third_party/licenses/rlimgui-LICENSE.txt`
- `third_party/licenses/nlohmann_json-LICENSE.txt`
- `third_party/licenses/entt-LICENSE.txt`
- `third_party/licenses/protobuf-LICENSE.txt`
- `third_party/licenses/googletest-LICENSE.txt`
- `third_party/licenses/abseil-LICENSE.txt`
- `third_party/licenses/utf8_range-LICENSE.txt`
- `third_party/licenses/doxygen-awesome-css-LICENSE.txt`

## Bottom Line

Based on the current repository and build configuration:

- I do not see a dependency license that should block normal enterprise use
- the dependency set is permissive and commercially usable
- the main thing to do correctly is attribution/notice packaging
- for stronger enterprise hygiene, pin `rlImGui` to a fixed revision
