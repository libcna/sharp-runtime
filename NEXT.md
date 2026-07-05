# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-07-05 (branch: `feature/work`, HEAD `1fa6cfd`) — 9121 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting, driven entirely by a `plan.sqlite3` namespace-review workflow (full rules in `prompt.md` — read that, not this file, for the process itself). The workflow is **fully autonomous**: no per-item user confirmation, classify and proceed. `System.Collections.Specialized` is now **fully complete**. Currently working through `System.Diagnostics` (one item left: `StackFrameExtensions`) then `System.Diagnostics.CodeAnalysis` (now fully complete) → `System.Globalization` next.
- **Header count:** ~610 `.hpp` files under `include/System/` (+ `SharpRuntime/`).
- **Key architectural decisions:** no runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), etc. Inner exceptions use `std::exception_ptr`, never `const std::exception&`.

---

## 2. Current status

### Build
**Clean.** `cmake --build build --parallel 4` — zero errors, zero warnings.

### Tests
**9121 tests passing** across 904 test suites. Zero failures.

### Branch / remote state
- `feature/work` — local working branch, HEAD `135a423`, **pushed to `origin/feature/work`** (routine push target — safe to push here anytime work is committed). This is where active porting happens.
- `develop` — last known state: fast-forwarded to `origin/develop` and merged with an earlier `feature/work` HEAD (`7056dcb`), pushed. **Not merged with this session's commits** — only merge/push to `develop` when the user explicitly asks in that turn.
- `master` — untouched. Do not touch without explicit instruction.
- `plan.sqlite3` is gitignored — local workflow state only, not part of what gets pushed.

### IMPORTANT: two local clones exist
This environment has **two separate git clones** of the same repo:
- `/rv/.../sharp-runtime_work` — primary working directory, **on `feature/work`**. All active porting must happen here.
- `/rv/.../sharp-runtime` — a second clone, was left on `develop`. **Do not edit files here** — a prior session in this conversation accidentally did a full port there before noticing. Always double-check `pwd`/absolute paths point at `sharp-runtime_work` before editing.

### What works (this session's commits, most recent first)
All of `System.Collections.Specialized` was completed this session — every item had at least one real bug, not just missing status:
- `StringDictionary` — `Add()` silently overwrote a duplicate (case-insensitive) key instead of throwing `ArgumentException`.
- `StringCollection` — `Insert()`/`CopyTo()` had zero bounds validation (undefined behavior / out-of-bounds writes on bad index); wrong exception types (`std::out_of_range` → `ArgumentOutOfRangeException`).
- `OrderedDictionary` — **`AsReadOnly()` mutated the original instance** (`this->readOnly_ = true; return *this;`), permanently breaking mutation on every other holder of that object. Rewrote storage behind `shared_ptr` so `AsReadOnly()` returns a live, independent read-only *view* instead, matching .NET. Also: missing `Insert()` bounds validation, wrong exception types, missing `Item[int]` setter.
- `NotifyCollectionChangedAction`/`EventArgs`/`EventHandler` — extracted from their previous location embedded (wrong namespace) inside `ObservableCollection.hpp` into proper standalone headers under `Collections/Specialized/`, and added real per-constructor action validation that didn't exist before. `Replace`/`Move` are exposed as named static factories, not constructor overloads — a `(action, T, T, intcs)` Replace constructor would collide with the `(action, T, intcs)` Add/Remove constructor whenever `T` is `intcs` (e.g. `NotifyCollectionChangedEventArgs<int>`, a real instantiation used by `ObservableCollection<int>` in this codebase's own tests).
- `NameValueCollection` — was case-*sensitive*; .NET's default constructor is case-*insensitive*. Fixed with custom hash/equal functors.
- `ListDictionary` — wrong exception types; indexer threw for a missing key instead of returning null/empty (.NET returns null).
- `HybridDictionary` — `Add()` silently overwrote a duplicate key; `caseInsensitive` constructor flag was accepted but never enforced at all.
- `BitVector32` — `CreateMask(int.MinValue)` silently wrapped instead of throwing `InvalidOperationException`; `CreateSection` had zero validation (non-positive `maxValue`, section overflow past bit 32).
- `ReadOnlySet<T>` — constructor didn't null-check its `shared_ptr`, so a null set segfaulted on first access instead of throwing `ArgumentNullException`.

