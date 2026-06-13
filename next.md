# next.md — sharp-runtime handoff document
*Last updated: 2026-06-13 (branch: develop) — session 69 — 3939 tests passing*

---

## Project summary

**sharp-runtime** is a C++23 static library reimplementing a practical subset of the .NET `System.*` namespace in C++. Foundation for **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game).

**Status:** See `plan.md` for per-file and per-assembly status.

---

## Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
./build/SharpRuntimeTests

# Run specific suite
./build/SharpRuntimeTests --gtest_filter="TcpClient*"

# Errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"
```

---

## Architecture

```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   <- intcs, bytecs, shortcs, longcs, ...
  SharpRuntime/Prop.hpp                 <- property macros
  SharpRuntime/Storage/StoragePaths.hpp <- platform storage root
  System/                               <- core types, exceptions, Math, ...
  System/Collections/                   <- Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            <- Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          <- StringBuilder, Encoding, Json/, Encodings/Web/
  System/Threading/                     <- Thread, Monitor, Mutex, Timer, Tasks/
  System/Numerics/                      <- BigInteger, Complex, BFloat16, Vector*, Matrix*, Quaternion
  System/Diagnostics/                   <- Debug, Trace, Stopwatch
  System/Globalization/                 <- CultureInfo, Calendar types, IdnMapping, ...
  System/Net/                           <- IPAddress, IPEndPoint, HttpStatusCode, Sockets/, Http/
  System/Xml/                           <- XmlReader, XmlWriter, Linq/
src/                                    <- .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
vendor/                                 <- googletest, nlohmann/json, tinyxml2, miniz
tests/                                  <- GoogleTest files
```

**Vendored libraries:**

| Library | Use |
|---------|-----|
| GoogleTest | test framework |
| nlohmann/json | `System::Text::Json` |
| tinyxml2 | `System::Xml::XmlReader/XmlWriter` |
| miniz | `System::IO::Compression::ZipArchive` |

---

## Invariants (must not be broken)

1. **Zero errors, zero warnings** before any commit (`-Wall -Wextra -Werror`)
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()`
3. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form)
4. **`SharpRuntime::intcs` not `int`** in public APIs mirroring .NET `int` parameters
5. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution
6. **Doxygen on all public declarations** — every public method/class in `.hpp` must have `///` or `/** */`
7. **No POSIX includes in public `.hpp` headers** — all platform code belongs in `.cpp` files guarded by `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else`
8. **Complex types:** `.hpp` declaration + `.cpp` body; simple/template types are header-only

---

## Known limitations (intentionally out of scope)

| Limitation | Reason |
|------------|--------|
| `GC` — no-op stubs | Not meaningful in C++ |
| `Regex` — no named groups | `std::regex` limitation; needs PCRE2 |
| `Task`/`TaskT` — one OS thread per task | `std::async` sufficient for game use |
| `HttpClient` — no HTTPS/TLS | Needs OpenSSL/mbedTLS |
| `Net::Sockets` — POSIX-only | Winsock2 path exists but untested at runtime |

---

## Constraints

- **Push only to `develop`** — never merge to master or create tags without explicit per-action user approval
- **No broad header refactor** — naming conventions touch 449+ files, would break CNA
- **No LINQ** — use `std::ranges` in ported code instead
