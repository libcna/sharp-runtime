# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-14 (branch: develop) — 4308 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — one type at a time, driven by game-code dependency analysis. Approaching ~180 headers across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`.
- **Key architectural decision:** No runtime reflection, no GC, no IL. Everything maps to native C++ idioms: `std::shared_ptr`, `std::function`, `std::optional`, `std::ranges`, templates. Property pattern is `getXxxProperty()` / `setXxxProperty()`.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **4308 tests passing** across 498 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int32`, `Int64`, `UInt16`, `UInt64`, `Int128`, `UInt128`, `Half`, `Single`, `Double`, `Decimal`, `Guid`, `BitConverter`, `Math`, `MathF`, `Random`, `HashCode`
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy (`Exception`, `SystemException`, `ArgumentException`, `ArgumentNullException`, `ArgumentOutOfRangeException`, `InvalidOperationException`, `NotImplementedException`, `NotSupportedException`, `PlatformNotSupportedException`, `FormatException`, `OverflowException`, etc.)
- Collections: `List<T>`, `Dictionary<K,V>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList<K,V>`, `SortedDictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, `ReadOnlyCollection<T>`, `ArraySegment<T>`, `Hashtable`, `ArrayList`, `BitArray`
- Immutable collections: `ImmutableArray<T>`, `ImmutableList<T>`, `ImmutableDictionary<K,V>`, `ImmutableHashSet<T>`, `ImmutableQueue<T>`, `ImmutableStack<T>`, `ImmutableSortedDictionary<K,V>`, `ImmutableSortedSet<T>`
- Span/Memory: `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`, `MemoryExtensions`, `SpanSplitEnumerator`, `TryWriteInterpolatedStringHandler`
- IO: `Stream`, `FileStream`, `MemoryStream`, `BinaryReader`, `BinaryWriter`, `StreamReader`, `StreamWriter`, `TextReader`, `TextWriter`, `File`, `Directory`, `Path`, `FileInfo`, `DirectoryInfo`, `RandomAccess`
- IO.Compression: `ZipArchive`, `ZipArchiveEntry`, `ZipFile`, `DeflateStream`, `GZipStream`
- IO.Hashing: `Crc32`, `Crc64`, `XxHash32`, `XxHash64`, `XxHash3`, `XxHash128`
- Text: `StringBuilder`, `Encoding` (UTF-8/16/32/ASCII), `Rune`, `Unicode.UnicodeRange/UnicodeRanges`, `StringNormalizationExtensions`, `ISpanFormattable`, `SpanFormattableHelper`
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
- Delegation/progress: `Delegate`, `IProgress<T>`, `Progress<T>`, `Lazy<T>`, `IObservable<T>`, `IObserver<T>`
- Environment: `Environment` (most properties/methods), `AppDomain` (stub), `AppContext` (stub), `GC` (no-op stubs)

### What does NOT work
- `Regex` — no named groups, lookbehind; uses `std::regex` which is limited.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — POSIX-only (Linux/macOS); will not compile on Windows without Winsock2 path.
- `SynchronizationContext` — stub; `Progress<T>` calls handlers synchronously instead of marshalling.
- Windows / Emscripten cross-compilation — untested; POSIX-only subsystems would fail to link.

---

## 3. Recent changes

All on branch `develop` within the last session:

| Commit | Change |
|--------|--------|
| `81d8fec` | `IProgress<T>`: Doxygen; `Progress<T>`: `OnReport` virtual, vector-based handler list, 4 new tests |
| `8de82ce` | `Lazy<T>`: 5 new ctors (`T value`, `bool`, `LazyThreadSafetyMode`, factory+bool, factory+mode), `getModeProperty()`, `ToString()`, template factory ctors with `requires` to fix lambda/bool ambiguity; 13 new tests |
| `761f4e5` | `Attribute`: `Equals`, `GetHashCode` (identity-based), `getTypeIdProperty()`, Doxygen; 6 new tests |
| `eedeb7e` | `AttributeUsageAttribute`: 3-arg ctor, `Default` static instance, Doxygen; 4 new tests |
| `9122916` / `3963303` | `Random`: complete .NET API — `Shared`, `NextBinaryFloat<T>`, `NextInteger<T>` (all overloads), `Shuffle<T>` (vector + Span), `GetItems<T>`, `GetString`, `GetHexString`; 30 new tests |
| `4a1de17` | `SpecialFolderOption`: per-value Doxygen, 6 tests |
| `4ec00a0` | `Environment`: `IsPrivilegedProcess`, `SetCurrentDirectory`, `GetEnvironmentVariables`, `GetCommandLineArgs`, `ProcessPath`, `SystemPageSize`, `WorkingSet`, `UserDomainName`; 17 new tests |
| `8c67834` / `ac48cee` | `Delegate`: full port with `Combine`, `Remove`, `RemoveAll`, `GetInvocationList`, `HasSingleTarget`, `DynamicInvoke` (stub), `InvocationListEnumerator<TDelegate>` with range-for; 29 tests |
| `a75ae14` | `Version`: `MajorRevision`, `MinorRevision`, `GetHashCode`, `ToString(fieldCount)`, Doxygen; 13 new tests |

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 4308 tests pass.

