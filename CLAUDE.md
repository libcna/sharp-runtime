# CLAUDE.md — sharp-runtime

## Project mission

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes. It is the foundation for **CNA** (C++ XNA port) and **mobile-eggbert**.

---

## Non-negotiable rules

1. **Zero errors, zero warnings** before any commit. `cmake --build build --parallel 4` must be clean.
2. **3080+ tests passing.** `./build/SharpRuntimeTests` must show no failures.
3. **Push only to `develop`.** Never push to `master` or create tags without explicit per-action user approval.
4. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
5. **Property naming:** always `getXxxProperty()` / `setXxxProperty()`.
6. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
7. **Use `SharpRuntime::intcs`, not `int`** in public APIs that mirror .NET `int` parameters.
8. **No LINQ.** Use `std::ranges` in ported code instead.
9. **No merge to master or tags** without explicit per-action user approval.
10. **No broad header refactor** — naming conventions touch 449+ files and would break CNA.

---

## Platform policy

### What is POSIX-only (known bugs, not features)

These subsystems currently work only on Linux/macOS and are **documented bugs**, not done status:

| Subsystem | POSIX-only API used | Status |
|-----------|--------------------|----|
| `System::Net::Sockets` | `<sys/socket.h>`, `<unistd.h>`, `pread`/`pwrite` | POSIX-only |
| `System::IO::RandomAccess` | `pread`, `pwrite`, `fsync` | POSIX-only |
| `System::AppDomain/AppContext` | `/proc/self/exe` | Linux-only |
| `System::TimeZoneInfo` | `localtime_r`, `/usr/share/zoneinfo` | POSIX-only |

### Correct platform abstraction approach

- POSIX includes (`<unistd.h>`, `<sys/socket.h>`, etc.) must **not** appear in public `.hpp` headers.
- Platform-specific code belongs in `.cpp` files guarded by `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else` (POSIX).
- On unsupported platforms, throw `System::PlatformNotSupportedException` with a clear message — never silently fail.
- Emscripten builds must compile without errors even when the feature is unavailable at runtime.

---

## Status terminology

- **✅ DONE** — implemented, tested, compiles clean on Linux, no known bugs.
- **⚠️ PARTIAL** — compiles and mostly works but has known gaps (documented in NEXT.md §4).
- **⚠️ POSIX-only** — works on Linux/macOS but will not compile or link on Windows/Emscripten without additional work. Treat as a known bug.
- **⚠️ STUB** — API surface exists, bodies are no-ops or throw `NotImplementedException`.

---

## Architecture invariants

- **Complex types:** `.hpp` declaration + `.cpp` body. Move bodies to `.cpp` when a header grows unwieldy.
- **Simple types:** header-only is fine.
- **CMake:** `GLOB_RECURSE` auto-discovers `src/*.cpp` — no manual registration needed.
- **Vendored libs:** GoogleTest, nlohmann/json, tinyxml2, miniz. Never commit binaries.
- **Templates:** deferred `inline` definitions after forward declarations to resolve circular includes.

---

## plan.md namespace review workflow

`plan.md` contains a numbered table of all .NET namespaces from dotnet/runtime. The **Status column starts empty** for each namespace. The workflow for filling it in:

1. For each namespace where Status is empty, describe what classes/enums/interfaces it contains (look in `/rv/tmp/runtime/src/libraries/`).
2. Ask the user: **todo / ignore / ported / in_progress**
   - `todo` — needs to be ported/implemented in sharp-runtime
   - `ignore` — out of scope (too complex, platform-specific, or irrelevant to game dev)
   - `ported` — already implemented in sharp-runtime
   - `in_progress` — partially implemented, work ongoing
3. Write the chosen status into the table and move to the next namespace.

Do this one namespace at a time. Never batch-decide without asking.

---

## Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
./build/SharpRuntimeTests

# Errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="TcpClient*"
```
