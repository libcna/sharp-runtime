# Sharp Runtime

Sharp Runtime is a C++23 implementation of a practical subset of the .NET
`System.*` libraries. It provides familiar APIs for native game and framework
ports, especially CNA, without attempting to implement a CLR, JIT, garbage
collector, or the complete .NET platform.

The repository currently builds as 41 independently selectable CMake
components. The verified Linux baseline on 2026-07-28 is a warning-free build
with **13,538 passing tests across 37 test executables**.

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
| Linux/GCC | Current full component build and all 13,538 tests. |
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

### 2026-07-28 — `System::Collections::IEnumerator::getCurrentProperty()` returns `std::any`

**This one requires a full rebuild of every consumer, and the linker will not
tell you if you skip it. Read the ABI paragraph below before upgrading.**

The non-generic enumerator accessor used to return a mutable `void*`. The
generic bridge filled it with `const_cast<T*>(&Current())`, so a consumer
holding nothing but the public non-generic interface could obtain a **writable,
untyped, unbounded-lifetime pointer into the live storage of the collection it
was walking** — including collections whose own members refuse to be mutated.
Writing through it changed the element while the owning collection's mutation
counter stayed at rest, so every outstanding enumerator remained valid and
silently observed the new value.

It now returns an **owning `std::any` by value**, the direct C++ counterpart of
.NET's `object IEnumerator.Current`, which returns a value, boxes value types,
and hands out no pointer at all. The typed
`Generic::IEnumerator<T>::Current()` is **unchanged** at `const T&`.

```cpp
// Before
void* raw = e->getCurrentProperty();
int   value = *static_cast<int*>(raw);
*static_cast<int*>(raw) = 5;              // ... and this reached the collection

// After
std::any boxed = e->getCurrentProperty();
int      value = std::any_cast<int>(boxed);
// There is no replacement for the write, and that is the point: use the
// collection's own setter (setItem, operator[], Insert) so the mutation
// counter advances and outstanding enumerators fail fast.
```

Migration, by shape:

| Was | Becomes |
|---|---|
| `T* p = static_cast<T*>(e->getCurrentProperty());` | `T v = std::any_cast<T>(e->getCurrentProperty());` |
| `*static_cast<T*>(e->getCurrentProperty())` | `std::any_cast<T>(e->getCurrentProperty())` |
| `if (e->getCurrentProperty() == nullptr)` | `if (!e->getCurrentProperty().has_value())` |
| `void* raw = …` on the non-generic `Stack`/`Queue`, whose element **is** a `void*` | `std::any_cast<void*>(e->getCurrentProperty())` |
| `std::any_cast<int>(*static_cast<std::any*>(…))` on an already-boxed element | `std::any_cast<int>(e->getCurrentProperty())` — one unwrapping disappears |
| `*static_cast<T*>(e->getCurrentProperty()) = v;` (a **write**) | no replacement — see above |
| keeping the pointer past `MoveNext()` | keep the `std::any`; it owns its value and never dangles |
| implementing `IEnumerator` by hand | change the return type; `return std::any(value);` |
| implementing `IEnumerator<T>` by hand | **nothing** — the boxing bridge is inherited |

Three further consequences:

- **The returned box is yours.** Nothing invalidates it — not `MoveNext()`, not
  `Reset()`, not mutating the collection, not destroying the enumerator, not
  destroying the collection. Writing to it cannot reach the collection, and
  reading it never advances any mutation counter. If the element type has shared
  reference semantics of its own (`std::shared_ptr<X>`), the box copies the
  *handle*, so mutating the pointee is still not mutating the collection —
  exactly .NET's reference-type behaviour.
- **A wrong cast is now diagnosed.** `std::any_cast<T>` throws
  `std::bad_any_cast` — a `std::` exception, not a `System::` one, consistent
  with how this port already exposes `std::any` — instead of silently
  reinterpreting bytes, which a same-width `static_cast` through the old `void*`
  did with no diagnostic from any sanitizer.
- **A non-copyable element type loses this path only.** If `T` is not
  copy-constructible it cannot be boxed, so `getCurrentProperty()` throws
  `System::NotSupportedException("The element type cannot be boxed; use the
  typed Current() accessor.")`. This mirrors .NET's own documented answer for a
  `ref struct` element type. `Current()`, `MoveNext()`, and `Reset()` all keep
  working, and the position check still runs first, so a before-start or
  after-end read still reports `InvalidOperationException`.

**ABI — a full consumer rebuild is mandatory.** Under the Itanium C++ ABI a
non-template function's return type is **not** part of its mangled name, so the
symbol `_ZNK…18getCurrentPropertyEv` is byte-identical before and after, the
vtable is identically named, and the accessor stays in the same vtable slot
(offset `0x20`). The calling convention is *not* the same: `std::any` is
returned through a hidden sret buffer, so `this` moves from `%rdi` to `%rsi`.
A translation unit compiled against the old header and linked against a library
built with the new one **links with zero diagnostics and then corrupts memory**;
that was reproduced in an isolated probe, where the mismatched call took a SEGV
with UndefinedBehaviorSanitizer reporting an invalid vptr. No tool in the
toolchain detects this at link time. Rebuild everything.

There is **no object-layout change**: `sizeof`/`alignof` of `IEnumerator`,
`Generic::IEnumerator<T>`, and every affected collection are identical before
and after, re-measured against the stored baseline. The cost is one allocation
class on the type-erased read only — 0 for `int`, a raw pointer, and an already
boxed `int`; 1 for a small `std::string` and a `std::shared_ptr`; 2 for a large
`std::string` and a `DictionaryEntry`. The typed `Current()` path is unchanged
and still allocation-free. `IDictionaryEnumerator`'s `getKeyProperty()` and
`getValueProperty()` deliberately still return `const void*` and are a separate,
recorded follow-on. The full record — inventory, reproductions, alternatives
analysis, .NET comparison, and ABI measurements — is in
[docs/IEnumeratorCurrentSafetyDesign.md](docs/IEnumeratorCurrentSafetyDesign.md).