Also this session:
- `System.Diagnostics.DebugProvider` — added as a real virtual base (`Write`/`WriteLine`/`Fail`/`OnIndentLevelChanged`/`OnIndentSizeChanged`); `Debug` now routes through a pluggable provider (`Debug::GetProvider()`/`SetProvider()`) instead of hardcoded `iostream` calls, plus real `IndentLevel`/`IndentSize`/`Indent()`/`Unindent()`.
- `System.Diagnostics.DebuggerGuidedStepThroughAttribute` — trivial marker attribute, same shape as its siblings.
- `System.Diagnostics.CodeAnalysis` — **fully complete** (28/28 items). 16 were already implemented in `CodeAnalysisAttributes.hpp`; added the remaining 7 real ports (`ConstantExpectedAttribute`, `ExperimentalAttribute`, `RequiresAssemblyFilesAttribute`, `RequiresUnsafeAttribute`, `SetsRequiredMembersAttribute`, `UnconditionalSuppressMessageAttribute`, `UnscopedRefAttribute`) plus fixed a real gap in the *already-ported* `SuppressMessageAttribute`: `Scope`/`Target`/`MessageId` had setters but no getters. Classified 5 as out-of-scope (IL-trimming/AOT tooling attributes with no meaning in a runtime with no trimmer: `DynamicDependencyAttribute`, `DynamicallyAccessedMemberTypes`, `DynamicallyAccessedMembersAttribute`, `FeatureGuardAttribute`, `FeatureSwitchDefinitionAttribute`).

