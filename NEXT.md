# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-13 (branch: develop) — session 42*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** All major subsystems implemented and tested (~91% header coverage). hpp→cpp migration complete. Sessions 41–42 focused on portability and quality: POSIX includes removed from public headers; Windows/Emscripten paths added; strict warnings enforced; GCC builtins replaced with C++20 std::bit; `__int128` MSVC guard added; ws2_32 explicit link. Remaining portability items in §8.

**Key architectural decisions:**
- Complex types: `.hpp` declarations + `.cpp` bodies; simple types remain header-only
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed
- Namespace: `System`, `System::IO`, etc. using C++17 nested syntax
- Property naming: `getXxxProperty()` / `setXxxProperty()`
- Primitives in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, `shortcs = int16_t`, `charcs = char16_t`, etc.)
- Immutable collections use `shared_ptr<const std::container<T>>`
- Platform policy: POSIX includes belong only in `.cpp` files behind `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else` guards; never in public `.hpp` headers

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` ✅
- Output: `build/libSHARP_RUNTIME.a`
- Languages: CXX + C (C needed for vendored miniz)
- Zero errors, zero warnings

### Tests
- **All 3080 tests pass** — `./build/SharpRuntimeTests` → `3080 tests from 465 test suites` ✅
- GoogleTest at `vendor/googletest/`; 74 test files in `tests/`
- CMake now checks for `vendor/googletest/CMakeLists.txt` and prints a fatal error if missing

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
| `System::IO::Hashing::XxHash32/XxHash64` | ✅ DONE | hpp→cpp split done |
| `System::IO::RandomAccess` | ✅ DONE | POSIX pread/pwrite; Win32 OVERLAPPED ReadFile/WriteFile; Emscripten throws |
| `System::Xml::XmlReader` | ✅ DONE | tinyxml2 DOM cursor, event list |
| `System::Xml::XmlWriter` | ✅ DONE | tinyxml2 DOM builder + XMLPrinter |
| `System::Xml::Linq` | ✅ DONE | XName, XAttribute, XElement, XDocument |
| `System::Text::Json` | ✅ DONE | backed by nlohmann/json |
| `System::Text::StringBuilder` | ✅ DONE | Insert, Remove, Replace, Append(long) |
| `System::DateTime` | ✅ DONE | Year/Month/Day/etc., Add*, Today, ISO-8601 ToString; gmtime_r/gmtime_s guarded |
| `System::Decimal` | ✅ DONE | full arithmetic |
| `System::Uri` | ✅ DONE | full parsing |
| `System::Numerics::BigInteger` | ✅ DONE | +/−/×/÷/%, TryParse, Knuth Algorithm D |
| `System::Numerics::Vector2/3/4` | ✅ DONE | constants, operators, Dot/Cross/Normalize/Lerp/Clamp |
| `System::Numerics::Matrix3x2/4x4` | ✅ DONE | full transforms, Invert |
| `System::Numerics::Quaternion` | ✅ DONE | Slerp, CreateFrom*, Conjugate, Inverse |
| `System::Numerics::Plane` | ✅ DONE | CreateFromVertices, Dot, Normalize, Transform |
| `System::Threading::Timer` | ✅ DONE | dangling-`this` UB fixed via `shared_ptr<State>` |
| `System::Threading::Thread` | ✅ DONE | deferred start — Start() once; 2nd call throws; Join, IsAlive, ManagedThreadId |
| `System::Threading::Task/TaskT` | ⚠️ PARTIAL | `std::async(launch::async)` — no real threadpool; safe shared_ptr<State>; no Emscripten guard yet |
| `System::Net::Sockets::TcpClient/TcpListener` | ✅ DONE | POSIX + Winsock2; Emscripten throws PlatformNotSupportedException |
| `System::Net::Sockets::UdpClient` | ✅ DONE | POSIX + Winsock2; Emscripten throws PlatformNotSupportedException |
| `System::Net::Sockets::NetworkStream` | ✅ DONE | POSIX + Winsock2; Emscripten throws PlatformNotSupportedException |
| `System::TimeZoneInfo` | ✅ DONE | POSIX localtime_r; Windows GetTimeZoneInformation; Emscripten returns UTC |
| `System::AppDomain/AppContext` | ✅ DONE | Linux /proc/self/exe; macOS _NSGetExecutablePath; Windows GetModuleFileNameW; Emscripten ./ |
| `System::Environment` | ✅ DONE | GetCurrentDirectory + ProcessorCount in .cpp with Win/POSIX/Emscripten guards |
| `System::Globalization::PersianCalendar` | ✅ DONE | Solar Hijri, 12053-day cycle, base 1600 |
| `System::Globalization::JulianCalendar` | ✅ DONE | leap year: year % 4 == 0 |
| `System::Globalization::ThaiBuddhistCalendar` | ✅ DONE | +543 |
| `System::Globalization::TaiwanCalendar` | ✅ DONE | −1911 |
| `System::Globalization::CompareInfo` | ✅ DONE | Compare, IsPrefix/IsSuffix, IndexOf, GetSortKey |
| `System::Globalization::CharUnicodeInfo` | ✅ DONE | GetDecimalDigitValue, GetNumericValue, GetUnicodeCategory |
| `System::Globalization::DateTimeFormatInfo` | ✅ DONE | invariant defaults, MonthNames, DayNames, format patterns |
| `System::Collections::ArrayList` | ✅ DONE | std::vector<std::any> wrapper, full IList |
| `System::Collections::Hashtable` | ✅ DONE | std::unordered_map<string,any> wrapper, IDictionary |
| `System::Text::Ascii` | ✅ DONE | IsValid, ToUpper/Lower, Trim, EqualsIgnoreCase |
| `System::GC` | ⚠️ STUB | no-op stub only |

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **incomplete** | `Task`/`TaskT` — one OS thread per task via `std::async`, no real threadpool |
| **incomplete** | `Task`/`TaskT` — no Emscripten guard; `std::async` without pthreads will fail on Wasm |
| **incomplete** | `GC` — no-op stub only |
| **Windows limitation** | `TcpClient::GetStream()` — no socket dup on Windows (transfers ownership instead of dup-ing fd) |
| **Windows limitation** | `TimeZoneInfo::FindSystemTimeZoneById()` — throws for IANA IDs on Windows (no IANA→Windows mapping) |
| **untested** | Winsock2 and Emscripten paths compiled but never run — build-time correct, runtime untested |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, …)
  SharpRuntime/Prop.hpp                 ← DDATA/DGETTER/IDATA/IGETTER macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root (Android/Emscripten/desktop)
  System/                               ← exceptions, Math, Convert, DateTime, Decimal, Uri, …
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, Timer, …, Tasks/
  System/Numerics/                      ← BigInteger, Complex, BFloat16, BitOperations, MathF, Vector*, Matrix*, Quaternion, Plane
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ISOWeek, …
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for all non-template complex types (73 files)
  System/DateTime.cpp, Decimal.cpp, Uri.cpp, Guid.cpp, Environment.cpp, …
  System/Numerics/BigInteger.cpp
  System/IO/Compression/GZipStream.cpp, DeflateStream.cpp, ZipArchive.cpp
  System/IO/Hashing/XxHash32.cpp, XxHash64.cpp
  System/IO/RandomAccess.cpp            ← POSIX/Win32/Emscripten guarded
  System/Net/Sockets/TcpClient.cpp, UdpClient.cpp, NetworkStream.cpp
  System/Xml/XmlReader.cpp, XmlWriter.cpp
  SharpRuntime/Storage/StoragePaths.cpp
vendor/
  googletest/, nlohmann/, tinyxml2/, miniz/
tests/                                  ← 74 GoogleTest files, 3080 tests
```

