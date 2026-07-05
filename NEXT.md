# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-05 (branch: `feature/work`, HEAD `743de40`)*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET
`System.*` namespace, so that C#/XNA game code ported to C++ can compile against these headers
with minimal source changes.

- **Main goal:** provide C++ counterparts of `System.*` types so that **CNA** (a C++ XNA port)
  and **mobile-eggbert** (a ported Windows Phone game) can build and run without a .NET runtime.
- **Current phase:** active, incremental porting. Progress is tracked in a local SQLite database,
  `plan.sqlite3` (gitignored, not part of the repo — local workflow state only), which lists every
  type from `dotnet/runtime` and its port status. The process is documented in `prompt.md` and
  `CLAUDE.md` at the repo root; this file is a point-in-time snapshot, not the process definition.
- **Important architectural decisions:**
  - No runtime reflection, no GC, no IL — `System::GC`, `System::Type`, `System::Activator` are
    intentional no-op/stub end states, not gaps.
  - Properties are exposed as `getXxxProperty()` / `setXxxProperty()` methods, never public fields.
  - .NET primitive types map to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), `longcs`,
    `uintcs`, etc. — public APIs mirroring a .NET `int` parameter must use `intcs`, not `int`.
  - Inner exceptions use `std::exception_ptr`, never `const std::exception&`.
  - No LINQ port — `std::ranges` is used instead in new code. `System::Linq` exists as a small,
    intentionally partial header (~25 of .NET's ~200 `Enumerable` methods), not a full LINQ engine.
  - Vendored third-party libraries: GoogleTest, nlohmann/json, tinyxml2, miniz. Never commit binaries.
  - Namespaces are opened with C++17 nested syntax: `namespace System::Collections::Generic {`.
  - Complex types get a `.hpp` + `.cpp` pair; simple types may be header-only.

---

## 2. Current status

### Build
**Clean** as of HEAD `743de40` — `cmake --build build --parallel 4` produced zero errors and zero
warnings on the last verified run. (Not rebuilt again for this handoff, per instruction not to
build/develop right now — treat as "last known good," not re-verified this instant.)

### Tests
**9616 / 9616 tests passing** across 975 GoogleTest suites, last verified at HEAD `743de40`,
stable across 3 consecutive runs (no flakiness observed). `./build/SharpRuntimeTests` is the
single test binary covering the whole library.

### CLI / tools / apps / libraries
This repository is a **library only** — there is no CLI, app, or standalone tool. The only build
products are the static library (`SHARP_RUNTIME`) and the test binary (`SharpRuntimeTests`). The
GoogleTest suite is the primary "demo" of working functionality; there is no separate sample app
in this repo (CNA and mobile-eggbert, which consume this library, are separate projects).

### Recently implemented (this session, all fully complete and tested)
- `System.Numerics` — `Vector2/3/4`, `Matrix3x2/4x4`, `Plane`, `Quaternion`, `BFloat16`,
  `BitOperations`, `Complex`, `BigInteger`, generic-math interface stubs, `TotalOrderIeee754Comparer`.
  (`Vector<T>`, the generic hardware-SIMD type, deliberately left undecided — see §4.)
- `System.Linq` — classified; only `Enumerable` counts as ported (existing practical-subset
  `System::Linq.hpp`), everything else (LINQ-to-Objects internals, `IQueryable`/PLINQ/async-LINQ)
  is out of scope per the "no LINQ" architectural rule.
- `System.IO.IsolatedStorage`, `System.IO.Hashing` (incl. `XxHash3`/`XxHash128`),
  `System.IO.Compression`, `System.IO` — all fully complete from earlier in this session.
- `System.Threading` (59/60 — `WaitHandleExtensions` ignored, needs a native `SafeWaitHandle` this
  port doesn't expose). Fixed a real bug: `Monitor` was a complete no-op stub.
- `System.Threading.Tasks` (16/17 — `ConcurrentExclusiveSchedulerPair` ignored, needs a real
  pluggable-scheduler queuing engine this runtime doesn't have).
- `System.Xml` (62/62). Classic `XmlDocument` DOM API wrapping vendored tinyxml2, `XmlReader`/
  `XmlWriter`, `XmlNamespaceManager`, resolvers, reader/writer settings, `XmlTextReader`/
  `XmlTextWriter`/`XmlValidatingReader` (via composition, not inheritance — see §6), `XmlParserContext`.
  Fixed three real bugs found during review (see §3).

### What does not work yet
Large namespaces remain unclassified/unported. By remaining `todo` count in `plan.sqlite3`:
`System.Security.Cryptography` (50), `System.Text` (36), `System.Text.Json.Serialization` (31),
`System.Xml.Serialization` (30), `System.Net` (26), `System.Net.Http.Headers` (25),
`System.Xml.Linq` (24, separate from `System.Xml` — untouched), `System.Net.Sockets` (21), and
smaller `System.Net.*`/`System.Text.*`/`System.Xml.XPath` namespaces. `System::Net::Sockets::Socket`
and `TcpListener` have no header at all (not POSIX-only — simply not started).

---

## 3. Recent changes

Most recent first (see `git log --oneline` for full history):

| Commit | Change |
|--------|--------|
| `743de40` | `NEXT.md` update only (System.Xml completion note). |
| `0d2a17e` | Added `XmlNamespaceManager` (real scope-stack), `XmlResolver`/`XmlUrlResolver`/`XmlSecureResolver`, `XmlReaderSettings`/`XmlWriterSettings`, `XmlParserContext`, `XmlTextReader`/`XmlTextWriter`/`XmlValidatingReader`. Completes `System.Xml` (62/62). New tests: `XmlNamespaceManagerTests.cpp`, `XmlResolverTests.cpp`, `XmlTextReaderWriterTests.cpp`. |
| `26a1073` | Added the classic `XmlDocument` DOM class hierarchy (`XmlNode`, `XmlDocument`, `XmlElement`, `XmlAttribute`, `XmlCharacterData` and its subclasses, `XmlLinkedNode` and its subclasses, etc.), wrapping vendored tinyxml2. **Fixed 3 real bugs**: (1) `InnerText`/`InnerXml` setters called the virtual `RemoveAll()`, which `XmlElement` overrides to also strip attributes, so setting `InnerText` silently wiped attributes too — added a non-virtual `RemoveAllChildren()` and rerouted the setters through it; (2) `XmlDocument`'s own `ownerDocument_` is correctly always null, but every internal node operation assumed it was set, so calling them directly on the document object silently no-op'd — added `GetDocument()` (mirrors .NET's internal `Document` property) and rerouted through it; (3) text/comment/CDATA nodes had no `WriteTo` override, so `XmlWriter`-based serialization silently dropped their content — added the overrides and a new `XmlWriter::WriteCData`. New test file: `XmlDomTests.cpp`. |
| `8b9cdbc` | Added `XmlNameTable`/`NameTable`, `XmlConvert`, `XmlQualifiedName`, `XmlNodeChangedEventArgs`/`XmlNodeChangedEventHandler`, `IHasXmlNode`, `IXmlLineInfo`, `IXmlNamespaceResolver`. |
| `b92d58a` | Added 18 `System.Xml` enums and `XmlException`; fixed `XmlReader`/`XmlWriter` (pre-existing, from before this session) throwing the wrong exception type. |
| `017b01b` | Fixed a flaky test (`IsolatedStorageFileTests.IsAnIsolatedStorageBase_ViaVirtualDispatch`) that compared live disk free-space across two separate calls for exact equality — legitimately racy under concurrent disk activity. |
| `170142b` | Ported `System.Threading.Tasks` (16/17). Fixed `Task::FromCanceled(CancellationToken)` silently discarding its argument; fixed `Parallel.hpp` using `int` instead of `intcs`. Added real `ParallelLoopState.Stop()`/`Break()` support. 80 new tests. |
| `62bfef5` | Ported `System.Threading` (59/60). Fixed `Monitor` (was a complete no-op stub — Enter/Exit/Wait/Pulse did nothing), `Mutex` (didn't derive from `WaitHandle`; `WaitOne(ms)` didn't actually block up to timeout), `Semaphore`/`SemaphoreSlim` (no argument validation), `ReaderWriterLockSlim` (`IsReadLockHeld` etc. always returned `false`), `Barrier` (post-phase exceptions silently swallowed). 33 new tests. |
| `df32df9` and earlier | `System.Numerics` completion, `System.Linq` classification, `System.IO.Hashing` (`XxHash3`/`128`), `System.IO.IsolatedStorage`, `System.IO.Compression`, `System.IO` (56 items) — see full `git log --oneline` for detail. |

---

## 4. Current blocker / main problem

**No build- or test-breaking blocker exists right now.** The last verified state is clean and
stable (9616/9616 tests, 3 consecutive runs, zero warnings).

Two things are worth flagging as the closest things to an open problem:

1. **`Vector<T>` (id `9228` in `plan.sqlite3`, namespace `System.Numerics`) is marked
   `tobedecided`**, not ported. It's .NET's generic, hardware-width SIMD vector type — a real
   architecture decision is needed before implementing it (fixed-width fallback vs. actual SIMD
   intrinsics vs. `std::experimental::simd`), since it's structurally unlike the already-ported
   fixed-size `Vector2/3/4`. This needs a human decision, not a default guess.
2. **Process risk, not a code bug:** earlier this session, a background porting agent reported
   task status "completed" while it had actually stopped mid-task — some work was uncommitted,
   with a known unresolved bug and 3 failing tests. This was only caught by explicitly checking
   `git log`/`git status`/re-running the test suite rather than trusting the agent's self-report.
   **Any future delegated/background work must be verified the same way** (actual commits, actual
   test run) before being treated as done.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| incomplete (needs decision) | `System::Numerics::Vector<T>` — no header exists; `tobedecided` pending an architecture choice (see §4). |
| missing | `System::Net::Sockets::Socket` / `TcpListener` — no header exists at all (not POSIX-only, simply not started). |
| documented limitation | `XmlUrlResolver::GetEntity` only reads local files (`file://` or plain paths) — no network stack for http(s) entity resolution. |
| documented limitation | `XmlResolver`/`XmlUrlResolver`/`XmlSecureResolver`'s `ofObjectToReturn` parameter (a .NET `Type?`) is accepted but ignored — no runtime reflection to act on it; always returns a `std::string` via `std::any`. |
| documented limitation | `XmlReaderSettings`/`XmlWriterSettings` — most properties are stored for API-name compatibility but not consulted by the concrete `XmlReader`/`XmlWriter` (documented per-property, not silent). |
| documented limitation | `XmlTextReader`/`XmlTextWriter`/`XmlValidatingReader` are implemented via composition (own an `XmlReader`/`XmlWriter`, forward calls) rather than inheritance, since this runtime's `XmlReader`/`XmlWriter` are single concrete classes, not .NET's extensible abstract hierarchy. |
| documented limitation | `XmlValidatingReader` performs no actual DTD/XSD validation regardless of `ValidationType` — matches this runtime's general no-DTD-validation stance. |
| documented limitation | `XmlDocument`'s `NodeInserting`/`NodeInserted`/`NodeRemoving`/`NodeRemoved`/`NodeChanging`/`NodeChanged` events exist as fields but are not yet wired into `AppendChild`/`RemoveChild`/etc. |
| documented limitation | `System::Threading::Tasks::TaskScheduler` doesn't actually route `Task` execution (`Task` always uses `std::async` directly); `TaskFactory` omits APM-pattern `FromAsync` overloads (no `IAsyncResult` port to bridge to). |
| ignore (outofscope) | `ConcurrentExclusiveSchedulerPair` — needs a real pluggable-`TaskScheduler` queuing engine this runtime doesn't have. |
| ignore (outofscope) | `WaitHandleExtensions` — wraps `SafeWaitHandle`, a native OS handle this port's `WaitHandle` doesn't expose. |
| POSIX-only (known, by design) | `System::Net::Sockets`, `System::IO::RandomAccess` — POSIX-only APIs (`<sys/socket.h>`, `pread`/`pwrite`). |
| POSIX/Linux-only (known, by design) | `System::AppDomain`/`AppContext` (`/proc/self/exe`), `System::TimeZoneInfo` (`localtime_r`, `/usr/share/zoneinfo`). |
| stub (by design, correct end state) | `System::GC`, `System::Type`, `System::Activator` — no-ops/stubs; this is the intended final state, not a gap. |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d, distinct from the current workflow's `'ignore'`) predating this workflow — inert legacy data, not something to clean up as part of normal porting work. |
| needs verification | Emscripten/Windows builds have never been CI-tested this session; POSIX guards exist in `.cpp` files but are unverified on those platforms. |

---

## 6. Architecture notes

### Directory layout
- `include/System/...` — public headers, mirroring .NET namespace paths (e.g. `include/System/Xml/XmlDocument.hpp` for `System.Xml.XmlDocument`).
- `src/System/...` — `.cpp` bodies for complex types, same mirrored path.
- `tests/System/...` — GoogleTest files, same mirrored path; CMake's `GLOB_RECURSE` auto-discovers every `tests/**/*.cpp` and `src/**/*.cpp` — no manual registration needed when adding a file.
- `vendor/` — GoogleTest, nlohmann/json, tinyxml2, miniz (vendored, never modify in place, never commit binaries).
- `plan.sqlite3` — gitignored, local-only porting-progress database (table `task`: `id, namespace, name, type, internal, outofscope, status`).

### Key invariants that must not be broken
- **`getXxxProperty()`/`setXxxProperty()`** naming on every property, including static factory-style accessors — this is checked/enforced repeatedly across the whole codebase; do not introduce plain getters/setters or public fields.
- **`SharpRuntime::intcs`/`bytecs`/`longcs`/`uintcs`**, not native C++ `int`/`uint8_t`/etc., in any public API mirroring a .NET primitive parameter.
- **C++17 nested namespace syntax** (`namespace System::Xml {`), not the older nested-brace form.
- **No LINQ** in new ported code — use `std::ranges`.
- **POSIX-only includes** (`<unistd.h>`, `<sys/socket.h>`, etc.) must stay inside `.cpp` files behind `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` / `#else`, never in public `.hpp` headers.
- **SPDX header required** on every `.hpp`/`.cpp` file:
  ```cpp
  // SPDX-License-Identifier: MIT
  // Copyright (c) Robert Vokac and contributors
  // Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
  ```
- **Doxygen `/** */` only** — never reintroduce `///` triple-slash comments.
- A derived class that declares **any** overload of a base-class method name hides *all* other
  base-class overloads of that name unless a `using BaseClass::MethodName;` is added — this exact
  bug pattern has recurred repeatedly (`StreamWriter`/`StringWriter`, `Crc32`/`Crc64`/`Adler32`/
  `XxHash32`/`XxHash64`, and elsewhere). Always check for this when adding an override in a class
  with inherited overloads of the same name.

### Data flow / notable patterns
- `System::Xml`'s DOM classes wrap `tinyxml2::XMLNode*`/`XMLDocument` — `XmlDocument` owns the
  native `tinyxml2::XMLDocument` and caches one C++ wrapper object per native pointer so repeated
  navigation calls return stable identities. Internal node operations must resolve the owning
  document via `XmlNode::GetDocument()` (not the raw `ownerDocument_` field), because
  `XmlDocument`'s own `ownerDocument_` is always null (matching .NET) — see the Bug 2 fix in §3.
- `System::IO::Compression` wraps zlib/miniz directly; `System::Text::Json` wraps nlohmann/json
  (check that directory for the established wrapping conventions before adding a new wrapped type).
- `System::Threading::Tasks::Task` is `std::async`-backed (eager/thread-based), not a lazy
  continuation/state-machine model — there is no C++ equivalent of compiler-generated async state
  machines, so `Task` is a deliberately simplified practical-subset design, not a 1:1 port.

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
For each `''`/`todo` item, classify without asking the user: port it (apply the full checklist in
`CLAUDE.md` — API surface, doc-comments, SPDX, logic parity vs. `/rv/tmp/runtime/src/libraries/`,
clean build, passing tests), mark `ignore` (`outofscope=1` for permanent-deviation categories:
reflection, GC internals, P/Invoke, serialization infra, etc.), or mark `tobedecided` only when
genuinely ambiguous. `in_progress` is not a valid status — porting happens directly.

---

## 7. Useful commands

```bash
# Build (zero errors/warnings required)
cmake --build build --parallel 4

# Build, showing only errors/warnings
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run the full test suite
./build/SharpRuntimeTests

# Run a specific suite/test (glob pattern)
./build/SharpRuntimeTests --gtest_filter="XmlDocumentTests.*"

# Check next unset/todo items in a namespace
sqlite3 plan.sqlite3 "SELECT id,name,type,status FROM task WHERE namespace='System.Text' AND (status='' OR status='todo') ORDER BY name;"

# Mark an item ported after review + tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=<id>;"

# Find the .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit (GPG signing times out in this sandboxed environment — always disable it explicitly)
git -c commit.gpgsign=false commit -m "message"
```

There is no separate lint/format tool configured in this repository, and no standalone demo/sample
binary beyond the GoogleTest suite.

---

## 8. Next smallest tasks

1. **Decide `System::Numerics::Vector<T>` scope** (id `9228`).
   - Goal: get a decision on whether to implement a fixed-width fallback, real SIMD intrinsics, or
     `std::experimental::simd`-backed generic vector — or leave it permanently out of scope.
   - Files: none yet (`include/System/Numerics/Vector.hpp` does not exist).
   - Verification: N/A until a decision is made — this is a design decision, not a coding task.

2. **Scope and start `System.Security.Cryptography`** (50 `todo` items — the largest remaining namespace).
   - Goal: query `plan.sqlite3` for the exact item list, check `include/System/Security/Cryptography/`
     for what (if anything) already exists, and classify/port the first batch (likely hash
     algorithms and simple enums first, following the same "check the filesystem before assuming a
     fresh port" lesson learned this session with `System.Numerics`/`System.Threading`).
   - Files: `include/System/Security/Cryptography/`, `src/System/Security/Cryptography/`,
     `tests/System/Security/Cryptography/`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

3. **Port `System.Xml.Linq`** (24 items — `XDocument`/`XElement`/`XAttribute` family).
   - Goal: this is a separate namespace from `System.Xml` (untouched this session); check
     `include/System/Xml/Linq/` for existing headers (some may already exist per earlier sessions)
     before starting fresh.
   - Files: `include/System/Xml/Linq/`, `src/System/Xml/Linq/`, `tests/System/Xml/Linq/`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="*XDocument*:*XElement*"`.

4. **Port `System.Text`** (36 items).
   - Goal: query the exact item list; this namespace includes core string-building/encoding types
     that other not-yet-ported namespaces (e.g. `System.Text.Json.Serialization`) likely depend on.
   - Files: `include/System/Text/`, `src/System/Text/`, `tests/System/Text/`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

---

## 9. Do not do yet

- **No broad header refactor** — `getXxxProperty()` naming and namespace style already touch
  449+ files across this project and CNA; do not attempt a sweeping rename/reformat pass.
- **No LINQ implementation expansion** — `System::Linq.hpp`'s partial subset is an intentional,
  accepted design point, not a gap to fill in.
- **No work on `Vector<T>`** until the architecture decision in §4/§8 task 1 is made by the user.
- **No Windows/Emscripten CI setup** — POSIX-only subsystems are documented, accepted bugs, not
  open work items to fix opportunistically.
- **Push only to `feature/work`** — never push to `develop`/`master`, never create tags, without
  explicit per-action user approval in that turn. Routine pushes to `origin/feature/work` are
  pre-authorized.
- **No mass rewrite or reformatting** in a single commit — this session's pattern has been small,
  reviewable, per-namespace (or per-batch) commits; keep following it.
- **No blind trust in background/delegated agent "completed" reports** — always verify via
  `git log`/`git status`/an actual test run before treating delegated work as done (see §4).
- **No speculative API additions** — only port methods/types that actually exist in .NET's
  published surface (check `/rv/tmp/runtime/src/libraries/` and the relevant `ref/*.cs` file).

---

## 10. Resume prompt

```
Read NEXT.md first. It reflects the actual, verified repository state as of HEAD 743de40
(9616/9616 tests passing, clean build, zero warnings) — do not assume anything beyond what it
documents.

Inspect only the files needed for the first task in NEXT.md §8. Do not refactor unrelated code,
and do not expand scope beyond that one task (see NEXT.md §9 for things explicitly out of bounds
right now).

Make one small, verified improvement:
  1. Read the relevant .NET reference source under /rv/tmp/runtime/src/libraries/ and any existing
     C++ header/source for the type(s) involved (check the filesystem first — several "todo" items
     this session turned out to already have a file, just unmarked in plan.sqlite3).
  2. Implement/fix per the full checklist in CLAUDE.md (API surface, doc-comments, SPDX header,
     logic parity, getXxxProperty()/setXxxProperty() naming, intcs/bytecs/etc. usage).
  3. Run: cmake --build build --parallel 4   (must be zero errors, zero warnings)
  4. Run: ./build/SharpRuntimeTests           (must show 9616+ passing, zero failures)
  5. If it's a plan.sqlite3-tracked item, update its status:
     sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=<id>;"
  6. Commit only the files for that one change: git -c commit.gpgsign=false commit -m "..."
  7. Update NEXT.md to reflect the new state (test count, HEAD commit, what changed) before ending
     the session.
```
