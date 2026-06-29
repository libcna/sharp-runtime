# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-29 (branch: develop) — 8142 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — driven by a `plan.sqlite3` namespace review workflow. The user approves each type one at a time before work begins. Currently working through the `System` namespace alphabetically.
- **Header count:** ~567 `.hpp` files across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`, `System.Buffers`, etc.
- **Key architectural decisions:** No runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (int32_t), `bytecs` (uint8_t), etc. Inner exceptions use `std::exception_ptr`.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **8142 tests passing** across 892 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int16`, `Int32`, `Int64`, `Int128`, `IntPtr`, `UInt16`, `UInt64`, `UInt128`, `Half`, `Single`, `Double`, `Decimal` (+ OACurrency), `Guid`, `BitConverter` (full API including Half/BFloat16/Int128/UInt128), `Math` (full overloads + BigMul/DivRem/ILogB), `MathF`, `Random`, `HashCode`, `Void`, `Index`, `Lazy<T>`
- Delegates/Events: `Func<T>`, `Converter<T,R>`, `EventHandler<T>`, `EventArgs`, `Delegate`
- Attributes: `Attribute`, `FlagsAttribute`, `ObsoleteAttribute`, `SerializableAttribute`, `CLSCompliantAttribute`
- Enums: `Casing`, `CrashReason`, `GCCollectionMode`, `GCKind`, `GCLatencyMode`, `GCNotificationStatus`, `EnvironmentVariableTarget`, `MidpointRounding`
- Formatting: `FormattableString`, `FormattableStringFactory`, `IFormatProvider`, `IFormattable`, `ISpanFormattable`, `IUtf8SpanFormattable<T>`, `ICustomFormatter`
- Interfaces: `IAsyncDisposable`, `IAsyncResult`, `ICloneable`, `IComparable<T>`, `IConvertible`, `IDisposable`, `IEquatable<T>`, `IObservable<T>`, `IObserver<T>`, `IParsable<T>`, `IProgress<T>`, `IServiceProvider`, `ISpanParsable<T>`, `IUtf8SpanParsable<T>`
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy — all `std::exception_ptr` inner-exception ctors, all `/** */` Doxygen on all types
- Collections (non-generic): `ArrayList` (full API), `BitArray` (full API), `Hashtable`, `Queue`, `Stack`, `Comparer`, `IList`, `ICollection`, `IComparer`, `IEnumerator`, `IDictionaryEnumerator`, `IEqualityComparer`, `IStructuralComparable`, `IStructuralEquatable`
- Collections (generic): `List<T>`, `Dictionary<K,V>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList<K,V>`, `SortedDictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, `ReadOnlyCollection<T>`, `ArraySegment<T>`, `PriorityQueue<T,P>`, `ImmutableArray/List/Dictionary/HashSet/Queue/Stack/SortedDictionary/SortedSet<T>`
- Span/Memory: `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`, `MemoryExtensions` (full), `SpanSplitEnumerator`
- Buffers: `ArrayPool<T>`, `MemoryPool<T>`, `MemoryHandle`, `IPinnable`, `MemoryManager<T>`, `IBufferWriter<T>`, `ArrayBufferWriter<T>`, `SearchValues<T>`, `SequencePosition`, `ReadOnlySequence<T>`, `ReadOnlySequenceSegment<T>`, `SequenceReader<T>`, `SequenceReaderExtensions`, `BinaryPrimitives` (full), `BuffersExtensions`
- Buffers.Text: `Base64`, `Base64Url`
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
- Other: `Environment` (full), `AppDomain`, `AppContext`, `GC` (stubs), `DBNull`, `Delegate`, `Nullable<T>`, `WeakReference`, `BinaryData`, `String.Intern/IsInterned`, `Convert`, `Enum` (stub)

### What does NOT work
- `Regex` — `std::regex` back-end; no named groups, no lookbehind.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — POSIX-only; will not compile on Windows without Winsock2 path.
- `SynchronizationContext` — stub; `Progress<T>` calls handlers synchronously.
- `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; not implemented.
- `CopyTo(Array, int)` on `ICollection`/`BitArray`/`ArrayList` — `System.Array` type does not exist.
- `ArrayList.GetEnumerator()` — returns `nullptr`; non-generic enumerator over `std::any` not yet implemented.
- Windows / Emscripten cross-compilation — untested; POSIX guards exist but not CI-validated.

---

## 3. Recent changes

All on branch `develop`, most recent first:

