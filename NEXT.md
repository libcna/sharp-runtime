# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-11 (branch: develop) — session 38*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** All major subsystems implemented and tested (~91% header coverage). hpp→cpp migration complete. Remaining gaps are low-priority stubs (`TimeZoneInfo`, `AppDomain/GC`) — implement only when CNA actually needs them.

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
- **All 2999 tests pass** — `./build/SharpRuntimeTests` → `2999 tests from 444 test suites` ✅
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
| `System::Threading::Thread` | ✅ DONE | Join, IsAlive, Start (no-op), ManagedThreadId |
| `System::Net::Sockets::TcpClient/TcpListener` | ✅ DONE | POSIX sockets, port-0 auto-assign |
| `System::Net::Sockets::UdpClient` | ✅ DONE | POSIX SOCK_DGRAM, send/recvfrom |
| `System::Net::Sockets::NetworkStream` | ✅ DONE | wraps socket fd as `System::IO::Stream` |
| `System::Threading::Task/TaskT` | ⚠️ PARTIAL | `std::async(launch::async)` — no real threadpool |
| `System::TimeZoneInfo` | ⚠️ PARTIAL | UTC, Local, few hardcoded zones only |
| `System::AppDomain/AppContext/GC` | ⚠️ STUB | minimal stubs only |

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **by design** | `Thread::Start()` — no-op; thread starts eagerly in constructor (documented) |
| **incomplete** | `Task`/`TaskT` — one OS thread per task via `std::async`, no real threadpool |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` — UTC, Local, few hardcoded zones |
| **incomplete** | `AppDomain`, `AppContext`, `GC` — stubs only |
| **incomplete** | `Char::Parse(string)` — 1-byte ASCII only, no multi-byte UTF-8 |
| **known warning** | `Char.hpp:16` — null character in `u' '` literal (cosmetic, harmless) |

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
tests/                                  ← 74 GoogleTest files, 2999 tests
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

---

## 8. Next tasks (priority order)

### Task 52 — `AppDomain.CurrentDomain.BaseDirectory` (when CNA needs it)

`AppDomain` and `AppContext` are stubs. The most likely real need is
`AppDomain.CurrentDomain.BaseDirectory` for asset path resolution in the game engine.
`GC` can stay a no-op stub indefinitely.

### Task 53 — `TimeZoneInfo` expansion (when CNA needs it)

Currently only UTC, Local, and a few hardcoded zones are supported.
Full implementation requires reading `/etc/localtime` or the IANA tz database.
Only needed if game code does timezone-aware date arithmetic.

### Task 54 — `Char::Parse` / UTF-8 multi-byte support (when CNA needs it)

`Char::Parse(string)` currently handles 1-byte ASCII only. Multi-byte UTF-8
decoding needed if game content uses non-ASCII characters in string parsing paths.

---

## 9. Constraints / do not do

- **No merge to master or tags** without explicit per-action user approval; push only to `develop`
- **No broad header refactor** — naming conventions touch 449 files, would break CNA
- **No LINQ** — use `std::ranges` in ported code instead
- **No changes to `SharpRuntime::` primitive typedefs** — API foundation
- **No port of Vector2/3/4, Matrix3x2/4x4** — CNA layer, not sharp-runtime

---

## 10. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md — Tasks 46–51 are all done. All 2999 tests pass. Zero errors, zero warnings.
> Networking (TcpClient/TcpListener/UdpClient/NetworkStream) is implemented via POSIX sockets.
> Thread::Start() and ManagedThreadId implemented. XxHash32/64 moved to .cpp.
> hpp→cpp migration is complete for all non-template types.
> Remaining gaps: `AppDomain/BaseDirectory`, `TimeZoneInfo`, `Char::Parse UTF-8` —
> implement only when CNA actually needs them.
>
> Build: `cmake --build build --parallel 4`
> Run full suite: `./build/SharpRuntimeTests` — must show 2999 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
> Push only to `develop` — never merge to master or create tags without explicit user approval.
