# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-21 (branch: develop) — 6412 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — driven by a `plan.md` namespace review workflow. The user provides types one at a time; Claude ports, tests, and commits. Currently working through `System.Collections.*`.
- **Header count:** ~567 `.hpp` files across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`, `System.Buffers`, etc.
- **Key architectural decisions:** No runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (int32_t), `bytecs` (uint8_t), etc.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **6412 tests passing** across 658 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int32`, `Int64`, `UInt16`, `UInt64`, `Int128`, `UInt128`, `Half`, `Single`, `Double`, `Decimal` (+ OACurrency), `Guid`, `BitConverter`, `Math` (full overloads + BigMul/DivRem/ILogB), `MathF`, `Random`, `HashCode`, `Void`
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy including inner-exception ctors and `/** */` Doxygen
- Collections (non-generic): `ArrayList` (full API: Sort/BinarySearch/GetRange/Clone/Repeat/IndexOf overloads), `BitArray` (full API: LeftShift/RightShift/PopCount/Clone/GetEnumerator), `Hashtable`, `Queue`, `Stack`, `Comparer`, `IList`, `ICollection` (+ SyncRoot), `IComparer`, `IEnumerator`, `IDictionaryEnumerator`, `IEqualityComparer`, `IStructuralComparable`, `IStructuralEquatable`
- Collections (generic): `List<T>`, `Dictionary<K,V>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList<K,V>`, `SortedDictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, `ReadOnlyCollection<T>`, `ArraySegment<T>`, `PriorityQueue<T,P>`, `ImmutableArray/List/Dictionary/HashSet/Queue/Stack/SortedDictionary/SortedSet<T>`
- Span/Memory: `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`, `MemoryExtensions` (full), `SpanSplitEnumerator`
- Buffers: `ArrayPool<T>` (+ Create overloads), `MemoryPool<T>`, `MemoryHandle`, `IPinnable`, `MemoryManager<T>`, `IBufferWriter<T>`, `ArrayBufferWriter<T>`, `SearchValues<T>`, `SequencePosition`, `ReadOnlySequence<T>` (+ CopyTo/ToArray/IsSingleSegment), `ReadOnlySequenceSegment<T>`, `SequenceReader<T>` (+ TryPeek/TryReadTo/AdvancePast/IsNext), `SequenceReaderExtensions`, `BinaryPrimitives` (full endian swap), `BuffersExtensions`
- Buffers.Text: `Base64` (encode/decode/validate, RFC 4648), `Base64Url` (encode/decode/validate, RFC 4648 §5)
- IO: `Stream`, `FileStream`, `MemoryStream`, `BinaryReader`, `BinaryWriter`, `StreamReader`, `StreamWriter`, `TextReader`, `TextWriter`, `File`, `Directory`, `Path`, `FileInfo`, `DirectoryInfo`, `RandomAccess`
- IO.Compression: `ZipArchive`, `ZipArchiveEntry`, `ZipFile`, `DeflateStream`, `GZipStream`
- IO.Hashing: `Crc32`, `Crc64`, `XxHash32`, `XxHash64`, `XxHash3`, `XxHash128`
- Text: `StringBuilder`, `Encoding` (UTF-8/16/32/ASCII), `Rune`, `Unicode.*`, `FormattableString`
- Text.Json: `JsonSerializer`, `JsonElement`, `JsonDocument`, `Utf8JsonReader`, `Utf8JsonWriter`
- Threading: `Thread`, `ThreadPool`, `Monitor`, `Mutex`, `SemaphoreSlim`, `AutoResetEvent`, `ManualResetEvent`, `ManualResetEventSlim`, `Interlocked`, `CancellationToken/Source`, `Barrier`, `CountdownEvent`, `Lock`, `AsyncLocal<T>`, `LazyInitializer`
- Threading.Tasks: `Task`, `Task<T>`, `ValueTask`, `ValueTask<T>`, `TaskCompletionSource<T>`
- Numerics: `BigInteger`, `Complex`, `BFloat16`, `Vector2/3/4`, `Matrix3x2`, `Matrix4x4`, `Quaternion`, `Plane`
- Diagnostics: `Debug`, `Trace`, `Stopwatch`, `DiagnosticListener`, `Activity`
- Globalization: `CultureInfo`, `DateTimeFormatInfo`, `NumberFormatInfo`, `TextInfo`, `IdnMapping`, `Calendar` types, `CompareInfo`, `RegionInfo`
- Net: `IPAddress`, `IPEndPoint`, `HttpStatusCode`, `HttpMethod`, `Uri`, sockets (POSIX-only)
- Net.Http: `HttpClient`, `HttpRequestMessage`, `HttpResponseMessage`, `HttpContent` (no TLS)
- Xml: `XmlReader`, `XmlWriter`, `XmlDocument`, `XElement`, `XDocument` (via tinyxml2)
- Runtime handles: `RuntimeTypeHandle`, `RuntimeMethodHandle`, `RuntimeFieldHandle`, `ModuleHandle`, `ValueType`
- Other: `Environment` (full), `AppDomain`, `AppContext`, `GC` (stubs), `DBNull`, `Delegate`, `Nullable<T>`, `NullableHelper`, `IObservable<T>`, `IObserver<T>`, `WeakReference`, `BinaryData`, `String.Intern/IsInterned`

### What does NOT work
- `Regex` — `std::regex` back-end; no named groups, no lookbehind.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — POSIX-only; will not compile on Windows without Winsock2 path.
- `SynchronizationContext` — stub; `Progress<T>` calls handlers synchronously.
- `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; not implemented.
- `CopyTo(Array, int)` on `ICollection`/`BitArray`/`ArrayList` — `System.Array` type does not exist in this project.
- `ArrayList.GetEnumerator()` — returns `nullptr`; non-generic enumerator over `std::any` not yet implemented.
- Windows / Emscripten cross-compilation — untested; POSIX guards exist but not CI-validated.