### Platform portability rules (enforced)
- No POSIX headers (`<unistd.h>`, `<sys/socket.h>`, etc.) in public `.hpp` files — ever
- All platform branches in `.cpp` files: `#if defined(_WIN32)` / `#elif defined(__EMSCRIPTEN__)` / `#else` (POSIX)
- Unsupported platforms throw `System::PlatformNotSupportedException` — never silently fail
- `<windows.h>` always preceded by `#define WIN32_LEAN_AND_MEAN` and `#define NOMINMAX`

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
| 72 | 41 | Quality: Char.hpp NUL warning fixed; Thread deferred-start (Start() once; 2nd throws); Task data race fixed (shared_ptr<State>); CMakeLists.txt fatal error if GTest missing; CLAUDE.md created | — |
| 73 | 41 | Portability: Networking (Winsock2+Emscripten), RandomAccess (Win32 OVERLAPPED ReadFile/WriteFile), AppDomain (Win/macOS/Emscripten), TimeZoneInfo (Win/Emscripten); all POSIX includes removed from public headers | — |
| 74 | 41 | Portability: `Environment.hpp` — GetCurrentDirectory + getProcessorCountProperty moved to `Environment.cpp`; `<windows.h>` kept out of public header | — |
| 75 | 42 | Portability: `Task`/`TaskT` Emscripten guard — throw `PlatformNotSupportedException` (std::async requires pthreads) | — |
| 76 | 42 | CMake strict warnings: `-Wall -Wextra -Werror` for SHARP_RUNTIME; vendor/ as SYSTEM include; fix FileMode switch; `-Wno-unused-result` for tests | — |
| 77 | 42 | `TimeZoneInfo::FindSystemTimeZoneById()` — IANA→Windows mapping ~85 zones via `EnumDynamicTimeZoneInformation` | — |
| P1 | 42 | `Path.cpp` — proper WIN32/POSIX guards for `GetTempFileName`/`mkstemp`/`close` | — |
| P2 | 42 | `DriveInfo::GetDrives()` — move Win32 API body from header to `DriveInfo.cpp` | — |
| P3 | 42 | `Parallel`, `ThreadPool`, `Timer` — Emscripten guards for all `std::thread`/`std::async` entry points | — |
| P4 | 42 | CMakeLists: explicit `ws2_32` link for Windows (GCC/mingw-w64 ignores `#pragma comment`) | — |
| P5 | 42 | `Int128`, `UInt128`, `Decimal` — `#error` on MSVC (`__int128` is GCC/Clang only) | — |
| P6 | 42 | `Barrier.hpp` — `long phaseCount_` → `int64_t` (Windows 64-bit: `long` = 32-bit) | — |
| P7 | 42 | `BitConverter.hpp` — `IsLittleEndian` hardcoded `true` → `std::endian::native == std::endian::little` | — |
| P8 | 42 | `CharUnicodeInfo.hpp` — `wchar_t` cast guarded by `WCHAR_MAX` (Windows: 16-bit wchar_t) | — |
| P9 | 42 | `BitOperations.hpp`, `BitVector32.hpp` — replace `__builtin_clz`/`__builtin_popcount` with C++20 `std::countl_zero`/`std::popcount` | — |

