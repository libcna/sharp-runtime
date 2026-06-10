# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-10 (branch: develop) — session 33*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** test coverage is ~77 % complete (347/449 headers). Remaining work is Task 42 (final headers) and then a merge to master.

**Key architectural decisions:**
- All new types are **header-only** `.hpp`; `.cpp` only for older/complex types (Exception hierarchy, Guid, DateTime, Convert, BinaryReader/Writer, Encoding, etc.)
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed
- Namespace: `System`, `System::IO`, `System::Collections::Generic`, etc. using C++17 nested syntax
- Property naming convention: `getXxxProperty()` / `setXxxProperty()` for all `.NET`-style properties
- Primitive typedefs live in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, `shortcs = int16_t`, `charcs = char16_t`, etc.)
- Immutable collections use `shared_ptr<const std::container<T>>` — mutations return new instances

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` → `[100%] Built target SHARP_RUNTIME` ✅
- Output: `build/libSHARP_RUNTIME.a`
- One pre-existing cosmetic warning: `Char.hpp:16` — null character in `u' '` literal (harmless)

### Tests
- **All 2691 tests pass** — `./build/SharpRuntimeTests` → `2691 tests from 397 test suites` ✅
- GoogleTest at `vendor/googletest/`; 64 test files in `tests/`

### Coverage overview

| Category | Headers | Status |
|----------|---------|--------|
| Tested (included in ≥1 test file) | ~375 / 449 | **~84 %** ✅ |
| Untested — pure interfaces (`IXxx`) | 43 | intentionally skipped (no logic) |
| Untested — marker/event types | 27 | intentionally skipped (no logic) |
| Untested — types with real logic | 0 | **Task 42 done** ✅ |

### Untested headers with real logic (Task 42 target)

**Trivial wrappers / typedefs — quick to cover:**
- `System/Byte.hpp`, `System/UInt64.hpp` — numeric wrappers
- `System/Action.hpp`, `Func.hpp`, `Predicate.hpp` — `std::function` typedefs
- `System/MarshalByRefObject.hpp` — empty stub
- `System/Threading/ThreadStart.hpp` — `std::function<void()>` typedef
- `SharpRuntime/SharpRuntimeHelper.hpp` — typedef sizes (intcs=int32_t etc.)

**With real logic:**
- `System/Object.hpp` — base class, GetType/ToString
- `System/Type.hpp` — GetType stub
- `System/String.hpp` — std::string wrapper
- `System/TimeZone.hpp` — legacy timezone API
- `System/AppContext.hpp`, `AppDomain.hpp`, `GC.hpp` — stubs with some methods
- `System/Diagnostics/Debugger.hpp` — IsAttached/Break stub
- `System/Collections/Comparer.hpp`, `Generic/Comparer.hpp`, `Generic/EqualityComparer.hpp`
- `System/IO/Stream.hpp`, `TextReader.hpp`, `TextWriter.hpp` — abstract bases
- `System/IO/IsolatedStorage/IsolatedStorage.hpp` — abstract stub
- `System/IO/Hashing/NonCryptographicHashAlgorithm.hpp` — base class for CRC32/XxHash
- `System/Text/EncodingProvider.hpp` — abstract
- `System/Text/Json/JsonElement.hpp` — tested indirectly via JsonDocument; worth direct test
- `System/Globalization/Calendar.hpp`, `GregorianCalendar.hpp` — excluded: reference DateTime properties not yet in DateTime.hpp
- `System/Collections/ObjectModel/KeyedCollection.hpp` — abstract template
- `System/Numerics/GenericMathInterfaces.hpp` — template interfaces
- `System/ApplicationId.hpp`
- **`System/Threading/Timer.hpp`** — ⚠️ has dangling-`this` UB: see Known Bugs

### What does NOT work yet (implementation gaps)

| Area | Status |
|------|--------|
| `GZipStream` / `DeflateStream` / `ZipArchive` | Always throw `NotImplementedException` — needs zlib/miniz |
| `XmlReader` / `XmlWriter` | Always throw `NotImplementedException` — needs tinyxml2/pugixml |
| `TcpClient` / `UdpClient` | Always throw `NotImplementedException` — needs POSIX/Winsock |
| `Threading::Timer` | **Dangling-`this` UB** — thread holds raw `this`, object may be destroyed before callback fires |
| `Task` / `TaskT` | Use `std::async(launch::async)` — one raw OS thread per task, no real threadpool |
| `Thread::CurrentThread()` | Returns proxy struct — `Join()` / `IsAlive` unavailable |
| `BigInteger::TryParse` | Not implemented |
| `Calendar.hpp` / `ISOWeek.hpp` | Excluded — `DateTime` missing `getYear/Month/Day` properties |
| `DefaultValueAttribute.hpp` | Conflicts with `DescriptionAttribute.hpp` (duplicate class def) |
| `AppDomain` / `AppContext` / `GC` | Stubs only |
| Vector2/3/4, Matrix3x2/4x4 | **Not ported at all** — belong to CNA layer, not sharp-runtime |

---

## 3. Recent changes (last 3 sessions)

**Session 33 — Task 42:**

| File | Change |
|------|--------|
| `include/System/Threading/Timer.hpp` | Fix: dangling-`this` UB → `shared_ptr<State>` shared between Timer and thread |
| `tests/Task42Tests.cpp` | New — 148 tests: Timer (4), Object (10), Type (6), String (7), Byte (6), UInt64 (5), AppContext (5), AppDomain (4), GC (6), Debugger (4), Comparer non-generic (4), Generic::Comparer (4), Generic::EqualityComparer (3), Stream (5), TextReader (5), TextWriter (7), NonCryptographicHashAlgorithm (6), IsolatedStorage (4), JsonElement (13), EncodingProvider (2), TimeZone (3), SharpRuntimeHelper (9), Action (4), Func (3), Predicate (1), MarshalByRefObject (1), ThreadStart (2), ApplicationId (2), GenericMathInterfaces (3), KeyedCollection (5) |

**Session 32 — Task 41:**

| File | Change |
|------|--------|
| `include/System/IntPtr.hpp` | Fix: `Zero{0}` ambiguous → `Zero{intptr_t(0)}` |
| `include/System/UIntPtr.hpp` | Fix: same — `Zero{uintptr_t(0)}` |
| `tests/Task41Tests.cpp` | New — 61 tests: IntPtr/UIntPtr, DaylightTime, DigitShapes, SortVersion, DecoderFallback, EncoderFallback, Win32Exception, DataAnnotations (7 types), JsonSerializerOptions, Json enums ×4, JsonSerializationAttributes ×5, JsonSerializer, DictionaryEntry, PropertyDescriptorCollection, Prop macros (DDATA/DGETTER/IDATA/IGETTER) |

**Session 31 — Task 40:**

| File | Change |
|------|--------|
| `include/System/Numerics/BFloat16.hpp` | Fix: factory methods had ambiguous `BFloat16(int)` → `BFloat16(uint16_t(...))` |
| `tests/Task40Tests.cpp` | New — 101 tests: Span/ReadOnlySpan, Half, Int128, UInt128, DateTimeOffset, TimeOnly, DBNull, FormattableString, OperatingSystem, BFloat16, DivisionRounding, StringComparer, Progress, UnicodeRange/Ranges, CancellationTokenRegistration, KeyNotFoundException, ReferenceEqualityComparer, ReadOnlyProperty |

**Session 30 — Task 39:**

| File | Change |
|------|--------|
| `include/System/Threading/PeriodicTimer.hpp` | Fix: `getTotalMilliseconds()` → `getTotalMillisecondsProperty()` |
| `tests/System/Threading/Tasks/TasksTests.cpp` | New — 44 tests: Task, TaskT, TaskCompletionSource<int/void>, ValueTask, ValueTaskT, Parallel, ParallelLoopState |
| `tests/Task39RemainingTests.cpp` | New — 48 tests: SynchronizationContext, PeriodicTimer, WaitHandle, ASCIIEncoding, UnicodeEncoding, UTF8Encoding, EncodingInfo, ReadOnlyObservableCollection, ReadOnlySet, CollectionExtensions, StoragePaths, Experimental::Property |

*For full history of Tasks 33–38 see `git log --oneline`.*

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **fixed** | `Threading::Timer` — dangling-`this` UB fixed in session 33: background thread now holds `shared_ptr<State>`, not raw `this`. |
| **confirmed** | `Task`/`TaskT` use `std::async(launch::async)` — one OS thread per task, no threadpool |
| **confirmed** | `XmlReader`/`XmlWriter` always throw `NotImplementedException` |
| **confirmed** | `GZipStream`, `DeflateStream`, `ZipArchive` always throw `NotImplementedException` |
| **confirmed** | `TcpClient`, `UdpClient` always throw `NotImplementedException` |
| **incomplete** | `Thread::CurrentThread()` returns proxy — no `Join()` / `IsAlive` |
| **incomplete** | `Char::Parse(string)` — only 1-byte ASCII; no multi-byte UTF-8 |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` — only UTC, Local, and a few hardcoded zones |
| **incomplete** | `BigInteger::TryParse` not implemented |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs |
| **known warning** | `Char.hpp:16` — null character in literal (cosmetic) |
| **excluded** | `Calendar.hpp` + `ISOWeek.hpp` — reference `DateTime` properties not yet in `DateTime.hpp` |
| **excluded** | `DefaultValueAttribute.hpp` conflicts with `DescriptionAttribute.hpp` |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, shortcs, charcs, etc.)
  SharpRuntime/Prop.hpp                 ← DDATA/DGETTER/IDATA/IGETTER property macros
  System/                               ← root namespace: exceptions, Math, Convert, ...
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, ..., Tasks/
  System/Numerics/                      ← BigInteger, Complex, BFloat16, BitOperations, MathF
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ...
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for types needing it (exceptions, Guid, DateTime, Encoding, etc.)
tests/                                  ← GoogleTest suites (63 files, 2543 tests)
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
8. **`static thread_local`** (not `mutable thread_local static`) for thread-local storage in templates

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
./build/SharpRuntimeTests --gtest_filter="SpanTests*:HalfTests*"

