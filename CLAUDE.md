# CLAUDE.md — sharp-runtime

## Project mission

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes. It is the foundation for **CNA** (C++ XNA port) and **mobile-eggbert**.

---

## Non-negotiable rules

1. **Zero errors, zero warnings** before any commit. `cmake --build build --parallel 4` must be clean.
2. **10711+ tests passing.** `./build/SharpRuntimeTests` must show no failures. (Baseline verified 2026-07-07 — this floor should be raised as new tests are added, never lowered.)
3. **Push only to `feature/work`.** Never push to `develop` or `master`, and never create tags, without explicit per-action user approval.
4. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
5. **Property naming:** always `getXxxProperty()` / `setXxxProperty()`. Exception: indexers
   (C# `this[key]` equivalents) use `getItem()` / `setItem()`, not
   `getItemProperty()`/`setItemProperty()` — a deliberate, consistent convention for the
   parameterized-property case, applied across every indexer in the codebase.
6. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
7. **Use `SharpRuntime::intcs`, not `int`** in public APIs that mirror .NET `int` parameters.
8. **No LINQ.** Use `std::ranges` in ported code instead.
9. **No merge to master or tags** without explicit per-action user approval.
10. **No broad header refactor** — naming conventions touch 449+ files and would break CNA.
11. **Copy doc-comments from .NET source** — when porting a type, if the `.NET` source (`/rv/tmp/runtime/src/libraries/`) has XML doc comments and the sharp-runtime header has none, copy them as Doxygen `/** */` comments where the meaning translates cleanly to C++.

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
| `System::Diagnostics::Process` | `fork`, `execve`, `waitpid` | POSIX-only |
| `System::Runtime::InteropServices::PosixSignal`/`PosixSignalRegistration` | `sigaction` | POSIX-only |
| `System::Net::NetworkInformation::NetworkInterface` | `getifaddrs` | Linux-only |
| `System::IO::FileSystemWatcher` | `inotify` | Linux-only |

### What requires compiler-provided native 128-bit integers

`System::Decimal`, `System::Int128`, and `System::UInt128` require a compiler-provided 16-byte
`__int128`/`unsigned __int128` type. This is a compiler-capability dependency, not an OS-name
test: x86_64 GCC/Clang and x86_64 MinGW GCC provide it, while MSVC and i686 MinGW GCC do not.
The public `SHARP_RUNTIME_HAS_NATIVE_INT128` macro is always `0` or `1`; CMake sets it from an
actual compile probe and `SharpRuntimeHelper.hpp` provides the same `__SIZEOF_INT128__`-based
fallback for non-CMake consumers.

When the macro is `0`, the three direct type headers reject inclusion with a clear diagnostic and
the library omits only `Decimal.cpp`. Otherwise-portable types remain available, with only their
native-128-dependent members absent: `Int64::BigMul(long,long)`, the 64-bit `Math::BigMul`
overload, 128-bit `BitConverter`/`BinaryPrimitives` overloads, `BinaryReader::ReadDecimal`,
`IConvertible`/`DBNull::ToDecimal`, and the Decimal `XmlConvert` overloads. This is the supported
i686 MinGW compile/link boundary used by CNA's Glide backend; it does not claim Decimal or
Int128/UInt128 support on that compiler.

The lack of native 128-bit integers is a **known, accepted, permanent limitation** — never hide
it with a hand-rolled representation or reduced semantics. The 2026-07-11 decision remains that
the risk and complexity of a from-scratch implementation outweigh its benefit.

| Type | Requires | Availability |
|------|----------|--------------|
| `System::Decimal` | 16-byte `unsigned __int128` | `SHARP_RUNTIME_HAS_NATIVE_INT128 == 1` |
| `System::Int128` | 16-byte `__int128` | `SHARP_RUNTIME_HAS_NATIVE_INT128 == 1` |
| `System::UInt128` | 16-byte `unsigned __int128` | `SHARP_RUNTIME_HAS_NATIVE_INT128 == 1` |

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

## Parity philosophy

sharp-runtime and .NET will naturally differ — C++ has no GC, no IL, no runtime reflection, and no delegate infrastructure. The goal is **maximum practical parity**: the public API, method semantics, default values, error messages, and algorithmic behaviour should match .NET as closely as C++ allows.

Known permanent deviations (not bugs, not TODO):
- **Reflection** (`System::Type`, `System::Activator`, `Enum.GetNames/GetValues`, etc.) — completely out of scope. Stubs are the correct end state.
- **GC** (`System::GC`) — all methods are no-ops. Memory is managed by RAII / `std::shared_ptr`.
- **Delegates** — three tiers, not one blanket rule (verified 2026-07-11 while auditing ticket 72,
  since the previous one-line version of this bullet was itself inaccurate for two of the three):
  the majority of delegate-shaped types (`Action`, `Func`, `EventHandler`, and most
  `*EventHandler`/`*Callback` aliases across the codebase) are bare `using X = std::function<...>;`
  aliases — single-target only, no multicast, no `BeginInvoke`/`EndInvoke` (async delegate
  invocation is out of scope entirely, matching .NET's own removal of the pattern). But
  `System::Delegate` (`include/System/Delegate.hpp`) is a real multicast delegate base class with
  working `Combine`/`Remove`/`RemoveAll`/`GetInvocationList`, and `System::MulticastAction<Args...>`
  (`include/System/MulticastAction.hpp`) is a purpose-built multicast event-field type with `+=`/`=`
  and reentrancy-safe snapshot invocation — both genuinely support multicast where a ported type
  needs it. `Delegate::DynamicInvoke` always throws `NotImplementedException` (no late-bound
  `object[]` invocation equivalent in C++) in all three tiers.
- **Serialization** (`[Serializable]`, `SerializationInfo`) — ignored; not needed for game code.
- **P/Invoke / interop** — out of scope.
- **Symmetric/asymmetric cryptography, X.509 certificates, TLS** (`System.Security.Cryptography`'s `Aes*`/`RSA*`/`EC*`/`ChaCha20Poly1305`/`CryptoStream`, `System.Security.Cryptography.X509Certificates`, `System.Net.Security`'s `SslStream` and friends) — out of scope by explicit decision (2026-07-07): implementing this correctly needs either a large new external dependency (OpenSSL/mbedTLS) or a hand-rolled, security-critical implementation, neither of which is worth it for game code. Hash algorithms (`MD5`/`SHA*`/`HMAC`/`PBKDF2` — no key material, no confidentiality guarantees to get wrong) are already ported and remain in scope; they are not affected by this deviation.

When a method cannot be meaningfully implemented (e.g. it requires reflection), it should throw `System::NotImplementedException` with a comment explaining why — never silently return a wrong value.

---

## Porting checklist — criteria for `ported` / ✅ DONE

A type may be marked `ported` only when **all** of the following hold:

### 1. Implementation complete
- Header `include/System/.../*.hpp` exists with the full public API: all public methods, constructors, properties, and operators that appear in the .NET `ref/` surface file.
- Properties follow `getXxxProperty()` / `setXxxProperty()` naming.
- Complex types have a `.cpp` body file; simple types may be header-only.
- No method body is a bare `throw NotImplementedException()` stub — those are **STUB**, not ported.

### 2. Correct C++ mapping
- `SharpRuntime::intcs` (not `int`) for public API parameters that mirror .NET `int`.
- Namespace opened with C++17 nested syntax: `namespace System::Collections::Generic {`.
- No LINQ — use `std::ranges` instead.
- POSIX-only internals are in `.cpp` files behind `#ifdef`, not in public `.hpp` headers.

### 3. Doc-comments
- Doxygen `/** */` block comments on every public type and method.
- Comments copied/adapted from .NET XML doc-comments in `/rv/tmp/runtime/src/libraries/` where available.

### 4. SPDX header
Every `.hpp` and `.cpp` file starts with:
```cpp
// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
```

### 5. Logic parity with .NET reference
- Compare the C++ implementation in sharp-runtime against the reference C# source in `/rv/tmp/runtime/src/libraries/`.
- All non-trivial method bodies must match the .NET logic (algorithm, edge-case handling, error conditions).
- Verify that default messages, constants, and HResult/error codes match the .NET source where applicable.
- Discrepancies must be either fixed or explicitly documented as intentional deviations.

### 6. Clean build
- `cmake --build build --parallel 4` — **zero errors, zero warnings**.

### 7. Tests passing
- `./build/SharpRuntimeTests` — all tests pass (no failures, no crashes).
- At least basic GoogleTest coverage exists for the ported type's key methods.

---

## Architecture invariants

- **Complex types:** `.hpp` declaration + `.cpp` body. Move bodies to `.cpp` when a header grows unwieldy.
- **Simple types:** header-only is fine.
- **CMake:** `GLOB_RECURSE` auto-discovers `src/*.cpp` — no manual registration needed.
- **Vendored libs:** GoogleTest, nlohmann/json, tinyxml2, miniz, all under `vendor/`. Never commit binaries. Files under `vendor/` are third-party source unmodified from upstream and are exempt from this project's SPDX-header, doc-comment, and `getXxxProperty()`/namespace-syntax naming rules — those rules apply only to `include/`, `src/`, and `tests/`.
- **Templates:** deferred `inline` definitions after forward declarations to resolve circular includes.

---

## plan.sqlite3 namespace review workflow

`plan.sqlite3` (table `task`) tracks all .NET types from dotnet/runtime. The **status column starts empty** for each type. Full workflow detail lives in `prompt.md` — this is the summary:

1. For each type where status is `''` or `todo` (System-namespace types first), look up what it does in `/rv/tmp/runtime/src/libraries/` and classify it **without asking the user**:
   - **Port it** → check if the file exists in sharp-runtime, review against the full checklist, port or fix, then set `status = 'ported'` and commit.
   - **Out of scope / irrelevant** → set `status = 'ignore'`, and set `outofscope = 1` for permanent-deviation categories (reflection, GC internals, P/Invoke, serialization infra, etc.) or `outofscope = 0` otherwise.
   - **Genuinely ambiguous** → set `status = 'tobedecided'` rather than guessing; the user reviews these by hand later.
2. Keep processing items back-to-back — do not stop between items to ask for confirmation.

Valid status values: `''` (unset), `todo`, `ported`, `ignore`, `tobedecided`. **`in_progress` does not exist** — porting happens directly with no intermediate state.

State lives in `plan.sqlite3` + git history, not conversation memory, so this process resumes cleanly after any context reset — just re-open `prompt.md` and continue from Step 1.

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
