# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-07 (branch: develop) — session 8*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** systematic porting of .NET BCL types — waves 1–20 completed. Coverage is now very broad (448 header files). Focus is shifting from breadth to correctness and test coverage.

**Key architectural decisions:**
- All new types are **header-only** `.hpp`; `.cpp` only for older/complex types (Exception hierarchy, Guid, DateTime, Convert, BinaryReader/Writer, etc.)
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed for new `.cpp` files
- Namespace: `System`, `System::IO`, `System::Collections::Generic`, etc. using C++17 nested syntax
- Property naming convention: `getXxxProperty()` / `setXxxProperty()` for all `.NET`-style properties
- Primitive typedefs live in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, etc.) and are re-exported in `System::` classes as constants/statics
- Immutable collections use `shared_ptr<const std::container<T>>` — mutations return new instances
- Streaming hash algorithms (XxHash32/64) buffer partial blocks internally

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` → `[100%] Built target SHARP_RUNTIME` ✅
- Output: `build/libSHARP_RUNTIME.a`

### Tests
- **Test files exist:** `tests/System/EventHandlerTests.cpp`, `RandomTests.cpp`, `TimeSpanTests.cpp`
- **Tests ARE built:** `SHARP_RUNTIME_BUILD_TESTS=ON` ✅
- **All 248 tests pass:** `ctest --output-on-failure` → `100% tests passed, 0 tests failed out of 248` ✅
- GoogleTest is present at `vendor/googletest/`

### What works
- 448 header files covering most of the .NET BCL surface used by XNA/game code
- All headers compile cleanly with `-std=c++23`
- Full `System::` namespace: exceptions, Math, Convert, DateTime/TimeSpan, Guid, String, Collections, IO, Threading, Text, Globalization, Diagnostics, Numerics, Buffers, Security, Net, Xml.Linq, Text.Json, Runtime.CompilerServices, Runtime.InteropServices
- `System.Collections.Immutable`: all 8 types (ImmutableArray, ImmutableList, ImmutableDictionary, ImmutableHashSet, ImmutableSortedDictionary, ImmutableSortedSet, ImmutableQueue, ImmutableStack)
- Hashing: CRC32, XxHash32, XxHash64 (full streaming)
- Threading: Thread, Monitor, Mutex, Semaphore/Slim, ManualResetEvent, AutoResetEvent, Interlocked, Timer, CancellationToken, SpinLock, ReaderWriterLockSlim, Barrier, CountdownEvent
- Threading.Tasks: Task, TaskT, TaskCompletionSource, ValueTask, Parallel
- Primitive boxes: Int16/Int32/Int64, UInt16/UInt32/UInt64, SByte, Byte, Char, Boolean, Single, Double, Decimal (128-bit fixed-point: 96-bit mantissa + scale 0–28 + sign)
- Collections.Generic: PriorityQueue, SortedSet (added), full interfaces
- ComponentModel: INotifyPropertyChanged, INotifyPropertyChanging, DataAnnotations, Category/Browsable/ReadOnly/DisplayName attributes
- Text.Json.Serialization: JsonPropertyName, JsonIgnore, JsonConverter, JsonPolymorphic, JsonDerivedType, etc.
- Runtime.InteropServices: StructLayout, FieldOffset, MarshalAs, DllImport, ComVisible, Guid, In, Out, Optional
- Diagnostics.CodeAnalysis: full nullable analysis + StringSyntax attributes
- Runtime.Versioning: TargetFramework, SupportedOSPlatform, UnsupportedOSPlatform, etc.
- Text.Encodings.Web: HtmlEncoder, JavaScriptEncoder, UrlEncoder

### What does NOT work yet
- ~~**Tests cannot be run** (`SHARP_RUNTIME_BUILD_TESTS=OFF` in cache)~~ — **DONE: 39/39 pass** ✅
- **GZipStream / DeflateStream:** throw `NotImplementedException` — awaiting zlib/miniz integration
- **ZipArchive:** throws `NotImplementedException` — awaiting miniz/libzip
- **XmlReader / XmlWriter:** throw `NotImplementedException` — awaiting tinyxml2/pugixml
- **TcpClient / UdpClient:** throw `NotImplementedException` — awaiting POSIX/Winsock
- **JsonDocument::Parse:** ~~returns a stub~~ **DONE** — uses nlohmann/json 3.10.4 ✅
- **Task/TaskT:** use `std::async(std::launch::async)`, not a full threadpool scheduler

---

## 3. Recent changes

**Session 8 (Decimal 128-bit precision):**

| File(s) | Change |
|---------|--------|
| `include/System/Decimal.hpp` | Complete rewrite — 96-bit mantissa using `unsigned __int128`, scale 0–28, sign bit; 192-bit multiplication via `uint192` struct; full Parse/ToString, arithmetic, comparison, math methods (Abs/Truncate/Floor/Ceiling/Round) |
| `tests/System/DecimalTests.cpp` | New — 47 tests: constants, Parse/ToString roundtrip, 0.1+0.2==0.3 precision test, all arithmetic operators, comparison, math methods, conversions |

**Session 7 (real JSON parsing via nlohmann/json):**

| File(s) | Change |
|---------|--------|
| `vendor/nlohmann/json.hpp` | New — nlohmann/json 3.10.4 (copied from mesh-craft vendor; MIT license) |
| `CMakeLists.txt` | Add `vendor/` to `target_include_directories` so `#include "nlohmann/json.hpp"` works |
| `include/System/Text/Json/JsonDocument.hpp` | Rewritten — `Parse()` now uses `nlohmann::json::parse()` and recursively builds the `JsonElement` tree; `ParseValue()` delegates to `Parse()` |
| `tests/System/Text/JsonTests.cpp` | New — 28 tests covering all JSON value kinds, object/array nesting, error handling, Dispose |

