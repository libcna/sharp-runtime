# Sharp Runtime

Sharp Runtime is a C++23 implementation of a practical subset of the .NET
`System.*` libraries. It provides familiar APIs for native game and framework
ports, especially CNA, without attempting to implement a CLR, JIT, garbage
collector, or the complete .NET platform.

The repository currently builds as 40 independently selectable CMake
components. The verified Linux baseline on 2026-07-25 is a warning-free build
with **12,586 passing tests across 36 test executables**.

## What is included

- Core value types, strings, spans, dates, times, exceptions, delegates, and
  environment helpers.
- Generic, immutable, object-model, concurrent, and asynchronous collections.
- Text, regular expressions, globalization, JSON, XML, and XML LINQ APIs.
- Streams, files, compression, ZIP archives, hashing, and isolated storage.
- Networking primitives, sockets, HTTP, MIME, WebSockets, and network
  information.
- Threads, tasks, task continuations, channels, timers, and synchronization
  primitives.
- Numerics plus non-encryption cryptography such as hashes, HMAC, PBKDF2, and
  secure random-number generation.

The public surface follows .NET naming and behavior where that maps cleanly to
C++. The implementation uses RAII, standard-library ownership types, and
fixed-width aliases such as `SharpRuntime::intcs` for .NET-sized integral API
values.

## Quick start

Requirements:

- CMake 3.20 or newer.
- A compiler with C++23 support.
- Git submodules when tests are enabled.
- A development package providing ZLIB when `All` or `IO.Compression` is
  selected.

Configure, build, and run the full repository suite:

```bash
git submodule update --init --recursive
cmake -S . -B build \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build --target SharpRuntimeTests --parallel 4
scripts/run_component_tests.sh build
```

`SharpRuntimeTests` is an aggregate build target, not a test executable. The
actual binaries are component-scoped, for example:

```bash
./build/SharpRuntimeTests_Net_Sockets \
  --gtest_filter="TcpClient*"
```

For a library-only build:

```bash
cmake -S . -B build-no-tests \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=OFF
cmake --build build-no-tests --parallel 4
```

The complete local validation gate performs boundary checks, a warning-free
build, and every component and integration test:

```bash
scripts/local_ci_check.sh build
```

Some HTTP, socket, and ping tests require the environment to permit local
network operations.

## Selecting CMake components

Applications should request only the components they use:

```cmake
set(SHARP_RUNTIME_COMPONENTS
    Text.Json
)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(path/to/sharp-runtimervc)

target_link_libraries(MyApp PRIVATE
    SharpRuntime::Text.Json
)
```

CMake resolves the dependency closure automatically. At the current code
baseline, `Text.Json` enables `Core.Base`, `Buffers`, `Text`, `TimeZone`,
`Threading`, `Collections.Core`, and `Text.Json`. The `TimeZone`/`Threading`
part is a known isolation regression introduced when `BlockingCollection`
made `Collections.Core` depend on `Threading`; the planned fix is to move the
threading-dependent concurrent surface into its own physical component.
Networking, XML, ZLIB, miniz, tinyxml2, and SDL remain outside this closure.

Multiple components form a normal CMake list:

```cmake
set(SHARP_RUNTIME_COMPONENTS
    IO
    IO.Hashing
)
```

The narrow physical targets are preferred for new consumers. Compatibility
targets remain available:

- `SharpRuntime::Core` aggregates `Core.Base`, `Console`, `Uri`, and
  `TimeZone`.
- `SharpRuntime::Collections` aggregates `Collections.Core`,
  `Collections.Async`, and `Collections.ObjectModel`.
- `SharpRuntime::Xml.XPath` aliases the physical `Xml` archive.
- `SharpRuntime::All` aggregates all physical components.
- The legacy `SHARP_RUNTIME` target forwards to `SharpRuntime::All` in an
  `All` configuration.

See [CMake components](docs/CMakeComponents.md) and the
[generated component catalogue](docs/ComponentCatalog.md) for the complete
dependency and external-library map.

### Selective component tests

A selective test configuration builds only the requested component's tests,
plus explicitly declared test-only dependencies:

```bash
cmake -S . -B build-json-tests \
  -DSHARP_RUNTIME_COMPONENTS=Text.Json \
  -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build-json-tests --target SharpRuntimeTests --parallel 4
scripts/run_component_tests.sh build-json-tests
```

## Repository layout

- `modules/<module>/include/` contains public headers.
- `modules/<module>/src/` contains compiled implementations.
- `modules/<module>/tests/` contains tests owned by that physical component.
- `tests/integration/` contains deliberately cross-component tests.
- `cmake/` contains component registration and dependency resolution.
- `scripts/` contains boundary, catalogue, selective-build, and local-CI
  checks.
- `vendor/` contains or references third-party dependencies.
- `docs/` contains architecture and generated component documentation.

Module `CMakeLists.txt` files are declarations consumed by the root project;
they are not standalone projects.

## Validation and CI

The component graph is enforced rather than documented only:

- `scripts/validate_module_boundaries.py` checks ownership, include
  resolution, declared visibility, stale edges, duplicate public paths, and
  cycles.
