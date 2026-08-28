# CLAUDE.md — sharp-runtime

## Project mission

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes. It is the foundation for **CNA** (C++ XNA port) and **mobile-eggbert**.

---

## Non-negotiable rules

1. **Zero errors, zero warnings** before any commit. `cmake --build build --parallel 2` must be clean.
2. **No test-count regression.** `scripts/run_component_tests.sh build` must show no failures or
   skips. The current verified baseline is **17,840 tests across 38 executables with THE GATE
   GREEN** — 17,840 run, 17,840 passed, 0 failed, 0 skipped, measured on 2026-08-22 by post-#1941
   consumer-audit ticket #2418 after a cache-disabled full repository build at two jobs and the
   complete local CI gate.
   It is +59 on #2417's 17,781 final-audit closure: Core.Base +22, Globalization +1, IO +5,
   Net +6, Net.Http.Headers +2, TimeZone +18, and Xml +5; all other executables are unchanged.
   Graph 41 / 96, test-only seams 5 / 22, negative fixtures 55 / 284. The Doxygen 1.9.8
   no-regression baseline is 2,675 and is enforced both locally and in
   CI. Ticket #2419 additionally makes the complete production graph a permanent Clang gate:
   Clang 19.1.7 builds all 219 first-party translation units with `-Werror`, 0 warnings and
   0 errors, from both `local_ci_check.sh` and the GitHub full job.
   Historical test-count ledger: see `docs/TestCountLedger.md` (moved out of always-loaded scope 2026-08-28; it was 88.6% of this file's bytes).
3. **Push only to `feature/work`.** Never push to `develop` or `master`, and never create tags, without explicit per-action user approval.
4. **SPDX header on every project source/header** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution. Vendored sources retain their upstream headers; Markdown uses an HTML SPDX comment where one is present.
5. **Property naming:** always `getXxxProperty()` / `setXxxProperty()`. Exception: indexers
   (C# `this[key]` equivalents) use `getItem()` / `setItem()`, not
   `getItemProperty()`/`setItemProperty()` — a deliberate, consistent convention for the
   parameterized-property case, applied across every indexer in the codebase.
6. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
7. **Use `SharpRuntime::intcs`, not `int`** in public APIs that mirror .NET `int` parameters.
8. **No LINQ** in the code this project writes — use `std::ranges` in ported bodies instead.
   **This does not mean the tree contains no LINQ surface, and the distinction is worth stating
   here because the two look like a contradiction otherwise**: `modules/core/include/System/Linq.hpp`
   is a 508-line `System::Linq` providing `Where`, `Select`, `FirstOrDefault` and ~17 more over
   `std::vector<T>`. It exists as a **compatibility surface for ported C#/XNA call sites**, not as
   a licence to use those operators in new code. Measured 2026-08-19: a strict search for
   `System::Linq::` finds **zero** uses in this repository's production code, **zero** in `cna` and
   **zero** in `mobile-eggbert` — its only users are its own two test files. So it is a capability
   offered *in advance* of a caller; removing it would be a public-surface decision rather than a
   cleanup.
9. **No merge to master or tags** without explicit per-action user approval.
10. **No broad header refactor** — naming conventions touch 449+ files and would break CNA.
11. **Copy doc-comments from .NET source** — when porting a type, if the `.NET` source (`/rv/tmp/runtime/src/libraries/`) has XML doc comments and the sharp-runtime header has none, copy them as Doxygen `/** */` comments where the meaning translates cleanly to C++.
12. **At most two parallel compilation jobs.** See "Build-resource policy" below. This is
    binding on every build, rebuild, sanitizer build, probe, fixture, and test script in this
    repository, permanently and for all future work. The ceiling was **four** until
    2026-07-28, when the user lowered it to **three**, and **two** from 2026-08-01;
    historical ticket records that state a four- or three-job measurement describe what was
    correct under the then-current rule and are not retro-edited.
13. **Push immediately after every commit.** A commit is not finished until it is on the
    remote. Run `git push -u origin <current-branch>` as the next action after each `git
    commit` — do not batch commits up for a later push, and do not end a turn with the local
    branch ahead of its upstream. This was introduced on 2026-08-09 after 108 commits (three
    to nine days old, +42,818/−1,051 across 277 files) were found sitting unpushed on
    `claude/remediation-batch-1804-namespace-b1yjh5`; the container that held them is
    ephemeral, so that backlog was one reclaim away from being lost. The rule does **not**
    relax rules 3 and 9: the push target is the session's designated working branch, and
    `develop`, `master` and tags still need explicit per-action user approval. If a push
    fails on a network error, retry up to four times with exponential backoff (2s, 4s, 8s,
    16s); if it still fails, say so explicitly rather than leaving the commit silently
    unpushed.
14. **Standing approvals live in `docs/StandingApprovals.md`** — read it before recording any
    ticket as `blocked` on an approval. Do not restate their content here; that file is the
    single source of truth for SA-1 through SA-16 and the environment facts they rest on.

---

## Build-resource policy

This section is **permanent and binding for all work in this repository**, by any
contributor and by any future Claude Code session, on every ticket — not only the ticket
that introduced it. It has two halves: a **CPU ceiling** and the pre-existing **SSD-saving**
rules. Both must be obeyed together.

### CPU ceiling — two jobs, always

1. **Every** compilation, link, build, rebuild, sanitizer build, consumer fixture, compile
   probe, dependency build, CMake configure step that compiles, and test script that performs
   compilation internally **may use at most two parallel jobs / two CPU cores.**
2. Use commands equivalent to:

   ```bash
   cmake --build <dir> --parallel 2
   ninja -C <dir> -j2
   make -C <dir> -j2
   ctest --test-dir <dir> -j2
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
   variable constrains it to two jobs (for example `CMAKE_BUILD_PARALLEL_LEVEL=2`,
   `MAKEFLAGS=-j2`, or the script's own job-count parameter), and record in the ticket that
   the constraint was applied.
5. **If a build script cannot currently be limited to two jobs, fix the script first**, or
   use a bounded alternative (a direct `cmake --build … --parallel 2` on the same targets).
   Do not run the unbounded script "just this once".

   `scripts/job_count_policy.py` is the single resolver used by
   `scripts/check_selective_components.sh`, `scripts/local_ci_check.sh`, and
   `scripts/check_negative_consumer_fixtures.py`. Precedence is explicit `--jobs`, then
   **`SHARP_RUNTIME_BUILD_JOBS`**, then the safe default **2**; only 1 or 2 is accepted, and
   wrappers export the resolved value to nested helpers. Local CI still passes the resolved
   value explicitly to the fixture checker.
6. **The two-job limit applies even when the machine has more CPU cores.** Core count is not
   a licence to raise it.
7. **Fewer than two jobs is always allowed** and is preferred whenever a target is
   memory-heavy (sanitizer or template-heavy translation units): drop to `-j1` rather
   than risking swap or an OOM kill.
8. **Exceeding two jobs requires new explicit user approval**, per action. A previous
   approval never carries over to another command, another ticket, or another session.
9. **Every final ticket report must list:**
   - every build directory used;
   - the maximum parallel job count actually used;
   - any script that required special handling (an argument, an environment variable, or a
     bounded substitute) to enforce the limit.

### SSD-saving rules — unchanged and still binding

**Why this matters:** repeated from-scratch builds wrote **3.5 TB to this SSD in five days**
(measured 2026-07-28), on top of the earlier ~270 GB / two-day scratchpad measurement. That is
real, irreversible flash wear, not just wasted time. Every avoided full rebuild is avoided
write endurance spent. **Reuse an existing build directory whenever it is usable** — a full
reconfigure/rebuild is the exception that must be justified, never the default.

10. **There is a FIXED, CLOSED set of build directory names. Never invent another one, and
    never suffix one with a ticket number, a date, a branch, or a topic.** The complete list:

    | Directory | Purpose |
    |---|---|
    | `build/` | the default build and the repository gate |
    | `build-modular/` | the modular/selective component build |
    | `build-asan/`, `build-ubsan/`, `build-tsan/` | sanitizer trees |
    | `build-probe/` | **every** ticket's throwaway probes, ABI experiments, shims and sweeps |
    | `build-consumer/` | **every** ticket's standalone consumer fixture |
    | `build-tmp/` | repository-local `TMPDIR` for `mktemp`-based scripts |
    | `cmake-build-debug/` | the IDE tree |

    Directories like `build-probe-1794/`, `build-consumer-1785/`, `build-asan-sortedset/`
    are the mistake this rule exists to stop: measured on 2026-07-28, twenty-one such
    one-shot directories held **441 MB** and guaranteed that nothing in them was ever reused.
    A ticket separates its own work by **file name prefix inside the shared directory**
    (`build-probe/1797_probe1_escapes.cpp`), never by a new directory.
11. **Delete a ticket's probe artefacts once its evidence is transcribed into the design
    record.** The design document is the durable evidence; the binaries are not. Sanitizer
    probe binaries reach hundreds of megabytes each.
12. Prefer incremental builds. Do not clean, delete, or reconfigure a build tree unless it is
    genuinely broken or the configuration genuinely changed; document any such build and why.
13. Retain `ccache` wherever it is already configured, and do not retrofit it where doing so
    would force an unnecessary full recompilation.
14. **Never create a build tree under `/tmp`, `/var/tmp`, or `/dev/shm`**, including the
    per-session scratchpad. Redirect `mktemp`-based scripts through a repository-local
    `TMPDIR` (this repository uses `build-tmp/`).
15. Remove large disposable binaries once their results are recorded, and never delete a build
    directory another session may still be using.

---

## Platform policy

### Compile support is not runtime support

The current full build/test baseline is Linux/GCC; the complete production-only `All` graph is
also warning-clean under Clang 19.1.7 and is enforced locally and in CI. Post-modular MinGW-w64 GCC
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
- **The unit of every public index, length and count is a UTF-8 storage byte**, where .NET's is a
  UTF-16 code unit — decided by ticket **#2015** (SR-AUD-290, SR-AUD-296) on 2026-08-17 and
  **declared, not repaired**. `System::String` is a UTF-8 `std::string` throughout this runtime,
  so `Encoding::GetCharCount` of U+1F600 is `4` where .NET reports `2`, and `StringBuilder`'s
  `Length`/`Insert`/`Remove`/`CopyTo` index bytes. Adopting .NET's unit is not a change to
  `System::Text`; it is a re-architecture of every index in the port. **What makes this the
  faithful adaptation rather than merely the cheap one** is that a byte index into UTF-8 is the
  exact analogue of a code-unit index into UTF-16, *including* the ability to split a character:
  .NET's `StringBuilder.Remove` validates only the numeric range (`StringBuilder.cs:1024-1042`),
  with no surrogate-pair guard, so it can leave a lone surrogate and an ill-formed string exactly
  as this port can leave an ill-formed UTF-8 sequence. Adopting .NET's unit would have moved that
  hazard to a different character, not removed it. Pinned by `TextUnitContractTests.Decl2015_*`,
  including that the unit is **consistent** across the component — a mixture would be far worse
  than either unit consistently applied.
- **Unicode normalization** — `StringNormalizationExtensions::IsNormalized` returns `true` and
  `Normalize` returns its argument unchanged for every input. Decided by ticket **#2338** on
  2026-08-19 and **declared, not repaired** — and the point is that **this is not a divergence**:
  it is exactly .NET's behaviour in **invariant globalization mode**, which says so in its own
  comment (*"In Invariant mode we assume all characters are normalized because we don't support
  any linguistic operations on strings"*, `Normalization.cs:11-40`). **.NET has no normalization
  tables of its own** — it delegates to ICU on Unix and NLS on Windows, and `CharUnicodeInfoData.cs`,
  the source of record SA-4 names, carries **zero** decomposition, combining-class,
  composition-exclusion or quick-check data. Two alternatives were offered and declined: **own UCD
  tables plus a UAX #15 implementation** (size L-to-XL, and a *second* Unicode version to keep in
  step with SA-4's 16.0), and **an ICU dependency**, which is the shape this list already declines
  for cryptography — *"a large new external dependency"* — so taking it would reverse a standing
  decision rather than make a new one. Measured: **zero call sites** in `cna` and in
  `mobile-eggbert`; the only in-repository uses are the type's own tests. **What a caller must
  read into it**: `true` means *this runtime performs no linguistic normalization*, **not** that
  the string is in the requested form. The form **is** validated on every platform, because
  `CheckNormalizationForm` runs *before* the invariant shortcut (#2386). Pinned by
  `StringNormalizationTests.Decl2338_*`.
- **tzdata rule structures** — `TimeZoneInfo::GetAdjustmentRules()` returns an empty array and
  `HasSameRules()` therefore cannot distinguish two zones that share a base offset and a DST flag
  (`America/New_York` and `America/Havana` report as same-rule zones where .NET reports `false`).
  Decided by ticket **#2185** on 2026-08-19 and **declared, not repaired**, on two measurements.
  **It is not closable by sampling libc at any granularity** — two zones can agree on every sampled
  instant and still differ in rule, so no finer sampling turns offsets into rules; closing it needs
  tzdata's *own* structures (the TZif POSIX-TZ footer, or the full transition table with its
  per-era type records), i.e. a TZif reader, which is out of scope for this port. And **the failure
  is one-directional**: this method can only ever be too *permissive*, never too strict, so a
  caller using it as a necessary condition is correct and one using it as a sufficient condition is
  not. The layout cost an earlier design measured (`sizeof(TimeZoneInfo)` 160 → 184) is real but is
  not what blocks it and is not paid. Pinned by `TimeZoneInfoTests.Decl2185_*`.
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
- `cmake --build build --parallel 2` — **zero errors, zero warnings**.

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
  `docs/CollectionVersionCounterSweep.md`. **`detail::NarrowMutationCounter` has no user
  left and must not gain one.** It was the 32-bit counter for the two types whose measured
  layout had no room for eight bytes; ticket #1788 moved `LinkedList<T>` off it (growing
  `sizeof(LinkedList<T>)` 40 → 48) and ticket #1789 moved `BitArray` off it (growing
  `sizeof(BitArray::Enumerator)` 32 → 40), each under its own explicit user approval, so
  **no collection retains a 2^32 enumerator-snapshot ABA horizon**. The alias survives only
  as history and as the second instantiation the counter tests pin. `SortedSet<T>` keeps its
  own `ulongcs` counter inside the shared `State` its live views co-own (ticket #1786) — do
  not migrate it. When a collection's counter is widened, its enumerator's snapshot must be
  widened in the **same** change: a narrow snapshot compared against a wide counter is a
  silent truncation that leaves the alias in place while the code claims otherwise.
- **Test-only access seams:** a class template that a production header declares inside
  `namespace SharpRuntime::Testing` and never defines
  (`CollectionVersionAccess`, `SortedSetVersionAccess`) may be **defined in exactly one
  file**, and every suite that needs it must include that file. For the collection mutation
  counters that file is
  `modules/collections/tests/support/CollectionVersionSeam.hpp`; add a new collection there,
  once, through its `SHARP_RUNTIME_COLLECTION_VERSION_SEAM` macro. Never write
  `template<> struct CollectionVersionAccess<…> { … }` in a test translation unit. Five
  suites did, in two divergent families, and two token-different definitions of one class in
  one program is a one-definition-rule violation that is **ill-formed with no diagnostic
  required**: measured on 2026-07-29, swapping two object files on the link line changed the
  answer a correctly written suite got, and `ld`, `-flto -Wodr`, ASan with
  `detect_odr_violation=2` and UBSan all said nothing
  (`docs/CollectionVersionTestSeamDesign.md`, ticket #1800).
  `scripts/check_version_seam_odr.py` enforces this and runs in
  `scripts/local_ci_check.sh`; never define a seam in `modules/*/include` or `modules/*/src`,
  because that would make it reachable from a consumer and break the consumer-side fixture
  that pins it — `test/consumer/collections_mutation_version_negative.cpp` for
  `CollectionVersionAccess` (2 sites, #1787/#1801) and
  `test/consumer/collections_sorted_set_version_negative.cpp` for `SortedSetVersionAccess`
  (15 sites, #1803). **Every seam needs both checks** — they catch different mutations.
  Ticket #1804 (2026-07-30) closed one earlier gap in the seam checker: giving a seam's
  *primary template* a body in a public header used to make it stop being discovered as a
  seam, so `check_version_seam_odr.py` exited 0 while one of two seams silently vanished; the
  checker now surfaces a defined primary and rejects it as a seam defined in a production tree
  (`docs/CollectionVersionTestSeamDesign.md` §15). The consumer fixture is still required for
  a mutation the checker cannot see — making a collection's private counter public — which is
  caught only by compilation (`docs/NegativeConsumerFixtureValidation.md` §18.4 row four). A
  seam added by a future ticket must therefore gain a `test/consumer/*_negative.cpp` site too,
  not only a single definition site. Note also what neither check can express: a consumer that reopens
  `namespace SharpRuntime::Testing` and writes its own explicit specialisation **does** get
  the access the friend declaration grants, for both seams; that is well-formed ISO C++, is
  unsupported, and is recorded in §18.5 rather than assumed away.
- **Negative consumer fixtures:** a `test/consumer/*_negative.cpp` proves that a spelling a
  ticket outlawed is **rejected by the compiler**. It must carry a
  `// NEGATIVE-FIXTURE: component=<Component>` directive, an
  `#ifndef SHARP_RUNTIME_NEGATIVE_SITE / #define … 0 / #endif` prelude, and one
  `#if SHARP_RUNTIME_NEGATIVE_SITE == N` guard per negative site, each holding exactly one
  `// NEGATIVE(<kebab-id>): <expected diagnostic fragment>` marker (further alternatives on
  following `//     | <fragment>` lines). Site numbers must be `1..N`; the `#else` branch is
  where the migrated spelling goes. **With no site selected the file must compile with zero
  diagnostics** — that clean baseline is what lets a per-site verdict be attributed to its own
  source, so add `(void)x;` wherever disabling a site orphans a local.
  `scripts/check_negative_consumer_fixtures.py` compiles the baseline plus each site
  separately (`-fsyntax-only`, `-Wall -Wextra -Wpedantic -Werror`, at most two jobs, include
  directories derived from the CMake component metadata) and runs in
  `scripts/local_ci_check.sh`. Never assert only that a whole fixture fails to compile: one
  broken line hides every other line, and a whole-file check reported a **false pass** while
  one of eleven claims had silently become legal again
  (`docs/NegativeConsumerFixtureValidation.md`, ticket #1801).

---

## plan.sqlite3 namespace review workflow

Classifying unclassified .NET types tracked in `plan.sqlite3` is the `classify-plan-types` skill — invoke it when reviewing or advancing that queue. Full workflow detail lives in `prompt.md`.

---

## Useful commands

```bash
# Build
cmake --build build --parallel 2

# Run all tests
scripts/run_component_tests.sh build

# Errors/warnings only
cmake --build build --parallel 2 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run a specific suite
./build/SharpRuntimeTests_Net_Sockets --gtest_filter="TcpClient*"
```