---

## 3. Recent changes

All on branch `develop`, most recent first:

| Commit | Change |
|--------|--------|
| `992010c` | `BitArray`: add `Clone`, `GetEnumerator` (inner `Enumerator` class), `IsReadOnly/IsSynchronized/SyncRoot` props; `ArrayList`: add `Clone`, `GetRange`, `Sort(IComparer)`, `Sort(int,int,IComparer)`, `BinarySearch` (2 overloads), `IndexOf` (2 overloads), `LastIndexOf` (2 overloads), `GetEnumerator(int,int)`, `Repeat`, `ArrayList(ICollection&)`, `getSyncRootProperty`; 25 new tests |
| `d89f8f8` | `System.Collections` Batch18: all 9 types upgraded to `/** */` Doxygen; `ICollection`+SyncRoot, `Comparer`+DefaultInvariant, `BitArray`+LeftShift/RightShift/PopCount/int-ctor; 19 new tests |
| `081e194` | `IEqualityComparer`: upgrade `///` → `/** */` Doxygen |
| `acdff17` | `IList`: fix `Add(void*)` return type `void` → `SharpRuntime::intcs`; upgrade to `/** */` Doxygen; `ArrayList`: match new signatures |
| `3ea15d7` | Batch17: `BinaryPrimitives.ReverseEndianness`, `SequenceReader` search methods, `ReadOnlySequenceSegment`, `BuffersExtensions`, `Base64`, `Base64Url`; 54 new tests |
| `5596206` | Batch16: `MemoryHandle`, `IPinnable`, `MemoryManager<T>`, `SearchValues<T>`, `SequenceReaderExtensions`; `Decimal` OACurrency; `String.Intern/IsInterned`; `ArrayPool::Create`; 37 new tests |
| `da64198` | Batch15: `Math` overloads (Abs/Min/Max/Clamp for all numeric types, ILogB, BigMul, DivRem), `BadImageFormatException` fileName ctor, `ValueType`, `RuntimeTypeHandle/MethodHandle/FieldHandle`, `ModuleHandle`; 59 new tests |

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 6412 tests pass.

The workflow is driven by the user providing types from the `plan.md` namespace review one at a time. There is no automated queue. The current area of focus is `System.Collections` (non-generic interfaces and classes).

