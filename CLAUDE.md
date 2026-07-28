# CLAUDE.md — sharp-runtime

## Project mission

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes. It is the foundation for **CNA** (C++ XNA port) and **mobile-eggbert**.

---

## Non-negotiable rules

1. **Zero errors, zero warnings** before any commit. `cmake --build build --parallel 4` must be clean.
2. **No test-count regression.** `scripts/run_component_tests.sh build` must show no failures. The verified baseline is 13,463 tests across 36 component executables and one integration executable; this floor should be raised as new tests are added and lowered only with an explicit, documented reason.
3. **Push only to `feature/work`.** Never push to `develop` or `master`, and never create tags, without explicit per-action user approval.
4. **SPDX header on every project source/header** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution. Vendored sources retain their upstream headers; Markdown uses an HTML SPDX comment where one is present.
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
12. **At most four parallel compilation jobs.** See "Build-resource policy" below. This is
    binding on every build, rebuild, sanitizer build, probe, fixture, and test script in this
    repository, permanently and for all future work.

---

## Build-resource policy

This section is **permanent and binding for all work in this repository**, by any
contributor and by any future Claude Code session, on every ticket — not only the ticket
that introduced it. It has two halves: a **CPU ceiling** and the pre-existing **SSD-saving**
rules. Both must be obeyed together.

### CPU ceiling — four jobs, always

1. **Every** compilation, link, build, rebuild, sanitizer build, consumer fixture, compile
   probe, dependency build, CMake configure step that compiles, and test script that performs
   compilation internally **may use at most four parallel jobs / four CPU cores.**
2. Use commands equivalent to:

   ```bash
   cmake --build <dir> --parallel 4
   ninja -C <dir> -j4
   make -C <dir> -j4
   ctest --test-dir <dir> -j4
   ```

   or any **lower** value.
3. **Never** use unrestricted or automatically detected parallelism. All of the following are
   forbidden:
   - a bare `ninja` invocation, which defaults to every CPU plus two;
   - `-j` with no number, which is unbounded;
   - `--parallel` with no explicit maximum, which uses all detected cores;
   - `$(nproc)`, `nproc`, `sysctl -n hw.ncpu`, `getconf _NPROCESSORS_ONLN`, or any equivalent
     core-count substitution;
   - `std::thread::hardware_concurrency()` — or any runtime core count — used to choose a
     build-job count;
   - `CMAKE_BUILD_PARALLEL_LEVEL`, `MAKEFLAGS`, `NINJA_STATUS`-adjacent environment variables,
     or CI defaults left to expand to all cores;
   - any script that defaults to "all available cores" when no job count is supplied.
