# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-07 (branch: develop) — session 11*

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
- **All 769 tests pass:** `./build/SharpRuntimeTests` → `769 tests from 40 test suites` ✅
- GoogleTest is present at `vendor/googletest/`

### What is tested (769 tests across 40 suites)
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
| `IO/HashingTests.cpp` | CRC32, XxHash32, XxHash64 (27) |
| `IO/StreamTests.cpp` | MemoryStream, StringReader, StringWriter (29) |
| `Collections/ImmutableCollectionTests.cpp` | ImmutableArray, ImmutableList, ImmutableDictionary (33) |
| `Collections/PriorityQueueTests.cpp` | PriorityQueue (20) |
| `Collections/Generic/CollectionsTests.cpp` | List, Dictionary, HashSet (52) |
| `Collections/Generic/QueueStackTests.cpp` | Queue, Stack (34) |
| `Collections/Generic/LinkedListSortedSetTests.cpp` | LinkedList, SortedSet (38) |
| `Numerics/BigIntegerTests.cpp` | BigInteger (45) |
| `Numerics/ComplexTests.cpp` | Complex (38) |

### What is NOT yet tested (priority order)
1. `System::Diagnostics::Stopwatch` — **next target A**
2. `System::Text::Encoding` (UTF8/ASCII) — **next target B**
3. `System::Diagnostics::Debug` / `Trace`
4. `System::TimeZoneInfo`
5. `System::Uri`
6. `System::Threading` primitives (Thread, Monitor, Mutex, Semaphore, etc.)

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