Remaining `///`-style Doxygen comments exist in **288 headers** — these should be upgraded opportunistically as types are touched, not in a mass sweep.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| POSIX-only | `System::Net::Sockets` — `<sys/socket.h>`, `pread`, `pwrite`; no Windows/Emscripten |
| POSIX-only | `System::IO::RandomAccess` — `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; not portable to macOS |
| POSIX-only | `System::TimeZoneInfo` — `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| incomplete | `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; throws |
| incomplete | `ArrayList.GetEnumerator()` — returns `nullptr`; not yet iterable via `IEnumerator*` |
| incomplete | `CopyTo(Array, int)` — `System.Array` type does not exist; skipped on all collections |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub | `System::GC` — all methods are no-ops |
| stub | `System::Type` — no runtime reflection |
| stub | `System::Activator` — `CreateInstance` not implementable without reflection |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` — placing it inside `namespace System` causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| incomplete | `WeakReferenceT<T>` is the generic form (C++ cannot have a class template and a plain class with the same name in the same namespace) |
| design note | `ArrayList` compares `std::any` elements by `type_info` only (not value); meaningful value comparison requires a typed `IComparer` |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  SharpRuntime/Prop.hpp                 ← property macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root
  System/                               ← ~567 .hpp files
    Collections/                        ← ArrayList, BitArray, Hashtable, IList, IComparer, …
    Collections/Generic/                ← List, Dictionary, Queue, SortedSet, …
    Collections/Concurrent/             ← ConcurrentDictionary, BlockingCollection
    Collections/Immutable/              ← ImmutableArray, ImmutableList, …
    Buffers/                            ← ArrayPool, MemoryPool, SearchValues, SequenceReader, …
    Buffers/Binary/                     ← BinaryPrimitives
    Buffers/Text/                       ← Base64, Base64Url
    IO/                                 ← Stream, File, Path, Compression/, Hashing/
    Text/                               ← StringBuilder, Encoding, Json/, Encodings/
    Threading/                          ← Thread, Monitor, Tasks/, LazyThreadSafetyMode
    Numerics/                           ← BigInteger, Vector*, Matrix*, Quaternion
    Diagnostics/                        ← Debug, Stopwatch, Activity
    Globalization/                      ← CultureInfo, Calendar, DateTimeFormatInfo
    Net/                                ← IPAddress, Http/, Sockets/
    Xml/                                ← XmlReader, XmlWriter, Linq/
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← 148 GoogleTest .cpp files
vendor/                                 ← googletest, nlohmann/json, tinyxml2, miniz
```

**Vendored libraries:**

| Library | Use |
|---------|-----|
| GoogleTest | test framework |
| nlohmann/json | `System::Text::Json` |
| tinyxml2 | `System::Xml::XmlReader/XmlWriter` |
| miniz | `System::IO::Compression::ZipArchive` |

### Invariants that must not be broken
1. **Zero errors, zero warnings** (`-Wall -Wextra -Werror`) before every commit.
2. **6412+ tests passing** — never go below the watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (= `int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
7. **Doxygen `/** */`** on all public declarations — upgrade `///` / `/// @brief` when encountered.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **No broad header refactor** — property naming touches 449+ files in CNA.
10. **Push only to `develop`** — never push to `master` or create tags without explicit per-action user approval.
11. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.

### Type alias summary
| Alias | Underlying | .NET equivalent |
|-------|-----------|-----------------|
| `SharpRuntime::intcs` | `int32_t` | `int` |
| `SharpRuntime::shortcs` | `int16_t` | `short` |
| `SharpRuntime::longcs` | `int64_t` | `long` |
| `SharpRuntime::bytecs` | `uint8_t` | `byte` |
| `SharpRuntime::sbytecs` | `int8_t` | `sbyte` |
| `SharpRuntime::uintcs` | `uint32_t` | `uint` |
| `SharpRuntime::ulongcs` | `uint64_t` | `ulong` |
| `SharpRuntime::ushortcs` | `uint16_t` | `ushort` |
| `SharpRuntime::charcs` | `char16_t` | `char` |

### Circular include resolution pattern
When two types mutually reference each other (e.g. `MemoryHandle` ↔ `IPinnable`, `RuntimeTypeHandle` ↔ `ModuleHandle`):
1. Forward-declare the second type in the first header.
2. Include the first header at the top of the second header (gets the full definition).
3. Define the second type completely.
4. Define the deferred methods of the first type as `inline` in the namespace block, after both types are complete.

### Test naming conventions
- `EXCEPT_SIMPLE(ExType)` macro in `ExceptionRemainingTests.cpp` already defines `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception` for many exception types. Do not re-add these names in dedicated test files — it causes a linker error (duplicate symbol).
- New type tests go in `tests/System/<TypeName>Tests.cpp` or in `tests/System/SystemTypesRemainingTests.cpp` for small types.
- Duplicate test name fix: append `_New` suffix.

---

## 7. Useful commands

```bash
# Configure (first time only)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel 4

# Build — errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run all tests
./build/SharpRuntimeTests

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="BitArray*"
./build/SharpRuntimeTests --gtest_filter="ArrayList*"
./build/SharpRuntimeTests --gtest_filter="Base64*"

# Find headers still using /// style Doxygen
grep -rl "^\s*///" include/System/ | head -20

# Check .NET source for a type
find /rv/tmp/runtime/src/libraries -name "Hashtable.cs" | head -3
cat /rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Hashtable.cs

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"

# Push (develop only)
git push origin develop
```

---

## 8. Next smallest tasks

