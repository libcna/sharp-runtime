# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-10 (branch: develop) — session 34*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** test coverage essentially complete. Remaining work is filling implementation gaps (XmlReader/Writer, TcpClient/UdpClient, ZipArchive) and namespace-by-namespace audit.

**Key architectural decisions:**
- Complex types have `.hpp` declarations + `.cpp` bodies; simple types remain header-only
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
- **All 2828 tests pass** — `./build/SharpRuntimeTests` → `2828 tests from 413 test suites` ✅
- GoogleTest at `vendor/googletest/`; 65 test files in `tests/`

### Coverage overview

| Category | Headers | Status |
|----------|---------|--------|
| Tested (included in ≥1 test file) | ~390 / 449 | **~87 %** ✅ |
| Untested — pure interfaces (`IXxx`) | 43 | intentionally skipped (no logic) |
| Untested — marker/event types | 27 | intentionally skipped (no logic) |
| Untested — types with real logic | ~0 | ✅ |

---

## 3. Recent changes (last 3 sessions)

**Session 34 — GZipStream / DeflateStream + audits:**

| File | Change |
|------|--------|
| `include/System/IO/Compression/GZipStream.hpp` | Rewritten: PIMPL (`ZlibGZipState`), full Doxygen, declarations only |
| `src/System/IO/Compression/GZipStream.cpp` | New — zlib `inflateInit2(16+MAX_WBITS)` / `deflateInit2(16+MAX_WBITS)`; 64 KB buffers; Z_FINISH on Close |
| `include/System/IO/Compression/DeflateStream.hpp` | Rewritten: PIMPL (`ZlibDeflateState`), full Doxygen |
| `src/System/IO/Compression/DeflateStream.cpp` | New — raw DEFLATE (`-MAX_WBITS`); XNB-compatible |
| `tests/CompressionTests.cpp` | New — 22 round-trip and property tests |
| `tests/System/IO/Compression/CompressionTests.cpp` | Updated — 4 stub `NotImplementedException` tests replaced with correct-behaviour tests |
| `CMakeLists.txt` | Added `find_package(ZLIB REQUIRED)` + `target_link_libraries(SHARP_RUNTIME PRIVATE ZLIB::ZLIB)` |
| `include/System/DateTime.hpp` + `src/System/DateTime.cpp` | Added Year/Month/Day/Hour/Minute/Second/Millisecond/DayOfWeek/DayOfYear properties; constructors (y,m,d), (y,m,d,h,m,s), (y,m,d,h,m,s,ms); AddDays/Hours/Minutes/Seconds/Milliseconds; Today; ISO-8601 ToString |
| `tests/DateTimePropertiesTests.cpp` | New — 32 tests |
| `tests/CalendarTests.cpp` | New — 60 tests: Calendar (28), GregorianCalendar (10), ISOWeek (13), DateTimeAdd (9) |
| `include/System/Decimal.hpp` + `src/System/Decimal.cpp` | Moved all method bodies to .cpp |
| `include/System/Uri.hpp` + `src/System/Uri.cpp` | Moved all method bodies to .cpp |
| `include/System/Numerics/BigInteger.hpp` + `src/System/Numerics/BigInteger.cpp` | Moved bodies; added `operator/`, `operator%`, `operator/=`, `operator%=`, `TryParse`; Knuth Algorithm D division |
| `tests/System/Numerics/BigIntegerTests.cpp` | +23 tests: TryParse (6), division (9), modulo (5), compound assignment (2), identity (2) |
| `include/System/Threading/Timer.hpp` | Fix: dangling-`this` UB → `shared_ptr<State>` |
| `tests/Task42Tests.cpp` | New — 148 tests covering 28 previously untested types |

**Session 33 — Task 42 (final header coverage):** see `git log --oneline`