### 2026-07-28 — assigning a collection now invalidates its outstanding enumerators

Fifteen collections carry a private mutation counter that their fail-fast
enumerators snapshot: `List<T>`, `HashSet<T>`, `Dictionary<K,V>`,
`SortedDictionary<K,V>`, `SortedList<K,V>`, `OrderedDictionary<K,V>`,
`LinkedList<T>`, `Queue<T>`, `Stack<T>`, `ArrayList`, `Hashtable`,
`ListDictionaryInternal`, the non-generic `Queue` and `Stack`, and `BitArray`.
Fourteen of them used the implicitly declared copy/move assignment operator,
which copied the **source's** counter into the destination — so an enumerator
outstanding over the destination saw no change even though the assignment had
just destroyed every element it could refer to. For the node-based containers
that was a confirmed use-after-free, not merely a wrong answer.

Assignment now advances the destination's own counter instead:

```cpp
Dictionary<int, int> a; a.Add(1, 1);
Dictionary<int, int> b; b.Add(7, 7);

auto it = a.begin();
a = b;                 // every element `it` referred to is destroyed here
// Before — `it` often compared "unmodified" and read freed memory.
// After  — *it throws InvalidOperationException, as the contract always intended.
```

Self-assignment (`c = c`) also advances the counter on those fourteen, which is
deliberate: member-wise self-assignment of the backing `std::unordered_map` or
`std::unordered_set` may reallocate the nodes an outstanding iterator points at.
`LinkedList<T>` already declared its own guarded assignment operators and keeps
its existing behaviour — self-assignment there changes nothing and invalidates
nothing.

Only source **behaviour** changes, and only for a program that keeps enumerating
a collection after that collection was wholesale replaced — which is precisely
what the fail-fast contract says must throw. No signature, return type,
`const` qualification, `sizeof`, `alignof`, or mangled symbol changes, and **no
consumer rebuild is required on this revision's account**. The same change
replaced every counter's signed 32-bit representation with an unsigned one
(64-bit for thirteen of the fifteen), removing fourteen instances of
signed-integer overflow; that part is invisible to any conforming program.

Two documented residuals remain, both blocked on an explicit object-size
approval: `LinkedList<T>` and `BitArray` keep a 32-bit counter, so a stale
enumerator over either can still be revalidated after 2^32 effective mutations
on one instance. The full record — inventory, reproductions, .NET comparison,
layout measurements, and the two blocked designs — is in
[docs/CollectionVersionCounterSweep.md](docs/CollectionVersionCounterSweep.md).

### 2026-07-28 — `System::Collections::Generic::SortedSet<T>::GetViewBetween`

`GetViewBetween` used to return an independent snapshot copy and was `const`. It
now returns a **live, bounded, bidirectionally write-through view** over the same
underlying tree, matching .NET's `TreeSubSet`, and it is **no longer `const`**.

```cpp
SortedSet<int> set{1, 2, 3, 4, 5};

// Before — the result was detached; the source never changed.
// After  — the result is a live handle; in-range mutations write through.
SortedSet<int> view = set.GetViewBetween(2, 4);
view.Add(3);          // now also visible in `set`
view.Remove(2);       // now also removed from `set`
view.Clear();         // now removes exactly [2, 4] from `set`
view.Add(99);         // now throws ArgumentOutOfRangeException("item")

// To keep the old detached behavior deliberately:
SortedSet<int> snapshot = set.GetViewBetween(2, 4).ToSortedSet();

// A const set can no longer produce a view — take a non-const reference, or
// copy the set first. Never const_cast: the qualifier is what documents that no
// mutable path into the set exists.
const SortedSet<int>& frozen = set;
// frozen.GetViewBetween(2, 4);        // compile error since this revision
SortedSet<int> ownCopy = set;          // copying a full set is still a deep clone
SortedSet<int> ownView = ownCopy.GetViewBetween(2, 4);
```

Three separate compatibility layers change, and returning the same public type
does **not** mean there is no impact:

- **Source** — only `const` callers break, as a compile error naming the
  non-`const` member. Every non-`const` call still compiles.
- **Semantics** — the silent one. Code that mutated the result expecting the
  source to be unaffected compiles unchanged and now behaves differently. Audit
  every call site for a snapshot assumption; `ToSortedSet()` is the replacement.
  Copying the result copies the *handle*, not the elements.
- **Binary** — the mangled name changes (`_ZNK…` → `_ZN…`, since the Itanium ABI
  encodes the implicit object parameter's `const`), and the object layout changes
  (`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
  `sizeof(SortedSet<int>::Iterator)` 24 → 40). Object files compiled against the
  old header are layout-incompatible with new ones, with no diagnostic, so **all
  C++ consumers must be fully rebuilt** when they adopt this revision.

Enumerating a view now fail-fasts with `InvalidOperationException` when the
source is mutated, and vice versa — one version counter is shared by a set and
all of its views, matching .NET. A view keeps the underlying elements alive, so
it stays valid after the set it came from is destroyed, copied, moved, or
reassigned.

The full contract, the alternatives considered, and the measured before/after
evidence are in
[docs/SortedSetLiveViewDesign.md](docs/SortedSetLiveViewDesign.md).
Downstream consumers such as CNA and mobile-eggbert are outside this repository
and have not been checked; they remain on an older revision and must perform a
full rebuild and a `GetViewBetween` call-site audit when they upgrade.

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
