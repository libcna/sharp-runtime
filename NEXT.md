# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-12 (branch: develop) — session 41*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** All major subsystems implemented and tested (~91% header coverage). hpp→cpp migration complete. Remaining gaps are low-priority stubs (`GC`) — implement only when CNA actually needs them.

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
- **All 3080 tests pass** — `./build/SharpRuntimeTests` → `3080 tests from 466 test suites` ✅
- GoogleTest at `vendor/googletest/`; 77 test files in `tests/`

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
| Tested headers | ~408 / 449 | **~91%** ✅ |
| Untested — pure interfaces (`IXxx`) | ~42 | intentionally skipped |
| Untested — types with real logic | ~0 | ✅ |

---

## 3. Implementation status per subsystem

| Subsystem | Status | Notes |
|-----------|--------|-------|
| `System::IO::Compression::GZipStream` | ✅ DONE | zlib, PIMPL, 64KB buffers |
| `System::IO::Compression::DeflateStream` | ✅ DONE | raw DEFLATE (-MAX_WBITS), XNB-compatible |
| `System::IO::Compression::ZipArchive` | ✅ DONE | miniz, Read+Create mode, PIMPL |
| `System::IO::Hashing::XxHash32/XxHash64` | ✅ DONE | hpp→cpp split done (Task 51) |
| `System::Xml::XmlReader` | ✅ DONE | tinyxml2 DOM cursor, event list |
| `System::Xml::XmlWriter` | ✅ DONE | tinyxml2 DOM builder + XMLPrinter |
| `System::Xml::Linq` | ✅ DONE | XName, XAttribute, XElement, XDocument |
| `System::Text::Json` | ✅ DONE | backed by nlohmann/json |
| `System::Text::StringBuilder` | ✅ DONE | Insert, Remove, Replace, Append(long) |
| `System::DateTime` | ✅ DONE | Year/Month/Day/etc., Add*, Today, ISO-8601 ToString |
| `System::Decimal` | ✅ DONE | full arithmetic |
| `System::Uri` | ✅ DONE | full parsing |
| `System::Numerics::BigInteger` | ✅ DONE | +/−/×/÷/%, TryParse, Knuth Algorithm D |
| `System::Threading::Timer` | ✅ DONE | dangling-`this` UB fixed via `shared_ptr<State>` |
| `System::Threading::Thread` | ✅ DONE | Join, IsAlive, Start() starts once (throws on second call), ManagedThreadId |
| `System::Net::Sockets::TcpClient/TcpListener` | ⚠️ POSIX-only | Linux/macOS only; needs Winsock for Windows |
| `System::Net::Sockets::UdpClient` | ⚠️ POSIX-only | Linux/macOS only; needs Winsock for Windows |
| `System::Net::Sockets::NetworkStream` | ⚠️ POSIX-only | Linux/macOS only; needs Winsock for Windows |
| `System::IO::RandomAccess` | ⚠️ POSIX-only | pread/pwrite/fsync — Linux/macOS only |
| `System::Threading::Task/TaskT` | ⚠️ PARTIAL | `std::async(launch::async)` — no real threadpool; data race fixed via shared_ptr state |
| `System::TimeZoneInfo` | ⚠️ POSIX-only | Local() reads real offset; FindSystemTimeZoneById() via /usr/share/zoneinfo (POSIX) |
| `System::AppDomain/AppContext` | ⚠️ Linux-only | BaseDirectory via /proc/self/exe — Linux only |
| `System::GC` | ⚠️ STUB | no-op stub only |

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **POSIX-only** | `TcpClient`, `TcpListener`, `UdpClient`, `NetworkStream` — POSIX sockets; needs Winsock for Windows |
| **POSIX-only** | `RandomAccess` — `pread`/`pwrite`/`fsync`; needs Win32 for Windows |
| **Linux-only** | `AppDomain/AppContext.BaseDirectory` — `/proc/self/exe` |
| **POSIX-only** | `TimeZoneInfo.FindSystemTimeZoneById` — `/usr/share/zoneinfo` |
| **incomplete** | `Task`/`TaskT` — one OS thread per task via `std::async`, no real threadpool |
| **incomplete** | `GC` — no-op stub only |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, …)
  SharpRuntime/Prop.hpp                 ← DDATA/DGETTER/IDATA/IGETTER macros
  System/                               ← exceptions, Math, Convert, DateTime, Decimal, Uri, …
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, Timer, …, Tasks/
  System/Numerics/                      ← BigInteger, Complex, BFloat16, BitOperations, MathF
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ISOWeek, …
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for all non-template complex types (migration complete)
  System/DateTime.cpp, Decimal.cpp, Uri.cpp, Guid.cpp, …
  System/Numerics/BigInteger.cpp
  System/IO/Compression/GZipStream.cpp, DeflateStream.cpp, ZipArchive.cpp
  System/IO/Hashing/XxHash32.cpp, XxHash64.cpp
  System/Net/Sockets/TcpClient.cpp, UdpClient.cpp, NetworkStream.cpp
  System/Xml/XmlReader.cpp, XmlWriter.cpp