4. **When a repository script compiles internally**, pass whatever argument or environment
   variable constrains it to four jobs (for example `CMAKE_BUILD_PARALLEL_LEVEL=4`,
   `MAKEFLAGS=-j4`, or the script's own job-count parameter), and record in the ticket that
   the constraint was applied.
5. **If a build script cannot currently be limited to four jobs, fix the script first**, or
   use a bounded alternative (a direct `cmake --build … --parallel 4` on the same targets).
   Do not run the unbounded script "just this once".
6. **The four-job limit applies even when the machine has more CPU cores.** Core count is not
   a licence to raise it.
7. **Fewer than four jobs is always allowed** and is preferred whenever a target is
   memory-heavy (sanitizer or template-heavy translation units): drop to `-j2` or `-j1` rather
   than risking swap or an OOM kill.
8. **Exceeding four jobs requires new explicit user approval**, per action. A previous
   approval never carries over to another command, another ticket, or another session.
9. **Every final ticket report must list:**
   - every build directory used;
   - the maximum parallel job count actually used;
   - any script that required special handling (an argument, an environment variable, or a
     bounded substitute) to enforce the limit.

### SSD-saving rules — unchanged and still binding

10. Reuse the persistent build directories (`build`, `build-asan*`, `build-ubsan*`,
    `build-tsan*`, `build-probe-*`, `cmake-build-*`) instead of creating new ones.
11. Prefer incremental builds. Do not clean, delete, or reconfigure a build tree unless it is
    genuinely broken or the configuration genuinely changed; document any such build and why.
12. Retain `ccache` wherever it is already configured, and do not retrofit it where doing so
    would force an unnecessary full recompilation.
13. **Never create a build tree under `/tmp`, `/var/tmp`, or `/dev/shm`**, including the
    per-session scratchpad. Redirect `mktemp`-based scripts through a repository-local
    `TMPDIR` (this repository uses `build-tmp/`).
14. Remove large disposable binaries once their results are recorded, and never delete a build
    directory another session may still be using.

---

## Platform policy

### Compile support is not runtime support

The current full build/test baseline is Linux/GCC. Post-modular MinGW-w64 GCC
14-win32/CMake 3.31.6 and Emscripten 5.0.7/CMake 3.31.6 library builds both
compile the `All` graph and a selective `Text.Json` graph without
GoogleTest/runtime execution. Real downstream Apple Clang/Xcode 15.4 builds
drove the portability fixes from `1d22a7b2` through `b797928f`. The
repository's tracked CI is Ubuntu-only, so do not describe Windows,
Emscripten, or macOS as having the same current test coverage as Linux.

Unsupported runtime operations must still compile. They should throw
`PlatformNotSupportedException` clearly rather than fail the build or silently
degrade.

Current platform-limited areas include:

| Subsystem | Implemented runtime platforms | Explicit limitation |
|-----------|-------------------------------|---------------------|
| `System::Net::Sockets` | Windows and POSIX | Socket operations throw on Emscripten. |
| `System::IO::RandomAccess` | Windows and POSIX | Operations throw on Emscripten. |
| `System::AppDomain` base directory | Windows, macOS, Linux/POSIX | Emscripten uses the virtual-FS-relative `./` fallback. |
| `System::TimeZoneInfo` | Windows and POSIX | Emscripten provides UTC/local fallback and rejects system-zone lookup. |
| `System::Diagnostics::Process` | POSIX | Operations throw on Windows/Emscripten. |
| `PosixSignal`/`PosixSignalRegistration` | POSIX | Registration throws on Windows/Emscripten. |
| `NetworkInterface` enumeration | Linux | Enumeration/query operations throw elsewhere. |
| `FileSystemWatcher` | Linux/inotify | Enabling events throws elsewhere. |

### What is MSVC-unsupported (compiler-extension dependency, not a platform bug)

These types require the GCC/Clang `__int128`/`unsigned __int128` extension and hard-`#error`
on MSVC. This is a compiler dependency, not an OS dependency — GCC and Clang on Windows (e.g.
MinGW, or Clang with `-target x86_64-pc-windows-msvc` using its own `__int128` support) are
unaffected; only the MSVC frontend itself lacks `__int128`. Documented here as a **known,
accepted, permanent limitation** — not a bug to silently "fix" by working around `__int128`
with hand-rolled 128-bit arithmetic, per an explicit 2026-07-11 decision (the risk/complexity
of a from-scratch 128-bit implementation outweighs the benefit for a project that doesn't
target MSVC as a first-class compiler).

| Type | Requires | Status |
|------|----------|--------|
| `System::Decimal` | `unsigned __int128` | MSVC-unsupported |
| `System::Int128` | `__int128` | MSVC-unsupported |
| `System::UInt128` | `unsigned __int128` | MSVC-unsupported |

### Correct platform abstraction approach

- POSIX includes (`<unistd.h>`, `<sys/socket.h>`, etc.) must **not** appear in public `.hpp` headers.
- Platform-specific code belongs in `.cpp` files guarded by `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else` (POSIX).
- On unsupported platforms, throw `System::PlatformNotSupportedException` with a clear message — never silently fail.
- Emscripten builds must compile without errors even when the feature is unavailable at runtime.

---

## Status terminology

- **✅ DONE** — implemented, tested, compiles clean on the verified native baseline, no known bugs.
- **⚠️ PARTIAL** — compiles and mostly works but has known, documented API or behavior gaps.
- **⚠️ PLATFORM-LIMITED** — compiles across the intended toolchains but some operations are available only on named platforms and throw explicitly elsewhere.
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
  `System::Delegate` (`modules/core/include/System/Delegate.hpp`) is a real multicast delegate base class with
  working `Combine`/`Remove`/`RemoveAll`/`GetInvocationList`, and `System::MulticastAction<Args...>`
  (`modules/core/include/System/MulticastAction.hpp`) is a purpose-built multicast event-field type with `+=`/`=`
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
- A header under the owning module's `include/System/.../*.hpp` exists with the full public API: all public methods, constructors, properties, and operators that appear in the .NET `ref/` surface file.
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
- `scripts/run_component_tests.sh build` — all component and integration tests
  pass exactly once (no failures, no crashes).
- At least basic GoogleTest coverage exists for the ported type's key methods.

---

## Architecture invariants

- **Complex types:** `.hpp` declaration + `.cpp` body. Move bodies to `.cpp` when a header grows unwieldy.
- **Simple types:** header-only is fine.
- **CMake:** component-specific `CONFIGURE_DEPENDS` globs discover
  `modules/*/{src,tests}/*.cpp`; `scripts/validate_module_boundaries.py`
  validates every implementation/header owner and public/private/test
  dependency. Every module declares its include root, sources, tests,
  dependencies, and platform setup in `modules/<module>/CMakeLists.txt`.
- **Component boundaries:** internal code depends on narrow physical targets
  (`Core.Base`, `Collections.Core`, etc.), never the `Core`, `Collections`, or
  `All` compatibility umbrellas. Public-header edges are
  `PUBLIC_DEPENDENCIES`, implementation-only edges are
  `PRIVATE_DEPENDENCIES`, and test-only edges are `TEST_DEPENDENCIES`.
  `BlockingCollection<T>` belongs to `Collections.Blocking`; do not add its
  `Threading` requirements back to `Collections.Core` or weaken the Text.Json
  isolation fixture.
- **Vendored libs:** GoogleTest, nlohmann/json, tinyxml2, miniz, all under `vendor/`. Never commit binaries. Files under `vendor/` are third-party source unmodified from upstream and are exempt from this project's SPDX-header, doc-comment, and `getXxxProperty()`/namespace-syntax naming rules — those rules apply only to module `include/`, `src/`, and `tests/` trees.
- **Templates:** deferred `inline` definitions after forward declarations to resolve circular includes.
- **Collection mutation counters:** a collection with a fail-fast enumerator must hold its
  counter as `System::Collections::detail::MutationCounter` and its enumerator must snapshot
  `detail::MutationVersion` (`System/Collections/detail/MutationCounter.hpp`). Never a bare
  `intcs` — `++` on a signed counter is undefined behaviour at `INTCS_MAX`, and the
  implicitly declared assignment operator would transplant the *source's* counter into the
  destination, leaving an enumerator apparently valid over storage the assignment destroyed.
  Both defects existed in fourteen collections and are recorded with reproductions in
  `docs/CollectionVersionCounterSweep.md`. `detail::NarrowMutationCounter` is for the two
  types (`LinkedList<T>`, `BitArray`) whose measured layout has no room for eight bytes;
  do not use it for anything new. `SortedSet<T>` keeps its own `ulongcs` counter inside the
  shared `State` its live views co-own (ticket #1786) — do not migrate it.

---

## plan.sqlite3 namespace review workflow

`plan.sqlite3` (table `task`) tracks indexed .NET types from dotnet/runtime.
Rows started with an empty status; the current maintainer snapshot is fully
classified. Full workflow detail lives in `prompt.md` — this is the summary:

1. For each type where status is `''` or `todo` (System-namespace types first), look up what it does in `/rv/tmp/runtime/src/libraries/` and classify it **without asking the user**:
   - **Port it** → check if the file exists in sharp-runtime, review against the full checklist, port or fix, then set `status = 'ported'` and commit.
   - **Out of scope / irrelevant** → set `status = 'ignore'`, and set `outofscope = 1` for permanent-deviation categories (reflection, GC internals, P/Invoke, serialization infra, etc.) or `outofscope = 0` otherwise.
   - **Genuinely ambiguous** → set `status = 'tobedecided'` rather than guessing; the user reviews these by hand later.
2. Keep processing items back-to-back — do not stop between items to ask for confirmation.

Valid statuses written by the current workflow are `''` (unset), `todo`,
`ported`, `ignore`, and `tobedecided`. The database also contains legacy
`ignored` rows; treat them as classified and do not rename them mechanically.
**`in_progress` does not exist** — porting happens directly with no
intermediate state.

State lives in `plan.sqlite3` + git history, not conversation memory, so this process resumes cleanly after any context reset — just re-open `prompt.md` and continue from Step 1.

---

## Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
scripts/run_component_tests.sh build

# Errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run a specific suite
./build/SharpRuntimeTests_Net_Sockets --gtest_filter="TcpClient*"
```
