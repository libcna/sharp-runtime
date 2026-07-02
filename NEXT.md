# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-07-02 (branch: develop) — 8467 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — driven by a `plan.sqlite3` namespace review workflow. **As of 2026-07-02 this workflow is fully autonomous** (see §3a) — no more per-item user confirmation. Currently working through the `System` namespace alphabetically.
- **Header count:** ~600 `.hpp` files across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`, `System.Buffers`, etc.
- **Test file count:** ~299 GoogleTest `.cpp` files.
- **Key architectural decisions:** No runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (int32_t), `bytecs` (uint8_t), etc. Inner exceptions use `std::exception_ptr`.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **8467 tests passing** across 895 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int16`, `Int32`, `Int64`, `Int128`, `IntPtr`, `UInt16`, `UInt64`, `UInt128`, `Half`, `Single`, `Double`, `Decimal` (+ OACurrency), `Guid`, `BitConverter` (full API including Half/BFloat16/Int128/UInt128), `Math` (full overloads + BigMul/DivRem/ILogB), `MathF`, `Random`, `HashCode`, `Void`, `Index`, `Lazy<T>`
- Delegates/Events: `Func<T>` (full `FuncT..FuncT16` arities), `Action`/`ActionT..ActionT16`, `Converter<T,R>`, `EventHandler<T>`, `EventArgs`, `Delegate`
- Attributes: `Attribute`, `FlagsAttribute`, `ObsoleteAttribute`, `SerializableAttribute`, `CLSCompliantAttribute`
- Enums: `Casing`, `CrashReason`, `GCCollectionMode`, `GCKind`, `GCLatencyMode`, `GCNotificationStatus`, `EnvironmentVariableTarget`, `MidpointRounding`
- Formatting: `FormattableString`, `FormattableStringFactory`, `IFormatProvider`, `IFormattable` (now includes the `ToString(format, provider)` overload), `ISpanFormattable`, `IUtf8SpanFormattable<T>`, `ICustomFormatter`
- Interfaces: `IAsyncDisposable` (`DisposeAsync()` returns `ValueTask`), `IAsyncResult` (full 4-member surface incl. `AsyncState`/`AsyncWaitHandle`), `ICloneable`, `IComparable<T>`, `IConvertible` (full surface incl. `ToDecimal`/`ToDateTime`), `IDisposable`, `IEquatable<T>`, `IObservable<T>`, `IObserver<T>`, `IParsable<T>`, `IProgress<T>`, `IServiceProvider`, `ISpanParsable<T>`, `IUtf8SpanParsable<T>`
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy — all `std::exception_ptr` inner-exception ctors, `HResult`/`Source`/`HelpLink` on the `Exception` base, `HResult` now correctly set per-type on `ExecutionEngineException`/`FieldAccessException`/`FormatException` (was previously inheriting the base `COR_E_EXCEPTION` default — audit other exception types for the same gap, see §5), all `/** */` Doxygen on all types
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
- Threading: `Thread`, `ThreadPool`, `Monitor`, `Mutex`, `SemaphoreSlim`, `AutoResetEvent`, `ManualResetEvent`, `ManualResetEventSlim`, `Interlocked`, `CancellationToken/Source`, `Barrier`, `CountdownEvent`, `Lock`, `AsyncLocal<T>`, `LazyInitializer`, `WaitHandle`, `EventWaitHandle`
- Threading.Tasks: `Task`, `Task<T>`, `ValueTask`, `ValueTask<T>`, `TaskCompletionSource<T>`
- Numerics: `BigInteger`, `Complex`, `BFloat16`, `Vector2/3/4`, `Matrix3x2`, `Matrix4x4`, `Quaternion`, `Plane`
- Diagnostics: `Debug`, `Trace`, `Stopwatch`, `DiagnosticListener`, `Activity`
- Globalization: `CultureInfo`, `DateTimeFormatInfo`, `NumberFormatInfo`, `TextInfo`, `IdnMapping`, `Calendar` types, `CompareInfo`, `RegionInfo`
- Net: `IPAddress`, `IPEndPoint`, `HttpStatusCode`, `HttpMethod`, `Uri`, sockets (POSIX-only)
- Net.Http: `HttpClient`, `HttpRequestMessage`, `HttpResponseMessage`, `HttpContent` (no TLS)
- Xml: `XmlReader`, `XmlWriter`, `XmlDocument`, `XElement`, `XDocument` (via tinyxml2)
- Runtime handles: `RuntimeTypeHandle`, `RuntimeMethodHandle`, `RuntimeFieldHandle`, `ModuleHandle`, `ValueType`
- Other: `Environment` (full), `AppDomain`, `AppContext`, `GC` (stubs, now with full overload surface — see recent changes), `DBNull` (full `IConvertible`), `Delegate`, `Nullable<T>`, `WeakReference`, `BinaryData`, `String.Intern/IsInterned`, `Convert`, `Enum` (stub)

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

