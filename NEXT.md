# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-06 (branch: develop)*

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
- **Tests are NOT built:** `SHARP_RUNTIME_BUILD_TESTS=OFF` in current build cache
- Running `ctest` reports: *No tests were found*
- GoogleTest is present at `vendor/googletest/`

### What works
- 448 header files covering most of the .NET BCL surface used by XNA/game code
- All headers compile cleanly with `-std=c++23`
- Full `System::` namespace: exceptions, Math, Convert, DateTime/TimeSpan, Guid, String, Collections, IO, Threading, Text, Globalization, Diagnostics, Numerics, Buffers, Security, Net, Xml.Linq, Text.Json, Runtime.CompilerServices, Runtime.InteropServices
- `System.Collections.Immutable`: all 8 types (ImmutableArray, ImmutableList, ImmutableDictionary, ImmutableHashSet, ImmutableSortedDictionary, ImmutableSortedSet, ImmutableQueue, ImmutableStack)
- Hashing: CRC32, XxHash32, XxHash64 (full streaming)
- Threading: Thread, Monitor, Mutex, Semaphore/Slim, ManualResetEvent, AutoResetEvent, Interlocked, Timer, CancellationToken, SpinLock, ReaderWriterLockSlim, Barrier, CountdownEvent
- Threading.Tasks: Task, TaskT, TaskCompletionSource, ValueTask, Parallel
- Primitive boxes: Int16/Int32/Int64, UInt16/UInt32/UInt64, SByte, Byte, Char, Boolean, Single, Double, Decimal (double-backed stub)
- Collections.Generic: PriorityQueue, SortedSet (added), full interfaces
- ComponentModel: INotifyPropertyChanged, INotifyPropertyChanging, DataAnnotations, Category/Browsable/ReadOnly/DisplayName attributes
- Text.Json.Serialization: JsonPropertyName, JsonIgnore, JsonConverter, JsonPolymorphic, JsonDerivedType, etc.
- Runtime.InteropServices: StructLayout, FieldOffset, MarshalAs, DllImport, ComVisible, Guid, In, Out, Optional
- Diagnostics.CodeAnalysis: full nullable analysis + StringSyntax attributes
- Runtime.Versioning: TargetFramework, SupportedOSPlatform, UnsupportedOSPlatform, etc.
- Text.Encodings.Web: HtmlEncoder, JavaScriptEncoder, UrlEncoder

### What does NOT work yet
- **Tests cannot be run** (`SHARP_RUNTIME_BUILD_TESTS=OFF` in cache)
- **GZipStream / DeflateStream:** throw `NotImplementedException` — awaiting zlib/miniz integration
- **ZipArchive:** throws `NotImplementedException` — awaiting miniz/libzip
- **XmlReader / XmlWriter:** throw `NotImplementedException` — awaiting tinyxml2/pugixml
- **TcpClient / UdpClient:** throw `NotImplementedException` — awaiting POSIX/Winsock
- **JsonDocument::Parse:** returns a stub with raw text, no real parsing
- **Decimal:** backed by `double`, not 128-bit fixed-point — precision differs from .NET
- **Task/TaskT:** use `std::async(std::launch::async)`, not a full threadpool scheduler

---

## 3. Recent changes

**Waves 16–20 (current session):**

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

**Tests cannot be run.** The existing GoogleTest suite in `tests/System/` is not built because the build cache was configured with `SHARP_RUNTIME_BUILD_TESTS=OFF`.

- **Symptom:** `ctest` reports *No tests were found*
- **Failing command:** `cd build && ctest --output-on-failure`
- **Affected:** `tests/System/EventHandlerTests.cpp`, `RandomTests.cpp`, `TimeSpanTests.cpp`
- **Cause:** Build was initially configured without `-DSHARP_RUNTIME_BUILD_TESTS=ON`; the OFF value is cached
- **Fix needed:** Reconfigure: `cmake -S . -B build -DSHARP_RUNTIME_BUILD_TESTS=ON && cmake --build build --parallel 4`

