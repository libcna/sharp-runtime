# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-07 (branch: develop) — session 13*

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
- **All 820 tests pass:** `./build/SharpRuntimeTests` → `820 tests from 43 test suites` ✅
- GoogleTest is present at `vendor/googletest/`

### What is tested (798 tests across 42 suites)
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

### What is NOT yet tested (priority order)
1. `System::TimeZoneInfo` — **next target**
2. `System::Uri`
3. `System::Threading` primitives (Thread, Monitor, Mutex, Semaphore, etc.)

### What does NOT work yet (implementation gaps)
- **GZipStream / DeflateStream / ZipArchive:** throw `NotImplementedException` — awaiting zlib/miniz
- **XmlReader / XmlWriter:** throw `NotImplementedException` — awaiting tinyxml2/pugixml
- **TcpClient / UdpClient:** throw `NotImplementedException` — awaiting POSIX/Winsock
- **Task/TaskT:** use `std::async(std::launch::async)`, not a full threadpool scheduler
- **Thread::CurrentThread():** returns a proxy struct, not a real `Thread` — no `Join()` / `IsAlive`
- **BigInteger::TryParse:** not yet implemented
- **AppDomain / AppContext / GC:** stubs only

---

## 3. Recent changes (last 3 sessions)

**Session 13 (Debug + Trace tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Diagnostics/DebugTraceTests.cpp` | New — 22 tests: Debug::Write/WriteLine (char\*/string/empty), Debug::Assert(true) all three overloads; Trace::Write/WriteLine/TraceInformation/TraceWarning/TraceError, Trace::Assert(true/false/no-message), Trace::Fail, Trace::Flush |

**Session 12 (Stopwatch + Encoding tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Diagnostics/StopwatchTests.cpp` | New — 15 tests: default ctor (not running, zero elapsed), Start/Stop toggling isRunning, elapsed accumulates after Start+sleep+Stop, elapsed grows while running, StartNew() already-running, Reset() zeros+stops, Reset()-while-running, Restart() zeros+starts, Stop-on-stopped idempotent, Start-on-running idempotent, getElapsedProperty() ticks == getElapsedTicks, accumulates across multiple Start/Stop cycles |
| `tests/System/Text/EncodingTests.cpp` | New — 14 tests: UTF8/ASCII/Unicode factory non-null, UTF8 name == "utf-8", ASCII name contains "ascii", ASCII GetBytes("ABC") == {65,66,67}, UTF8 GetBytes("hello") correct bytes, round-trip GetString(GetBytes(s)) for both encodings, empty string edge cases, GetString with non-zero offset |

**Session 20 (primitive box tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/PrimitiveTypeTests2.cpp` | New — 98 tests: Int16/UInt16/SByte (Parse boundary+overflow+invalid, TryParse, ToString), Boolean (TrueString/FalseString, Parse case-insensitive, TryParse), Char/char16_t (IsLetter/Digit/WhiteSpace/Upper/Lower/Punctuation/Control, ToUpper/ToLower, GetNumericValue, Parse, surrogate helpers, ConvertToUtf32), Single/Double (NaN/Infinity constants, IsNaN/IsInfinity/IsFinite/IsNormal, Parse/TryParse) |

**Session 19 (Queue, Stack, LinkedList, SortedSet tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Collections/Generic/QueueStackTests.cpp` | New — 34 tests: Queue FIFO/Contains/Clear/ToArray/stress-1000; Stack LIFO/Contains/Clear/ToArray-top-first/stress-1000 |
| `tests/System/Collections/Generic/LinkedListSortedSetTests.cpp` | New — 38 tests: LinkedList/SortedSet with set-algebra, GetViewBetween, range-for |

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
tests/System/                           ← GoogleTest suites (built, 798 tests pass)
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

## 7. Next task — Task 23: TimeZoneInfo

~~Task 21 (Stopwatch + Encoding) — DONE ✅ — 29 new tests (15 Stopwatch + 14 Encoding), total 798~~
~~Task 22 (Debug + Trace) — DONE ✅ — 22 new tests, total 820~~

### `System::TimeZoneInfo` — `tests/System/TimeZoneInfoTests.cpp`

Headers to read first:
- `include/System/TimeZoneInfo.hpp`

Typical API to test (mirror .NET):
- `TimeZoneInfo::Utc` — non-null / valid UTC zone
- `TimeZoneInfo::Local` — non-null
- `TimeZoneInfo::FindSystemTimeZoneById("UTC")` — returns UTC zone, Id == "UTC"
- `TimeZoneInfo::getIdProperty()` — returns non-empty string
- `TimeZoneInfo::getDisplayNameProperty()` / `getStandardNameProperty()` — non-empty
- `TimeZoneInfo::IsAmbiguousTime(...)` / `IsDaylightSavingTime(...)` — for UTC returns false
- `TimeZoneInfo::ConvertTime(dateTime, fromZone, toZone)` — UTC→UTC identity

**Run:** `./build/SharpRuntimeTests --gtest_filter="TimeZoneInfoTests.*"`

After: run full suite `./build/SharpRuntimeTests` — must show 820+ passing, 0 failing. Then update NEXT.md (bump count, mark Task 23 done, add Task 24).

---

## 8. Do not do yet

- **No broad header refactor** — changing naming conventions across 448 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No zlib/tinyxml2/pugixml integration** until the test suite has stable broad coverage
- **No changes to `SharpRuntime::` primitive typedefs** — API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No merge to master** until test coverage is substantially broader (currently 798 tests)

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Read NEXT.md section 7 — Task 23 is `System::TimeZoneInfo` tests.
>
> Read `include/System/TimeZoneInfo.hpp`. Write `tests/System/TimeZoneInfoTests.cpp` covering the API that is actually implemented. Use `--gtest_filter="TimeZoneInfoTests.*"` to verify before the full run.
>
> Build: `cmake --build build --parallel 4` (must be clean — zero errors, zero warnings)
> Run new tests: `./build/SharpRuntimeTests --gtest_filter="TimeZoneInfoTests.*"`
> Run full suite: `./build/SharpRuntimeTests` — must show 820+ passing, 0 failing.
> Commit, then update NEXT.md: bump test count, mark Task 23 done, add Task 24.