All on branch `develop`, most recent first (38 commits since the previous NEXT.md update on 2026-06-29 — showing the most recent batch):

| Commit | Change |
|--------|--------|
| `d01d1b4` | Port IFormattable: add `ToString(format, IFormatProvider)` overload (default forwards to 1-arg version); stale doc-comment corrected |
| `00e651b` | Port IConvertible: add missing `ToDecimal()`/`ToDateTime()`; implement in `DBNull` |
| `d61ffb8` | Port IAsyncResult: add missing `AsyncState`/`AsyncWaitHandle` properties |
| `47b2f90` | Port IAsyncDisposable: fix `DisposeAsync()` to return `ValueTask` instead of `void` |
| `da58de8` | Port GCMemoryInfo: fix `PinnedObjectsCount` type mismatch (was `bool`, should be `long`); add 5 missing properties |
| `1dfb24a` | Port GC: add missing `TryStartNoGCRegion`/`WaitForFullGCApproach`/`Complete` overloads |
| `74ac3ff` | Port Func: extend `FuncT4..FuncT16` for full arity parity with `Action` |
| `9473526` | Port FormatException: fix missing `HResult` (`COR_E_FORMAT`) |
| `80a1089` | **plan.sqlite3 workflow: switch to autonomous batch processing, add `tobedecided` status** (see §3a) |
| `51487b3` | Port FieldAccessException: fix missing `HResult` (`COR_E_FIELDACCESS`) |
| `195bd31` | Port ExecutionEngineException: fix missing `HResult` (`COR_E_EXECUTIONENGINE`) |
| `bab45c2` | Port Exception: add `HResult`/`Source`/`HelpLink` properties |
| `4cf6c68`–`d78e6c8` | Ports of EventHandler, EventArgs, EnvironmentVariableTarget, Environment, Enum, DuplicateWaitObjectException, Double, DllNotFoundException, Decimal, DateTimeOffset, DateTime, DateOnly, DBNull, Convert, ContextMarshalException, ConsoleSpecialKey, ConsoleKeyInfo, CharEnumerator, Char, Casing, CLSCompliantAttribute, Byte, System.Buffer — see `git log` for full messages |

### §3a — Workflow change: autonomous plan.sqlite3 processing (2026-07-02)

The `plan.sqlite3` review workflow **no longer asks the user before each type**. Full rules live in
`prompt.md` (the canonical source — read it, not this summary). Short version:

- Classify each `todo`/empty item yourself: port it, `ignore` it (with `outofscope` set appropriately),
  or — if genuinely ambiguous — set `status = 'tobedecided'` and move on without guessing.
- Never stop between items to ask for confirmation.
- State lives in `plan.sqlite3` + git history, not conversation memory, so this resumes cleanly from
  a fresh context: just re-open `prompt.md` and continue from its Step 1.
- Still build clean + all tests passing before every commit; still one port per commit; still never push
  without being explicitly asked in that turn.