- `test/validate_module_boundaries_test.py` exercises negative validator
  fixtures.
- `scripts/generate_component_catalog.py --check` rejects catalogue drift.
- `scripts/check_selective_components.sh` defines nine isolated positive
  consumers and negative leakage fixtures.
- `.github/workflows/components.yml` runs the selective matrix and the full
  compatibility build on Ubuntu for pushes and pull requests.

At the current baseline the graph has **40 physical modules and 88 direct
production dependency edges**, with no allow-listed exception. The boundary
validator and full build/test gate pass, but the complete selective matrix is
**not green**: its `Text.Json` job rejects the newly reintroduced
`sharp_runtime_threading` target. Until the Collections split described in
`plan.md` is completed, the corresponding GitHub Actions job is expected to
fail even though `scripts/local_ci_check.sh build` passes.

## Platform status

The complete build and test baseline is currently verified on Linux with GCC.
Other platform evidence is narrower:

| Platform/toolchain | Verified scope |
|---|---|
| Linux/GCC | Current full component build and all 12,586 tests. |
| Windows/MinGW | The pre-component library build was warning-free in the ticket #40 audit; GoogleTest was not cross-built, and the post-modular tree is not covered by repository CI. |
| Emscripten | The pre-component library build was warning-free in the ticket #41 audit; tests were not cross-built, and some runtime APIs deliberately throw `PlatformNotSupportedException`. |
| macOS/Apple Clang | Real downstream Xcode 15.4 builds drove portability fixes on 2026-07-20; this repository has no macOS job or recorded full standalone test baseline. |
| MSVC | `Decimal`, `Int128`, and `UInt128` remain unsupported because they require the GCC/Clang `__int128` extension. |

Compile portability and runtime feature availability are separate. Unsupported
operations should compile and fail explicitly with
`PlatformNotSupportedException`, rather than silently degrade. The detailed
policy and known runtime-limited areas are in [CLAUDE.md](CLAUDE.md).

## Intentional differences from .NET

Sharp Runtime intentionally excludes:

- CLR execution, JIT compilation, and garbage collection.
- General runtime reflection and APIs that depend on it.
- Serialization infrastructure and P/Invoke/interop.
- Late-bound delegate `DynamicInvoke`.
- Symmetric/asymmetric encryption, X.509 certificates, and TLS/`SslStream`.

Hash algorithms, HMAC, PBKDF2, and random-number generation remain in scope.
Individual APIs can also document smaller, explicit deviations where C++ has
no safe or useful equivalent.

## Planning and implementation status

Versioned planning is split by purpose:

- [plan.md](plan.md) records the current roadmap and completed architecture
  milestones.
- [NEXT.md](NEXT.md) is the concise cold-start handoff: verified baseline,
  recent changes, known gaps, and the next bounded tasks.
- [CLAUDE.md](CLAUDE.md) defines contributor invariants and the porting
  checklist.
- [prompt.md](prompt.md) defines the local SQLite workflow.

Maintainers also use a local, git-ignored `plan.sqlite3` database:

- `task` classifies .NET types as `ported`, `ignore`/legacy `ignored`, or
  `tobedecided`.
- `ticket` tracks concrete stabilization work as `todo`, `doing`, `done`,
  `blocked`, `needs_user`, or `wontfix`.

Useful queries:

```bash
sqlite3 plan.sqlite3 \
  "SELECT status, COUNT(*) FROM task GROUP BY status ORDER BY status;"

sqlite3 plan.sqlite3 \
  "SELECT status, priority, COUNT(*) FROM ticket GROUP BY status, priority ORDER BY priority, status;"

sqlite3 plan.sqlite3 \
  "SELECT ticket_no, priority, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```

The database is not part of a fresh clone; these commands are for maintainers
who have the local planning database.

## API documentation

`Doxyfile` scans the module include trees and writes generated HTML under the
git-ignored `docs/generated/` directory:

```bash
mkdir -p docs/generated
doxygen Doxyfile
```

The existing codebase has a known documentation-warning backlog. New or
modified public APIs should not add warnings and must satisfy the doc-comment
rules in `CLAUDE.md`.

## License and attribution

Sharp Runtime is licensed under the [MIT License](LICENSE).

The API design and parts of the implementation are based on
[dotnet/runtime](https://github.com/dotnet/runtime), also under the MIT
License. Public names, signatures, namespace structure, enum values, and some
algorithms follow the .NET source; the C++ headers, implementations, build
system, and tests are maintained by Robert Vokac and contributors.

Vendored components keep their upstream licenses:

- [GoogleTest](https://github.com/google/googletest) — BSD 3-Clause.
- [nlohmann/json](https://github.com/nlohmann/json) — MIT.
- [tinyxml2](https://github.com/leethomason/tinyxml2) — zlib.
- [miniz](https://github.com/richgel999/miniz) — MIT.

ZLIB is discovered from the host only when `IO.Compression` is enabled. On
Android, `Storage` can privately use an SDL3 target supplied by its parent
project.
