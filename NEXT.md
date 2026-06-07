# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-07 (branch: develop) — session 18*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** systematic test coverage expansion. Porting (waves 1–20) is largely complete. Focus is now writing GoogleTest suites to verify every ported type.

**Key architectural decisions:**
- All new types are **header-only** `.hpp`; `.cpp` only for older/complex types (Exception hierarchy, Guid, DateTime, Convert, BinaryReader/Writer, Encoding, etc.)
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed for new `.cpp` files
- Namespace: `System`, `System::IO`, `System::Collections::Generic`, etc. using C++17 nested syntax
- Property naming convention: `getXxxProperty()` / `setXxxProperty()` for all `.NET`-style properties
- Primitive typedefs live in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, `shortcs = int16_t`, `charcs = char16_t`, etc.)
- Immutable collections use `shared_ptr<const std::container<T>>` — mutations return new instances

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` → `[100%] Built target SHARP_RUNTIME` ✅
- Output: `build/libSHARP_RUNTIME.a`
- One pre-existing warning: `Char.hpp:16` — null character in `u' '` literal (cosmetic, harmless)

### Tests
- **Tests ARE built:** `SHARP_RUNTIME_BUILD_TESTS=ON` in CMake cache ✅
- **All 1058 tests pass:** `./build/SharpRuntimeTests` → `1058 tests from 53 test suites` ✅
- GoogleTest is present at `vendor/googletest/`

### What is tested (997 tests across 50 suites)
| Suite file | Tests |
|------------|-------|
| `PrimitiveTypeTests.cpp` | Int32, Int64, UInt32 (18) |
| `PrimitiveTypeTests2.cpp` | Int16, UInt16, SByte, Boolean, Char, Single, Double (98) |
| `DecimalTests.cpp` | Decimal 128-bit (47) |
| `MathTests.cpp` | Math static methods (37) |
| `ConvertTests.cpp` | Convert static methods (43) |
| `GuidTests.cpp` | Guid (24) |
| `DateTimeTests.cpp` | DateTime + TimeSpan (23) |
| `TimeSpanTests.cpp` | TimeSpan (existing) |
| `RandomTests.cpp` | Random (existing) |
| `EventHandlerTests.cpp` | EventHandler (existing) |
| `ExceptionTests.cpp` | 11 exception types (33) |
| `Text/StringBuilderTests.cpp` | StringBuilder (27) |
| `Text/EncodingWebTests.cpp` | HtmlEncoder, UrlEncoder, JavaScriptEncoder (36) |
| `Text/JsonTests.cpp` | JsonDocument / JsonElement (28) |
| `Text/EncodingTests.cpp` | Encoding UTF8/ASCII/Unicode (14) |
| `IO/HashingTests.cpp` | CRC32, XxHash32, XxHash64 (27) |
| `IO/StreamTests.cpp` | MemoryStream, StringReader, StringWriter (29) |
| `Collections/ImmutableCollectionTests.cpp` | ImmutableArray, ImmutableList, ImmutableDictionary (33) |
| `Collections/PriorityQueueTests.cpp` | PriorityQueue (20) |
| `Collections/Generic/CollectionsTests.cpp` | List, Dictionary, HashSet (52) |
| `Collections/Generic/QueueStackTests.cpp` | Queue, Stack (34) |
| `Collections/Generic/LinkedListSortedSetTests.cpp` | LinkedList, SortedSet (38) |
| `Numerics/BigIntegerTests.cpp` | BigInteger (45) |
| `Numerics/ComplexTests.cpp` | Complex (38) |
| `Diagnostics/StopwatchTests.cpp` | Stopwatch (15) |
| `Diagnostics/DebugTraceTests.cpp` | Debug + Trace (22) |
| `TimeZoneInfoTests.cpp` | TimeZoneInfo (27) |
| `UriTests.cpp` | Uri (34) |
| `Threading/ThreadingTests.cpp` | Thread/Interlocked/Monitor/Mutex/Semaphore/SemaphoreSlim/MRE/ARE/CancellationToken/SpinLock/Volatile/Timeout (49) |
| `BitConverterTests.cpp` | BitConverter (23) |
| `ConsoleTests.cpp` | Console (17) |
| `EnvironmentTests.cpp` | Environment (8) |
| `VersionTests.cpp` | Version (19) |
| `ArrayTests.cpp` | Array (26) |
| `BufferTests.cpp` | Buffer (15) |
| `TupleTests.cpp` | Tuple2/Tuple3/Tuple4 (20) |

### What is NOT yet tested (priority order)
1. `System::Globalization` (CultureInfo, NumberFormatInfo) — **next target**
2. `System::Net` (IPAddress, HttpStatusCode)
3. `System::Buffers` (ArrayPool, OperationStatus)

### What does NOT work yet (implementation gaps)
- **GZipStream / DeflateStream / ZipArchive:** throw `NotImplementedException` — awaiting zlib/miniz
- **XmlReader / XmlWriter:** throw `NotImplementedException` — awaiting tinyxml2/pugixml
- **TcpClient / UdpClient:** throw `NotImplementedException` — awaiting POSIX/Winsock
- **Task/TaskT:** use `std::async(std::launch::async)`, not a full threadpool scheduler
- **Thread::CurrentThread():** returns a proxy struct, not a real `Thread` — no `Join()` / `IsAlive`
- **BigInteger::TryParse:** not yet implemented
- **AppDomain / AppContext / GC:** stubs only

---

## 3. Recent changes (last 6 sessions)

**Session 18 (Array/Buffer/Tuple):**

| File(s) | Change |
|---------|--------|
| `tests/System/ArrayTests.cpp` | New — 26 tests |
| `tests/System/BufferTests.cpp` | New — 15 tests |
| `tests/System/TupleTests.cpp` | New — 20 tests (Tuple2/3/4) |

**Session 17 (BitConverter/Console/Environment/Version):**

| File(s) | Change |
|---------|--------|
| `include/System/Console.hpp` | Fix: remove nonexistent `using SharpRuntime::doublecs` |
| `tests/System/BitConverterTests.cpp` | New — 23 tests |
| `tests/System/ConsoleTests.cpp` | New — 17 tests |
| `tests/System/EnvironmentTests.cpp` | New — 8 tests |
| `tests/System/VersionTests.cpp` | New — 19 tests |

**Session 16 (Threading tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Threading/ThreadingTests.cpp` | New — 49 tests: Thread/Interlocked/Monitor/Mutex/Semaphore/SemaphoreSlim/ManualResetEvent/AutoResetEvent/CancellationToken/SpinLock/Volatile/Timeout |