**Session 32 — Task 41:** see `git log --oneline`

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **confirmed** | `Task`/`TaskT` use `std::async(launch::async)` — one OS thread per task, no threadpool |
| **confirmed** | `XmlReader`/`XmlWriter` always throw `NotImplementedException` — needs tinyxml2/pugixml |
| **confirmed** | `ZipArchive` always throws `NotImplementedException` — needs miniz/libzip |
| **confirmed** | `TcpClient`, `UdpClient` always throw `NotImplementedException` — needs POSIX/Winsock |
| **incomplete** | `Thread::CurrentThread()` returns proxy — no `Join()` / `IsAlive` |
| **incomplete** | `Char::Parse(string)` — only 1-byte ASCII; no multi-byte UTF-8 |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` — only UTC, Local, and a few hardcoded zones |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs |
| **known warning** | `Char.hpp:16` — null character in literal (cosmetic) |
| **excluded** | `DefaultValueAttribute.hpp` conflicts with `DescriptionAttribute.hpp` (duplicate class def) |

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
src/                                    ← .cpp for types needing it (exceptions, Guid, DateTime, Encoding, Decimal, Uri, BigInteger, GZipStream, DeflateStream, etc.)
tests/                                  ← GoogleTest suites (65 files, 2828 tests)
vendor/googletest/                      ← bundled test framework
vendor/nlohmann/json.hpp                ← nlohmann/json 3.10.4 (MIT)
```

### Invariants that must not be broken
1. **Complex types get `.hpp` + `.cpp` split** — move bodies to `.cpp` whenever the header grows unwieldy or causes ODR issues
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()` — never bare public fields
3. **Doxygen on all public declarations** — every public method/class/member in `.hpp` must have a `///` or `/** */` doc comment
4. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET Foundation attribution
5. **Namespace syntax:** `namespace System::Collections::Generic {` — C++17 nested form
6. **`SharpRuntime::intcs` not `int`** in public APIs that mirror .NET `int` parameters
7. **Build must stay clean** — zero errors, zero warnings before any commit
8. **`inline` statics** in headers for ODR-safe static members; non-inline in `.cpp`
9. **`static thread_local`** (not `mutable thread_local static`) for thread-local storage in templates

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
./build/SharpRuntimeTests --gtest_filter="GZipStream*:DeflateStream*"

# Check for errors/warnings only
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

### Task 44 — XmlReader / XmlWriter via tinyxml2 or pugixml

Both headers currently throw `NotImplementedException`. Options:
- **tinyxml2** — single `.cpp`, MIT, minimal API, easiest to embed
- **pugixml** — single header + `.cpp`, MIT, more complete XPath support

Steps:
1. Drop vendor into `vendor/tinyxml2/` (or pugixml)
2. Add to CMakeLists: `add_subdirectory(vendor/tinyxml2)` + link
3. Implement `XmlReader` (SAX-style forward-only) and `XmlWriter` (element/attribute output)
4. Add round-trip tests

### Task 45 — ZipArchive via miniz

`ZipArchive` stub at `include/System/IO/Compression/ZipArchive.hpp`. miniz is a single-file
public-domain zlib/zip implementation — already used by some game engines.

Steps:
1. Drop `vendor/miniz/miniz.h` + `miniz.c`
2. Implement `ZipArchive::GetEntry`, `CreateEntry`, `Entries` over miniz
3. Add read + create tests

### Task 46 — Namespace audit pass (ongoing)

Go namespace by namespace, file by file:
- Verify each header compiles cleanly when included alone
- Move any remaining header-only bodies > ~100 lines to `.cpp`
- Fill implementation stubs that are needed by CNA

Priority order (most needed by CNA):
1. `System::Globalization` — `CultureInfo`, `NumberFormatInfo`
2. `System::Text` — `StringBuilder` edge cases, `Encoding` completeness
3. `System::Collections::Generic` — `SortedDictionary`, `LinkedList` completeness
4. `System::Net` — `IPAddress`, `HttpStatusCode` (read-only, no actual networking)

---

## 8. Constraints / do not do

- **No merge to master or tags** without explicit per-action user approval
- **No broad header refactor** — changing naming conventions across 449 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No changes to `SharpRuntime::` primitive typedefs** — API foundation used by hundreds of headers
- **No port of Vector2/3/4, Matrix3x2/4x4** — these belong to the CNA layer, not sharp-runtime

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md — see section 7 for next tasks.
>
> Build: `cmake --build build --parallel 4` (zero errors, zero warnings)
> Run full suite: `./build/SharpRuntimeTests` — must show 2828 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
> Push only to `develop` — never merge to master or create tags without explicit user approval.