---

## 8. Next tasks (priority order)

### Portability — remaining items

| Task | Description | Complexity | Notes |
|------|-------------|------------|-------|
| 79 | Emscripten build test — `emcmake cmake` build in CI or local | medium | Code paths written but never compiled with emcc |
| 80 | Windows build test — verify Winsock2 path with MSVC or mingw-w64 | medium | Code paths written but never compiled on Windows |
| 81 | `Convert::ToDouble` locale safety — `strtod` uses locale decimal separator; fix with `std::from_chars` | small | Rare in practice for game code but technically incorrect |

### Calendar types — awaiting user decision

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
- **No POSIX includes in public `.hpp` headers** — all platform code belongs in `.cpp` files

---

## 10. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md and CLAUDE.md before starting.
>
> Current state: 3080 tests pass, zero warnings. Sessions 41–42 completed portability sweep:
> - All POSIX includes removed from public `.hpp` headers
> - Networking: POSIX + Winsock2 + Emscripten guards; ws2_32 explicit CMake link
> - Task/TaskT, Parallel, ThreadPool, Timer: Emscripten guards (pthreads required)
> - CMake: -Wall -Wextra -Werror for SHARP_RUNTIME; vendor/ as SYSTEM include
> - TimeZoneInfo: IANA→Windows mapping ~85 zones via EnumDynamicTimeZoneInformation
> - DriveInfo, Path, DriveInfo, IsolatedStorageFileStream: platform guards fixed
> - BitOperations, BitVector32: __builtin_* → C++20 std::bit (MSVC compatible)
> - Int128/UInt128/Decimal: #error on MSVC (requires __int128)
> - Barrier: long → int64_t; BitConverter: IsLittleEndian → std::endian
> - CharUnicodeInfo: wchar_t cast guarded by WCHAR_MAX
>
> Next: Task 79 (Emscripten build test), Task 80 (Windows build test).
> See §8 for full remaining task list.
>
> Build: `cmake --build build --parallel 4`
> Run full suite: `./build/SharpRuntimeTests` — must show 3080 passing, 0 failing.
> Commit each logical change separately, then update NEXT.md.
> Push only to `develop` — never merge to master or create tags without explicit user approval.
