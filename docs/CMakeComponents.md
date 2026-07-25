<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Sharp Runtime CMake components

Sharp Runtime exposes independently selectable CMake targets. Applications
request only their direct components; Sharp Runtime resolves and enables the
transitive dependency closure.

The registered graph currently contains 41 physical modules and 90 direct
production dependency edges. The boundary validator reports no cycles,
duplicate include paths, orphan files, undeclared edges, stale edges, or
visibility mismatches. The dependency allow-list is empty.

The complete component, ownership, dependency, external-library, and
representative-header table is generated from the CMake registrations:
[generated component catalogue](ComponentCatalog.md). Local validation and
the tracked GitHub Actions workflow reject an out-of-date catalogue.

`BlockingCollection<T>` is owned by the narrow `Collections.Blocking`
component. This preserves the `Collections.Core` boundary: consumers such as
`Text.Json` do not configure `Threading` or `TimeZone` unless they explicitly
request a component that needs them.

## Selecting components

Set `SHARP_RUNTIME_COMPONENTS` before adding the project:

```cmake
set(SHARP_RUNTIME_COMPONENTS
    Net.WebSockets
    Text.Json
)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(path/to/sharp-runtime)

target_link_libraries(MyApp PRIVATE
    SharpRuntime::Net.WebSockets
    SharpRuntime::Text.Json
)
```

The application does not list transitive dependencies. For a standalone
configuration, pass a semicolon-separated list:

```bash
cmake -S . -B build-components \
  -DSHARP_RUNTIME_BUILD_TESTS=OFF \
  '-DSHARP_RUNTIME_COMPONENTS=IO;IO.Hashing'
cmake --build build-components --parallel 4
```

An unset or empty list selects `All` in a standalone repository build.
Embedding applications should always set the list explicitly.

## Dependency visibility

Each physical module owns its public headers, implementation, tests, and CMake
declaration under `modules/<module>/{include,src,tests,CMakeLists.txt}`.
Existing `System/...` and `SharpRuntime/...` include spellings are unchanged.

- `PUBLIC_DEPENDENCIES` are used by a module's public headers. Their include
  roots and link requirements propagate to consumers.
- `PRIVATE_DEPENDENCIES` are used only by implementation sources. Static-link
  requirements remain correct, but their include roots do not leak to
  consumers.
- `TEST_DEPENDENCIES` are available only to the owning module's test binary.
  They do not affect production targets or consumers.
- Platform and vendor targets are attached by the owning module's setup
  function with the narrowest valid visibility.

The validator derives actual edges from project-local includes and rejects a
missing, stale, or incorrectly visible declaration.

## Lean targets and compatibility umbrellas

New code should prefer the narrow physical targets:

- `SharpRuntime::Core.Base` owns foundation types. `SharpRuntime::Console`,
  `SharpRuntime::Uri`, and `SharpRuntime::TimeZone` are optional physical
  components.
- `SharpRuntime::Collections.Core` is intended to own synchronous collection
  fundamentals without optional high-level closures. `BlockingCollection<T>`
  belongs to `SharpRuntime::Collections.Blocking`, which isolates its
  `Threading` dependency. `SharpRuntime::Collections.Async` and
  `SharpRuntime::Collections.ObjectModel` similarly isolate asynchronous and
  notification-specific dependencies.

Compatibility targets preserve the historical broad surfaces:

- `SharpRuntime::Core` aggregates `Core.Base`, `Console`, `Uri`, and
  `TimeZone`.
- `SharpRuntime::Collections` aggregates all four collection components.
- `SharpRuntime::Xml.XPath` aliases the physical `SharpRuntime::Xml` archive.
- `SharpRuntime::All` aggregates every physical component.
- The legacy `SHARP_RUNTIME` target forwards to `SharpRuntime::All` when `All`
  is enabled.

The legacy target is deliberately absent from selective configurations because
creating it unconditionally would instantiate every optional dependency.

## Component-scoped tests

Tests no longer force `All`. A selective configuration builds the requested
component's tests plus explicitly declared test-only production dependencies:

```bash
cmake -S . -B build-json-tests \
  -DSHARP_RUNTIME_COMPONENTS=Text.Json \
  -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build-json-tests --target SharpRuntimeTests --parallel 4
scripts/run_component_tests.sh build-json-tests
```

That command runs only `SharpRuntimeTests_Text_Json`. It does not build tests
for dependencies or unrelated components.