### Database bookkeeping fix (this session)
Found **28 items stuck at `status='in_progress'`** (not a valid status per the workflow — `in_progress` doesn't exist). Audited all 28: **21 were actually fully implemented and tested** (just never got their status flipped to `ported`) — corrected to `ported`. **7 were genuinely unfinished** (no header file at all) — reset to `todo`:
- `System.Diagnostics.StackFrameExtensions`
- `System.IO.Compression.CompressionLevel`, `ZipCompressionMethod`, `ZipFile`, `ZipFileExtensions`
- `System.Net.Sockets.Socket`, `TcpListener`

If you hit more `status='in_progress'` rows anywhere in the DB, apply the same audit: check whether the header/test files actually exist and the type is exercised by passing tests before deciding `ported` vs `todo`.

### What does NOT work
- `Regex` — `std::regex` backend; no named groups, no lookbehind.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — `Socket` and `TcpListener` classes don't exist yet (see bookkeeping fix above); `TcpClient`/`UdpClient`/`NetworkStream` do.
- `IO::Compression` — `ZipFile`/`ZipFileExtensions`/`CompressionLevel`/`ZipCompressionMethod` don't exist yet; `ZipArchive`/`ZipArchiveEntry`/`DeflateStream`/`GZipStream`/`CompressionMode` do.
- `IO::RandomAccess` / `TimeZoneInfo` — POSIX-only (documented bugs per `CLAUDE.md`, not silent gaps).
- `AppDomain`/`AppContext` — Linux-only (`/proc/self/exe`).
- `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info.
- `ArrayList.GetEnumerator()` — returns `nullptr`; non-generic enumerator over `std::any` not implemented.
- Windows / Emscripten cross-compilation — untested; POSIX guards exist but are not CI-validated.
- `Debug::Write`/`WriteLine` do not auto-prefix output with the current indent after a newline (.NET tracks this per-provider); indent state is tracked and notifies the provider, but prefixing is left to callers — a deliberate, documented simplification, not a bug.

---

## 3. Recent changes

Full history: `git log --oneline`. Most recent first, this session's commits:

| Commit | Change |
|--------|--------|
| `1fa6cfd` | Ported remaining 7 `System.Diagnostics.CodeAnalysis` attributes; fixed missing getters on `SuppressMessageAttribute`; classified 5 trimming/AOT attributes out of scope. |
| `2f6a720` | Ported `DebuggerGuidedStepThroughAttribute` (trivial marker). |
| `fa898a9` | Ported `DebugProvider`: pluggable output provider + `IndentLevel`/`IndentSize`. |
| `547b899` | Fixed `StringDictionary.Add()` silent duplicate-key overwrite. |
| `8d8509a` | Fixed `StringCollection` missing bounds validation (`Insert`/`CopyTo`). |
| `37ffb2d` | Fixed `OrderedDictionary.AsReadOnly()` corrupting the original instance. |
| `9526f92` | Extracted `NotifyCollectionChangedAction`/`EventArgs`/`EventHandler` to `Collections/Specialized/`; added constructor validation. |
| `87dff59` | Fixed `NameValueCollection` missing case-insensitive key comparison. |
| `a13d179` | Fixed `ListDictionary` wrong exception types + missing-key indexer behavior. |
| `56c124d` | Fixed `HybridDictionary` silent duplicate-key overwrite + unenforced case-insensitivity. |
| `5647530` | Fixed `BitVector32` missing overflow/validation checks. |
| `f5d17e9` | Fixed `ReadOnlySet<T>` null-set segfault in constructor. |

Earlier history (prior sessions): see `git log` — `System.Collections.ObjectModel` full pass (`Collection`, `ReadOnlyCollection`, `KeyedCollection`, `ObservableCollection`, `ReadOnlyDictionary`, `ReadOnlyObservableCollection`), `System.Collections` (non-generic/.Concurrent/.Frozen/.Generic/.Immutable), core value types, exception hierarchy, `DateTime`/`TimeSpan`/`TimeZoneInfo`, `Span`/`Memory`, `Buffers`, `IO`/`IO.Compression`/`IO.Hashing`, `Text`/`Text.Json`, `Threading`/`Threading.Tasks`, `Numerics`, `Globalization`, `Net`/`Net.Http`, `Xml`.

### Recurring bug patterns worth knowing (confirmed again this session)
1. **Silent duplicate-key overwrite instead of `ArgumentException`** — found in `HybridDictionary`, `StringDictionary`, and (already fixed in prior sessions) `KeyedCollection`. Always check `Add()` on a dictionary-like type for this.
2. **Wrong exception types** — `std::out_of_range`/`std::invalid_argument`/`std::runtime_error` instead of `ArgumentOutOfRangeException`/`ArgumentException`/`NotSupportedException`. Endemic across `Specialized` types ported in early sessions before the exception hierarchy existed.
3. **Missing bounds validation before iterator arithmetic** — `Insert()`/`CopyTo()` on vector-backed types frequently had zero index validation, meaning a bad index was silent undefined behavior rather than a thrown exception.
4. **Read-only wrapper mutates the original** — `OrderedDictionary.AsReadOnly()` did `this->readOnly_ = true; return *this;`, breaking the *source* object. Whenever a type has an "AsReadOnly()"-shaped method, verify it returns an independent (or shared-storage live-view) instance, not `*this`.
5. **Case-insensitivity accepted as a constructor parameter but never enforced** — found in `HybridDictionary` (explicit `caseInsensitive` param) and `NameValueCollection` (implicit default case-insensitivity). Check whether `.NET`'s default comparer for the type is actually case-insensitive before assuming case-sensitive `std::unordered_map` semantics are correct.

---

## 4. Current blocker / main problem

**No active blocker.** Build is clean, 9121 tests pass. Pushed to `origin/feature/work`. Not merged into `develop` this session — only do that when the user explicitly asks.

Next queued item (per `plan.sqlite3`, `System`-namespace-first ordering):
```
id=5344  System.Diagnostics  StackFrameExtensions  (status='todo', genuinely unimplemented — see §2 bookkeeping fix)
```
After that, `System.Diagnostics` is fully done and the queue moves into `System.Globalization` (first item: `Calendar`, id=6454).

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| missing | `System::Net::Sockets::Socket` / `TcpListener` — no header exists at all (not POSIX-only, just not ported) |
| missing | `System::IO::Compression::ZipFile` / `ZipFileExtensions` / `CompressionLevel` / `ZipCompressionMethod` — no header exists |
| missing | `System::Diagnostics::StackFrameExtensions` — no header exists |
| POSIX-only | `System::IO::RandomAccess` — `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; not portable to macOS |
| POSIX-only | `System::TimeZoneInfo` — `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| incomplete | `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; throws |
| incomplete | `ArrayList.GetEnumerator()` — returns `nullptr`; not yet iterable via `IEnumerator*` |
| incomplete | `CopyTo(Array, int)` — `System.Array` type does not exist; skipped on all collections |
| deliberate simplification | `Debug`'s output does not auto-indent after newlines (see §2) |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub (by design) | `System::GC` — all methods are no-ops; correct end state, not a gap |
| stub (by design) | `System::Type` / `System::Activator` — no runtime reflection, correct end state |
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse` not implemented |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| workflow risk | Duplicate GoogleTest suite names cause linker errors — always check for collisions |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d, different from the workflow's `'ignore'`) — predates the current workflow, inert legacy data |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  System/
    Collections/Specialized/            ← now complete: BitVector32, HybridDictionary,
                                           ListDictionary, NameValueCollection,
                                           NotifyCollectionChanged{Action,EventArgs,EventHandler},
                                           OrderedDictionary, StringCollection, StringDictionary
    Diagnostics/                        ← Debug, DebugProvider (new), Trace, Stopwatch,
                                           StackFrame, StackTrace, Debugger*Attribute family
    Diagnostics/CodeAnalysis/           ← CodeAnalysisAttributes.hpp (now complete, 28/28)
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← GoogleTest .cpp files, "Batch##Tests.cpp" per session
plan.sqlite3                            ← porting-status tracker (gitignored, local-only)
prompt.md                               ← canonical plan.sqlite3 workflow instructions
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** before every commit.
2. **9121+ tests passing** — never go below this watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (`int32_t`) in public APIs mirroring .NET `int` parameters; **`SharpRuntime::shortcs`** (`int16_t`) for .NET `short`.
6. **SPDX header on every file.**
7. **Doxygen `/** */`** on all public declarations — never `///`.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **Inner exceptions use `std::exception_ptr`**, never `const std::exception&`.
10. **No broad header refactor** — property naming touches 449+ files in CNA.
11. **Push only to `feature/work`** — never push to `develop` or `master`, and never create tags, without explicit per-action user approval.
12. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.
13. **plan.sqlite3 processing is fully autonomous** — classify and proceed without asking per item.

### Established local conventions worth following
- **Exception message text** for key-not-found is the plain `KeyNotFoundException()` default message; duplicate-key `ArgumentException` message is `"An item with the same key has already been added."` — used consistently across containers, match it rather than inventing new text.
- **Shared live-view wrappers** (`ReadOnlyDictionary<K,V>`, `ReadOnlyObservableCollection<T>`, now also `OrderedDictionary::AsReadOnly()`) use `std::shared_ptr<Underlying>` to share ownership with the original mutable object. Plain value-copying wrappers (`ReadOnlyCollection<T>`) are a deliberate, different, already-established choice. Don't conflate the two.
- **Ambiguous constructor-overload hazard with `SharpRuntime::intcs`**: when a templated type `Foo<T>` has two constructors shaped `(X, T, intcs)` and `(X, T, T, intcs=default)`, they become identically-shaped when `T == intcs` (a very plausible instantiation, e.g. `Foo<int>`). Resolve by making one of them a named static factory instead of an overloaded constructor (see `NotifyCollectionChangedEventArgs<T>::Replace`/`::Move`), not by trying to disambiguate via SFINAE/tag types on the constructor itself.
- **Case-insensitive key comparison for `std::unordered_map`-backed Specialized types**: implement via custom hash+equal functor *instances* (not template parameters) holding a runtime `bool caseInsensitive` flag, passed to the map's constructor. See `HybridDictionary`/`NameValueCollection` for the pattern.

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`.
- Valid status values: `''` (unset), `todo`, `ported`, `ignore`, `tobedecided`. **`in_progress` is not valid** — if found, audit (see §2) rather than ignoring.
- Query next unset/todo item, `System`-namespace-first:
  ```sql
  SELECT id,namespace,name,type FROM task
  WHERE (status='' OR status='todo')
  ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name
  LIMIT 1;
  ```

---

## 7. Useful commands

```bash
# Build
cmake --build build --parallel 4

# Build — errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run all tests
./build/SharpRuntimeTests

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="StackFrame*"

# Check next unset/todo type (System namespace prioritized)
sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 8;"

# Mark an item ported after review+tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"

# Check .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"
```

---

## 8. Next smallest tasks

### Task 1 — System.Diagnostics.StackFrameExtensions (id=5344)
- **Goal:** No header exists yet — this is a genuine new port, not a review. Look up `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Diagnostics/StackFrameExtensions.cs`. It's a small static-method extension class (extends `StackFrame` with `GetNativeIP()`/`GetNativeImageBase()`/`HasNativeImage()`/`HasSource()`) — check whether these concepts (native IP, image base) are meaningful in this codebase's `StackFrame` implementation before deciding how much to port vs. stub with `NotImplementedException`.
- **Files:** likely new `include/System/Diagnostics/StackFrameExtensions.hpp`, extend `tests/System/Diagnostics/DiagnosticsRemainingTests.cpp` or similar.
- **Verify:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="*StackFrame*"`

### Task 2 — System.Globalization.Calendar and family (id=6454+)
- **Goal:** Next namespace after Diagnostics. `Calendar` is an abstract base with many derived calendar types (`GregorianCalendar`, `HebrewCalendar`, etc.) — check what's already implemented in `include/System/Globalization/` before assuming a fresh port; NEXT.md from prior sessions claims `Globalization` is "done" but that was written before this session's discovery that `in_progress`/stale-status bookkeeping can't be trusted at face value. Verify against actual files + passing tests, same as the audit done this session for the 28 `in_progress` rows.
- **Files:** `include/System/Globalization/`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="*Calendar*"`

### Task 3 — Continue plan.sqlite3 in order
- After Globalization, keep following `System`-namespace-first order per `prompt.md`. Given this session found real bugs in essentially every "already ported" type reviewed, **do not rubber-stamp** existing files — apply the full `CLAUDE.md` checklist every time, especially checking for the 5 recurring bug patterns in §3.

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA.
- **No LINQ port** — use `std::ranges` in all new ported code.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs, not open work items.
- **Push only to `feature/work`** — pushing to `develop` or `master`, or merging `feature/work` → `develop`, requires the user explicitly asking in that turn.
- **No new vendored libraries** without discussing scope impact.
- **No speculative API additions** — only add methods present in .NET's published API surface.
- **No work on `System::Type` / `System::Activator`** — stubs are the correct end state.
- **No duplicate GoogleTest suite names** — check for collisions.
- **No reintroduction of `///` Doxygen** — all headers use `/** */`.
- **No mass rewrite or reformatting** in a single commit — incremental changes only.
- **No editing files in the second clone (`/rv/.../sharp-runtime`, currently on `develop`)** — always verify the absolute path targets `sharp-runtime_work` before writing.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation). NEXT.md is a snapshot for context, not the source of truth for process.

