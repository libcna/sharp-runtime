# NEXT.md

*Last updated: 2026-07-13. Branch: `feature/work`. HEAD: `9d4e77e`.*

This document was rewritten from scratch on 2026-07-13 into a structured handoff format,
replacing a long chronological session-log that had grown to ~6000 lines. That prior log is not
lost — it is preserved in full in git history (`git log --oneline -- NEXT.md`) and in
`POST_STABILIZATION_AUDIT.md` (a standalone audit report still present in the repo root). This
file intentionally contains only the current, load-bearing state.

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of .NET's
`System.*` namespace, so that ported C#/XNA game code compiles against C++ headers with minimal
changes. It is the foundation for **CNA** (a C++ XNA port) and **mobile-eggbert** — this repo
does not contain either of those consumers; it is a standalone dependency.

**Main goal**: provide broad, behaviorally-faithful coverage of the .NET BCL surface that a 2D/3D
game (and its tooling) would realistically need, implemented as idiomatic modern C++ rather than
a literal transliteration.

**Current development phase**: post-stabilization. The original porting/stabilization backlog
(tracked in `plan.sqlite3`'s `ticket` table) is fully complete — every ticket is `done`, none
`blocked` or `todo`. A subsequent "assume nothing is perfect" fresh audit
(`POST_STABILIZATION_AUDIT.md`) found 20 concrete, verified issues across the already-"complete"
codebase; all 19 tickets it spawned are also now `done`. There is currently no open, pre-scoped
work item anywhere in `plan.sqlite3`. The project is in a stable, unblocked state awaiting
direction for the next body of work (see §8 for candidate next tasks, §10 for a resume prompt).

**Important architectural decisions**:
- Fixed-width integer aliases (`SharpRuntime::intcs`/`longcs`/`shortcs`/`uintcs`/`ulongcs`/
  `ushortcs`/`bytecs`/`sbytecs`, defined in `include/SharpRuntime/SharpRuntimeHelper.hpp` as
  plain `using` aliases for `int32_t`/`int64_t`/`int16_t`/`uint32_t`/`uint64_t`/`uint16_t`/
  `uint8_t`/`int8_t`) are mandatory in every public API parameter/return value that mirrors a
  .NET `int`/`long`/etc. This rollout was just completed project-wide (previously blocked, then
  explicitly authorized and finished this session).
- Properties use `getXxxProperty()`/`setXxxProperty()` naming with **no exceptions**, except a
  documented one: C# indexers (`this[key]`) map to `getItem()`/`setItem()`. The last outstanding
  naming gap (`getCurrent()` on `IEnumerator` and 13 implementers) was renamed to
  `getCurrentProperty()` this session — **this is a deliberate breaking change**; no
  backward-compatibility alias was kept, by explicit decision.
- A fixed, documented set of .NET subsystems are **permanently out of scope**: reflection, GC
  internals (all `GC` methods are no-ops), delegate `DynamicInvoke`, serialization
  infrastructure, P/Invoke/interop, and symmetric/asymmetric cryptography + TLS + X.509 (hash
  algorithms MD5/SHA*/HMAC/PBKDF2 remain in scope and are ported). See `CLAUDE.md`'s "Known
  permanent deviations" section for the full, authoritative list.
- A fixed, documented set of subsystems are POSIX-only or Linux-only by necessity, not oversight:
  `System::Net::Sockets`, `System::IO::RandomAccess`, `System::AppDomain`/`AppContext`,
  `System::TimeZoneInfo`, `System::Diagnostics::Process`, `System::Runtime::InteropServices::
  PosixSignal`/`PosixSignalRegistration`, `System::Net::NetworkInformation::NetworkInterface`,
  `System::IO::FileSystemWatcher`. All of these now correctly throw
  `System::PlatformNotSupportedException` on unsupported platforms (verified by code review, not
  by actually compiling a Windows/Emscripten build — see §5).
- Every mutable generic collection (`List`, `Dictionary`, `HashSet`, `LinkedList`, `Queue`,
  `Stack`, `SortedDictionary`, `SortedList`, `SortedSet`, `OrderedDictionary`) now tracks a
  `version_` counter, incremented on structural mutation, checked by its
  enumerator/iterator on every step — enumerating while mutating throws
  `System::InvalidOperationException`, matching .NET's fail-fast contract. This is a new
  invariant established this session (previously entirely absent); see §6 for what this means
  for any new collection-like type.

---

## 2. Current status

**Build status**: last verified clean — 0 errors, 0 warnings — at HEAD (`9d4e77e`), via
`cmake --build build --parallel 4`.

**Test status**: last verified **12310/12310 passing**, 1221 test suites, via
`./build/SharpRuntimeTests`, at the same HEAD. Zero known failing tests.

**CLI/tools/apps/libraries currently available**: this repository produces a single static
library target, `SHARP_RUNTIME` (`libSHARP_RUNTIME.a`), plus one test executable,
`SharpRuntimeTests` (GoogleTest-based). **There is no CLI tool, sample app, or demo binary in
this repo** — it is a library-only dependency, consumed by CNA/mobile-eggbert (neither of which
lives in this repository).

**Recently implemented features** (this session's tail — see §3 for the full list): version-
tracking fail-fast enumeration across all mutable generic collections; `NumberStyles`-aware
`Parse`/`TryParse` for all 8 integer primitive types (partial — see §5); `CopyTo` for `List<T>`
and `StringBuilder`; disposed-state tracking for `MemoryStream`; a reentrancy-safe
`ConcurrentDictionary::GetOrAdd`/`AddOrUpdate`; correct round-to-nearest behavior for
`Convert::ToXxx(double/float)`; project-wide `getXxxProperty()` naming compliance.

**Known working examples/demos**: none exist. The GoogleTest suite (`tests/`) is the only
executable verification surface in this repo.

**What does not work yet / is not verified**:
- No Windows or Emscripten build has ever actually been compiled for this repo — `#ifdef`
  branches for those platforms exist and are code-reviewed, but are unverified by compilation
  (this sandbox is Linux-only). See §5.
- No performance/benchmark suite exists anywhere in this project's history — allocation patterns,
  algorithmic complexity, and hot-path efficiency have never been measured.
- Several documented API gaps remain (NumberStyles.Currency parsing, `Task.WhenAll`/`WhenAny`,
  `Channel::CreateUnboundedPrioritized`, `ImmutableList`'s missing LINQ-family methods — full list
  in §5).

---

## 3. Recent changes

Scoped to this session's most recent, most significant work (a much larger historical backlog —
1709 tickets covering the original porting/stabilization pass — was completed earlier in this
same session and is not re-itemized here; see `git log` for the full trail).

**Added**:
- `POST_STABILIZATION_AUDIT.md` — a fresh, independent audit report (7 parallel find-only
  passes covering API inconsistencies, undocumented .NET deviations, `NotImplementedException`
  residue, platform problems, exception-type mismatches, silent-wrong-behavior, and missing
  tests). 20 concrete findings, each backed by a .NET reference comparison and/or a standalone
  execution repro.
- Version-tracking (`version_` counter + fail-fast iterator/enumerator) added to `List`,
  `Dictionary`, `HashSet`, `LinkedList`, `Queue`, `Stack`, `SortedDictionary`, `SortedList`,
  `SortedSet`, `OrderedDictionary` (10 of 11 originally-targeted types — `PriorityQueue` has no
  enumeration surface at all, correctly left unmodified).
- `NumberStyles`-aware `Parse`/`TryParse` overloads for `Int16`/`Int32`/`Int64`/`UInt16`/
  `UInt32`/`UInt64`/`SByte`/`Byte`.
- `CopyTo` on `List<T>` and `StringBuilder`.
- Multi-threaded stress tests for `ConcurrentQueue`, `ConcurrentDictionary`, `Channel<T>`.
- `~180` new regression tests total across this session's tail (test count grew from an earlier
  12173 baseline to 12305).