For the repository-wide suite:

```bash
cmake -S . -B build \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build --target SharpRuntimeTests --parallel 4
scripts/run_component_tests.sh build
```

`SharpRuntimeTests` is a convenient aggregate build target. Executable targets
are named `SharpRuntimeTests_<Component>`; genuinely cross-module scenarios
are in `SharpRuntimeIntegrationTests`. CTest also discovers every individual
GoogleTest case.

The verified 2026-07-25 `All` baseline contains 12,625 tests across 36
component executables and one integration executable.

## Boundary validation and CI

Run the full native gate with:

```bash
scripts/local_ci_check.sh build
```

Run the selective consumer matrix with:

```bash
scripts/check_selective_components.sh
```

`.github/workflows/components.yml` runs the ten selective configurations and
the full compatibility build on Ubuntu for pushes and pull requests. It does
not currently provide Windows, macOS, or Emscripten coverage.

The full selective matrix is green. Its Text.Json job explicitly verifies that
the configured target graph excludes `Threading`, `ComponentModel`, networking
and external-library targets, then compiles negative include-leakage fixtures.

## External dependency isolation

External libraries are configured only by their owning component:

- `IO.Compression` finds ZLIB privately.
- `IO.Compression.Zip` builds vendored miniz privately.
- `Xml` builds vendored tinyxml2 and exposes it publicly because
  `XmlDocument.hpp` exposes tinyxml2 types.
- `Net` links `ws2_32` privately on Windows.
- `Security.Cryptography.Random` links `bcrypt` privately on Windows.
- `Storage` privately links an existing SDL3 target on Android.

A `Text.Json`-only build configures none of `Threading`, `TimeZone`, ZLIB,
miniz, tinyxml2, SDL, socket, or platform-crypto targets. The negative
consumer fixtures also cover private/sibling header leakage: `Text.Json` must not expose
`Collections.Core` or `Collections.ObjectModel`, and `Xml.Linq` must not
expose Xml's private `Diagnostics` dependency.

## Intentional ownership exceptions

The detailed Core classification is recorded in
[Core ownership](CoreOwnership.md). In particular, selected cross-namespace
foundation types remain in `Core.Base` when moving them would create a static
dependency cycle. Examples include `System::IO::IOException`,
`System::IO::DirectoryNotFoundException`, and
`System::Buffers::MemoryHandle`.

Xml and XPath remain one physical archive because their existing
implementations have mutual binary dependencies. This preserves both public
component names without introducing a graph cycle.

## Maintainer checklist

When adding or moving runtime code:

1. Place every public header, source, and module test under one physical
   `modules/<module>` owner. Put only genuinely cross-module scenarios under
   `tests/integration`.
2. Register each new physical directory once in
   `cmake/SharpRuntimeModules.cmake`.
3. Declare public-header edges as `PUBLIC_DEPENDENCIES`, source-only edges as
   `PRIVATE_DEPENDENCIES`, and test-only edges as `TEST_DEPENDENCIES`.
4. Attach vendor or platform libraries only in the owning module setup
   function. Propagate them publicly only if a public header exposes their
   types.
5. Avoid depending internally on the `Core`, `Collections`, or `All`
   compatibility umbrellas.
6. Regenerate the catalogue and run all boundary fixtures:

   ```bash
   python3 scripts/validate_module_boundaries.py
   python3 test/validate_module_boundaries_test.py
   python3 scripts/generate_component_catalog.py
   ```

7. Run `scripts/check_selective_components.sh` and
   `scripts/local_ci_check.sh` before committing.

Allow-list entries in
`cmake/SharpRuntimeModuleDependencyAllowlist.json` are reserved for genuine
link-only or generated edges. Every entry requires an owner, visibility, and
specific reason.

## Isolation result

`Collections.Blocking` restores the lean closures below while preserving the
same public header path, namespace, and `Collections` compatibility umbrella:

| Requested component | Production closure |
|---|---|
| `Text.Json` | Core.Base, Buffers, Text, Collections.Core, Text.Json |
| `Net.Http.Headers` | Core.Base, Uri, Collections.Core, Net.Http.Headers |
| `Net.Mime` | Core.Base, Collections.Core, Net.Mime |
| `Numerics` | Core.Base, Buffers, Collections.Core, Numerics |

These closures avoid `Threading`, `TimeZone`, `ComponentModel`, the broad
Collections umbrella, Console, networking/XML, and unrelated external
libraries.