A recurring pattern found while re-reviewing already-"ported" types under this workflow: several
exception types set no custom `HResult` because the `HResult` property itself was added to the
`Exception` base class after those types were originally ported. `ExecutionEngineException`,
`FieldAccessException`, and `FormatException` have been fixed so far — **assume other exception
types ported before `bab45c2` have the same gap** and check `HResult` against `HResults.cs` /
`/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs` when reviewing them.

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 8467 tests pass.

The workflow is now autonomous (§3a) — do **not** revert to asking the user before each type; that was
the old behavior and has been intentionally replaced.

Next unprocessed types in `System` namespace (from `plan.sqlite3`, in processing order): `Guid`, `Half`,
`HashCode`, `IObservable`, `IObserver`, `IParsable`, `IProgress`, `IServiceProvider`, `ISpanFormattable`
(known related gap: its `TryFormat` is also missing the `IFormatProvider` parameter that `.NET` declares
— fix while there, same pattern as the IFormattable fix in `d01d1b4`), `ISpanParsable`, `IStructuralComparable`, …
944 `todo` + 62 empty items remain across all namespaces; 139 were in `System` at last count.

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
| incomplete | `ISpanFormattable::TryFormat` — missing the `IFormatProvider` parameter .NET declares; fix when this item comes up in `plan.sqlite3` (id 2748) |
| audit needed | Exception types ported before `bab45c2` may set no per-type `HResult` — see §3a |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub | `System::GC` — all methods are no-ops (by design — this is the correct end state, not a gap) |
| stub | `System::Type` — no runtime reflection |
| stub | `System::Activator` — `CreateInstance` not implementable without reflection |
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse`/reflection methods not implemented; `HasFlag`/`ToUnderlying`/`ToInt32`/`ToString` work via templates |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` — placing it inside `namespace System` causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| design note | `ArrayList` compares `std::any` elements by `type_info` only; meaningful value comparison requires a typed `IComparer` |
| workflow risk | Duplicate test suite names cause linker errors — always check `--gtest_filter` output and use `Tests2` suffix when collisions exist |
| in-flight (not this session) | Uncommitted changes to `Stream.hpp`/`MemoryStream.hpp`/`StreamTests.cpp` were present in the working tree as of 2026-07-02 (adding `Position` get/set) — belongs to a different, unfinished piece of work; do not discard, but do not assume it's done either |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  SharpRuntime/Prop.hpp                 ← property macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root
  System/                               ← ~600 .hpp files
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
tests/                                  ← ~299 GoogleTest .cpp files
vendor/                                 ← googletest, nlohmann/json, tinyxml2, miniz
plan.sqlite3                            ← tracks porting status per type (gitignored, local-only)
prompt.md                               ← canonical plan.sqlite3 workflow instructions (read this, not just NEXT.md)
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** (`-Wall -Wextra -Werror`) before every commit.
2. **8467+ tests passing** — never go below the watermark.
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
13. **plan.sqlite3 processing is autonomous** (since 2026-07-02, see §3a) — do NOT ask the user per type; classify and proceed. This reverses the previous "always ask" rule.

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

### plan.sqlite3 workflow (see prompt.md for the full canonical version)
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`
- Status values: `ported`, `ignore`, `todo`, `tobedecided`, `''` (empty = unset). **`in_progress` does not exist.**
- Current counts: 95 ported, 6 ignore, 944 todo, 62 empty (unset), plus large `ignored`/`in_progress` legacy buckets not part of the active workflow
- Query next unset: `sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 1;"`
- **Fully autonomous — no per-type user approval required** (changed 2026-07-02)

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

### HResult pattern for exception types (added 2026-07-02, see §3a)
```cpp
// In every constructor, after the base-class init list:
FooException::FooException()
    : SystemException(DefaultMsg) {
    setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131XXX)); // COR_E_FOO — from HResults.cs
}
```
Look up the exact HResult constant in `/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs`.

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

# Check next unset types (System namespace prioritized)
sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 8;"