| Commit | Change |
|--------|--------|
| `e9c4fec` | Port Boolean: fix Parse/TryParse to be case-insensitive with whitespace trimming (.NET parity); add 12 new case/whitespace tests |
| `315324a` | Port BitConverter: add GetBytes/To* overloads for Half, BFloat16, Int128, UInt128; BFloat16 bit-reinterpretation quartet; fix missing `<algorithm>` in Int128.hpp and UInt128.hpp; 14 new tests |
| `4b4ff33` | Port BinaryData: fix ownership, add FromStream/ToStream, implicit operators |
| `9ed4090` | CLAUDE.md: add parity philosophy, logic-parity checklist step, fix test count and plan.sqlite3 workflow |

### Fixes applied this session
- `Int128.hpp` and `UInt128.hpp` both used `std::reverse` without `#include <algorithm>` — fixed.
- `Boolean.Parse`/`TryParse` now correctly case-insensitive and whitespace-trimming per .NET spec.

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 8142 tests pass.

The workflow is user-driven: user approves each type with "ano" (yes in Czech) before porting begins. The assistant must **always ask before porting or ignoring** — this was violated in a prior session and must not happen again.

Next unprocessed type in `System` namespace (from `plan.sqlite3`): `Buffer` — a static class for low-level byte-array operations. Its `BlockCopy`/`ByteLength`/`GetByte`/`SetByte` methods require `System::Array` which does not exist in sharp-runtime; only `MemoryCopy` is directly portable.

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
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse`/reflection methods not implemented; `HasFlag`/`ToUnderlying`/`ToInt32`/`ToString` work via templates |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` — placing it inside `namespace System` causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| design note | `ArrayList` compares `std::any` elements by `type_info` only; meaningful value comparison requires a typed `IComparer` |
| workflow risk | Duplicate test suite names cause linker errors — always check `--gtest_filter` output and use `Tests2` suffix when collisions exist |

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
    Numerics/                           ← BigInteger, Vector*, Matrix*, Quaternion, BFloat16
    Diagnostics/                        ← Debug, Stopwatch, Activity
    Globalization/                      ← CultureInfo, Calendar, DateTimeFormatInfo
    Net/                                ← IPAddress, Http/, Sockets/
    Xml/                                ← XmlReader, XmlWriter, Linq/
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← ~175 GoogleTest .cpp files
vendor/                                 ← googletest, nlohmann/json, tinyxml2, miniz
plan.sqlite3                            ← tracks porting status per type (todo/ported/ignored/in_progress)
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** (`-Wall -Wextra -Werror`) before every commit.
2. **8142+ tests passing** — never go below the watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (= `int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
7. **Doxygen `/** */`** on all public declarations — `///` has been fully eliminated; never reintroduce it.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **Inner exceptions use `std::exception_ptr`** — never `const std::exception&`. Pattern: `FooException(const std::string& msg, std::exception_ptr inner) : Base(msg, std::move(inner)) {}`
10. **No broad header refactor** — property naming touches 449+ files in CNA.
11. **Push only to `develop`** — never push to `master` or create tags without explicit per-action user approval.
12. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.
13. **Always ask before porting or ignoring** — user must say "ano" for each type.

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

### plan.sqlite3 workflow
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`
- Status values: `ported`, `ignore`, `todo`, `''` (empty = unset). **`in_progress` does not exist.**
- Current counts: 30 ported, 6 ignored, 1071 remaining in System namespace
- Query next unset: `SELECT id,name,type FROM task WHERE namespace='System' AND (status IS NULL OR status='' OR status='todo') ORDER BY id LIMIT 1;`
- **Never set status without user approval per type**

### Inner exception ctor pattern (correct)
```cpp
// Header (.hpp):
FooException(const std::string& message, std::exception_ptr inner);

// Body (.cpp):
FooException::FooException(const std::string& message, std::exception_ptr inner)
    : BaseException(message, std::move(inner)) {}

// Test caller:
auto inner = std::make_exception_ptr(std::runtime_error("cause"));
FooException e("msg", inner);
EXPECT_NE(std::string(e.what()).find("msg"), std::string::npos);
// Do NOT check for inner.what() content — it is not concatenated into what()
```

### Duplicate test suite names
`EXCEPT_SIMPLE(ExType)` macro in `ExceptionRemainingTests.cpp` already defines `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception`. New test files for those same types must use a `Tests2` suffix (e.g. `ExecutionEngineExceptionTests2`) to avoid linker duplicate symbol errors.

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
./build/SharpRuntimeTests --gtest_filter="BitConverter*"

# Check next unset types in System namespace
sqlite3 plan.sqlite3 "SELECT id,name,type FROM task WHERE namespace='System' AND (status IS NULL OR status='' OR status='todo') ORDER BY id LIMIT 8;"

# Verify no /// Doxygen remains
grep -rl "^\s*///" include/System/ | wc -l

# Verify no old inner-exception pattern remains
grep -rl "const std::exception& inner" include/System/ | wc -l

# Check .NET source for a type
find /rv/tmp/runtime/src/libraries -name "Buffer.cs" | head -3

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"

# Push (develop only)
git push origin develop
```

---

## 8. Next smallest tasks

Ordered by priority. Each fits one focused coding session. **User must approve each type with "ano" before work begins.**

### Task 1 — System.Buffer
- **Goal:** Describe to user → await "ano" → port `MemoryCopy` (two overloads, raw `std::memcpy` wrapper); stub/omit `BlockCopy`/`ByteLength`/`GetByte`/`SetByte` since `System::Array` does not exist.
- **Files:** `include/System/Buffer.hpp` (create), `tests/System/BufferTests.cpp` (create)
- **Verify:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="Buffer*"`

### Task 2 — System.Byte
- **Goal:** Describe to user → await "ano" → check/port the `Byte` struct helpers (Parse, TryParse, ToString, MinValue, MaxValue constants).
- **Files:** `include/System/Byte.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="Byte*"`

### Task 3 — System.CLSCompliantAttribute
- **Goal:** Describe to user → await "ano" → likely a one-line stub attribute class (CLS compliance is not enforced in C++).
- **Files:** `include/System/CLSCompliantAttribute.hpp`
- **Verify:** Build clean.

### Task 4 — System.Char
- **Goal:** Describe to user → await "ano" → full checklist review of existing `Char` implementation; verify API parity with .NET `System.Char`.
- **Files:** `include/System/Char.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="Char*"`

### Task 5 — System.Console
- **Goal:** Describe to user → await "ano" → `Console.WriteLine`/`Write`/`ReadLine` wrappers around `std::cout`/`std::cin`; `ForegroundColor`/`BackgroundColor` via ANSI codes on POSIX.
- **Files:** `include/System/Console.hpp`, `src/System/Console.cpp`
- **Verify:** Build clean, basic output test.

---

## 9. Do not do yet

- **Do not port or ignore any type without user's "ano"** — learned from 2026-06-26 session.
- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA.
- **No LINQ port** — use `std::ranges` in all new ported code.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs.
- **No merge to `master`** — always push to `develop` only; master merge requires explicit per-action approval.
- **No new vendored libraries** without discussing scope impact.
- **No speculative API additions** — only add methods present in .NET's published API surface.
- **No work on `System::Type` / `System::Activator`** — stubs are the correct end state.
- **No `SynchronizationContext` full implementation** — synchronous stub is correct for game use.
- **No duplicate test suite names** — always check for collisions; use `Tests2` suffix.
- **No reintroduction of `///` Doxygen** — all headers now use `/** */`; never write `///` in new code.
- **No `ArrayList.Sort()` without comparer** — `std::any` cannot be compared without type info.
- **No mass rewrite or reformatting** in a single commit — incremental changes only.
- **No `System.Buffer.BlockCopy`/`ByteLength`/`GetByte`/`SetByte`** — these require `System::Array` which does not exist and is out of scope.

---

## 10. Resume prompt

```
Read NEXT.md first to understand the current state of the project.

Then open plan.sqlite3 and find the next unprocessed type:
  sqlite3 plan.sqlite3 "SELECT id,name,type FROM task WHERE namespace='System' AND (status IS NULL OR status='' OR status='todo') ORDER BY id LIMIT 1;"

IMPORTANT workflow rule: describe the type to the user and WAIT for their approval ("ano")
before doing any porting or ignoring. Never port or mark ignored without explicit approval.

For each approved type:
  1. Check the existing header (include/System/<Type>.hpp) and tests.
  2. Verify all checklist items: full API, intcs/namespace syntax, Doxygen /** */, SPDX, build clean, tests pass, logic parity with /rv/tmp/runtime/src/libraries/.
  3. Fix any issues found.
  4. Add tests if missing. Use Tests2 suffix if suite name already exists.
  5. Run: cmake --build build --parallel 4  (zero errors, zero warnings)
  6. Run: ./build/SharpRuntimeTests  (all 8142+ tests must pass)
  7. Mark ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"
  8. Commit: git -c commit.gpgsign=false commit -m "..."
  9. Push: git push origin develop

After a batch of types, update NEXT.md.

First task: next unset type in System namespace — describe it and await user approval.
```
