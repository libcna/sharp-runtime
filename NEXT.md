# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-20 (branch: develop) — 4579 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — one type at a time, driven by game-code dependency analysis. ~503 headers across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`.
- **Key architectural decision:** No runtime reflection, no GC, no IL. Everything maps to native C++ idioms: `std::shared_ptr`, `std::function`, `std::optional`, `std::ranges`, templates. Property pattern is `getXxxProperty()` / `setXxxProperty()`.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **4579 tests passing** across 505 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int32`, `Int64`, `UInt16`, `UInt64`, `Int128`, `UInt128`, `Half`, `Single`, `Double`, `Decimal`, `Guid`, `BitConverter`, `Math`, `MathF`, `Random`, `HashCode`
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy (`Exception`, `SystemException`, `ArgumentException`, `ArgumentNullException`, `ArgumentOutOfRangeException`, `InvalidOperationException`, `NotImplementedException`, `NotSupportedException`, `PlatformNotSupportedException`, `FormatException`, `OverflowException`, `ArithmeticException`, `ArrayTypeMismatchException`, `ExecutionEngineException`, `TypeAccessException`, `WeakReference`, etc.)
- Collections: `List<T>`, `Dictionary<K,V>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList<K,V>`, `SortedDictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, `ReadOnlyCollection<T>`, `ArraySegment<T>`, `Hashtable`, `ArrayList`, `BitArray`
- Immutable collections: `ImmutableArray<T>`, `ImmutableList<T>`, `ImmutableDictionary<K,V>`, `ImmutableHashSet<T>`, `ImmutableQueue<T>`, `ImmutableStack<T>`, `ImmutableSortedDictionary<K,V>`, `ImmutableSortedSet<T>`
- Span/Memory: `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`, `MemoryExtensions` (AsSpan, ContainsAny, IndexOf/LastIndexOf subsequence, SequenceCompareTo, Count, BinarySearch, Overlaps, Trim/TrimStart/TrimEnd), `SpanSplitEnumerator`, `TryWriteInterpolatedStringHandler`
- IO: `Stream`, `FileStream`, `MemoryStream`, `BinaryReader`, `BinaryWriter`, `StreamReader`, `StreamWriter`, `TextReader`, `TextWriter`, `File`, `Directory`, `Path`, `FileInfo`, `DirectoryInfo`, `RandomAccess`
- IO.Compression: `ZipArchive`, `ZipArchiveEntry`, `ZipFile`, `DeflateStream`, `GZipStream`
- IO.Hashing: `Crc32`, `Crc64`, `XxHash32`, `XxHash64`, `XxHash3`, `XxHash128`
- Text: `StringBuilder`, `Encoding` (UTF-8/16/32/ASCII), `Rune`, `Unicode.UnicodeRange/UnicodeRanges`, `FormattableString`, `FormattableStringFactory`, `ISpanFormattable`, `SpanFormattableHelper`
- Text.Json: `JsonSerializer`, `JsonElement`, `JsonDocument`, `Utf8JsonReader`, `Utf8JsonWriter`
- Threading: `Thread`, `ThreadPool`, `Monitor`, `Mutex`, `SemaphoreSlim`, `AutoResetEvent`, `ManualResetEvent`, `ManualResetEventSlim`, `Interlocked`, `CancellationToken`, `CancellationTokenSource`, `Barrier`, `CountdownEvent`, `Lock`, `AsyncLocal<T>`, `LazyInitializer`, `LazyThreadSafetyMode`, `ITimer`
- Threading.Tasks: `Task`, `Task<T>`, `ValueTask`, `ValueTask<T>`, `TaskCompletionSource<T>`
- Numerics: `BigInteger`, `Complex`, `BFloat16`, `Vector2/3/4`, `Matrix3x2`, `Matrix4x4`, `Quaternion`, `Plane`
- Diagnostics: `Debug`, `Trace`, `Stopwatch`, `DiagnosticListener`, `Activity`
- Globalization: `CultureInfo`, `DateTimeFormatInfo`, `NumberFormatInfo`, `TextInfo`, `IdnMapping`, `Calendar` types, `CompareInfo`, `RegionInfo`
- Net: `IPAddress`, `IPEndPoint`, `HttpStatusCode`, `HttpMethod`, `Uri`, sockets (POSIX-only)
- Net.Http: `HttpClient`, `HttpRequestMessage`, `HttpResponseMessage`, `HttpContent` (no TLS)
- Xml: `XmlReader`, `XmlWriter`, `XmlDocument`, `XElement`, `XDocument` (via tinyxml2)
- Attributes: `Attribute`, `AttributeUsageAttribute`, `AttributeTargets`, `ObsoleteAttribute`, `CLSCompliantAttribute`, `FlagsAttribute`, `ThreadStaticAttribute`, `LoaderOptimizationAttribute`
- Delegation/generics: `Delegate`, `IProgress<T>`, `Progress<T>`, `Lazy<T>`, `IObservable<T>`, `IObserver<T>`, `Nullable<T>`, `NullableHelper`, `Void`
- Environment: `Environment` (full — including `CurrentManagedThreadId`, `Version`, `FailFast(string,exception)`), `AppDomain` (singleton stub with full API surface), `AppContext` (stub), `GC` (no-op stubs), `DBNull`, `EnvironmentVariableTarget`

