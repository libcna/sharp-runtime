# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-11 (branch: develop) — session 36*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** Task 46 namespace audit is largely complete. All subsystems are now tested. Remaining work: fix the `EqualityComparer` dual-definition conflict (Task 49), fix the `DefaultValueAttribute` conflict (Task 47), and optionally implement networking (Task 48).

**Key architectural decisions:**
- Complex types: `.hpp` declarations + `.cpp` bodies; simple types remain header-only
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed
- Namespace: `System`, `System::IO`, etc. using C++17 nested syntax
- Property naming: `getXxxProperty()` / `setXxxProperty()`
- Primitives in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, `shortcs = int16_t`, `charcs = char16_t`, etc.)
- Immutable collections use `shared_ptr<const std::container<T>>`

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` ✅
- Output: `build/libSHARP_RUNTIME.a`
- Languages: CXX + C (C needed for vendored miniz)
- One pre-existing cosmetic warning: `Char.hpp:16` — null character in `u' '` literal (harmless)

### Tests
- **All 2980 tests pass** — `./build/SharpRuntimeTests` → `2980 tests from 443 test suites` ✅
- GoogleTest at `vendor/googletest/`; 74 test files in `tests/`

### Vendored libraries

| Library | Location | Version | Use |
|---------|----------|---------|-----|
| GoogleTest | `vendor/googletest/` | bundled | test framework |
| nlohmann/json | `vendor/nlohmann/json.hpp` | 3.10.4 | `System::Text::Json` |
| tinyxml2 | `vendor/tinyxml2/` | master | `System::Xml::XmlReader/XmlWriter` |
| miniz | `vendor/miniz/` | master | `System::IO::Compression::ZipArchive` |

### Coverage overview

| Category | Count | Status |
|----------|-------|--------|
| Tested headers | ~404 / 449 | **~90%** ✅ |
| Untested — pure interfaces (`IXxx`) | 42 | intentionally skipped |
| Untested — `DefaultValueAttribute` | 1 | Task 47 conflict (name collision) |
| Untested — `EqualityComparer.hpp` | 1 | Task 49 — dual-definition conflict with Comparer.hpp |
| Untested — types with real logic | ~0 | ✅ |

---

## 3. Implementation status per subsystem

| Subsystem | Status | Notes |
|-----------|--------|-------|
| `System::IO::Compression::GZipStream` | ✅ DONE | zlib, PIMPL, 64KB buffers, Z_FINISH |
| `System::IO::Compression::DeflateStream` | ✅ DONE | raw DEFLATE (-MAX_WBITS), XNB-compatible |
| `System::IO::Compression::ZipArchive` | ✅ DONE | miniz, Read+Create mode, PIMPL |
| `System::Xml::XmlReader` | ✅ DONE | tinyxml2 DOM cursor, event list |
| `System::Xml::XmlWriter` | ✅ DONE | tinyxml2 DOM builder + XMLPrinter |
| `System::DateTime` | ✅ DONE | Year/Month/Day/etc. properties, Add*, Today, ISO-8601 ToString |
| `System::Decimal` | ✅ DONE | full arithmetic, moved to .cpp |
| `System::Uri` | ✅ DONE | full parsing, moved to .cpp |
| `System::Numerics::BigInteger` | ✅ DONE | +/−/×/÷/%, TryParse, Knuth Algorithm D |
| `System::Threading::Timer` | ✅ DONE | dangling-`this` UB fixed via `shared_ptr<State>` |
| `System::Text::Json` | ✅ DONE | backed by nlohmann/json |
| `System::Xml::Linq` | ✅ DONE | XName, XAttribute, XElement, XDocument |
| `System::Net::Sockets::TcpClient/UdpClient` | ❌ STUB | throws NotImplementedException |
| `System::Xml::XmlReader/XmlWriter (SAX)` | ✅ DONE | DOM-cursor, not true SAX |
| `System::Threading::Task/TaskT` | ⚠️ PARTIAL | `std::async(launch::async)` — no threadpool |
| `System::Threading::Thread` | ⚠️ PARTIAL | no `Join()` / `IsAlive` |
| `System::AppDomain/AppContext/GC` | ⚠️ STUB | minimal stubs only |
| `System::TimeZoneInfo` | ⚠️ PARTIAL | UTC, Local, few hardcoded zones |

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **confirmed** | `Task`/`TaskT` — one OS thread per task via `std::async`, no real threadpool |
| **confirmed** | `TcpClient`, `UdpClient` — always throw `NotImplementedException` |
| **incomplete** | `Thread::CurrentThread()` — returns proxy, no `Join()` / `IsAlive` |
| **incomplete** | `Char::Parse(string)` — 1-byte ASCII only, no multi-byte UTF-8 |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` — UTC, Local, few hardcoded zones |
| **incomplete** | `AppDomain`, `AppContext`, `GC` — stubs only |
| **known warning** | `Char.hpp:16` — null character in `u' '` literal (cosmetic) |
| **design flaw** | `EqualityComparer<T>` is defined TWICE: pointer-based API in `EqualityComparer.hpp` and value-based API in `Comparer.hpp` — see Task 49 |
| **excluded** | `DefaultValueAttribute.hpp` conflicts with `DescriptionAttribute.hpp` (duplicate class name) — see Task 47 |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs
  SharpRuntime/Prop.hpp                 ← DDATA/DGETTER/IDATA/IGETTER macros
  System/                               ← exceptions, Math, Convert, DateTime, Decimal, Uri, ...
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, Timer, ..., Tasks/
  System/Numerics/                      ← BigInteger, Complex, BFloat16, BitOperations, MathF
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ISOWeek, ...
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for complex types
  System/DateTime.cpp, Decimal.cpp, Uri.cpp, Guid.cpp, ...
  System/Numerics/BigInteger.cpp
  System/IO/Compression/GZipStream.cpp, DeflateStream.cpp, ZipArchive.cpp
  System/Xml/XmlReader.cpp, XmlWriter.cpp
