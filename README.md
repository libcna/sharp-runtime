# Sharp Runtime

Sharp Runtime is a C++23 implementation of a practical subset of the .NET
`System.*` libraries. It provides familiar APIs for native game and framework
ports, especially CNA, without attempting to implement a CLR, JIT, garbage
collector, or the complete .NET platform.

The repository currently builds as 41 independently selectable CMake
components. The verified Linux baseline on 2026-07-27 is a warning-free build
with **12,921 passing tests across 37 test executables**.

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
baseline, `Text.Json` enables only `Core.Base`, `Buffers`, `Text`,
`Collections.Core`, and `Text.Json`. `BlockingCollection<T>` lives in the
separate `Collections.Blocking` component, so its `Threading`/`TimeZone`
requirements do not broaden ordinary collections consumers. Networking, XML,
ZLIB, miniz, tinyxml2, and SDL remain outside this closure.

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
  `Collections.Blocking`, `Collections.Async`, and `Collections.ObjectModel`.
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
- `scripts/check_selective_components.sh` defines ten isolated positive
  consumers and negative leakage fixtures.
- `.github/workflows/components.yml` runs the selective matrix and the full
  compatibility build on Ubuntu for pushes and pull requests.

At the current baseline the graph has **41 physical modules and 90 direct
production dependency edges**, with no allow-listed exception. The boundary
validator, the complete ten-job selective matrix, and the full build/test gate
pass. The Text.Json negative assertion confirms that the target does not
configure `Threading` or `TimeZone`.

## Platform status

The complete build and test baseline is currently verified on Linux with GCC.
Other platform evidence is narrower:

| Platform/toolchain | Verified scope |
|---|---|
| Linux/GCC | Current full component build and all 12,921 tests. |
| Windows/MinGW | MinGW-w64 GCC 14-win32/CMake 3.31.6 compiled the post-component `All` and selective `Text.Json` library graphs under ticket #1741. GoogleTest was not cross-built and repository CI remains Ubuntu-only. |
| Emscripten | Emscripten 5.0.7/CMake 3.31.6 compiled the post-component `All` and selective `Text.Json` library graphs under ticket #1741. Tests were not cross-built or run, and some runtime APIs deliberately throw `PlatformNotSupportedException`. |
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

## Breaking changes

### 2026-07-27 — `System::Collections::ICollection::CopyTo`

`virtual void CopyTo(void* array, intcs index) = 0;` has been **removed** from
the non-generic `ICollection`. A raw pointer carries no destination element
type, element count, element size, alignment, or construction state, and the six
implementations each cast it to a different element type, so no `ICollection*`
caller could allocate a correct destination. Copying now goes through a
length-aware, statically typed destination:

```cpp
// Before
void* storage = ...;
collection.CopyTo(storage, index);

// After — interface level
std::vector<std::any> destination(collection.getCountProperty());
collection.CopyTo(destination, 0);
int value = std::any_cast<int>(destination[0]);

// After — concrete typed overloads
std::vector<void*>          queueDestination(queue.getCountProperty());
queue.CopyTo(queueDestination, 0);            // also Stack
std::vector<DictionaryEntry> tableDestination(table.getCountProperty());
table.CopyTo(tableDestination, 0);            // also ListDictionaryInternal
std::vector<std::any>        listDestination(list.getCountProperty());
list.CopyTo(listDestination, 0);              // ArrayList stores std::any already
```

This is **source-breaking and ABI-breaking**: removing a pure virtual member
changes the vtable of `ICollection`, `IList`, and `IDictionary`, so **all C++
consumers must be rebuilt**. No deprecated compatibility overload was retained —
one that only threw would let stale call sites compile and fail at run time,
whereas removal turns each into a compile error that names the replacement, so
callers should migrate by following the compiler's `note: candidate:` lines.

Full guidance, including how to migrate a class that implemented the interface
and what each collection puts in a destination slot, is in
[docs/Migration-ICollectionCopyTo.md](docs/Migration-ICollectionCopyTo.md).
Downstream consumers such as CNA and mobile-eggbert are outside this repository
and have not been checked; §9 of that document lists what each of them needs to
do.

**Follow-up correction (2026-07-27, ticket #1774):** the initial landing
rejected every zero-length destination with a null pointer, including a valid
empty `ObjectSpan{nullptr, 0}` or a default-constructed empty
`std::vector<std::any>` copied from an empty collection. That is corrected: a
null pointer paired with a zero length is now a valid empty destination; only
a null pointer paired with a *positive* length is rejected. A non-empty
collection copied into a zero-length destination still fails, but on capacity
(`ArgumentException`), not nullness. See `docs/Migration-ICollectionCopyTo.md`
§7 (linked above).

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

The tracked Doxygen 1.9.8 baseline is **1,942 warnings**, measured on
2026-07-25. Check it before submitting public-API documentation changes:

```bash
scripts/check_doxygen_warnings.sh
```

The check permits fewer warnings but fails if the total increases. Warning
totals are Doxygen-version-sensitive, so deliberately re-establish the
baseline when upgrading Doxygen. Do not mass-rewrite comments just to reduce
the number; new or modified public APIs must satisfy the doc-comment rules in
`CLAUDE.md`.

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
