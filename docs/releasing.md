<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Releasing Sharp Runtime

*Current as of 0.1.0-alpha.1 (2026-08-20).*

Sharp Runtime follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html). A release
is a git tag plus a `CHANGELOG.md` entry — there is no separate release branch, and nothing is
published to a package registry yet.

## Where the version lives

The version is decided in exactly **one** place:

```cmake
# CMakeLists.txt (repository root)
project(SHARP_RUNTIME VERSION 0.1.0 LANGUAGES CXX C)
set(SHARP_RUNTIME_VERSION_PRERELEASE "alpha.1")   # empty on a final release
```

`project(VERSION …)` accepts numeric components only, so the pre-release identifier sits beside
it and the two are joined into `SHARP_RUNTIME_VERSION_STRING` (`0.1.0-alpha.1`).
`SHARP_RUNTIME_VERSION_PRERELEASE` is deliberately a normal variable and not a cache entry: a
cached copy would keep an existing build directory reporting the previous release after a bump.

Everything else derives from that:

| Consumer | How it gets the version |
|---|---|
| C++ code | `#include "SharpRuntime/Version.hpp"` → `SharpRuntime::getVersionString()`, `SHARP_RUNTIME_VERSION_MAJOR`, … |
| CMake consumers | `SHARP_RUNTIME_VERSION` / `PROJECT_VERSION`, set by `project()` |
| The configure banner | `-- Sharp Runtime: version <x.y.z>` |

`SharpRuntime/Version.hpp` is **generated** by `cmake/Version.cmake` from
`cmake/templates/Version.hpp.in` into `<build>/generated/include/SharpRuntime/Version.hpp`, and
that directory is published on `SharpRuntime::Headers` — the interface target every component and
consumer links transitively, the same carrier the build already uses for
`SHARP_RUNTIME_HAS_NATIVE_INT128`. Never edit the generated file, and never hard-code the version
anywhere else.

Because the header exists only in a configured build tree, `scripts/validate_module_boundaries.py`
— which deliberately runs without one — names it in `GENERATED_INCLUDE_PATHS`. That list is a set
of exact paths, not a prefix: an unknown `SharpRuntime/…` include is still reported as unresolved,
and `test/validate_module_boundaries_test.py` pins both halves.

Two copies are maintained by hand and must be updated as part of a bump:

- `CHANGELOG.md` — the release entry and its link definitions at the bottom.
- `Doxyfile` — `PROJECT_NUMBER`.

## Numbers that are *not* the product version

- **`System::Version`** is the ported .NET *type* for representing a version number. It is part
  of the API surface and says nothing about which release of this library you are holding.
- **The .NET API level being reimplemented** is a property of the reference tree, not of this
  release, and moves independently of it.
- **`SHARP_RUNTIME_HAS_NATIVE_INT128`** is a compiler capability, not a version.
- **The test-count baseline** in `CLAUDE.md` is a floor for the gate, not a release number.

## Pre-1.0 policy

While the major version is 0, a minor bump may change the public API — which for this project is
routine rather than exceptional: `docs/StandingApprovals.md` SA-2 governs public source breaks and
they land with a migration note. Pre-release identifiers are `alpha.N` → `beta.N` → `rc.N`,
ordered as SemVer orders them. `0.1.0-alpha.1` precedes `0.1.0`.

## Downstream consumers

CNA and mobile-eggbert consume this repository as a **sibling checkout**, with
`add_subdirectory(../sharp-runtime)`, so a downstream build takes whatever revision that checkout
happens to be on. CNA's own `CHANGELOG.md` records the sharp-runtime revision each of its releases
was built against; once a release here is tagged, that entry can name the tag instead of a raw
commit hash. Tagging is therefore worth doing before a downstream release, not after.

## Cutting a release

1. **Choose the version.** Edit `project(SHARP_RUNTIME VERSION …)` and/or
   `SHARP_RUNTIME_VERSION_PRERELEASE` in the root `CMakeLists.txt`, and set `PROJECT_NUMBER` in
   `Doxyfile` to the same string.
2. **Write the changelog entry.** Move what is under `## [Unreleased]` into a new
   `## [x.y.z] — YYYY-MM-DD` section in `CHANGELOG.md` and add the two link definitions at the
   bottom of the file.
3. **Build and test** in the existing build directory — never a fresh one, and never more than
   two parallel jobs (`CLAUDE.md` § *Build-resource policy*, which is binding here too):

   ```bash
   cmake -S . -B build
   cmake --build build --parallel 2
   scripts/run_component_tests.sh build
   ```

   The configure banner prints `-- Sharp Runtime: version <x.y.z>` — check it matches. The gate
   must be **zero warnings, zero errors** and show no test-count regression against the baseline
   recorded in `CLAUDE.md`; `scripts/local_ci_check.sh` runs the same checks plus the module
   boundary, seam and negative-fixture validators.
4. **Commit** the version-bearing files by explicit name (`CMakeLists.txt`, `Doxyfile`,
   `CHANGELOG.md`), never `git add -A`.
5. **Tag** with a `v` prefix and an annotated tag:

   ```bash
   git tag -a v0.1.0-alpha.1 -m "Sharp Runtime 0.1.0-alpha.1"
   ```

   The tag string carries the `v`; `SHARP_RUNTIME_VERSION_STRING` never does.
6. **Push only with the project owner's explicit per-action approval** — `CLAUDE.md` rules 3 and 9
   cover both the branch and the tag — and push the tag explicitly:

   ```bash
   git push origin <branch>
   git push origin v0.1.0-alpha.1
   ```
7. **Open the next cycle** by adding an empty `## [Unreleased]` section back to `CHANGELOG.md` if
   step 2 consumed it.