**Session 20 (primitive box tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/PrimitiveTypeTests2.cpp` | New — 98 tests covering Int16/UInt16/SByte (Parse boundary+overflow+invalid, TryParse, ToString), Boolean (TrueString/FalseString, Parse case-insensitive, TryParse), Char/char16_t (IsLetter/Digit/WhiteSpace/Upper/Lower/Punctuation/Control, ToUpper/ToLower, GetNumericValue, Parse, surrogate helpers, ConvertToUtf32), Single/Double (NaN/Infinity constants, IsNaN/IsInfinity/IsFinite/IsNormal, Parse/TryParse) |

**Session 19 (Queue, Stack, LinkedList, SortedSet tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Collections/Generic/QueueStackTests.cpp` | New — 34 tests: Queue FIFO/Contains/Clear/ToArray/stress-1000; Stack LIFO/Contains/Clear/ToArray-top-first/stress-1000 |
| `tests/System/Collections/Generic/LinkedListSortedSetTests.cpp` | New — 38 tests: LinkedList AddFirst/AddLast/getFirst/getLastProperty/RemoveFirst/RemoveLast-noop/Remove-value/Contains/Clear/range-for; SortedSet Add-bool-return/Min+Max/sorted-iteration/ToVector/set-algebra/GetViewBetween |

**Session 18 (List, Dictionary, HashSet tests + IEnumerable/List bug fix):**

| File(s) | Change |
|---------|--------|
| `include/System/Collections/Generic/IEnumerable.hpp` | Bugfix — removed duplicate `GetEnumerator()` with conflicting return type; covariant override handles it |
| `include/System/Collections/Generic/List.hpp` | Bugfix — removed illegal qualified-name method definition inside class body |
| `tests/System/Collections/Generic/CollectionsTests.cpp` | New — 52 tests for List, Dictionary, HashSet |

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
tests/System/                           ← GoogleTest suites (built, 769 tests pass)
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

## 7. Next task — Task 21: Stopwatch + Encoding

Do **both** in one session. Both headers are fully implemented and ready to test.

### A. Stopwatch — `tests/System/Diagnostics/StopwatchTests.cpp`

Header: `include/System/Diagnostics/Stopwatch.hpp` (fully header-only, no .cpp needed)

API summary:
- `Stopwatch()` — default ctor, not running, zero elapsed
- `Start()` / `Stop()` — `getIsRunningProperty()` toggles; elapsed accumulates across Stop/Start cycles
- `Reset()` — stops and zeros elapsed; `Restart()` — zeros + starts immediately
- `StartNew()` — static factory, returns an already-running stopwatch
- `getElapsedMillisecondsProperty()` — ms since start (int64)
- `getElapsedTicksProperty()` — .NET ticks (100 ns units, int64)
- `getElapsedProperty()` — `System::TimeSpan` (ticks == getElapsedTicks)

Suggested test coverage:
- Default ctor: `getIsRunningProperty() == false`, `getElapsedMillisecondsProperty() == 0`
- `Start()` → `getIsRunningProperty() == true`
- `Stop()` → `getIsRunningProperty() == false`
- Elapsed > 0 after Start + tiny `std::this_thread::sleep_for(1ms)` + Stop
- `StartNew()` → `getIsRunningProperty() == true`
- `Reset()` after running → not running, elapsed == 0
- `Restart()` → `getIsRunningProperty() == true`; `Reset()` then `Start()` produces the same result
- Stop on already-stopped: no crash, elapsed unchanged
- `getElapsedProperty().getTicksProperty() == getElapsedTicksProperty()`

**Create directory first:** `mkdir -p tests/System/Diagnostics`
**Run:** `./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*"`

---

### B. Encoding — `tests/System/Text/EncodingTests.cpp`

Header: `include/System/Text/Encoding.hpp` (factory methods implemented in `src/`)

API summary:
- `Encoding::UTF8()` → `shared_ptr<Encoding>`, `getEncodingNameProperty()` == `"utf-8"`
- `Encoding::ASCII()` → `shared_ptr<Encoding>`, name contains `"ascii"`
- `GetBytes(const string& str)` → `vector<uint8_t>`
- `GetString(const uint8_t* data, int index, int count)` → `string`

Suggested test coverage:
- `UTF8()` returns non-null
- `ASCII()` returns non-null
- `UTF8()->getEncodingNameProperty() == "utf-8"`
- `ASCII()->GetBytes("ABC")` == `{65, 66, 67}`
- `UTF8()->GetBytes("hello")` == `{104, 101, 108, 108, 111}`
- Round-trip: `GetString(GetBytes("hello world").data(), 0, n)` == `"hello world"` for both UTF8 and ASCII
- Empty string: `GetBytes("")` is empty; `GetString(nullptr or empty, 0, 0)` == `""`

**Run:** `./build/SharpRuntimeTests --gtest_filter="EncodingTests.*"`

---

After both: run `./build/SharpRuntimeTests` — must show 769+ passing, 0 failing. Then update NEXT.md (bump count, mark Task 21 done, add Task 22).

---

## 8. Do not do yet

- **No broad header refactor** — changing naming conventions across 448 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No zlib/tinyxml2/pugixml integration** until the test suite has stable broad coverage
- **No changes to `SharpRuntime::` primitive typedefs** — API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No merge to master** until test coverage is substantially broader (currently 769 tests)

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Read NEXT.md section 7 — Task 21 covers two targets this session: **Stopwatch** and **Encoding**.
>
> **Step 1 — Stopwatch:** Read `include/System/Diagnostics/Stopwatch.hpp`. Create directory `tests/System/Diagnostics/` with `mkdir -p`. Write `tests/System/Diagnostics/StopwatchTests.cpp`. Include `<thread>` and `<chrono>` for the sleep-based elapsed test. See section 7A for full API and suggested test list.
>
> **Step 2 — Encoding:** Read `include/System/Text/Encoding.hpp`. Write `tests/System/Text/EncodingTests.cpp`. See section 7B for full API and suggested test list.
>
> Build: `cmake --build build --parallel 4` (must be clean — zero errors, zero warnings)
> Run new tests: `./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*:EncodingTests.*"`
> Run full suite: `./build/SharpRuntimeTests` — must show 769+ passing, 0 failing.
> Commit, then update NEXT.md: bump test count, mark Task 21 done, add Task 22.