This is not a code defect — just a configuration issue. Once tests build, their actual pass/fail status is unknown.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **confirmed** | `Decimal` is backed by `double` — loses precision beyond ~15 decimal digits; .NET `Decimal` is 128-bit |
| **confirmed** | `Task` / `TaskT` use `std::async(std::launch::async)` — spawns a raw OS thread per task, no threadpool |
| **confirmed** | `JsonDocument::Parse()` is a stub — returns an element with `rawText_` set but no real JSON parsing |
| **confirmed** | `XmlReader` / `XmlWriter` throw `NotImplementedException` always |
| **confirmed** | `GZipStream`, `DeflateStream`, `ZipArchive` throw `NotImplementedException` always |
| **confirmed** | `TcpClient`, `UdpClient` throw `NotImplementedException` always |
| **incomplete** | Tests suite exists but is not built (`SHARP_RUNTIME_BUILD_TESTS=OFF` in cache) |
| **incomplete** | `Thread::CurrentThread()` returns a proxy struct, not a full `Thread` object — cannot `Join()` or check `IsAlive` on it |
| **incomplete** | `Char::Parse(string)` only works for 1-byte ASCII chars — does not handle multi-byte UTF-8 sequences |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` only knows UTC, Local, and a handful of hardcoded zones |
| **incomplete** | `BigInteger` uses a base-10⁹ representation — correct but slower than binary; `TryParse` not present |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs with no real implementation |
| **needs verification** | `XxHash32` / `XxHash64` produce correct hashes — implementation matches the reference spec but no test verifies it |
| **needs verification** | `CRC32` lookup table correctness — polynomial 0xEDB88320, no test |
| **needs verification** | `ImmutableDictionary` / `ImmutableHashSet` compile but have zero test coverage |
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

### Task 1 — Enable and run the test suite
**Goal:** Confirm existing 3 test files pass.
**Files:** `CMakeLists.txt`, `tests/System/EventHandlerTests.cpp`, `RandomTests.cpp`, `TimeSpanTests.cpp`
**Command:**
```bash
cmake -S . -B build -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build --parallel 4
cd build && ctest --output-on-failure
```

---

### Task 2 — Add tests for Int32/Int64/UInt32 primitive boxes
**Goal:** Verify Parse/TryParse/MaxValue/MinValue are correct.
**Files:** New `tests/System/PrimitiveTypeTests.cpp`
**Command:** `./build/SharpRuntimeTests --gtest_filter="PrimitiveTypeTests.*"`

---

### Task 3 — Add tests for XxHash32/XxHash64/CRC32
**Goal:** Verify hash outputs against known test vectors from the xxHash spec.
**Files:** New `tests/System/IO/HashingTests.cpp`
**Command:** `./build/SharpRuntimeTests --gtest_filter="HashingTests.*"`

---

### Task 4 — Add tests for ImmutableArray/ImmutableList/ImmutableDictionary
**Goal:** Verify Add/Remove/SetItem return new instances and leave originals unchanged.
**Files:** New `tests/System/Collections/ImmutableCollectionTests.cpp`
**Command:** `./build/SharpRuntimeTests --gtest_filter="ImmutableCollectionTests.*"`

---

### Task 5 — Add tests for PriorityQueue<T,P>
**Goal:** Verify min-heap ordering with mixed priorities.
**Files:** New `tests/System/Collections/PriorityQueueTests.cpp`
**Command:** `./build/SharpRuntimeTests --gtest_filter="PriorityQueueTests.*"`

---

### Task 6 — Add tests for HtmlEncoder / UrlEncoder
**Goal:** Verify `&`, `<`, `>`, `"`, `'` are correctly escaped; URL percent-encoding roundtrips.
**Files:** New `tests/System/Text/EncodingWebTests.cpp`
**Command:** `./build/SharpRuntimeTests --gtest_filter="EncodingWebTests.*"`

---

### Task 7 — Wire up real JSON parsing in JsonDocument::Parse()
**Goal:** Replace the raw-text stub with actual parse logic using a bundled parser (e.g., nlohmann/json in `vendor/`).
**Files:** `include/System/Text/Json/JsonDocument.hpp`, `include/System/Text/Json/JsonElement.hpp`
**Command:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="JsonTests.*"`

---

### Task 8 — Add Decimal 128-bit precision using `__int128` or compiler intrinsics
**Goal:** Replace the double-backed `Decimal` with a fixed-point representation matching .NET semantics.
**Files:** `include/System/Decimal.hpp`
**Command:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="DecimalTests.*"`

---

## 9. Do not do yet

- **No broad header refactor** — changing naming conventions across 448 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — the plan explicitly excludes it; use `std::ranges` algorithms in ported code
- **No zlib/tinyxml2/pugixml integration** until the test suite passes cleanly — adding external deps while tests are broken creates noise
- **No changes to `SharpRuntime::` primitive typedefs** — these are API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No changes to the `getXxxProperty()` / `setXxxProperty()` convention** without updating all existing usages
- **No merge to master** until the test suite is enabled and all 3 existing tests pass
- **No new API design discussions** in code — use conversation or DOTNET_PORTING_PLAN.md instead

---

## 10. Resume prompt

> Read NEXT.md first. Then inspect only the files needed for the first task listed in section 8 (enabling and running the test suite). Do not refactor any unrelated code. Make one small, verified improvement — specifically reconfigure the build with `SHARP_RUNTIME_BUILD_TESTS=ON`, build, run `ctest`, and report the result. After finishing, update NEXT.md to reflect the new test status and promote Task 2 to the top of the list.