vendor/
  googletest/, nlohmann/, tinyxml2/, miniz/
tests/                                  ← 74 GoogleTest files, 3010 tests
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
./build/SharpRuntimeTests --gtest_filter="TcpClient*"

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

## 7. Completed tasks (changelog)

| Task | Session | Description | Tests Δ |
|------|---------|-------------|---------|
| 46 | 36 | Namespace audit — Globalization, Text::Unicode, attributes, Diagnostics, events | 2851 → 2980 |
| 47 | 37 | Fix `DefaultValueAttribute` duplicate in `DescriptionAttribute.hpp` | +5 |
| 49 | 37 | Fix `EqualityComparer` dual-definition; canonical `.hpp`, `Comparer.hpp` includes it | +3 |
| 48 | 38 | Implement `TcpClient/TcpListener/UdpClient/NetworkStream` via POSIX sockets | 2988 → 2995 |
| 50 | 38 | `Thread::Start()` (no-op, .NET compat) + `getManagedThreadIdProperty()` on instance | 2995 → 2999 |
| 51 | 38 | Move `XxHash32`/`XxHash64` bodies to `.cpp`; hpp→cpp migration now complete | — |
| 52 | 39 | `AppDomain.BaseDirectory` + `AppContext.getBaseDirProperty()` via `/proc/self/exe` | 2999 → 3001 |
| 53 | 39 | `TimeZoneInfo::Local()` real offset; `FindSystemTimeZoneById()` IANA via `/usr/share/zoneinfo` | 3001 → 3004 |
| 54 | 39 | `Char::Parse` + `Char::ToString` full UTF-8 multi-byte support (1–3 byte BMP) | 3004 → 3010 |
| 55–69 | 40 | Gap analysis + implement: Argb/Rgba, ArrayList, Hashtable, RandomAccess, Ascii, TextInfo, TextElementEnumerator, SortKey, CompareInfo, CharUnicodeInfo, DateTimeFormatInfo, JulianCalendar, ThaiBuddhistCalendar, TaiwanCalendar, PersianCalendar, Vector2/3/4, Matrix3x2/4x4, Quaternion, Plane | 3010 → 3080 |
| 72 | 41 | Portability+quality fixes: Char.hpp NUL warning, Thread deferred-start (Start() once; 2nd throws), Task data race (shared_ptr<State>), CMakeLists.txt GTest guard, CLAUDE.md, NEXT.md honest status | — |

---

## 8. Next tasks (priority order)

Gap analysis against .NET CoreLib completed (session 40). Tasks below cover all missing non-internal public types found.

All tasks 55–69 completed in session 40.

Remaining open questions (awaiting user decision):
| Task | Description | Complexity |
|------|-------------|------------|
| 70 | `HebrewCalendar`, `HijriCalendar`, `JapaneseCalendar`, `KoreanCalendar`, `UmAlQuraCalendar`, lunisolar variants | very complex |
| 71 | `IdnMapping` — Punycode/IDNA internationalized domain names | very complex |

---

## 9. Constraints / do not do

- **No merge to master or tags** without explicit per-action user approval; push only to `develop`
- **No broad header refactor** — naming conventions touch 449 files, would break CNA
- **No LINQ** — use `std::ranges` in ported code instead
- **No changes to `SharpRuntime::` primitive typedefs** — API foundation
- **Port Vector2/3/4, Matrix3x2/4x4, Plane, Quaternion** — sharp-runtime implements these (Tasks 68–69)

---

## 10. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md — Tasks 46–54 are all done. All 3010 tests pass. Zero errors, zero warnings.
> Networking (TcpClient/TcpListener/UdpClient/NetworkStream) implemented via POSIX sockets.
> Thread::Start() and ManagedThreadId implemented. XxHash32/64 moved to .cpp.
> AppDomain.BaseDirectory and AppContext.getBaseDirProperty() return real exe directory.
> TimeZoneInfo::Local() reads real system offset; FindSystemTimeZoneById() resolves IANA zones.
> Char::Parse and ToString support full UTF-8 multi-byte (BMP). hpp→cpp migration complete.
> No further planned tasks — next gaps will emerge from CNA integration.
>
> Build: `cmake --build build --parallel 4`
> Run full suite: `./build/SharpRuntimeTests` — must show 3010 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
> Push only to `develop` — never merge to master or create tags without explicit user approval.