Ordered by value and readiness. Each fits one focused coding session.

### Task 1 — Upgrade `Hashtable` Doxygen + fill API gaps
- **Goal:** Check `Hashtable.hpp` against .NET source; upgrade `///` → `/** */`; add any missing methods.
- **Files:** `include/System/Collections/Hashtable.hpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Hashtable.cs`
- **Verify:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`

### Task 2 — Port `System.Collections.Queue` + `Stack` gap-fill
- **Goal:** Check `Queue.hpp` and `Stack.hpp` against .NET source; upgrade Doxygen; fill missing API.
- **Files:** `include/System/Collections/Queue.hpp`, `include/System/Collections/Stack.hpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Queue.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="QueueTests*:StackTests*"`

### Task 3 — `ArrayList.GetEnumerator()` — implement non-generic enumerator
- **Goal:** Return a working `IEnumerator*` from `ArrayList::GetEnumerator()` instead of `nullptr`; add tests.
- **Files:** `include/System/Collections/ArrayList.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="ArrayList*"`

### Task 4 — Continue `plan.md` namespace review: next `System.Collections` types
- **Goal:** The user provides the next type from `plan.md`; port/verify/commit it following the established batch workflow.
- **Files:** varies by type
- **Verify:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`

### Task 5 — Sweep `///` Doxygen in `include/System/Collections/`
- **Goal:** Find all `.hpp` files in `include/System/Collections/` still using `///` and upgrade to `/** */`.
- **Find command:** `grep -rl "^\s*///" include/System/Collections/`
- **Verify:** Build clean, test count unchanged.

### Task 6 — Port `InvalidCastException` fully
- **Goal:** Verify `/** */` Doxygen, `const char*` ctor, inner-exception ctor; add dedicated tests.
- **Files:** `include/System/InvalidCastException.hpp`, new test file or existing `ExceptionTests.cpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/InvalidCastException.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="InvalidCast*"`

### Task 7 — Port `ConsoleKey` / `ConsoleKeyInfo` / `ConsoleModifiers`
- **Goal:** Add the Console key-input enum and struct needed by game input code.
- **Files:** `include/System/ConsoleKey.hpp` (new), `include/System/ConsoleKeyInfo.hpp` (new), `include/System/ConsoleModifiers.hpp` (new)
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Console/src/System/ConsoleKey.cs`
- **Verify:** Build clean, new tests pass.

### Task 8 — Broad `///` sweep in `include/System/` (deferred)
- **Goal:** 288 headers still use `///` style. Sweep the highest-traffic ones (IO, Text, Threading).
- **Find command:** `grep -rl "^\s*///" include/System/ | grep -v Collections | head -20`
- **Verify:** Build clean, test count unchanged.

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA. Any rename silently breaks the consumer project.
- **No LINQ port** — use `std::ranges` in all new ported code; do not add a LINQ layer.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs; do not attempt fixes until a cross-compile environment is available.
- **No merge to `master`** — always push to `develop` only; master merge requires explicit user approval per action.
- **No new vendored libraries** — do not add dependencies (e.g., Boost, PCRE2, OpenSSL) without discussing scope impact.
- **No speculative API additions** — only add methods that exist in .NET's published API surface and are needed by CNA/mobile-eggbert.
- **No work on `System::Type` / `System::Activator`** — they require runtime reflection that C++ cannot provide; stubs are the correct end state.
- **No `SynchronizationContext` full implementation** — the current synchronous stub is correct for single-threaded game use.
- **No duplicate test names** — `EXCEPT_SIMPLE` macro already defines `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception` for many exception types; adding them again causes a linker error.
- **No mass `///` → `/** */` sweep in one commit** — upgrade Doxygen only on files being actively modified; a mass sweep touches too many files at once and risks merge conflicts.
- **No `ArrayList.Sort()` without comparer** — `std::any` cannot be compared without type info; this is a known limitation, not a bug to fix.

---

## 10. Resume prompt

```
Read NEXT.md first to understand the current state of the project.

Then read only the files needed for the first task in section 8.
Do not read or refactor any unrelated code.

Make one small, fully verified improvement:
  - implement or update a single type or small group of related types,
  - run: cmake --build build --parallel 4   (must produce zero errors, zero warnings)
  - run: ./build/SharpRuntimeTests          (all 6412+ tests must pass)
  - commit with: git -c commit.gpgsign=false commit -m "..."
  - push to origin/develop only.

After finishing, update NEXT.md (sections 2, 3, 4, 8) to reflect the new state.

First task: see section 8 "Next smallest tasks", Task 1 — Hashtable Doxygen + API gap-fill.
```