### What does NOT work
- `Regex` — no named groups, lookbehind; uses `std::regex` which is limited.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — POSIX-only (Linux/macOS); will not compile on Windows without Winsock2 path.
- `SynchronizationContext` — stub; `Progress<T>` calls handlers synchronously instead of marshalling.
- Windows / Emscripten cross-compilation — untested; POSIX-only subsystems would fail to link.

---

## 3. Recent changes

All on branch `develop`, most recent first:

| Commit | Change |
|--------|--------|
| `42b3e6f` | `Environment`: add `getCurrentManagedThreadIdProperty()`, `getVersionProperty()` (stub 1.0.0), `FailFast(string, exception)` overload; 4 new tests |
| `4261d15` | `ArrayTypeMismatchException`: upgrade to `/** */` Doxygen, add `const char*` ctor, inner-exception ctor; 3 new tests (6 total) |
| `339a0de` | `AppDomain`: add `final`, `Id`, `IsFullyTrusted`, `IsHomogenous`, `IsDefaultAppDomain`, `IsFinalizingForUnload`, `RelativeSearchPath`, `DynamicDirectory`, `ApplyPolicy`, `ToString`; full `/** */` Doxygen; 9 new tests |
| `0515259` | `Void`: new `System::Void` empty struct with `/** */` Doxygen, equality operators, `ToString`; 5 tests |
| `f173cea` | `IObserver`: upgrade to full `/** */` Doxygen from .NET source, 2 new tests |
| `9b23dc5` | `Nullable<T>`: throw `InvalidOperationException` (not `std::runtime_error`), add `Equals`/`GetHashCode`/`ToString`, `NullableHelper` with `Compare`/`Equals`; 14 new tests (23 total) |
| `fa16e66` | `ExecutionEngineException`: add `final`, inner-exception ctor, `/** */` Doxygen with `[Obsolete]` note; 3 new tests |
| `bda364d` | `ArithmeticException`: fix default message, add inner-exception ctor, convert to header-only, `/** */` Doxygen; 4 new tests |
| `b97f80e` | `TypeAccessException`: upgrade to `/** */` Doxygen, add `IsA_TypeLoadException` and inner-exception ctor tests |
| `66d68c8` | `WeakReference`: add `getTrackResurrectionProperty`, `/** */` Doxygen, `trackResurrection_` storage; `WeakReferenceT<T>`: same; 8 new tests (13 total) |
| `bdb617a` | `EnvironmentVariableTarget`: upgrade to `/** */` Doxygen, add `User_IsOne` test |
| `71cd5e6` | `DBNull`: implement `IConvertible` (all `ToXxx` throw `InvalidCastException`), add `GetTypeCode`, `ToString(IFormatProvider*)`, `final`, `/** */` Doxygen; 9 new tests |
| `66fecea` | `MemoryExtensions`: add `AsSpan(string)`, `ContainsAny`, sub-span `IndexOf`/`LastIndexOf`, `SequenceCompareTo`, `Count`, `BinarySearch`, `Overlaps`, `Trim`/`TrimStart`/`TrimEnd`; 32 new tests |

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 4579 tests pass.