The only constraint on velocity is **scope management**: `plan_System.md` has 232 rows tracking every `.cs` file in the `System` namespace, but only 6 have an explicit status (`ported` or `ignore`). The remaining 226 rows are unclassified — each needs a `todo` / `ignore` / `ported` / `in_progress` decision before systematic porting can proceed with a clear backlog.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| POSIX-only | `System::Net::Sockets` — uses `<sys/socket.h>`, `pread`, `pwrite`; will not link on Windows/Emscripten |
| POSIX-only | `System::IO::RandomAccess` — uses `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; macOS path not implemented |
| POSIX-only | `System::TimeZoneInfo` — uses `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — `std::regex` back-end; no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub | `System::GC` — all methods are no-ops |
| stub | `System::Type` — no runtime reflection |
| stub | `System::Activator` — `CreateInstance` not implementable without reflection |
| incomplete | `plan_System.md` — 226 of 232 rows have no assigned status |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` (not inside `namespace System`) — if refactored into the namespace block it causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested in this repo; POSIX guards exist but not validated |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs
  SharpRuntime/Prop.hpp                 ← property macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root
  System/                               ← core types, exceptions, Math (~182 files)
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
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← GoogleTest suites
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
2. **3080+ tests passing** — currently 4308; never go below the watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (= `int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
7. **Doxygen `/** */` on all public declarations** — copy from .NET source XML doc comments where they exist.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else`.
9. **No broad header refactor** — naming conventions touch 449+ files in CNA; any rename would break it.

### CMake
- `GLOB_RECURSE src/*.cpp` — no manual registration needed when adding new `.cpp` files.
- Build directory: `build/` (in-tree). Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` once; after that, only `cmake --build build` is needed.

### Type alias summary
| Alias | Underlying | .NET equivalent |
|-------|-----------|-----------------|
| `SharpRuntime::intcs` | `int32_t` | `int` |
| `SharpRuntime::shortcs` | `int16_t` | `short` |
| `SharpRuntime::longcs` | `int64_t` | `long` |
| `SharpRuntime::bytecs` | `uint8_t` | `byte` |
| `SharpRuntime::charcs` | `char16_t` | `char` |

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
./build/SharpRuntimeTests --gtest_filter="Lazy*"
./build/SharpRuntimeTests --gtest_filter="Progress*"
./build/SharpRuntimeTests --gtest_filter="Delegate*"

# Check what .NET source exists for a type (example: IProgress)
find /rv/tmp/runtime/src/libraries -name "IProgress*"
cat /rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/IProgress.cs

# Check git log
git log --oneline -10
```

---

## 8. Next smallest tasks

Ordered by value and readiness. Each should take one focused session.

### Task 1 — Classify remaining `plan_System.md` rows
- **Goal:** Assign `todo` / `ignore` / `ported` / `in_progress` to the ~226 unclassified rows.
- **Files:** `plan_System.md`
- **How:** Go row-by-row per the CLAUDE.md workflow — describe what the type is, ask the user to decide, write the decision.
- **Verify:** Every row in `plan_System.md` has a non-empty Status column.

### Task 2 — Port `Console` input types
- **Goal:** Add `ConsoleKey`, `ConsoleKeyInfo`, `ConsoleModifiers`, `ConsoleSpecialKey`, `ConsoleCancelEventArgs` (stubs are acceptable for CNA).
- **Files:** `include/System/ConsoleKey.hpp` (new), `ConsoleKeyInfo.hpp` (new), `ConsoleModifiers.hpp` (new), `ConsoleSpecialKey.hpp` (new), `ConsoleCancelEventArgs.hpp` (new)
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Console/src/System/ConsoleKey.cs` etc.
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="Console*"`

### Task 3 — Port `EventArgs` subclasses needed by Console
- **Goal:** `ConsoleCancelEventArgs` inherits `EventArgs`; verify `EventArgs` base is complete and add doc-comments.
- **Files:** `include/System/EventArgs.hpp`, `include/System/ConsoleCancelEventArgs.hpp` (new)
- **Verify:** Build clean, tests pass.

### Task 4 — Port `WeakReference<T>`
- **Goal:** Add the generic `WeakReference<T>` (the existing `WeakReference.hpp` is likely non-generic).
- **Files:** `include/System/WeakReference.hpp`
- **Reference:** `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/WeakReference.cs`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="WeakReference*"`

### Task 5 — Complete `Delegate` doc-comments and `DynamicInvoke`
- **Goal:** `DynamicInvoke` currently throws `NotImplementedException`; decide if a type-erased invocation via `std::any` is viable, or document it as a confirmed stub.
- **Files:** `include/System/Delegate.hpp`, `src/System/Delegate.cpp`
- **Verify:** Existing 29 delegate tests still pass.

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

---

## 10. Resume prompt

```
Read NEXT.md first to understand current state.

Then read only the files needed for the first task below.
Do not read or refactor any unrelated code.

Make one small, fully verified improvement:
  - implement or update a single type,
  - run `cmake --build build --parallel 4` — must be clean,
  - run `./build/SharpRuntimeTests` — all tests must pass,
  - commit with a descriptive message and push to origin/develop.

After finishing, update NEXT.md (sections 2, 3, 4, 8) to reflect the new state.

First task: see section 8 "Next smallest tasks", Task 1.
```