# Check for errors/warnings
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Find untested headers
python3 -c "
import os, re
all_h = {os.path.join(r,f)[len('include/'):] for r,_,fs in os.walk('include') for f in fs if f.endswith('.hpp')}
tested = set()
for r,_,fs in os.walk('tests'):
    for f in fs:
        if f.endswith('.cpp'):
            for m in re.finditer(r'#include [\"<]([^\"<>]+\.hpp)', open(os.path.join(r,f)).read()):
                inc=m.group(1)
                for p in ['System/','SharpRuntime/']:
                    i=inc.find(p)
                    if i>=0: inc=inc[i:]; break
                tested.add(inc)
for h in sorted(all_h-tested): print(h)
"

# Git log
git log --oneline -10
```

---

## 7. Next tasks

### Task 42 — DONE ✅ (session 33)

All 148 new tests pass. Total: **2691 tests**, 0 failing.

### Task 43 — Merge develop → master ← NEXT

- Merge `develop` → `master` (fast-forward or merge commit)
- Tag release `v0.1.0-test-coverage`

```bash
git checkout master
git merge develop
git tag v0.1.0-test-coverage
```

---

### Completed tasks (summary)

| Task | Session | Tests after |
|------|---------|-------------|
| Task 33 — Net::Sockets + IO remaining | 24 | 1610 |
| Task 34 — Collections remaining | 25 | 1732 |
| Task 35 — Diagnostics + Text remaining | 26 | 1824 |
| Task 36 — Collections::Immutable remaining | 27 | 1884 |
| Task 37 — System types, Collections, Numerics, Globalization | 28 | 2071 |
| Task 38 — Exception types + Threading remaining | 29 | 2289 |
| Task 39 — Threading::Tasks + remaining stubs | 30 | 2381 |
| Task 40 — Span, Half, Int128/UInt128, DateTimeOffset, TimeOnly + 13 more | 31 | 2482 |
| Task 41 — IntPtr/UIntPtr, Fallbacks, DataAnnotations, Json, Prop macros | 32 | 2543 |
| Task 42 — Timer UB fix + 28 remaining headers | 33 | 2691 |

---

## 8. Constraints / do not do

- **No broad header refactor** — changing naming conventions across 449 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No zlib/tinyxml2/pugixml integration** until test suite is merged to master
- **No changes to `SharpRuntime::` primitive typedefs** — API foundation used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No port of Vector2/3/4, Matrix3x2/4x4** — these belong to the CNA layer, not sharp-runtime
- **No Calendar/ISOWeek tests** until DateTime gains `getYear/Month/Day` properties

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md — **Task 43** is next: merge `develop` → `master` and tag `v0.1.0-test-coverage`.
>
> Build: `cmake --build build --parallel 4` (zero errors, zero warnings)
> Run full suite: `./build/SharpRuntimeTests` — must show 2691 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