The only constraint on velocity is **scope management**: `plan_namespaces.md` tracks 311 .NET namespaces, most without an assigned status. The user drives prioritization by naming the next type to port; there is no automated queue. Porting continues type-by-type on request.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| POSIX-only | `System::Net::Sockets` — uses `<sys/socket.h>`, `pread`, `pwrite`; will not link on Windows/Emscripten |
| POSIX-only | `System::IO::RandomAccess` — uses `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — `getBaseDirectoryProperty()` reads `/proc/self/exe`; not implemented on macOS |
| POSIX-only | `System::TimeZoneInfo` — uses `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — `std::regex` back-end; no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub | `System::GC` — all methods are no-ops |
| stub | `System::Type` — no runtime reflection |
| stub | `System::Activator` — `CreateInstance` not implementable without reflection |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` (not inside `namespace System`) — refactoring into the namespace block causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested in this repo; POSIX guards exist but not validated |
| incomplete | `WeakReferenceT<T>` is the generic form (not `WeakReference<T>`) because C++ cannot have a class template and a plain class with the same name in the same namespace |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs
  SharpRuntime/Prop.hpp                 ← property macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root
  System/                               ← ~503 .hpp files
    Collections/Generic/                ← List, Dictionary, Queue, etc.
    Collections/Concurrent/             ← ConcurrentDictionary, BlockingCollection
    Collections/Immutable/              ← ImmutableArray, ImmutableList, etc.
    IO/                                 ← Stream, File, Path, Compression/, Hashing/
    Text/                               ← StringBuilder, Encoding, Json/, Encodings/
    Threading/                          ← Thread, Monitor, Tasks/, LazyThreadSafetyMode
    Numerics/                           ← BigInteger, Vector*, Matrix*, Quaternion
    Diagnostics/                        ← Debug, Stopwatch, Activity
    Globalization/                      ← CultureInfo, Calendar, DateTimeFormatInfo
    Net/                                ← IPAddress, Http/, Sockets/
    Xml/                                ← XmlReader, XmlWriter, Linq/
src/System/                             ← 83 .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← 87 GoogleTest suites
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
1. **Zero errors, zero warnings** (`-Wall -Wextra -Werror`) — run before every commit.
2. **3080+ tests passing** — currently 4579; never go below the watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (= `int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
7. **Doxygen `/** */` on all public declarations** — copy from .NET source XML doc comments where they exist; upgrade `///` or `/// @brief` style when encountered.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else`.
9. **No broad header refactor** — naming conventions touch 449+ files in CNA; any rename would break it.
10. **Push only to `develop`** — never push to `master` or create tags without explicit per-action user approval.

### Type alias summary
| Alias | Underlying | .NET equivalent |
|-------|-----------|-----------------|
| `SharpRuntime::intcs` | `int32_t` | `int` |
| `SharpRuntime::shortcs` | `int16_t` | `short` |
| `SharpRuntime::longcs` | `int64_t` | `long` |
| `SharpRuntime::bytecs` | `uint8_t` | `byte` |
| `SharpRuntime::charcs` | `char16_t` | `char` |

### Test naming conventions
- `EXCEPT_SIMPLE(ExType)` macro in `ExceptionRemainingTests.cpp` generates three tests per exception type: `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception`. Do not re-add these names when writing dedicated tests.
- Exception-specific tests go in `ExceptionTests.cpp` or `ExceptionRemainingTests.cpp`.
- New type tests go either in a dedicated `tests/System/<TypeName>Tests.cpp` or in `tests/System/SystemTypesRemainingTests.cpp` for smaller types.

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
./build/SharpRuntimeTests --gtest_filter="Nullable*"
./build/SharpRuntimeTests --gtest_filter="Environment*"
./build/SharpRuntimeTests --gtest_filter="ArrayTypeMismatch*"

# Check what .NET source exists for a type (example: Console)
find /rv/tmp/runtime/src/libraries -name "Console.cs" | head -5
cat /rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Console.cs

# Check git log
git log --oneline -10

# Push (develop only)
git push origin develop
```

---

## 8. Next smallest tasks

Ordered by value and readiness. Each should take one focused coding session.

### Task 1 — Port `InvalidCastException`
- **Goal:** Verify header has `/** */` Doxygen, `const char*` ctor, inner-exception ctor; add dedicated tests.
- **Files:** `include/System/InvalidCastException.hpp`, `tests/System/ExceptionTests.cpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/InvalidCastException.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="InvalidCast*"`