# Verify no /// Doxygen remains
grep -rl "^\s*///" include/System/ | wc -l

# Verify no old inner-exception pattern remains
grep -rl "const std::exception& inner" include/System/ | wc -l

# Check .NET source for a type
find /rv/tmp/runtime/src/libraries -name "Buffer.cs" | head -3

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"

# Push (develop only, only when explicitly asked)
git push origin develop
```

---

## 8. Next smallest tasks

Ordered by `plan.sqlite3` processing order. Workflow is now autonomous (§3a) — no approval needed,
just classify and proceed per `prompt.md`.

### Task 1 — System.Guid
- **Goal:** Full checklist review of the existing `Guid` struct against `.NET`'s ref surface (many overloads: byte-array ctors, `ReadOnlySpan<byte>` ctors incl. big-endian, `Parse`/`TryParse` family, `IComparable`/`IEquatable`/`IFormattable`/`IParsable`/`ISpanFormattable`/`IUtf8SpanFormattable`/`IUtf8SpanParsable` interface conformance).
- **Files:** `include/System/Guid.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="Guid*"`

### Task 2 — System.Half
- **Goal:** Full checklist review of the existing `Half` struct (IEEE 754 binary16) against `.NET`'s ref surface.
- **Files:** `include/System/Half.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="Half*"`

### Task 3 — System.HashCode
- **Goal:** Full checklist review of `HashCode.Combine`/`Add`/`ToHashCode` against `.NET`'s ref surface.
- **Files:** `include/System/HashCode.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="HashCode*"`

### Task 4 — System.IObservable / System.IObserver
- **Goal:** Checklist review of both generic interfaces together (they're a pair — `IObservable<T>.Subscribe(IObserver<T>)`).
- **Files:** `include/System/IObservable.hpp`, `include/System/IObserver.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="IObservable*:IObserver*"`

### Task 5 — System.ISpanFormattable
- **Goal:** Checklist review; known gap going in — `TryFormat` is missing the `IFormatProvider` parameter .NET declares (`bool TryFormat(Span<char>, out int, ReadOnlySpan<char>, IFormatProvider?)`). Fix the same way `IFormattable::ToString` was fixed in `d01d1b4` (non-pure default overload forwarding to the simpler one, so existing implementers aren't broken).
- **Files:** `include/System/ISpanFormattable.hpp`, `tests/System/InterfaceTests2.cpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="ISpanFormattable*"`

---

## 9. Do not do yet

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
- **No per-item user confirmation in the plan.sqlite3 workflow** — this was the *old* rule and has been intentionally reversed as of 2026-07-02 (§3a). Do not reintroduce it; classify and proceed autonomously per `prompt.md`.
- **No touching the in-flight `Stream`/`MemoryStream` `Position` changes** currently sitting uncommitted in the working tree unless you're the one continuing that specific piece of work — see §5.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation). NEXT.md is a snapshot for context, not the source of truth for process.

Then open plan.sqlite3 and find the next unprocessed type (System namespace first):
  sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 1;"

For each item, per prompt.md:
  1. Look up what it does in /rv/tmp/runtime/src/libraries/ and classify it yourself — port / ignore
     (+ outofscope) / tobedecided (if genuinely ambiguous). Do not ask the user.
  2. If porting: check the existing header/tests, review against the FULL checklist in CLAUDE.md
     (API surface, doc-comments, SPDX, logic parity incl. HResult where applicable, build, tests)
     as if it were new — fix gaps, don't rubber-stamp.
  3. Run: cmake --build build --parallel 4  (zero errors, zero warnings)
  4. Run: ./build/SharpRuntimeTests  (all 8467+ tests must pass)
  5. Mark ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"
  6. Commit only the files for that port: git -c commit.gpgsign=false commit -m "..."
  7. Loop back to step 1 — keep going, do not stop to ask between items.
  8. Never push without the user explicitly asking in that turn.

After a batch of types, update NEXT.md.
```