vendor/
  googletest/, nlohmann/, tinyxml2/, miniz/
tests/                                  ← 67 GoogleTest files, 2851 tests
```

### Invariants that must not be broken
1. **Complex types get `.hpp` + `.cpp` split** — move bodies to `.cpp` when header grows unwieldy
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()`
3. **Doxygen on all public declarations** — every public method/class in `.hpp` must have `///` or `/** */`
4. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution
5. **Namespace syntax:** `namespace System::Collections::Generic {` — C++17 nested form
6. **`SharpRuntime::intcs` not `int`** in public APIs mirroring .NET `int` parameters
7. **Zero errors, zero warnings** before any commit
8. **`inline` statics in headers**; non-inline in `.cpp`

---

## 6. Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
./build/SharpRuntimeTests

# Run specific suite
./build/SharpRuntimeTests --gtest_filter="ZipArchive*"

# Errors/warnings only
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

### Task 46 — Namespace audit pass ✅ DONE (session 36)

All namespaces audited. Added 129 new tests (2851 → 2980). Gaps resolved:
- StringBuilder: added `Insert`, `Remove`, `Replace`, `Append(long)`
- Globalization: DaylightTime, SortVersion tested; stale Calendar comment fixed
- Text::Unicode: UnicodeRange/UnicodeRanges tested
- System attributes: Attribute, AttributeTargets, AttributeUsageAttribute, CLSCompliantAttribute, ObsoleteAttribute, marker attributes
- Diagnostics: ConditionalAttribute, DebuggableAttribute, DebuggerTypeProxy/Visualizer, CodeAnalysis (12 types)
- Events: EventArgs, AssemblyLoadEventArgs, ResolveEventArgs, UnhandledExceptionEventArgs, ThreadExceptionEventArgs

### Task 47 — Fix DefaultValueAttribute conflict ← NEXT

`include/System/ComponentModel/DefaultValueAttribute.hpp` and
`include/System/ComponentModel/DescriptionAttribute.hpp` define the same class name.
Fix by renaming or merging.

### Task 49 — Fix EqualityComparer dual-definition

`include/System/Collections/Generic/EqualityComparer.hpp` defines
`EqualityComparer<T>` with a pointer-based `Equals(T*, T*)` API and
`shared_ptr<> Default()`. `Comparer.hpp` defines a SECOND `EqualityComparer<T>`
with value-based `Equals(T&, T&)` and `const T& Default()`.
They cannot be included together.

Fix options:
1. Remove the block from `Comparer.hpp` and make `EqualityComparer.hpp` the
   single authoritative definition (preferred), updating existing tests.
2. Rename one to avoid the conflict.

### Task 48 — TcpClient / UdpClient (POSIX)

Lowest priority — not needed for the game engine itself, but needed for any networking feature.
Only implement if CNA requires it.

---

## 8. Constraints / do not do

- **No merge to master or tags** without explicit per-action user approval; push only to `develop`
- **No broad header refactor** — naming conventions touch 449 files, would break CNA
- **No LINQ** — use `std::ranges` in ported code instead
- **No changes to `SharpRuntime::` primitive typedefs** — API foundation
- **No port of Vector2/3/4, Matrix3x2/4x4** — CNA layer, not sharp-runtime

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md — **Task 47** is next: fix DefaultValueAttribute / DescriptionAttribute class name conflict.
> **Task 49** is next after that: resolve EqualityComparer dual-definition between Comparer.hpp and EqualityComparer.hpp.
>
> Build: `cmake --build build --parallel 4` (zero errors, zero warnings, C + CXX)
> Run full suite: `./build/SharpRuntimeTests` — must show 2851 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
> Push only to `develop` — never merge to master or create tags without explicit user approval.