### Task 2 — Port `IndexOutOfRangeException`
- **Goal:** Same pattern — Doxygen, ctors, inner-exception overload, dedicated tests.
- **Files:** `include/System/IndexOutOfRangeException.hpp`, `tests/System/ExceptionTests.cpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/IndexOutOfRangeException.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="IndexOutOfRange*"`

### Task 3 — Port `StackOverflowException`
- **Goal:** Same pattern — Doxygen, ctors, tests. Note: .NET marks it as unrecoverable; C++ port is just a normal exception.
- **Files:** `include/System/StackOverflowException.hpp`, `tests/System/ExceptionTests.cpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/StackOverflowException.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="StackOverflow*"`

### Task 4 — Port `OutOfMemoryException`
- **Goal:** Same pattern — Doxygen, ctors, tests.
- **Files:** `include/System/OutOfMemoryException.hpp`, `tests/System/ExceptionTests.cpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/OutOfMemoryException.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="OutOfMemory*"`

### Task 5 — Port `ConsoleKey` / `ConsoleKeyInfo` / `ConsoleModifiers`
- **Goal:** Add the Console key-input enums and struct needed by game input code.
- **Files:** `include/System/ConsoleKey.hpp` (new), `include/System/ConsoleKeyInfo.hpp` (new), `include/System/ConsoleModifiers.hpp` (new)
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Console/src/System/ConsoleKey.cs`
- **Verify:** Build clean, new tests pass.

### Task 6 — Port `BinaryData`
- **Goal:** Lightweight blob type wrapping `std::vector<uint8_t>` with `ToString()`, `ToArray()`, `FromString()`, `FromBytes()`.
- **Files:** `include/System/BinaryData.hpp` (new), `src/System/BinaryData.cpp` (new)
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Memory.Data/src/System/BinaryData.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="BinaryData*"`

### Task 7 — Port `SequencePosition`
- **Goal:** Simple `(object?, int)` pair used by `System.IO.Pipelines`; useful for future buffer work.
- **Files:** `include/System/SequencePosition.hpp` (new)
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Memory/src/System/SequencePosition.cs`
- **Verify:** Build clean.

### Task 8 — Sweep remaining `///`-style Doxygen in `include/System/`
- **Goal:** Find all `.hpp` files still using `///` or `/// @brief` and upgrade them to `/** */`.
- **Files:** Any header in `include/System/` with `///` comments.
- **Find command:** `grep -rl "^\s*///" include/System/ | head -20`
- **Verify:** Build clean, test count unchanged.

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA. Any rename would silently break the consumer project.
- **No LINQ port** — use `std::ranges` in all new ported code; do not add a LINQ layer.
- **No Windows / Emscripten CI** — do not attempt to fix POSIX-only subsystems until a Windows build environment is available; they are documented bugs, not scope.
- **No merge to `master`** — always push to `develop` only; master merge requires explicit user approval per action.
- **No new vendored libraries** — do not add dependencies (e.g., Boost, PCRE2, OpenSSL) without discussing scope impact first.
- **No speculative API additions** — only add methods that exist in .NET's published API surface and are needed by CNA/mobile-eggbert.
- **No work on `System::Type` / `System::Activator`** — they require runtime reflection that C++ cannot provide; stubs are the correct end state.
- **No `SynchronizationContext` full implementation** — the current synchronous stub is correct for single-threaded game use.
- **No duplicate test names** — the `EXCEPT_SIMPLE` macro in `ExceptionRemainingTests.cpp` already defines `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception` for many exception types; adding them again causes a linker error.

---

## 10. Resume prompt

```
Read NEXT.md first to understand current state.

Then read only the files needed for the first task in section 8.
Do not read or refactor any unrelated code.

Make one small, fully verified improvement:
  - implement or update a single type,
  - run `cmake --build build --parallel 4` — must produce zero errors, zero warnings,
  - run `./build/SharpRuntimeTests` — all tests must pass (currently 4579),
  - commit with a descriptive message and push to origin/develop only.

After finishing, update NEXT.md (sections 2, 3, 4, 8) to reflect the new state.

First task: see section 8 "Next smallest tasks", Task 1 — InvalidCastException.
```