**Session 15 (Uri implementation + tests):**

| File(s) | Change |
|---------|--------|
| `include/System/Uri.hpp` | New — header-only URI parser (scheme/host/port/path/query/fragment/userInfo; UriKind; TryCreate) |
| `tests/System/UriTests.cpp` | New — 34 tests |

**Session 14 (TimeZoneInfo fix + tests):**

| File(s) | Change |
|---------|--------|
| `include/System/TimeZoneInfo.hpp` | Fix: `TimeSpan::Zero()` → `TimeSpan::Zero`; `make_shared` → `new` (private ctor) |
| `tests/System/TimeZoneInfoTests.cpp` | New — 27 tests |

**Session 13 (Debug + Trace tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Diagnostics/DebugTraceTests.cpp` | New — 22 tests |

**Session 12 (Stopwatch + Encoding tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Diagnostics/StopwatchTests.cpp` | New — 15 tests |
| `tests/System/Text/EncodingTests.cpp` | New — 14 tests |

*For older session history see `git log --oneline`.*

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **confirmed** | `Task` / `TaskT` use `std::async(std::launch::async)` — raw OS thread per task, no threadpool |
| **confirmed** | `XmlReader` / `XmlWriter` throw `NotImplementedException` always |
| **confirmed** | `GZipStream`, `DeflateStream`, `ZipArchive` throw `NotImplementedException` always |
| **confirmed** | `TcpClient`, `UdpClient` throw `NotImplementedException` always |
| **incomplete** | `Thread::CurrentThread()` returns a proxy struct — cannot `Join()` or check `IsAlive` |
| **incomplete** | `Char::Parse(string)` only handles 1-byte ASCII — no multi-byte UTF-8 |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` only knows UTC, Local, and a handful of hardcoded zones |
| **incomplete** | `BigInteger::TryParse` not implemented |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs |
| **risky** | `SharpRuntime::charcs = char16_t` — some headers cast to `wint_t`; not identity on all platforms |
| **known warning** | `Char.hpp:16` emits "null character in literal" — cosmetic, does not affect behaviour |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, shortcs, charcs, etc.)
  System/                               ← root namespace: exceptions, Math, Convert, ...
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, ..., Tasks/
  System/Numerics/                      ← BigInteger, Complex, GenericMathInterfaces
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ...
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for types needing it (exceptions, Guid, DateTime, Encoding, etc.)
tests/System/                           ← GoogleTest suites (built, 997 tests pass)
vendor/googletest/                      ← bundled test framework
vendor/nlohmann/json.hpp                ← nlohmann/json 3.10.4 (MIT)
```

### Invariants that must not be broken
1. **All new types are header-only** — only add `.cpp` if there is a genuine ODR or compile-time reason
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()` — never bare public fields
3. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET Foundation attribution
4. **Namespace syntax:** `namespace System::Collections::Generic {` — C++17 nested form
5. **`SharpRuntime::intcs` not `int`** in public APIs that mirror .NET `int` parameters
6. **Build must stay clean** — zero errors, zero warnings before any commit
7. **`inline` statics** in headers for ODR-safe static members

### API compatibility rules
- Method names mirror .NET exactly (PascalCase)
- Template parameter names: `TKey`, `TValue`, `TElement`, `TPriority`, etc.
- Static factory methods preferred where .NET uses them (`Empty()`, `Create()`, `Default()`)

---

## 6. Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
./build/SharpRuntimeTests

# Run a single suite
./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*"

# Run multiple suites
./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*:EncodingTests.*"

# Check for warnings/errors
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Git log
git log --oneline -15

# Count test files
find tests -name "*.cpp" | wc -l

# Count header files
find include -name "*.hpp" | wc -l
```

---

## 7. Next task — Task 28: Globalization

~~Task 21 (Stopwatch + Encoding) — DONE ✅ — 29 new tests, total 798~~
~~Task 22 (Debug + Trace) — DONE ✅ — 22 new tests, total 820~~
~~Task 23 (TimeZoneInfo fix + tests) — DONE ✅ — 27 new tests, total 847~~
~~Task 24 (Uri implementation + tests) — DONE ✅ — 34 new tests, total 881~~
~~Task 25 (Threading tests) — DONE ✅ — 49 new tests, total 930~~
~~Task 26 (BitConverter/Console/Environment/Version) — DONE ✅ — 67 new tests, total 997~~
~~Task 27 (Array/Buffer/Tuple) — DONE ✅ — 61 new tests, total 1058~~

### Batch: Globalization

Headers to read first:
- `include/System/Globalization/CultureInfo.hpp`
- `include/System/Globalization/NumberFormatInfo.hpp`
- Scan `include/System/Globalization/` for other implemented types

Write test files:
- `tests/System/Globalization/CultureInfoTests.cpp`
- `tests/System/Globalization/NumberFormatInfoTests.cpp` (or combined)

**Run:** `./build/SharpRuntimeTests --gtest_filter="CultureInfoTests.*:NumberFormatInfoTests.*"`

After: run full suite `./build/SharpRuntimeTests` — must show 1058+ passing, 0 failing. Then update NEXT.md (bump count, mark Task 28 done, add Task 29).

---

## 8. Do not do yet

- **No broad header refactor** — changing naming conventions across 448 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No zlib/tinyxml2/pugixml integration** until the test suite has stable broad coverage
- **No changes to `SharpRuntime::` primitive typedefs** — API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No merge to master** until test coverage is substantially broader (currently 997 tests)

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Read NEXT.md section 7 — Task 28 is Globalization.
>
> Scan `include/System/Globalization/` to see what's implemented. Write test files for CultureInfo, NumberFormatInfo, and any other types that have real implementation. Use `--gtest_filter="CultureInfoTests.*:NumberFormatInfoTests.*"` (or a broader filter) to verify before the full run.
>
> Build: `cmake --build build --parallel 4` (must be clean — zero errors, zero warnings)
> Run new tests with filter above.
> Run full suite: `./build/SharpRuntimeTests` — must show 1058+ passing, 0 failing.
> Commit, then update NEXT.md: bump test count, mark Task 28 done, add Task 29.