**Session 6 (encoder tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Text/EncodingWebTests.cpp` | New — 36 tests for HtmlEncoder (13), UrlEncoder (13), JavaScriptEncoder (10); vectors sourced from the official .NET runtime tests |

**Session 5 (PriorityQueue tests + official hash vectors):**

| File(s) | Change |
|---------|--------|
| `tests/System/Collections/PriorityQueueTests.cpp` | New — 20 tests for PriorityQueue min-heap (ordering, edge cases, EnqueueRange, Clear, 1000-element stress) |
| `tests/System/IO/HashingTests.cpp` | Extended — 3 new tests using official .NET runtime test vectors for CRC32, XxHash32, XxHash64 |

**Note:** The .NET runtime source is available at `/rv/tmp/runtime/src/libraries` — use it for official test vectors before writing any new hash/collection tests.

**Session 4 (immutable collection tests):**

| File(s) | Change |
|---------|--------|
| `tests/System/Collections/ImmutableCollectionTests.cpp` | New — 33 tests for ImmutableArray, ImmutableList, ImmutableDictionary |

**Session 3 (hashing tests + bug fix):**

| File(s) | Change |
|---------|--------|
| `tests/System/IO/HashingTests.cpp` | New — 24 tests for Crc32, XxHash32, XxHash64 (spec vectors, streaming, Reset, seed variation) |
| `include/System/IO/Hashing/Crc32.hpp`, `XxHash32.hpp`, `XxHash64.hpp` | Bug fix — added `using NonCryptographicHashAlgorithm::Append;` to expose vector overload hidden by the override |

**Waves 16–20 (session 2):**

| File(s) | Change |
|---------|--------|
| `include/System/Int16.hpp`, `UInt16.hpp`, `UInt32.hpp`, `UInt64.hpp`, `SByte.hpp`, `Byte.hpp` | New — primitive type boxes with MaxValue/MinValue/Parse/TryParse |
| `include/System/Int32.hpp` | Updated — filled in MaxValue/MinValue/Parse/TryParse (was empty) |
| `include/System/Char.hpp` | New — IsLetter/IsDigit/IsSurrogate/ToUpper/ToLower/GetNumericValue |
| `include/System/Boolean.hpp` | New — TrueString/FalseString, Parse/TryParse |
| `include/System/Single.hpp`, `Double.hpp` | New — NaN/Infinity/Epsilon constants, IsNaN/IsFinite/Parse |
| `include/System/Decimal.hpp` | New — double-backed stub with full arithmetic operators |
| `include/System/Collections/Generic/PriorityQueue.hpp` | New — min-heap, Enqueue/Dequeue/TryDequeue/TryPeek |
| `include/System/Collections/Generic/SortedSet.hpp` | New — wraps `std::set`, GetViewBetween/UnionWith/IntersectWith |
| `include/System/Collections/Specialized/ListDictionary.hpp` | New — ordered key-value list |
| `include/System/Collections/ObjectModel/ReadOnlyObservableCollection.hpp` | New |
| `include/System/Threading/Tasks/Task.hpp` | New — `std::async`-backed Task/TaskT |
| `include/System/Threading/Tasks/TaskCompletionSource.hpp` | New — `std::promise`-backed |
| `include/System/Threading/Tasks/ValueTask.hpp` | New |
| `include/System/Threading/Tasks/Parallel.hpp` | New — For/ForEach/Invoke |
| `include/System/Threading/CountdownEvent.hpp`, `Barrier.hpp` | New |
| `include/System/Threading/Thread.hpp` | Modified — added `CurrentThread()` proxy |
| `include/System/Text/Encodings/Web/HtmlEncoder.hpp`, `UrlEncoder.hpp`, `JavaScriptEncoder.hpp` | New |
| `include/System/Text/Json/Serialization/JsonSerializationAttributes.hpp` | New |
| `include/System/ComponentModel/INotifyPropertyChanging.hpp` | New |
| `include/System/ComponentModel/CategoryAttribute.hpp` | New — Category/Browsable/ReadOnly/DisplayName/TypeConverter/Designer |
| `include/System/ComponentModel/DataAnnotations/DataAnnotationAttributes.hpp` | New |
| `include/System/Runtime/InteropServices/InteropAttributes.hpp` | New |
| `include/System/Runtime/Versioning/VersioningAttributes.hpp` | New |
| `include/System/Diagnostics/CodeAnalysis/CodeAnalysisAttributes.hpp` | New |
| `include/System/Numerics/GenericMathInterfaces.hpp` | New — INumber/IFloatingPoint/IBinaryInteger stubs |
| `include/System/ISpanFormattable.hpp`, `ISpanParsable.hpp` | New |
| `DOTNET_PORTING_PLAN.md` | Updated — all completed namespaces/types marked ✅ DONE |

---

## 4. Current blocker / main problem

**No critical blocker.** The test suite is now enabled and all 39 tests pass.

- `cmake -S . -B build -DSHARP_RUNTIME_BUILD_TESTS=ON && cmake --build build --parallel 4` — ✅ clean
- `cd build && ctest --output-on-failure` — ✅ `100% tests passed, 0 tests failed out of 39`

**Next focus:** expand test coverage — hashing, immutable collections (see section 8).

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| ~~**confirmed**~~ **fixed** | `Decimal` is now 128-bit fixed-point (96-bit mantissa, scale 0–28, sign); 0.1+0.2==0.3 passes; 47 tests cover full API |
| **confirmed** | `Task` / `TaskT` use `std::async(std::launch::async)` — spawns a raw OS thread per task, no threadpool |
| ~~**confirmed**~~ **fixed** | `JsonDocument::Parse()` now uses nlohmann/json 3.10.4 (vendor/nlohmann/json.hpp) to build a full `JsonElement` tree; 28 tests cover primitives, objects, arrays, nesting, error handling, and Dispose |
| **confirmed** | `XmlReader` / `XmlWriter` throw `NotImplementedException` always |
| **confirmed** | `GZipStream`, `DeflateStream`, `ZipArchive` throw `NotImplementedException` always |
| **confirmed** | `TcpClient`, `UdpClient` throw `NotImplementedException` always |
| ~~**incomplete**~~ **fixed** | Test suite is now built and all 39 tests pass (`SHARP_RUNTIME_BUILD_TESTS=ON`) |
| **incomplete** | `Thread::CurrentThread()` returns a proxy struct, not a full `Thread` object — cannot `Join()` or check `IsAlive` on it |
| **incomplete** | `Char::Parse(string)` only works for 1-byte ASCII chars — does not handle multi-byte UTF-8 sequences |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` only knows UTC, Local, and a handful of hardcoded zones |
| **incomplete** | `BigInteger` uses a base-10⁹ representation — correct but slower than binary; `TryParse` not present |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs with no real implementation |
| ~~**needs verification**~~ **verified** | `XxHash32` / `XxHash64` spec test vectors pass (empty-string canonical values); streaming == one-shot confirmed for short, medium, and 100-byte inputs |
| ~~**needs verification**~~ **verified** | `CRC32` lookup table correct — standard test vector `"123456789"` → `0xCBF43926` passes |
| ~~**needs verification**~~ **verified** | `ImmutableArray`, `ImmutableList`, `ImmutableDictionary` — 33 tests covering Add/Remove/SetItem/Insert/Sort/Contains/IndexOf/Clear; all operations confirmed to return new instances and leave originals unchanged |
| **risky assumption** | `SharpRuntime::charcs = char16_t` — some headers cast char16_t to/from `wint_t` which may not be identity on all platforms |

---

## 6. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, etc.)
  System/                               ← root namespace: exceptions, Math, Convert, ...
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/, IsolatedStorage/
  System/Text/                          ← StringBuilder, Encoding, Rune, Unicode/, RegularExpressions/, Json/, Encodings/
  System/Threading/                     ← Thread, Monitor, Mutex, ..., Tasks/
  System/Numerics/                      ← BigInteger, Complex, BFloat16, GenericMathInterfaces
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ...
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, StandardFormat, IMemoryOwner, OperationStatus
src/                                    ← .cpp for types that need it (exceptions, Guid, DateTime, BinaryReader, etc.)
tests/System/                           ← GoogleTest suites (currently not built)
vendor/googletest/                      ← bundled test framework
```

### Invariants that must not be broken
1. **All new types are header-only** — only add a `.cpp` if there is a genuine ODR or compile-time reason (e.g., a non-inline `inline` static that would cause linker errors)
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()` — never bare public fields for ported .NET properties
3. **SPDX header on every file** — three lines: MIT license, Robert Vokac copyright, .NET Foundation attribution
4. **Namespace syntax:** `namespace System::Collections::Generic {` — C++17 nested form, never nested braces
5. **`SharpRuntime::intcs` not `int`** in public APIs that mirror .NET `int` parameters
6. **Build must stay clean** — `cmake --build build --parallel 4` must produce zero errors and zero warnings before any commit
7. **`inline` statics** in headers for ODR-safe static members (e.g., `Decimal::Zero`, `Rune::ReplacementChar`, CRC32 lookup table)

### API compatibility rules
- Method names mirror .NET exactly (PascalCase)
- Template parameter names mirror .NET exactly (`TKey`, `TValue`, `TElement`, `TPriority`, etc.)
- Static factory methods preferred over constructors where .NET uses them (`Empty()`, `Create()`, `Default()`)
- `Shared()` for pool singletons, `Default()` for options singletons

---

## 7. Useful commands

```bash
# Configure (first time or to enable tests)
cmake -S . -B build -DSHARP_RUNTIME_BUILD_TESTS=ON

# Build
cmake --build build --parallel 4

# Run tests (after configuring with SHARP_RUNTIME_BUILD_TESTS=ON)
cd build && ctest --output-on-failure

# Run tests directly
./build/SharpRuntimeTests

# Run a single test suite
./build/SharpRuntimeTests --gtest_filter="TimeSpanTests.*"

# Count header files
find include -name "*.hpp" | wc -l

# Check build is clean (no errors, no warnings)
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# See recent git log
git log --oneline -15

# Check what is on develop vs master
git diff master..develop --stat
```

---

## 8. Next smallest tasks

### ~~Task 1 — Enable and run the test suite~~ DONE ✅
All 39 tests pass. `SHARP_RUNTIME_BUILD_TESTS=ON` is now in the build cache.

---

### ~~Task 1 (was Task 2) — Add tests for Int32/Int64/UInt32 primitive boxes~~ DONE ✅
18 tests in `tests/System/PrimitiveTypeTests.cpp` — all pass.
Also fixed: `Int64` was missing Parse/TryParse (added). `UInt32::Parse` silently truncated overflow on 64-bit (fixed with range check).

---

### ~~Task 1 (was Task 3) — Add tests for XxHash32/XxHash64/CRC32~~ DONE ✅
24 tests in `tests/System/IO/HashingTests.cpp` — all pass.
Also fixed: `XxHash32`, `XxHash64`, and `Crc32` were missing `using NonCryptographicHashAlgorithm::Append;`, which hid the base-class vector overload (the static `HashToUInt32/64` methods were also affected). Fixed with a one-line `using` declaration in each header.

---

### ~~Task 1 (was Task 4) — Add tests for ImmutableArray/ImmutableList/ImmutableDictionary~~ DONE ✅
33 tests in `tests/System/Collections/ImmutableCollectionTests.cpp` — all pass.
All operations confirmed immutable: originals unchanged, mutations return new instances.

---

### ~~Task 1 (was Task 5) — Add tests for PriorityQueue<T,P>~~ DONE ✅
20 tests in `tests/System/Collections/PriorityQueueTests.cpp` — all pass.
Covers: empty-queue throws, single-element Peek/Dequeue, min-heap ordering, negative/equal priorities, TryDequeue/TryPeek, EnqueueRange, Clear, 1000-element stress.

---

### ~~Task 1 (was Task 6) — Add tests for HtmlEncoder / UrlEncoder~~ DONE ✅
36 tests in `tests/System/Text/EncodingWebTests.cpp` — all pass.
Covers HtmlEncoder (5 special chars + composites from official .NET tests), UrlEncoder (unreserved chars, percent-encoding, Decode roundtrip, + handling), JavaScriptEncoder (backslash, quote, newline, control chars, composite).

---

### ~~Task 1 (was Task 7) — Wire up real JSON parsing in JsonDocument::Parse()~~ DONE ✅
nlohmann/json 3.10.4 added to `vendor/nlohmann/json.hpp`. `JsonDocument::Parse()` now builds a full `JsonElement` tree. 28 tests in `tests/System/Text/JsonTests.cpp` — all pass.

---

### ~~Task 1 (was Task 8) — Add Decimal 128-bit precision using `__int128`~~ DONE ✅
47 tests in `tests/System/DecimalTests.cpp` — all pass. 0.1 + 0.2 == 0.3 exactly. MaxValue/MinValue correct. Full arithmetic, comparison, and math methods implemented.

---

### Task 1 (was Task 9) — Add tests for BigInteger
**Goal:** Verify the base-10⁹ `BigInteger` implementation handles large values correctly.
**Files:** `include/System/Numerics/BigInteger.hpp`
**Command:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="BigIntegerTests.*"`
**Reference vectors:** `/rv/tmp/runtime/src/libraries/System.Numerics.BigInteger/tests/`

---

## 9. Do not do yet

- **No broad header refactor** — changing naming conventions across 448 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — the plan explicitly excludes it; use `std::ranges` algorithms in ported code
- **No zlib/tinyxml2/pugixml integration** until the test suite passes cleanly — adding external deps while tests are broken creates noise
- **No changes to `SharpRuntime::` primitive typedefs** — these are API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No changes to the `getXxxProperty()` / `setXxxProperty()` convention** without updating all existing usages
- **No merge to master** until the test suite has broad coverage beyond the existing 248 tests
- **No new API design discussions** in code — use conversation or DOTNET_PORTING_PLAN.md instead

---

## 10. Resume prompt

> Read NEXT.md first. The .NET runtime source is at `/rv/tmp/runtime/src/libraries`. Task 1 in section 8 is BigInteger tests: read `include/System/Numerics/BigInteger.hpp` to understand the current base-10⁹ implementation, check reference vectors at `/rv/tmp/runtime/src/libraries/System.Numerics.BigInteger/tests/`, and write tests in `tests/System/Numerics/BigIntegerTests.cpp`. Build with `cmake --build build --parallel 4`, run with `./build/SharpRuntimeTests --gtest_filter="BigIntegerTests.*"`. Update NEXT.md when done.