**Modified (behavior changes)**:
- `Convert::ToXxx(double/float)` (all 8 integer-width overloads): now round to nearest
  (round-half-to-even), previously truncated toward zero. `ToInt64(double)` and `ToInt64(float)`
  now throw `OverflowException` out of range, previously silently wrapped.
- `Span<T>`/`ReadOnlySpan<T>` indexer: now throws `IndexOutOfRangeException`, previously threw
  `ArgumentOutOfRangeException`.
- `Dictionary`/`SortedDictionary`/`SortedList` non-const `operator[]`: now throws
  `KeyNotFoundException` on a missing key, previously silently inserted a default value.
- `ConcurrentDictionary::GetOrAdd`/`AddOrUpdate`: no longer hold the internal lock across the
  user-supplied factory callback (previously deadlocked on a reentrant factory call).
- `MemoryStream::Write` and `DeflateStream`/`GZipStream`/`ZLibStream::Write`: now validate
  buffer/offset/count and throw appropriately, previously silently no-op'd on invalid input (a
  negative offset previously caused a confirmed out-of-bounds read).
- `MemoryStream`: now tracks disposed state; `Read`/`Write`/`Seek` throw
  `ObjectDisposedException` after `Close()` (matching real .NET's exact split — `GetBuffer()`/
  `ToArray()` remain usable post-dispose).