IMPORTANT: work only in /rv/.../sharp-runtime_work (branch feature/work). A second clone at
/rv/.../sharp-runtime exists on develop — do not edit files there.

Then inspect only the files needed for Task 1 in NEXT.md §8 (currently: StackFrameExtensions in
System.Diagnostics, plan.sqlite3 id=5344 — a genuine new port, no header exists yet).

For that task:
  1. Look up the .NET reference in /rv/tmp/runtime/src/libraries/ and read the existing C++
     StackFrame.hpp for context.
  2. Implement per the full checklist in CLAUDE.md (API surface, doc-comments, SPDX, logic parity,
     bounds/null validation) — this is a new port, not a review.
  3. Run: cmake --build build --parallel 4   (zero errors, zero warnings)
  4. Run: ./build/SharpRuntimeTests           (9121+ tests must still pass)
  5. Mark it ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=5344;"
  6. Commit only the files for that change: git -c commit.gpgsign=false commit -m "..."
  7. Continue to the next todo item per prompt.md's Step 1 — don't stop to ask before each item.
  8. Update NEXT.md with what changed before ending the session.
  9. It's fine to push to `feature/work` (origin) as work lands — that's the routine push target.
     Never push to `develop` or `master`, and never merge feature/work → develop, without the user
     explicitly asking in that turn.
```
