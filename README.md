# Sharp Runtime

Sharp Runtime is a C++23 implementation of a practical subset of the .NET
`System.*` libraries. It provides familiar APIs for native game and framework
ports, especially CNA, without attempting to implement a CLR, JIT, garbage
collector, or the complete .NET platform.

The repository currently builds as 41 independently selectable CMake
components. The verified Linux baseline on 2026-07-29 is a warning-free build
with **14,002 passing tests across 37 test executables**. (This figure had been
stale at 13,538 for several remediation tickets; ticket #1802 corrected it to
13,790, ticket #1791 raised it to 13,840, ticket #1788 to 13,880, ticket #1789
to 13,923 — those last two measured from a fresh configuration and a
clean-first rebuild, which their object-layout changes made mandatory — ticket
#1805 to 13,937, ticket #1806 to 13,948, ticket #1807 to 13,958, ticket #1810
to 13,970, ticket #1811 to 13,979, ticket #1812 to 13,987, ticket #1814 to 13,994, and
ticket #1816 to the current value.)

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
cmake --build build --target SharpRuntimeTests --parallel 3
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
cmake --build build-no-tests --parallel 3
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
cmake --build build-json-tests --target SharpRuntimeTests --parallel 3
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
- `scripts/check_negative_consumer_fixtures.py` compiles every negative consumer
  fixture in `test/consumer/` **once per marked site** and requires each site to
  be rejected for its own declared reason. A fixture marks each site with
  `#if SHARP_RUNTIME_NEGATIVE_SITE == N` and a `// NEGATIVE(id): <expected
  diagnostic>` comment, and must compile cleanly with no site selected;
  `docs/NegativeConsumerFixtureValidation.md` is the full contract and
  `test/check_negative_consumer_fixtures_test.py` carries the checker's own
  fixtures.
- `.github/workflows/components.yml` runs the selective matrix and the full
  compatibility build on Ubuntu for pushes and pull requests.

At the current baseline the graph has **41 physical modules and 91 direct
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

### 2026-07-29 — `sizeof(System::Collections::BitArray::Enumerator)` grew from 32 to 40 bytes

**This is a binary-compatibility change only. No source changes, and a full
rebuild of every consumer is mandatory — the linker will not tell you if you
skip it, because not one mangled name changed.**

`BitArray`'s private mutation counter — the number its fail-fast
`GetEnumerator()` enumerator snapshots and compares — was 32 bits, and so was the
snapshot. After 2^32 effective mutations on one instance the counter returned to
a value an outstanding enumerator had captured and the equality guard silently
accepted that stale enumerator. Unlike `LinkedList<T>`'s, the consequence is a
**wrong answer rather than a use-after-free**: `BitArray::Enumerator` holds an
index that is bounds-checked against the current length on every step. It was
still a silent breach of the fail-fast contract, reachable in roughly **43
seconds** of hot mutation at ~10^8 mutations per second.

The counter and the enumerator's snapshot are now both 64-bit unsigned, so the
horizon is 2^64 — over 580 years of uninterrupted mutation of one instance. That
is a bound, not an impossibility, and it is not guarded by a per-mutation branch.
With this change **no collection in this library retains a 2^32 horizon.**

Every member of `BitArray` and `BitArray::Enumerator` keeps its exact signature,
return type and `const` qualification, so **ordinary source code needs no change
at all**:

```cpp
BitArray bits(64);                        // unchanged
bits.Set(3, true);
bits.SetAll(false);
bits.Not().And(other);
for (bool v : bits) { /* ... */ }
auto* e = bits.GetEnumerator();           // still IEnumerator*
```

What changed, measured on LP64:

| | Before | After |
|---|---:|---:|
| `sizeof(BitArray::Enumerator)` | 32 | **40** |
| `alignof(BitArray::Enumerator)` | 8 | 8 |
| `sizeof(BitArray)` | 48 | 48 |
| `alignof(BitArray)` | 8 | 8 |
| mangled symbols added, removed or renamed | — | **0** |

`sizeof(BitArray)` is unchanged because the counter grew into the four bytes of
tail padding the container already had. The **enumerator** could not absorb it:
its members were exactly packed — vptr (8) + array pointer (8) + snapshot (4) +
index (4) + cached bit (1) + state (4) = 32 — and nine bytes are needed after an
eight-byte snapshot where eight were available, in any member order.

`BitArray::Enumerator` is a **public nested class**, so a consumer may name one,
store one by value, or embed one in its own type. Every use inside this
repository hands it out as an `IEnumerator*` from `GetEnumerator()`, but yours
need not.

**Why you must rebuild everything.** A translation unit compiled against the old
header believes the enumerator is 32 bytes; one compiled against the new header
believes 40. Mixing them is an ODR violation, and it was reproduced in an
isolated probe: **both link orders link with zero diagnostics** — no error, no
warning, nothing. What follows depends on optimisation level and link order.
Embedding a `BitArray::Enumerator` by value in your own type silently corrupts
the member that follows it (a sentinel went from `0xFEEDFACECAFEBEED` to
`0xFEEDFACE00000002`) with **no AddressSanitizer report at all**, because the
corrupted bytes are inside the same allocation. At `-O2` an enumerator allocated
by old code and driven by new code reported **zero elements for a non-empty
array** — a silently wrong answer with no diagnostic — and under AddressSanitizer
the same case is a `new-delete-type-mismatch` ("allocated 32 bytes, deallocated
40") or an uncaught `ArgumentOutOfRangeException` abort. At `-O0` one link order
corrupts the neighbouring member and the other appears to work. Rebuild
everything.

The full record — reproductions, layout and symbol measurements, the
stale-object probe, alternatives, and performance — is section 20 of
`docs/CollectionVersionCounterSweep.md`, linked from the entry below.

### 2026-07-29 — `sizeof(System::Collections::Generic::LinkedList<T>)` grew from 40 to 48 bytes

**This is a binary-compatibility change only. No source changes, and a full
rebuild of every consumer is mandatory — the linker will not tell you if you
skip it, because not one mangled name changed.**

`LinkedList<T>`'s private mutation counter — the number its fail-fast
`GetEnumerator()` enumerator snapshots and compares — was 32 bits. After 2^32
effective mutations on one instance it returned to a value an outstanding
enumerator had captured, the equality guard silently accepted that stale
enumerator, and because the enumerator holds a raw pointer into node storage that
may since have been freed, the consequence was a potential **use-after-free**
rather than merely a wrong answer. At roughly 10^8 mutations per second the
horizon was about **43 seconds** of hot mutation.

The counter and the enumerator's snapshot are now both 64-bit unsigned, so the
horizon is 2^64 — over 580 years of uninterrupted mutation of one instance. That
is a bound, not an impossibility, and it is not guarded by a per-mutation branch.

Every member of `LinkedList<T>` and `LinkedListNode<T>` keeps its exact
signature, return type and `const` qualification, so **ordinary source code needs
no change at all**:

```cpp
LinkedList<int> list;                     // unchanged
LinkedListNode<int> node = list.AddLast(1);
list.AddAfter(node, 2);
for (int v : list) { /* ... */ }
auto* e = list.GetEnumerator();           // still IEnumerator<int>*
```

What changed, measured on LP64:

| | Before | After |
|---|---:|---:|
| `sizeof(LinkedList<T>)`, every `T` | 40 | **48** |
| `alignof(LinkedList<T>)` | 8 | 8 |
| `sizeof(LinkedList<T>::Enumerator)` | 40 | 40 |
| `sizeof(LinkedListNode<T>)`, `iterator`, `const_iterator` | 16 | 16 |
| mangled symbols added, removed or renamed | — | **0** |

`sizeof` grew because the members were exactly packed — `shared_ptr` (16) +
`weak_ptr` (16) + `int` count (4) + counter (4) = 40 with **no padding at all**,
so an 8-byte counter makes it 48 in any member order. The enumerator's snapshot
widened for free, into padding it already had.

**Why you must rebuild everything.** A translation unit compiled against the old
header believes the object is 40 bytes; one compiled against the new header
believes 48. Mixing them is an ODR violation, and it was reproduced in an
isolated probe: **both link orders link with zero diagnostics** — no error, no
warning, nothing from `-flto -Wodr` — and then behave completely differently.
With the new object file first, allocating a list in old code and using it from
new code is an AddressSanitizer **heap-buffer-overflow** and a **SEGV**;
embedding a `LinkedList<T>` by value in your own type silently corrupts the
member that follows it, with **no sanitizer report at all**, because the
corrupted bytes are inside the same allocation; and mutation invalidation is
**silently lost**, so a stale enumerator walks freed nodes without throwing. With
the old object file first, everything appears to work. Which of those you get
depends on link order. Rebuild everything.

`BitArray` kept its 32-bit counter and its 2^32 residual when this entry was
written; **that was closed the same day under its own separate approval — see the
entry above it.** The full record — reproductions, layout and symbol
measurements, the stale-object probe, alternatives, and performance — is in
[docs/CollectionVersionCounterSweep.md](docs/CollectionVersionCounterSweep.md).

### 2026-07-29 — the non-const `List<T>`/`IList<T>` indexer returns a tracked proxy, and the mutable `List<T>::ToVector()` is gone

**This is a source-breaking change, an interface change for every `IList<T>`
implementer, and an object-layout change to `ObjectModel::Collection<T>`. A full
rebuild of every consumer is mandatory, and the linker will not tell you if you
skip it — the mangled name of `operator[]` did not change even though its return
type did. A consumer that skips the rebuild does not crash; it silently loses
mutation tracking.** This was measured, not assumed: a stale object file linked
against a rebuilt program with no diagnostic at `-O0` and `-O2`, in both link
orders, read correct values, and quietly failed to invalidate an outstanding
enumerator. See `docs/ListIndexerVersioningDesign.md` §29.

Real .NET's `List<T>` index setter advances `_version` unconditionally
(`List.cs:161-162`), so `list[i] = value` fails an in-progress enumeration fast.
This port's `operator[]` returned a plain `T&`, and no C++ mechanism can notify a
container of a write through a reference it already handed out — so an indexed
write was invisible to the fail-fast guard, and a retained reference was a
reproduced use-after-free. The non-const indexer now returns
`System::Collections::detail::ElementReference<T>`: it reads as `const T&` and
routes every write back through the mutation counter.

What changed:

| Declaration | Before | After |
|---|---|---|
| `List<T>::operator[](intcs)` | `T&` | `detail::ElementReference<T>` |
| `IList<T>::operator[](intcs)` | `virtual T&` | `virtual detail::ElementReference<T>` |
| `IList<T>::getItem` / `setItem` | — | **new pure virtuals** |
| `List<T>::ToVector()` (non-const) | `std::vector<T>&` | **removed** |
| `sizeof(ObjectModel::Collection<T>)` | 32 | **40** (LP64) |
| `sizeof(List<T>)`, `sizeof(ReadOnlyCollection<T>)` | 40, 24 | unchanged |
| `operator[] const`, `ToVector() const`, `begin()`, `end()` | — | unchanged |

`Collection<T>` also gained a fail-fast enumerator. It previously version-checked
nothing at all — not even `Add()` — because it had no counter to check.

Migration:

| Was | Becomes |
|---|---|
| `list[i] = v;` | **unchanged** — this is the spelling the design exists to keep |
| `int x = list[i];` `const T& r = list[i];` | **unchanged** — reads are unaffected |
| `EXPECT_EQ(list[i], "abc")` | **unchanged** — the proxy has its own `operator==` |
| `T& r = list[i];` / `auto& r = list[i];` then `r = v;` | `list[i] = v;` or `list.setItem(i, v);` |
| `&list[i]` | `&*(list.begin() + i)` — explicitly opting into the unsafe surface |
| `list[i].member = v;` | `T c = list.getItem(i); c.member = v; list.setItem(i, c);` |
| `list[i].constMethod();` | `list.getItem(i).constMethod();` or `list[i]->constMethod()` |
| `std::swap(list[i], list[j]);` | `T t = list.getItem(i); list.setItem(i, list.getItem(j)); list.setItem(j, t);` |
| `f(list[i])` where `f` takes `T&` | `T c = list.getItem(i); f(c); list.setItem(i, c);` |
| `list.ToVector().push_back(v);` | `list.Add(v);` |
| `std::vector<T>& v = list.ToVector();` | `const std::vector<T>& v = list.ToVector();` or `std::vector<T> v = list.ToArray();` |
| `int* p = list.ToVector().data();` | `const int* p = list.ToVector().data();` |
| implementing `IList<T>` by hand | change the non-const `operator[]` return type, add `getItem`/`setItem`, and hold a `detail::MutationCounter` to hand the proxy |

`list[i].member` and `list[i].method()` are the unavoidable cost: `operator.`
cannot be overloaded in C++. C# rejects the same expression for a value-type
element (CS1612), so ported C# is not affected — only C++-idiomatic code is.

Two escapes remain, deliberately and documented rather than silently:
`begin()`/`end()` still yield a mutable `T&` whose writes are untracked (the
STL-interop surface, mirroring .NET's own `CollectionsMarshal.AsSpan` hatch), and
a *retained* proxy still aliases a slot across a structural mutation. Both are
pinned by permanent tests so they cannot be mistaken for closed.

### 2026-07-29 — `System::Collections::Hashtable::Remove` no longer invalidates enumerators for a key that was not there

**This one requires a full rebuild of every consumer, and the linker will not
tell you if you skip it — and like the entry below, a consumer that skips it does
not crash. It silently keeps the old behaviour.**

Nothing's signature changed. One behaviour did, and it is a *removal* of a
spurious exception.

All three `Remove` overloads — `Remove(const void*)`, `Remove(const std::string&)`
and `Remove(const char*)` — were `_map.erase(key); ++version_;`, so the fail-fast
mutation counter advanced whether or not the key was present. Removing a key that
was not in the table changed nothing, and then threw
`InvalidOperationException` out of **every** outstanding enumerator: the
`IDictionaryEnumerator`, the `Keys` view, the `Values` view, and the same reached
through an `IDictionary&`. A full walk after one absent `Remove` yielded **0 of 3**
entries; `Reset()` threw too. `Count` and the contents were correct throughout —
this was a false positive in the counter, not corruption.

`Remove` now advances the counter **only when an entry was actually erased**:

| Operation | Advances the counter? |
|---|---|
| `Remove` of a present key | yes — outstanding enumerators still fail fast |
| `Remove` of an absent key | **no** — outstanding enumerators stay valid |
| `Remove` of the same key twice | yes, then no |
| `Remove` rejected for a null key | no |
| `Clear`, even on an empty table | **yes, unconditionally** — see below |

This matches .NET `Hashtable.Remove`, which calls `UpdateVersion()` only inside
the branch that found and cleared a bucket. It also completes the rule the entry
below introduced on the other `IDictionary` implementation: **advance on
effective mutation**. Both of this port's non-generic dictionaries now answer
identically on every version row.

**What you may need to change.** Code that *relied* on an absent `Remove`
invalidating an enumerator — which would have been relying on a bug — no longer
gets the exception. Code that removes keys mid-enumeration and re-acquires the
enumerator on `InvalidOperationException` keeps working; it simply re-acquires
less often. Nothing that was correct before becomes incorrect.

**`Clear()` is deliberately unchanged, and deliberately unlike .NET.** It bumps
unconditionally, including on an already-empty table, where .NET
`Hashtable.Clear` early-returns. .NET's guard is `_count == 0 && _occupancy == 0`,
and `_occupancy` — a count of buckets whose collision bit was ever set — has no
`std::unordered_map` analogue, so the obvious `if (empty) return;` would *not*
reproduce .NET's rule. The unconditional bump also errs safely: it can only
invalidate an enumerator that had nothing to read. .NET
`ListDictionaryInternal.Clear` bumps unconditionally too.

**Rebuild — mandatory, and silent if skipped.** No signature, return type,
parameter type, vtable slot, calling convention, object size or mangled name
changed: the 19-entry vtable is byte-identical with `Remove` still at slot
`0x70`, `this` stays in `%rdi` with no hidden `sret`, the undefined-symbol list
is identical, and `sizeof(System::Collections::Hashtable)` is unchanged at **72**.
That is what makes it dangerous. Every affected body is `inline` in a header, so
a stale object file links with **zero diagnostics** and silently keeps the old
false positive — link-order dependent at `-O0` (a stale object first on the link
line drags correctly rebuilt translation units back with it) and per-translation-
unit at `-O2`. `-flto -Wodr` diagnoses nothing, because only an inline function
*body* differs. The linker cannot enforce this rebuild; only this note can.

Full record, measurements and rejected alternatives:
`docs/HashtableValueAccessSafetyDesign.md` §35.

### 2026-07-29 — `System::Collections::ListDictionaryInternal` rejects null keys and versions every effective mutation

**This one requires a full rebuild of every consumer, and the linker will not
tell you if you skip it — and unlike the three entries below, a consumer that
skips it does not crash. It silently keeps the old, defective behaviour. Read
the rebuild paragraph before upgrading.**

Nothing's signature changed. Three behaviours did.

**1. A null key now throws.** `getItem`, `setItem`, `Add`, `Contains` and
`Remove` all throw `System::ArgumentNullException` with parameter name `"key"`
before they look at storage. Previously all five accepted `nullptr`, and
`setItem`/`Add` **stored** it: a null key could be found, enumerated, copied out
and removed like any other. .NET begins each of the five with
`ArgumentNullException.ThrowIfNull(key)`, and this port's `Hashtable` has
rejected null since 2026-07-27, so the two implementations of one `IDictionary`
disagreed on every null-key row and no polymorphic consumer could predict which
answer it would get. Nothing legitimate is lost: keys here are compared by raw
address, no valid object has the null address, and a stored null key was
measured **not** to alias any real key. If you passed `nullptr` deliberately,
choose a real sentinel address.

Validation is now structural rather than conventional: a private `ValidatedKey`
is the only type the single internal locator accepts, and the only way to obtain
one is to pass the null check, so a future entry point cannot reach storage
without validating.

**2. Replacing a value invalidates outstanding enumerators.** `setItem` on a key
that is already present advances the fail-fast mutation counter, **including for
an equal-value replacement** — the value is never compared, because equality of
a `void*` is address equality and .NET compares neither. Previously the replace
branch returned before the counter moved, so four enumerator kinds walked to the
end after a replacement with no diagnostic at all: the dictionary's
`IDictionaryEnumerator`, the key view, the value view, and the same reached
through an `IDictionary&`. The value view — the surface whose entire content is
the thing that was replaced — **enumerated the post-mutation value**, and
AddressSanitizer and UndefinedBehaviorSanitizer both reported nothing. Code that
replaced a value mid-enumeration and kept going was reading post-mutation data
silently; it must now re-acquire the enumerator, as it already had to across an
insert.

**Deliberately NOT copied from .NET.** .NET `ListDictionaryInternal` does
`version++` **first and unconditionally**, before it even searches, so a
duplicate-key `Add` that throws and a `Remove` of an absent key both invalidate
every outstanding enumerator there. This port does neither, because that would
manufacture two new false-positive `InvalidOperationException`s out of calls that
changed nothing — and .NET's own `Hashtable` does neither either, so "match .NET"
is not a specification here. The rule is **advance on effective mutation**:

| Operation | Advances the counter? |
|---|---|
| `setItem` insert, replace, **and equal-value replace** | yes |
| `Add` of a new key | yes |
| `Add` of a duplicate (throws) | **no** — deliberate deviation from .NET |
| `Remove` of a present key | yes |
| `Remove` of an absent key | **no** — deliberate deviation from .NET |
| Any call rejected for a null key | **no** |
| `getItem` / `Contains` / `Count` / views | never |
| `Clear`, even on an empty dictionary | **yes, unconditionally** — matches .NET |
| copy, move and self assignment | yes, on the destination's own counter |

**3. The key view's `CopyTo` boxes `const void*`, not `void*`.** This is the
only change here that keeps compiling and changes meaning at run time.

```cpp
// Before                                        // After
std::any_cast<void*>(copiedKeys[0])              std::any_cast<const void*>(copiedKeys[0])
```

The old spelling still compiles and now throws `std::bad_any_cast`.
`MemberCollection::copyToCore` used to `const_cast<void*>` the key, so a single
view had **two incompatible element types** — its `Current` boxed `const void*`
while its `CopyTo` boxed `void*`, and `std::any_cast<const void*>` on a `CopyTo`
slot threw. Worse, the library, not the caller, manufactured a writable pointer
to an object the caller had declared `const`: writing through it was reproduced
as an **AddressSanitizer SEGV on a write to read-only storage**. One rule now
holds on every surface: a key is recovered with `std::any_cast<const void*>`, a
value with `std::any_cast<void*>`, and an entry with
`std::any_cast<DictionaryEntry>`. Value surfaces are unchanged.

Everything else is unchanged: `getItem` on an absent key still yields an empty
`std::any` and stays distinguishable from a key present with a null value;
`Count`, both views' liveness and ownership, enumerator ownership, and every
`CopyTo` validation rule are untouched.

**Rebuild — mandatory, and silent if skipped.** No signature, return type,
parameter type, vtable slot, calling convention or object size changed: 53 of 53
mangled names are byte-identical, the 19-entry vtable is identical, `this` stays
in `%rdi` with no hidden `sret`, and `sizeof(ListDictionaryInternal)` is
unchanged at **40**. That is precisely what makes this dangerous. Every affected
body is `inline` in a header, so a stale object file links with **zero
diagnostics** and then silently keeps the old behaviour — no crash, no warning,
no sanitizer report. The outcome is also **link-order dependent at both `-O0`
and `-O2`**: with a stale object first on the link line, a correctly *rebuilt*
translation unit reverts to the defective bodies. `-flto -Wodr` diagnoses
nothing, because every declaration and the class layout are identical and only
inline function *bodies* differ. The linker cannot enforce this rebuild; only
this note can.

Full design record, measurements, rejected alternatives and the implementation
record: `docs/ListDictionaryInternalSetterDesign.md`.

### 2026-07-28 — `System::Collections::Hashtable`'s value accessors return owning values

**This one requires a full rebuild of every consumer, and the linker will not
tell you if you skip it. Read the ABI paragraph below before upgrading.**

`Hashtable` had **four** routes by which a caller could obtain, or write
through, something that aliased live value storage:

- `void* getItem(const void*) const` — on the `IDictionary` interface, `const`,
  and it returned `const_cast<std::any*>(&it->second)`: a **writable pointer
  into the live `std::unordered_map`, handed out of a read accessor**. A caller
  holding even a `const IDictionary&` — the most restrictive reference the
  interface offered — could rewrite a stored value with the fail-fast mutation
  counter unmoved.
- `std::any& operator[](const std::string&)` — a mutable reference into storage,
  and, because it forwarded to `std::unordered_map::operator[]`, **a bare *read*
  of an absent key structurally inserted an entry**. That is the worst of the
  four and it produces no crash: at 4,008 entries an outstanding enumerator saw
  an unmoved counter, walked a rehashed bucket array, visited **2,045 distinct
  keys, reached only 6 of its 8 seed keys, threw nothing, and produced no
  AddressSanitizer, UndefinedBehaviorSanitizer or LeakSanitizer report at all**.
  Memory-safe and silently wrong.
- `const std::any& at(const std::string&) const` — a `const` alias to a
  **non-`const`** `std::any`, so `const_cast<std::any&>(h.at(k)) = v;` was not a
  trick and not undefined behaviour: it was well-formed, fully defined C++ that
  rewrote the dictionary with the counter unmoved. It also threw
  `std::out_of_range`, which `catch (const System::Exception&)` cannot see.
- Retained aliases from all three dangled after `Remove`, `Clear`, copy
  assignment, move assignment and destruction — **nine AddressSanitizer
  `heap-use-after-free` reports across fourteen scenarios**. (Rehash does *not*
  dangle: `std::unordered_map` is node-based, measured across 8,000 insertions.)

The new surface is **owning reads, tracked writes, and no public alias into
storage**:

| Member | Was | Now |
|---|---|---|
| `IDictionary::getItem(const void*) const` | `void*` into live storage | **`std::any` by value** |
| `Hashtable::operator[](const std::string&)` | `std::any&`, inserted on read | **`Hashtable::ValueReference` proxy** |
| `Hashtable::operator[](const std::string&) const` | *did not exist* | **`std::any` by value** |
| `Hashtable::at(const std::string&) const` | `const std::any&`, `std::out_of_range` | **`std::any` by value, `KeyNotFoundException`** |
| `Hashtable::setItem(const std::string&, const std::any&)` | *did not exist* | **new typed tracked setter** |
| `setItem`/`Add` raw-key `void*` *value* parameter | — | **unchanged, deliberately** (see below) |

```cpp
// Before                                   // After
std::any& r = table[key]; r = value;        table[key] = value;   // one tracked write
void* raw = table.getItem(key);             std::any v = table.getItem(key);
const std::any& v = table.at(key);          std::any v = table.at(key);
const_cast<std::any&>(table.at(key)) = v;   // no replacement — that is the point
```

Migration, by shape:

| Was | Becomes |
|---|---|
| `*static_cast<std::any*>(d.getItem(k))` | `d.getItem(k)` |
| `d.getItem(k) != nullptr` | `d.getItem(k).has_value()` — or `d.Contains(k)` |
| `std::any& r = table[key]; r = value;` | `table[key] = value;` |
| `auto& r = table.at(k);` | `auto r = table.at(k);` |
| `catch (const std::out_of_range&)` around `at` | `catch (const System::Collections::Generic::KeyNotFoundException&)` |
| `&table[key]`, `std::any_cast<T&>(table[key])` | no replacement — use the mutating API so the counter advances |
| implementing `IDictionary` by hand | change `getItem`'s return type; `void*` is **not** a covariant return for `std::any`, so it cannot compile lazily |

**A caller that only reads and never aliases needs no source change at all.**

Behaviour changes to know about:

- **A read no longer inserts.** `table[missing]` yields an empty `std::any` and
  leaves `Count`, the mutation counter, both views and every outstanding
  enumerator untouched. This matches .NET, whose getter returns `null` and
  inserts nothing.
- **An absent key and a present-but-empty value both read as an empty
  `std::any`** — again .NET parity, which is why .NET also has `ContainsKey`.
  Use `ContainsKey()`/`Contains()`, or the proxy's `hasValueProperty()`, to tell
  them apart; `at()` is the throwing read.
- **Every write through the indexer advances the counter, including an
  equal-value replacement**, because .NET's `Hashtable.Insert` calls
  `UpdateVersion()` on both branches and never compares the old value. This is
  deliberately the *opposite* of `Generic::Dictionary`'s rule in this same
  component, and both match their own .NET reference.
- **`const std::any& r = table[key];` keeps compiling and changes meaning
  silently.** It now binds a lifetime-extended **temporary** — a snapshot, not a
  live view. It is memory-safe, and it is the *only* silent meaning change here;
  every other one is a compile error. **Do not write it**; write
  `std::any r = table[key];`.
- **Returned values are snapshots.** They survive `Remove`, `Clear`, assignment
  and the table's destruction. For a pointer-valued entry the copy owns the
  *pointer*, not the pointee: mutating the pointee is still not a dictionary
  mutation and still does not bump, while replacing the stored pointer is one
  and now does.
- **`setItem`/`Add`'s raw-key `void*` *value* parameter is deliberately
  unchanged.** Migrating it to `const std::any&` makes the raw-address overload
  viable for `Add("literal", v)`; the standard `const char*` → `const void*`
  conversion then beats the user-defined `const char*` → `std::string` one, and
  the entry silently lands under the **stringified address of the literal** —
  measured, and clean under `-Wall -Wextra -Wpedantic -Werror`. Use the typed
  `setItem(const std::string&, const std::any&)` instead.

**The proxy is non-copyable, and that is load-bearing rather than stylistic.**
`std::any`'s template converting constructor `any(T&&)` is constrained only on
`is_copy_constructible_v<decay_t<T>>`, so with a *copyable* proxy
`std::any b = table[key];` would prefer that constructor over the proxy's own
conversion operator: `b` would silently hold a `ValueReference`, and the next
`std::any_cast` would throw `std::bad_any_cast` **at run time with nothing wrong
at compile time**. Its read conversion returns `std::any` **by value** for a
second measured reason: a conversion returning `const std::any&` trips GCC 14's
`-Wdangling-reference` on the ordinary read spelling, and every module here
builds with `-Werror`.

**ABI — a full consumer rebuild is mandatory.** Under the Itanium C++ ABI a
non-template function's return type is **not** part of its mangled name, so the
caller symbol for `IDictionary::getItem` is **byte-identical** before and after,
and its vtable slot is unchanged at `0x38` — both re-measured on the real
production headers. The calling convention is *not* the same: `std::any` is
neither trivially copyable nor trivially destructible, so it is returned through
a hidden `sret` pointer and `this` moves from `%rdi` to `%rsi`. A translation
unit compiled against the old header and linked against a library built with the
new one **links with zero diagnostics (`exit=0`) and then segfaults
(`exit=139`)**, with UndefinedBehaviorSanitizer emitting 14 diagnostics first,
beginning `member access within misaligned address … for type 'const struct
Hashtable'` — the callee using the caller's *key* pointer as `this`. The linker
cannot enforce this rebuild; only this note can.

`sizeof(Hashtable)` is **unchanged at 72** and `sizeof(ListDictionaryInternal)`
at **40**, so this is not an object-layout break. `Hashtable::ValueReference` is
40 bytes and is never stored by the collection.

`ListDictionaryInternal::getItem` was migrated **mechanically** — it still boxes
the caller's own pointer, recovered with `std::any_cast<void*>`. Its other
divergences (its `setItem` skips the version bump on the replace branch, and
both its accessors accept a null key where .NET and `Hashtable` throw) are
**not** fixed here and remain open as a separate ticket. *(Those were closed the
following day; see the 2026-07-29 entry above, which also corrects the count —
there were six divergences, not two.)*

Full design record, measurements and rejected alternatives:
`docs/HashtableValueAccessSafetyDesign.md`.

### 2026-07-28 — `System::Collections::IDictionaryEnumerator`'s `Key` and `Value` return `std::any`

**This one requires a full rebuild of every consumer, and the linker will not
tell you if you skip it — through *two* independent mechanisms. Read the ABI
paragraph below before upgrading.**

`getKeyProperty()` and `getValueProperty()` used to return `const void*`. They
were const-qualified, which made them look safe, and they were not:

- `Hashtable::Enumerator::getValueProperty()` returned a pointer to the live
  `std::unordered_map`'s `mapped_type`, which is a **non-`const` `std::any`**.
  `const_cast` plus assignment through it was not a trick and not undefined
  behaviour — it was well-formed, fully defined C++ that **rewrote live
  dictionary storage**, left the mutation counter unmoved, and was invisible to
  a second, outstanding enumerator.
- The key path reached the `const std::string` map key, where the same write
  *is* undefined behaviour. At 64 entries it left an entry that `Count` still
  reported and that **no lookup could return by either its old or its new key**.
- Neither accessor version-checks, so an accessor called after a mutation
  dereferenced an invalidated container iterator. On `ListDictionaryInternal`,
  which cached nothing, that reached **all four** accessors — including
  `getEntryProperty()` and the already-owning `getCurrentProperty()`. Nine
  AddressSanitizer `heap-use-after-free` reports, three of which needed no
  caller misuse at all.

Both now return an **owning `std::any` by value**, the direct C++ counterpart of
.NET's `object Key` / `object? Value`. `getEntryProperty()` is unchanged and is
now the canonical representation: `Key` and `Value` are exactly that entry's
members, and `getCurrentProperty()` is exactly that entry, boxed.

```cpp
// Before
const void* raw = e->getKeyProperty();
const std::string& key = *static_cast<const std::string*>(raw);
*const_cast<std::any*>(                                   // ... and this rewrote
    static_cast<const std::any*>(e->getValueProperty())) = v;   // the dictionary

// After
std::any boxedKey = e->getKeyProperty();
const std::string& key = std::any_cast<const std::string&>(boxedKey);
// There is no replacement for the write, and that is the point: use the
// dictionary's own mutating API (setItem, Add) so the mutation counter advances
// and outstanding enumerators fail fast.
```

Migration, by shape:

| Was | Becomes |
|---|---|
| `const std::string* k = static_cast<const std::string*>(e->getKeyProperty());` | `auto k = std::any_cast<std::string>(e->getKeyProperty());` |
| `const std::any* v = static_cast<const std::any*>(e->getValueProperty());` | `std::any v = e->getValueProperty();` — already the payload, not a nested box |
| `const void* k = e->getKeyProperty();` *(`ListDictionaryInternal`)* | `auto k = std::any_cast<const void*>(e->getKeyProperty());` |
| `void* v = const_cast<void*>(e->getValueProperty());` | `auto v = std::any_cast<void*>(e->getValueProperty());` |
| `if (e->getKeyProperty() != nullptr)` | `std::any_cast<const void*>(e->getKeyProperty()) != nullptr` |
| any write through a `const_cast` of either result | no replacement — see above. `const_cast` cannot turn a `std::any` into a pointer, so there is no "just add a cast" path |
| implementing `IDictionaryEnumerator` by hand | change both return types, **and** snapshot the entry in `MoveNext()` — see below |
| reading all three of `Entry`, `Key`, `Value` | read `getEntryProperty()` once; three accessors cost three copies |

**Two `ListDictionaryInternal` behaviour changes**, both .NET parity corrections:

- `getCurrentProperty()` now boxes the `DictionaryEntry` instead of the key
  alone, matching .NET's `public object Current => Entry;` and what
  `Hashtable::Enumerator` already did. `getCurrentProperty()`'s signature and
  ownership contract are unchanged; one implementation's *payload* became
  correct.
- The **value view**'s element changes from `const void*` to `void*`, agreeing
  with `DictionaryEntry::Value` and `copyToCore`, where it previously agreed
  with neither. The **key** view is unchanged at `const void*`.

Three further consequences:

- **The returned box is yours.** Nothing invalidates it — not `MoveNext()`, not
  `Reset()`, not mutating the dictionary, not `Clear()`, not destroying the
  enumerator, not destroying the dictionary. Writing to it cannot reach the
  dictionary, and reading it never advances any mutation counter. If the boxed
  value is itself a handle (`ListDictionaryInternal`'s `void*`, or a
  `std::shared_ptr<X>` value in a `Hashtable`), the box owns the *handle*, not
  the pointee: mutating the pointee is still not mutating the dictionary, and
  replacing the handle inside the box replaces no entry. These are pointer
  semantics, not a deep copy.
- **The boxed type differs between implementations, and is now discoverable.**
  `Hashtable` boxes `std::string` keys and the stored value's own payload
  (never a `std::any` nested inside another `std::any`);
  `ListDictionaryInternal` boxes `const void*` keys and `void*` values. Code
  written generically must branch on `std::any::type()` or use
  `getEntryProperty()`. That was always true; it was previously undiagnosable,
  and a same-width wrong cast through the old `const void*` was silently wrong
  against one implementation and an AddressSanitizer stack-buffer-overflow
  against the other. A wrong `std::any_cast` now throws `std::bad_any_cast`.
- **The accessors deliberately did *not* gain a version check.** .NET's do not
  either. The `MoveNext()`-time snapshot makes a post-mutation read *safe*
  rather than turning a read .NET permits into an exception. `MoveNext()` and
  `Reset()` keep their fail-fast check unchanged. Calling `MoveNext()` or
  `Reset()` after the dictionary itself is destroyed remains undefined; this
  change does not close that, and does not claim to.

**Implementing the interface by hand: the return types are only half of it.**
After `MoveNext()` returns `true`, an implementation must be able to answer
every accessor from state the enumerator itself *owns*. Reading container
storage inside an accessor is a defect even with the right signature, because no
accessor performs the fail-fast version check. .NET's own `HashtableEnumerator`
snapshots the key and value into enumerator fields at `MoveNext()` time and
reads `_buckets` from no accessor.

**ABI — a full consumer rebuild is mandatory, for two independent reasons.**
First, under the Itanium C++ ABI a non-template function's return type is **not**
part of its mangled name, so `_ZNK…14getKeyPropertyEv` and
`_ZNK…16getValuePropertyEv` are byte-identical before and after and both keep
their vtable slots (offsets `0x30` and `0x38`, re-measured on the real headers).
The calling convention is *not* the same: `std::any` is returned through a hidden
sret buffer, so `this` moves from `%rdi` to `%rsi`. A translation unit compiled
against the old header and linked against a library built with the new one
**links with zero diagnostics and then corrupts memory** — reproduced as a SEGV,
with UndefinedBehaviorSanitizer reporting an invalid vptr and a bogus
`System::InvalidOperationException` raised out of garbage, which a consumer
catching `System::Exception&` would log as "enumeration not started" and carry
on. Second, `ListDictionaryInternal::NodeEnumerator` grows **40 → 72 bytes** to
hold its snapshot while `GetEnumerator()` stays `inline` in the public header, so
a stale consumer's own object file allocates the old size for the new object —
reproduced as links-clean-then-AddressSanitizer-`heap-use-after-free`. No tool in
the toolchain detects either at link time. Rebuild everything.

`NodeEnumerator` is a **private nested class**, so no consumer can name,
allocate, embed, or derive from it: this is not a *public* object-layout change.
`sizeof`/`alignof` of `IDictionaryEnumerator`, `DictionaryEntry`, `Hashtable`,
`Hashtable::Enumerator`, and `ListDictionaryInternal` are all unchanged. The cost
is one allocation class on the type-erased read: 0 for `ListDictionaryInternal`
keys and values and for a small `Hashtable` value, 1 for a short `Hashtable`
string key and a `std::shared_ptr` value, 2 for a heap `std::string` key or
value; `Entry` and `Current` are unchanged. Measured worst case 2.4 → 15.6 ns per
read. `ListDictionaryInternal::MoveNext()` also became more expensive (2.8 →
23.9 ns per position) because it now builds the snapshot it previously did not
have; `Hashtable::MoveNext()` is unchanged, having always snapshotted. The full
record — inventory, reproductions, alternatives analysis, .NET comparison, ABI
and layout measurements — is in
[docs/IDictionaryEnumeratorKeyValueSafetyDesign.md](docs/IDictionaryEnumeratorKeyValueSafetyDesign.md).

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
`getValueProperty()` were deliberately left at `const void*` here and migrated
separately, by the entry above. The full record — inventory, reproductions, alternatives
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

Two documented residuals remained at the time, both blocked on an explicit
object-size approval: `LinkedList<T>` and `BitArray` kept a 32-bit counter, so a
stale enumerator over either could still be revalidated after 2^32 effective
mutations on one instance. **Both were closed on 2026-07-29, under two separate
approvals, and both are entries at the top of this section — that is where the
mandatory rebuild comes from.** `LinkedList<T>`'s cost `sizeof(LinkedList<T>)`
40 → 48; `BitArray`'s cost `sizeof(BitArray::Enumerator)` 32 → 40. No collection
in this library retains a 2^32 horizon; every one is 2^64. The full record —
inventory, reproductions, .NET comparison, layout measurements, and both designs
— is in
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