- `Math::Round(double, MidpointRounding::ToEven)`: no longer depends on the ambient
  `fesetround()` floating-point state.
- `String::LastIndexOf(value, substr, startIndex)`: `startIndex == length` is now valid
  (previously threw).
- `StringInfo`'s `GetNextTextElement`/`GetNextTextElementLength`/`ParseCombiningCharacters`/
  `LengthInTextElements`: now UTF-8-aware (previously truncated multi-byte characters at the
  first continuation byte).
- `FileSystemWatcher`: now throws `PlatformNotSupportedException` on non-Linux platforms,
  previously silently no-op'd.
- **`getCurrent()` → `getCurrentProperty()`**: renamed across 23 files, 61 occurrences, in one
  coordinated commit. **Breaking change** — no old-name alias was kept.

**Bugs fixed** (headline list, see `POST_STABILIZATION_AUDIT.md` and `git log` for full detail
including exact repro cases):
Convert rounding; Span wrong exception type; Dictionary/SortedList silent key auto-insert;
ConcurrentDictionary reentrancy deadlock; MemoryStream + 3 compression streams' missing argument
validation (with a confirmed out-of-bounds read); missing version tracking → dangling
iterators/silent wrong iteration across 10 collection types; missing MemoryStream disposed-state
tracking; `Math::Round`'s FP-rounding-mode dependency; `String::LastIndexOf`'s over-eager
exception; `StringInfo`'s UTF-8 truncation; `FileSystemWatcher`'s silent platform failure; two
"bonus" bugs found incidentally while fixing siblings (`Int64`/`UInt64` hex-parse overflow
misclassification; `SortedList`'s indexer had the same auto-insert bug as `Dictionary`).

**Tests added**: ~180 new regression tests across the fixes above, each verified flake-free
across repeated runs where concurrency/timing was a factor.

**Since the rewrite above** (post-audit, self-directed continuation via "co dale?"/"what next?"):
- Re-verified the build/test baseline (`d403015`) and resolved `TypedReference`'s classification
  as a correct, permanent `ignore` (false alarm, not a bug).
- Implemented `Task::WhenAll(std::vector<Task>)` (`9d4e77e`) — waits on every input task without
  short-circuiting on the first fault, rethrows the first fault directly on `Wait()` (matching this
  class's existing no-`AggregateException`-wrapping convention), throws `TaskCanceledException` if
  no task faulted but at least one was canceled, and returns `CompletedTask()` immediately for an
  empty input without spawning a thread. 5 new regression tests in
  `tests/System/Threading/Tasks/TasksTests.cpp` (`TaskWhenAllTests` suite). Test count grew from
  12305 to 12310.

---

## 4. Current blocker / main problem

**There is no active build/test blocker right now.** Build was clean and all 12310 tests passed
at the last verification (HEAD `9d4e77e`). `plan.sqlite3`'s `ticket` table has zero `blocked`,
`todo`, or `doing` rows; the `task` table has zero unclassified (`''`/`todo`) rows.

The actual open question at this point is **direction, not a technical problem**: what body of
work to tackle next. Two candidates were proposed and are awaiting a decision (see §8 for
smaller, more immediately actionable alternatives):
1. A performance/algorithmic-complexity audit — genuinely never done in this project's history.
2. Verifying real integration with CNA (this library's actual consumer) — especially relevant
   now that `getCurrent()` was deliberately renamed, a confirmed breaking change for any
   downstream code still using the old name.

If you are resuming this session and find something is failing, that means the state has changed
since this document was written — trust the failing command's own output over this document, and
update this section (and the whole file) once you understand what changed.

---

## 5. Known bugs and limitations

**Confirmed, deliberately deferred (not bugs — documented scope decisions)**:
- `NumberStyles`-aware `Parse`/`TryParse` (all 8 integer types) supports `Integer`, `HexNumber`,
  and the `Allow*` whitespace/sign flags, but **not** `Currency`/`AllowThousands`/
  `AllowDecimalPoint` — deferred per ticket 1717's own acceptance criteria, documented in-code.
- `Span<char>`/UTF-8 `ReadOnlySpan<byte>`-based `Parse`/`TryParse` overloads for the same 8 types
  — not implemented at all, explicitly out of scope for the ticket that added the NumberStyles
  overloads.
- `ImmutableList<T>` is missing `Sort`/`Reverse`/`ForEach`/`CopyTo`/`GetRange`/`ConvertAll`/the
  `Find` family/`ToBuilder`/comparer overloads — cataloged in a class-level doc-comment, not
  implemented.
- `TaskCompletionSource<TResult>.Task` property is missing entirely — an architectural gap, since
  this port's `Task` always launches immediately on construction with no "pending" bridge mode to
  hang a `TaskCompletionSource` off of.
- `Task.WhenAny` is still missing — needs a race-free "first of N" mechanism deserving its own
  design pass (`Task.WhenAll` was implemented 2026-07-13, see §3).
- `UnboundedPrioritizedChannelOptions<T>` references a `Channel::CreateUnboundedPrioritized()`
  factory that does not exist anywhere in the codebase, making the options type currently unusable.
- `System::Xml::Linq::XText`'s `WriteTo` doesn't distinguish `WriteWhitespace` vs `WriteString`
  the way real .NET does when the parent is an `XDocument` — needs a larger `XmlWriter` change to
  close correctly (a `WriteWhitespace` primitive doesn't exist in this port's `XmlWriter` at all).

**Needs verification (unknown status)**:
- No Windows or Emscripten build has ever been compiled for this repository. Every platform
  `#ifdef` branch for those targets is unverified beyond code review.
- No performance characteristics (allocation counts, algorithmic complexity, hot-path cost) have
  ever been measured for any type in this codebase.

**Confirmed, permanent (by design, not something to "fix")**:
- Reflection (`System::Type`, `System::Activator`, `Enum.GetNames/GetValues`), GC internals, most
  delegate types' `DynamicInvoke`, serialization infrastructure, P/Invoke/interop, and
  symmetric/asymmetric cryptography + TLS + X.509 are all permanently out of scope. See
  `CLAUDE.md`.
- `System::Decimal`, `System::Int128`, `System::UInt128` require the GCC/Clang `__int128`
  extension and hard-`#error` on MSVC — permanent, accepted (2026-07-11 decision, not to be
  "fixed" with hand-rolled 128-bit arithmetic).
- `getCurrent()` was renamed to `getCurrentProperty()` this session with **no backward-compat
  alias** — any code (including CNA) still calling the old name will fail to compile until
  updated on the consumer's side. This was an explicit user decision, not an oversight.
- `TypedReference` (`include/System/TypedReference.hpp`) — RE-VERIFIED 2026-07-13: correctly
  classified `status='ignore'`/`outofscope=1` in the `task` table. An earlier audit pass flagged
  this as a possible misclassification, but on inspection its entire real functionality depends
  on C# compiler intrinsics (`__makeref`/`__reftype`/`__refvalue`) and CLR reflection
  (`FieldInfo`, `RuntimeTypeHandle`) — squarely inside the documented "reflection is permanently
  out of scope" deviation. The stub-with-`NotSupportedException` shape already matches CLAUDE.md's
  stated correct end-state for this category. No further action needed; this was a false alarm,
  not a bug.

---

## 6. Architecture notes

**Layout**:
- `include/System/...` — public headers, mirroring the .NET namespace hierarchy 1:1
  (`System::Collections::Generic::List<T>` lives at
  `include/System/Collections/Generic/List.hpp`, etc.).
- `src/System/...` — `.cpp` bodies for complex types (simple types are header-only).
- `tests/System/...` — GoogleTest suites, generally mirroring the `include/` structure.
- `vendor/` — third-party sources (GoogleTest, nlohmann/json, tinyxml2, miniz) — **exempt** from
  this project's SPDX-header, doc-comment, and naming-convention rules.
- `plan.sqlite3` (gitignored) — the two-table (`task`, `ticket`) work-tracking database that has
  driven this session's autonomous work. See `prompt.md` for the full workflow it implements.

**Build**: CMake with `GLOB_RECURSE` auto-discovering `src/*.cpp` — adding a new `.cpp` file
requires re-running `cmake .` in the build directory but no manual `CMakeLists.txt` edit.

**Important invariants** (must be preserved by any new code):
1. Every public API parameter/return value that mirrors a .NET `int`/`long`/`short`/`byte`/etc.
   must use the `SharpRuntime::intcs`/`longcs`/`shortcs`/`bytecs`/etc. alias, never the raw C++
   type. This is now enforced project-wide; do not regress it in new code.
2. Every property getter/setter is named `getXxxProperty()`/`setXxxProperty()`. The only
   documented exception is `getItem()`/`setItem()` for C# indexer equivalents. There are
   currently **zero** other exceptions anywhere in the codebase — a new one should not be
   introduced without updating `CLAUDE.md` to document it, matching the precedent this session
   set for `getItem()`/`setItem()`.
3. Any collection-like type with a mutable structure and an enumerator/iterator must track a
   `version_` counter (or equivalent), incremented on every structural mutation (Add/Remove/
   Clear/Insert/etc. — but NOT on value-only updates that don't change structure, e.g.
   overwriting an existing dictionary key's value), and its enumerator/iterator must check that
   counter on every step, throwing `System::InvalidOperationException("Collection was modified;
   enumeration operation may not execute.")` on mismatch. Reference implementation: any of
   `List<T>`, `Dictionary<K,V>`, or the legacy `ArrayList`/`Hashtable` (the original template this
   pattern was copied from). **Note**: this port has twice found it necessary to deviate from
   literal .NET `_version`-bump parity for C++ memory-safety reasons (`Dictionary`/`HashSet`'s
   `Remove()`/`Clear()` bump `version_` even though real .NET's don't, because
   `std::unordered_map`/`set` iterators are genuinely invalidated by erase/clear in a way .NET's
   array-backed entries aren't) — when implementing this pattern for a new type, verify against
   both the .NET reference AND actual C++ container iterator-invalidation rules, don't copy .NET
   blindly.
4. Any `operator[]` on a dictionary-like type must distinguish get-intent (throw
   `KeyNotFoundException` on a missing key) from set-intent (insert on a missing key) — a single
   C++ `operator[]` returning a plain reference can't express this by itself. The established
   pattern is a `ValueProxy` wrapper class; see `ConcurrentDictionary.hpp`, `Dictionary.hpp`,
   `SortedDictionary.hpp`, or `SortedList.hpp` for reference implementations.

**Boundaries that must not be broken**:
- POSIX-specific `#include`s (`<unistd.h>`, `<sys/socket.h>`, etc.) must never appear in a public
  `.hpp` header — only in `.cpp` files, behind `#ifdef _WIN32` / `#elif defined(__EMSCRIPTEN__)` /
  `#else` (POSIX) branching. On an unsupported platform, throw
  `System::PlatformNotSupportedException` with a clear message — never silently degrade.
- No LINQ-style chaining in code this project writes internally — use `std::ranges` instead
  (auditing/porting an existing `System::Linq` surface is a different matter and is fine).
- SPDX header (`// SPDX-License-Identifier: MIT` + copyright + .NET attribution) on every file
  under `include/`, `src/`, `tests/` (not `vendor/`).

**Compatibility/API rules**:
- Push only to `feature/work`. Never `develop`/`master`, never create tags, without explicit
  per-action user approval in that specific turn.
- `getCurrent()` → `getCurrentProperty()` is a completed, intentional breaking change — do not
  reintroduce the old name as an alias.
- No further broad, project-wide renames/refactors should be started without the same kind of
  explicit, per-action authorization that both the `intcs` rollout and the `getCurrentProperty()`
  rename required (see `CLAUDE.md` rule #10).

---

## 7. Useful commands

```bash
# Configure + build (from repo root)
cmake -S . -B build
cmake --build build --parallel 4      # use --parallel 2 if the machine is thermally constrained

# Run the full test suite
./build/SharpRuntimeTests

# Run one suite/type's tests only
./build/SharpRuntimeTests --gtest_filter="TypeName*"

# Repeat a suite to check for flakiness (useful after any concurrency-related change)
./build/SharpRuntimeTests --gtest_filter="*Concurrent*" --gtest_repeat=5

# CI-parity check (clean build + zero warnings + full suite, mirrors CLAUDE.md rules #1/#2)
./scripts/local_ci_check.sh

# Errors/warnings only, from a build log
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Reconfigure after adding a new .cpp file (GLOB_RECURSE needs a re-run to pick it up)
cd build && cmake . && cd ..

# Generate API docs (output already exists under docs/generated/ from a prior run)
doxygen Doxyfile
```

**Lint/format**: no `.clang-format` or other formatter/linter configuration exists in this repo.
There is no lint command to run.

**Reproduce the current bug**: not applicable — no known failing reproduction exists as of this
writing (see §4).

**Most important demo/sample**: none exists. `./build/SharpRuntimeTests` is the closest thing to
a "does this actually work" check available in this repo.

---

## 8. Next smallest tasks

Ordered by size/risk (smallest and safest first). None of these are currently blocking anything —
pick based on what's actually wanted next, or ask the user first if unsure which to prioritize.

~~1. Re-verify the build/test baseline.~~ **DONE 2026-07-13**: confirmed 12305/12305 passing,
   0 errors/0 warnings at HEAD `9be09bc`/`05c9f45`.

~~2. Resolve `TypedReference`'s `task`-table classification.~~ **DONE 2026-07-13**: re-verified
   correct as-is (`ignore`/`outofscope=1`) — its entire real functionality depends on reflection
   and compiler intrinsics, squarely inside the documented permanent-deviation scope. No change
   made; see §5's "Confirmed, permanent" list for the full reasoning. This was a false alarm from
   an earlier audit pass, not a genuine misclassification.

~~3. Implement `Task::WhenAll`.~~ **DONE 2026-07-13** (`9d4e77e`): static
   `Task::WhenAll(std::vector<Task>)`, 5 new regression tests. See §3 for the exact semantics
   implemented (no short-circuit, direct-rethrow-first-fault, empty-input fast path).

1. **Add `NumberStyles.Currency`/`AllowThousands` support to the 8 integer `Parse`/`TryParse`
   overloads.**
   Goal: extend the existing `NumberStyles`-aware parser (added this session, currently supports
   `Integer`/`HexNumber`/`Allow*` whitespace-and-sign flags only) to also handle thousands
   separators and currency symbols, matching real .NET's grammar.
   Files: `include/System/{Int16,Int32,Int64,UInt16,UInt32,UInt64,SByte,Byte}.hpp` (whichever
   shared parsing helper they route through — check for one before touching all 8 independently).
   Verify: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="*Int32*Parse*"`.

2. **Implement `Channel::CreateUnboundedPrioritized` (or correct the dangling reference to it).**
   Goal: either implement a real priority-queue-backed channel variant so
   `UnboundedPrioritizedChannelOptions<T>` becomes usable, or — if that's too large for one
   session — remove/correct the dangling doc-comment reference and file a proper follow-up ticket
   for the real implementation.
   Files: `include/System/Threading/Channels/Channel.hpp`, `ChannelOptions.hpp`.
   Verify: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="*Channel*"`.

3. **Scope a performance-audit pilot on one hot-path type.**
   Goal: rather than a full performance audit (a much larger undertaking), pick ONE
   heavily-used, allocation-sensitive type (e.g. `String`, `List<T>`, or `StringBuilder`) and do
   a focused pass: look for unnecessary copies, repeated reallocation, O(n²) patterns where O(n)
   is achievable. Treat this as a pilot to gauge whether a full performance audit is worthwhile
   before committing to one.
   Files: TBD based on which type is picked.
   Verify: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests` (no regressions) — no
   benchmark harness currently exists, so this task should also produce a minimal one if it finds
   something worth measuring.

---

## 9. Do not do yet

- **No restarting the `intcs`/`getXxxProperty()` naming rollouts** — both are complete,
  project-wide, as of this session. Don't re-scan for "one more" instance without a specific,
  concrete finding first.
- **No backward-compatibility shim for the `getCurrent()` → `getCurrentProperty()` rename** — this
  was an explicit, deliberate decision. Downstream consumers fix their own call sites.
  - **No Windows/Emscripten CI setup** — still explicitly out of scope per `CLAUDE.md`.
- **No TLS/`SslStream`, asymmetric cryptography, or X.509 implementation** — permanent,
  documented deviation.
- **No reflection, GC-internals, serialization-infrastructure, or P/Invoke implementation
  attempts** — permanent, documented deviations.
- **No speculative performance optimization without measurement first** — if a performance pass
  happens, it should be driven by an actual benchmark/profile, not guesswork (see task 6 in §8,
  which is scoped as a measurement-first pilot for exactly this reason).
- **No further broad, many-file refactors or renames** without the same explicit, per-action user
  authorization the `intcs` and `getCurrentProperty()` changes required.
- **No mass rewrite or reformatting in a single commit** — keep changes small, reviewable, and
  scoped to one ticket/task per commit, matching this project's established pattern.
- **No merge to `master`/`develop`, and no tags**, without explicit per-action user approval.
- **No touching `Decimal`/`Int128`/`UInt128`'s `__int128`-based MSVC limitation** — permanent,
  accepted (2026-07-11 decision); do not attempt a hand-rolled 128-bit arithmetic workaround.

---

## 10. Resume prompt

```
Read NEXT.md first. It reflects the repository state as of HEAD 9d4e77e (12310/12310 tests
passing, 0 errors/0 warnings, all verified at that commit) — re-verify first anyway:
cmake --build build --parallel 4 && ./build/SharpRuntimeTests.

Do not assume anything beyond what NEXT.md documents. There is no known active blocker — the
open question is which of §8's candidate next tasks (or something else entirely) to work on.

Pick ONE task — from NEXT.md §8 if nothing else has been specified — and inspect only the
files needed for that task. Do not refactor unrelated code, do not touch files outside that
task's stated scope, and do not restart the completed intcs/getXxxProperty() rollouts.

Make one small, verified improvement: implement the task, run its stated verification command
(build clean, zero warnings; full test suite passes; new/changed behavior has a regression
test), and only then consider it done.

Update NEXT.md after finishing: move the completed task out of §8, note what changed in §3,
and update the "Last updated" line and test count at the top of the file.
```
