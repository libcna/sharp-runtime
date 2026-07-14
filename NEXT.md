# NEXT.md

*Last updated: 2026-07-14. Branch: `feature/work`. HEAD: `1cdc80a`.*

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

**Build status**: last verified clean — 0 errors, 0 warnings, full clean rebuild — at HEAD
(`1cdc80a`), via `cmake --build build --parallel 4`.

**Test status**: last verified **12378/12378 passing**, 1224 test suites, via
`./build/SharpRuntimeTests`, at the same HEAD. Additionally verified under **the full
sanitizer trio** (all three firsts in this project's history — see §3): ThreadSanitizer (4 full
runs, 0 warnings), AddressSanitizer (3 full runs, 0 errors/leaks), UndefinedBehaviorSanitizer (3
full runs, 0 diagnostics) — 12378/12378 passing every single run, each completing in a few
seconds. Also verified flake-free across 3 additional normal-build full runs plus 20 repeats of
the `Task::WhenAny` suite specifically. Zero known failing tests, zero known races, zero known
memory-safety or undefined-behavior issues.

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
- Performance work is now started but far from comprehensive: only `System::String`'s
  `Split(char)`/`Concat`/`Join` have been measured and optimized (see §3), via a minimal
  dependency-free `bench/StringBenchmark.cpp` harness (gated behind
  `SHARP_RUNTIME_BUILD_BENCHMARKS`, default OFF). No other hot-path type (`List<T>`,
  `StringBuilder`, `Dictionary`, etc.) has been profiled.
- Remaining documented API gaps: `ImmutableList`'s missing LINQ-family methods — full list in §5
  (`Task.WhenAny` was implemented 2026-07-13, see §3 — the last remaining `Task` gap is closed).

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
- Extended `include/System/detail/IntegerNumberStylesParser.hpp` (`7d3deca`) — the shared
  NumberStyles-aware parsing core behind all 8 integer types' `Parse`/`TryParse(string,
  NumberStyles, IFormatProvider*)` overloads — to support `AllowThousands`, `AllowDecimalPoint`,
  `AllowCurrencySymbol`, `AllowParentheses`, and `AllowTrailingSign`, so `NumberStyles.Number` and
  `NumberStyles.Currency` are now fully supported (previously only `.Integer`/`.HexNumber`).
  Verified against real .NET's `TryParseNumber`/`TryNumberBufferToBinaryInteger`
  (`Number.Parsing.Common.cs`/`Number.Parsing.cs`), including its quirk that a nonzero fractional
  digit (e.g. `"123.5"`) throws `OverflowException`, not `FormatException`. Separators/currency
  symbol use `NumberFormatInfo.InvariantInfo`'s fixed defaults (`.`/`,`/U+00A4 "¤"), since this
  port ignores `IFormatProvider`. Unsigned types deliberately reject any negative-indicating token
  (`-` or closed parens) as a format failure rather than importing .NET's
  negative-unsigned-throws-`OverflowException` quirk, keeping unsigned parsing uniform across all
  styles — a documented deviation, not an oversight. The refactor is verified
  behavior-preserving for the pre-existing `.Integer`/`.HexNumber` scope (all 12310 prior tests
  passed unchanged before the 26 new ones were added). No changes were needed in the 8 type
  headers themselves beyond doc-comments — they all route through this one shared parser. Test
  count grew from 12310 to 12336.
- Implemented `Channel<T>::CreateUnboundedPrioritized` (`3a8adfa`) — resolves the previously
  dangling doc-comment reference in `UnboundedPrioritizedChannelOptions<T>`. Backed by
  `std::multiset<T, Comparer>` (ascending order, smallest-first dequeue, defaulting to
  `operator<` when no `Comparer` is supplied), kept as an entirely separate
  `detail::PrioritizedChannelState/Reader/WriterImpl` trio rather than generalizing the existing
  FIFO `ChannelState<T>`/`std::deque` machinery — the FIFO type's bounded-specific fields
  (capacity, drop policies) don't apply to an unbounded priority queue, and this keeps the
  already-tested FIFO code path completely untouched (zero risk of regression there). Verified
  against real .NET's `UnboundedPrioritizedChannel<T>` (backed by `PriorityQueue<bool, T>`):
  supports `TryPeek`/`Count` (the plain FIFO channel doesn't), and writes never block since the
  channel is unbounded (matches `CreateUnbounded()`'s own contract). 14 new regression tests,
  including a concurrent multi-writer/multi-reader stress test (flake-checked across 10 repeats).
  Test count grew from 12336 to 12350.
- Performance-audit pilot on `System::String` (`890b569`) — per user decision, measured with a
  standalone `std::chrono` timing script before touching anything (no speculative optimization),
  then applied two measurement-justified fixes: (1) `Split(value, char)` rewritten from
  `std::stringstream`+`getline` to a direct `find`/`substr` scan — measured ~2.6x faster (302ns →
  116ns/call for a typical 10-field line), and matches the approach the string-delimiter overload
  already used. As a verified-against-.NET side effect, this also fixed a pre-existing correctness
  bug: `Split("", ',')` previously returned an empty vector instead of real .NET's single-element
  `{""}` result (`String.Manipulation.cs`'s `CreateSplitArrayOfThisAsSoleValue`). (2)
  `Concat(vector<string>)`/`Join(separator, vector<string>)` now `reserve()` the exact total size
  upfront instead of relying on `std::string`'s amortized-growth `+=` — measured ~1.4x faster for
  a realistic 50-item join. Added `bench/StringBenchmark.cpp`, a minimal dependency-free
  `std::chrono` timing harness (per explicit user decision: no vendored benchmarking library),
  gated behind a new `SHARP_RUNTIME_BUILD_BENCHMARKS` CMake option (default OFF, doesn't affect
  the default build/test loop) — confirmed the Split fix's real-world speedup (127.6 ns/call)
  matches the standalone measurement when built with `-DCMAKE_BUILD_TYPE=Release`. 6 new
  regression tests. Test count grew from 12350 to 12356.

**Post-pilot audit round** (once §8's original 4 tasks were done, per the user's standing
autonomous-session authorization to keep finding high-value work): 4 parallel find-only agents
covering categories the original `POST_STABILIZATION_AUDIT.md` pass didn't specifically target —
TODO/FIXME/stub markers, weak/happy-path-only test coverage, resource-management/RAII issues, and
`plan.sqlite3`-vs-source drift. Markers and drift audits came back clean (genuinely nothing new —
see the audits' own reports for methodology). Resource-management and weak-tests audits found real
issues, all now fixed and pushed:
- **`fix: resource-management audit findings`** (`0c6427f`) — 4 verified fd-leak/double-close
  bugs: `Process::Start` leaked the stdout pipe if stderr pipe creation failed after; `File-
  SystemWatcher::startWatchingIfPossible` orphaned 3 fds permanently if `std::thread`'s
  constructor threw; `DeflateStream`/`GZipStream`/`ZLibStream::Close()` leaked zlib state (and
  risked `std::terminate`) if the inner stream's `Write()` threw mid-flush — fixed by mirroring
  `BufferedStream::Close()`'s already-correct pattern; `TcpClient`/`TcpListener`/`UdpClient`/
  `NetworkStream` were missing `= delete` on copy construction/assignment despite owning a raw fd
  closed by the destructor (their sibling `Socket` already had this) — a double-close bug if ever
  copied. 6 new regression tests + 8 `static_assert(!is_copy_constructible)` compile-time checks.
- **`fix: String::Format` validation** (`1ed11cd`) — `String::Format` never validated its format
  string at all: an out-of-range argument index (`"{5}"` with only `arg0`) was silently left
  unreplaced instead of throwing `FormatException`, unlike real .NET (verified against
  `String.Manipulation.cs`'s `FormatHelper`). Fixed via a new `FinalizeFormat()` helper wrapping
  all 18 leaf `Format()` overloads. While implementing this, also found and fixed a second bug in
  the shared `replaceArg()` helper: naive substring `find()` treated `"{1"` as a prefix match
  inside `"{10}"`, silently corrupting output instead of leaving it for the new validation to
  reject. 6 new regression tests.
- **`fix: Task::Delay` + `Stream::Seek`** (`97a639a`) — two more bugs surfaced while verifying
  "weak test coverage" audit findings: `Task::Delay` had zero validation of negative input (real
  .NET throws `ArgumentOutOfRangeException` for `< -1`); `Stream::Seek` routed entirely through
  `setPositionProperty()` for validation, so a resulting negative position threw
  `ArgumentOutOfRangeException` instead of real .NET's `IOException` (verified against
  `MemoryStream.cs`'s `SeekCore` — a genuinely different validation rule from the Position
  setter's own, still-correct, `ArgumentOutOfRangeException`). Fixed in the shared base
  `Stream::Seek()`, used by every Stream subtype in this port (none override it). 5 new tests.
- **`test:` coverage additions** (`0713156`) — of the weak-test audit's 11 findings, 9 turned out
  to be **false positives** on verification: the audit agent only checked one test file per type
  and missed that this codebase spreads a type's tests across multiple files (e.g. `List<T>`
  tests exist in `ListTests.cpp`, `CollectionsTests.cpp`, AND `Ticket1717And1718Tests.cpp` —
  `CopyTo`/`AsReadOnly`/`FindLastIndex`/`FindLast`/`LastIndexOf`/`IndexOf(startIndex)`/
  `InsertRange`'s success path, and `Dictionary::Remove` on a missing key, were all already
  covered). The 2 genuine gaps got regression tests: `Int32::Abs(Int32::MinValue)` throwing
  `OverflowException` (sibling types already had this test; `Int32` didn't), and
  `NumberFormatInfo`'s `CheckRange`-before-`VerifyWritable` check ordering on a read-only
  instance with an out-of-range value (verified this port's ordering already matches real .NET's
  `NumberFormatInfo.cs` exactly — a pure coverage gap, not a bug).

Test count grew from 12356 to 12371 across this whole audit round. **Lesson for future audit
rounds in this codebase**: always grep for `TEST(<TypeName>Tests,` across the WHOLE `tests/`
tree, not just the one file whose name matches the type — this codebase's test suite grew
incrementally across many `BatchNN`/`TicketNNNN`/feature-named files, not one file per type.

**`Task::WhenAny`** (`2be9c3c`) — the last remaining documented `Task` gap. Returns
`TaskT<Task>`, matching real .NET's contract exactly: the wrapper always completes successfully
with its Result set to the first-completed input task, even if that task faulted or was
canceled — callers inspect the winning `Task`'s own state separately. Spawns one watcher thread
per input task (no native `std::future` "first of N" combinator exists); the first to observe
its task completing wins via an atomic compare-exchange and notifies a shared
`condition_variable`. **Caught one real bug via testing before this landed**: an early version
`join()`'d every watcher thread, including losing ones still blocked on a slower task — making
`WhenAny` consistently take as long as the SLOWEST input task, exactly backwards from its
contract. A timing regression test (fast task racing a 500ms slow task, asserting elapsed time
stays low) caught this deterministically — the first sign was the test failing at exactly the
slow task's sleep duration on every run, not an intermittent flake, which is what pointed to a
logic bug rather than scheduling jitter. Fixed by `detach()`-ing watchers instead of `join()`ing
them: each one captures its own `Task` copy by value (not a reference into the enclosing stack
frame) plus `shared_ptr` copies of the synchronization primitives, so it's self-contained and
safe to keep running in the background after `WhenAny` returns — matching real .NET's own
behavior of not canceling non-winning tasks. 7 new regression tests; verified flake-free across
20 repeats of the suite plus 3 full-test-suite runs. Test count grew from 12371 to 12378.

**First-ever ThreadSanitizer run in this project's history** (`57bf75f`, `8e490a8`) — set up a
separate TSan-instrumented build (`-fsanitize=thread -g -O1`, plus `-Wno-array-bounds
-Wno-stringop-overread -Wno-stringop-overflow -Wno-error=tsan` to work around GCC `-O1`-specific
false-positive warnings and one genuine `atomic_thread_fence`-under-TSan incompatibility
unrelated to this codebase). **Process note, important for next time**: the first attempt hung
for real (not sanitizer overhead — once fixed, the full 12k+ test suite runs under TSan in ~4
seconds) because of a genuine deadlock in the code below, and it was launched without a bounded
`timeout` and without active monitoring, so it sat unnoticed for hours before the user had to
flag it directly. Every TSan invocation after that used `timeout <N>`; see the
`feedback_bounded_timeouts_for_risky_commands` memory for the full incident writeup — the
practical rule now: any new/unverified long-running command gets a hard timeout from the start,
not just after something has already gone wrong once.

Found and fixed 3 real, verified issues, none cosmetic:
- **`Channel<T>::CreateUnboundedPrioritized`'s `TryRead()` was missing a `notEmpty.notify_all()`
  call** its sibling FIFO `ChannelReaderImpl::TryRead()` already has — a genuine lost-wakeup bug.
  Draining the last item while the channel is already closed is exactly the transition
  `getCompletionProperty()`'s waiter blocks on; without the notify, that wait can hang forever.
  This is what caused the real (non-sanitizer-overhead) hang described above, reproduced
  deterministically as a deadlock in `PrioritizedChannelTests.Completion_CompletesOnceClosed-
  AndDrained` under TSan's instrumented scheduling — it happened to pass under normal execution
  purely because of a timing coincidence (the Completion task's background thread usually didn't
  check its predicate until after `TryRead` had already run), not a correctness guarantee. Fixed
  by adding the identical `notify_all()` call the FIFO implementation already has.
- **Two test-only races from `sleep_for`-as-synchronization** (`AsyncLocalTests`/`ThreadLocalTests`
  `InstanceDestroyedOnDifferentThread...` tests): a worker thread constructed an object and a
  main thread destroyed/reconstructed it at the same stack address after a fixed
  `sleep_for(20ms)` "should be enough" delay — no actual happens-before relationship. Fixed with
  a second `promise`/`future` pair so the worker explicitly signals construction-complete.
- **A subtler synchronization-scope bug** in `PosixSignalTests.MultipleHandlers_FireInReverse-
  RegistrationOrder`: each handler's `fired++` (the atomic the main thread polled) was NOT
  actually the last action on the watcher thread — the enclosing `std::lock_guard`'s destructor
  (unlocking a stack-local mutex) ran afterward, since it was declared before (and so destructs
  after) the increment. Fixed by scoping the lock so it unlocks strictly before `fired++`.
- **A plain (non-atomic) pointer race** in `TimerTests.Change_DuringCallback_NotClobberedByPeriod`
  — a background timer thread could read a test-local `Timer*` before the main thread's write to
  it became visible. Fixed with `std::atomic<Timer*>`.

Re-verified clean (0 warnings) across 4 full-suite TSan runs after all four fixes landed. Normal
build re-verified: 0 errors/0 warnings, 12378/12378 passing.

**First-ever AddressSanitizer run** (`1f2e4b5`, `35817d9`) — same diagnostic-build pattern as
TSan (separate build dir, `-fsanitize=address -g -O1`, same GCC `-O1` false-positive warning
suppressions). Found and fixed two categories of real issues:
- **A confirmed heap-buffer-overflow** in `NativeMemory::AlignedRealloc`'s POSIX path: it
  unconditionally `memcpy`'d the caller's NEW size from the OLD (smaller) allocation when
  growing a block — ASan caught it reading 128 bytes from a 32-byte source. This is the SAME bug
  GCC's `-Warray-bounds`/`-Wstringop-overread` had already flagged during the TSan build setup
  (see §3's TSan entry) — at the time those warnings were dismissed as `-O1` false positives
  without further investigation, which was a mistake; they were GCC correctly seeing this exact
  bug. Root cause: POSIX has no portable aligned-realloc, so this port had no way to know the
  old allocation's actual size. Fixed with a standard hand-rolled aligned-allocator technique —
  `AlignedAlloc` now over-allocates and manually aligns the returned pointer, storing the raw
  `malloc`'d pointer and requested size in a header immediately before the address handed to the
  caller, so `AlignedRealloc` can correctly bound its copy to `min(newSize, oldSize)`. Windows'
  `_aligned_malloc`/`_aligned_realloc` (which already track sizes internally) are untouched.
- **576 bytes leaked across 9 allocations in the XML subsystem**, from 5 sources, triaged into
  two categories: (1) a genuine production bug — `XmlDocument::CreateAttribute`/
  `CreateEntityReference` returned a bare `new`'d object with no owner, the only two `Create*`
  factory methods that didn't participate in the document's existing `nodeCache_` RAII pool
  (they have no native tinyxml2 node to key that cache by) — fixed with a parallel
  `unattachedNodes_` ownership pool plus a `ReleaseUnattachedNode()` transfer point
  `XmlElement::SetAttributeNode` now calls once an attribute is actually attached; (2)
  caller-owns-the-return-value contracts matching real .NET's own semantics for `XPathNavigator.
  Clone()`/`SelectSingleNode()`, `XmlNode.SelectNodes()`, `XmlDocument.GetElementsByTagName()`
  (the latter two already had an explicit, previously-reviewed "caller's responsibility" comment
  at their call site) — these were test-only cleanup gaps, fixed in the 4 affected tests rather
  than changing any production API.

Re-verified clean (0 errors/leaks) across 3 full-suite ASan runs after both fixes landed. Normal
build re-verified: 0 errors/0 warnings, 12378/12378 passing.

**First-ever UndefinedBehaviorSanitizer run** (`1cdc80a`) — completes the sanitizer trio for this
session (same diagnostic-build pattern: separate build dir, `-fsanitize=undefined -g -O1`, same
GCC `-O1` false-positive warning suppressions). Found exactly **one** diagnostic across the
entire 12378-test suite: writing a zero-length zip entry passed an empty vector's `.data()`
(`nullptr`, per libstdc++) all the way down through `mz_zip_writer_add_mem` into vendored
miniz's own `mz_zip_file_write_func`, which calls `fwrite(pBuf, 1, 0, file)` — `fwrite`'s first
parameter is declared `nonnull`, so this is UB by the letter of the standard even though 0 bytes
are never actually read through the null pointer (universally harmless in every real libc).
`vendor/miniz` is third-party, unmodified-from-upstream source (CLAUDE.md) — not the place to
fix this. Added a small `safeDataPtr()` helper in `ZipArchive.cpp` that substitutes a non-null,
never-dereferenced dummy pointer for empty entries, applied at all 4 `mz_zip_writer_add_mem`
call sites (file-based/memory-based writers × existing/pending-entry loops), avoiding the UB at
this project's own call site rather than touching vendored code.

Re-verified clean (0 diagnostics) across 3 full-suite UBSan runs after the fix landed. Normal
build re-verified: 0 errors/0 warnings, 12378/12378 passing.

**Sanitizer-trio summary for this session**: TSan found 1 real production deadlock + 3 real
test-only races (all fixed); ASan found 1 real heap-buffer-overflow + 5 real memory leaks across
2 categories (all fixed); UBSan found 1 real (if practically harmless) UB call (fixed). All
three now run clean project-wide — a first in this project's history for all three.

**`-Wshadow` audit** — added to the existing `-Wall -Wextra -Werror` flag set as a diagnostic-only
build (same pattern as the sanitizer investigations). Came back essentially clean: **zero**
shadow warnings anywhere in `include/`/`src/` (the actual library), and only 3 in `tests/`
(~700 files) — all harmless, idiomatic naming coincidences (a constructor parameter matching a
member name; a lambda parameter matching its enclosing local variable's name), not real bugs. No
code changes made — a genuinely clean result, not something that needed fixing. `-Wconversion`/
`-Wsign-conversion` were considered but skipped: this codebase's own `SharpRuntime::intcs`
convention means there are thousands of deliberate, already-safe `size_t`↔`intcs` narrowing
conversions throughout, which would make those flags overwhelmingly noisy without a good way to
distinguish real bugs from expected, intentional casts.

**Performance-audit pass extended to `List<T>`/`StringBuilder`** — per the "measure first" rule,
read through both for the same class of anti-pattern the `String` pilot found (`stringstream`,
missing `reserve()`, O(n²) patterns), then measured the one real candidate found
(`StringBuilder::Append(intcs)`'s `std::to_string`-based implementation vs. writing digits
directly into the buffer with `std::to_chars`, avoiding the intermediate string `Append(double)`/
`Append(float)` already avoid in the same file). **Honest result: no significant win** —
directly-into-buffer measured only a ~9% speedup, within normal microbenchmark noise, and would
trade a clean one-line implementation for a more error-prone manual resize/to_chars/resize
sequence. Per CLAUDE.md's "no speculative optimization" and "don't add complexity for marginal
gains," **no code changed**. `List<T>` itself is already a thin, direct `std::vector<T>` wrapper
with no unnecessary copies or missing `reserve()` calls found; its `Contains`/`IndexOf`/`Remove`
being O(n) matches real .NET's own `List<T>` complexity exactly, not a port-specific inefficiency.
This is a legitimate audit outcome, not a gap — not every investigation has to find something to
fix, and manufacturing a marginal "optimization" just to show activity would be the wrong call
here.

---

## 4. Current blocker / main problem

**There is no active build/test blocker right now.** Build was clean and all 12378 tests passed
at the last verification (HEAD `1cdc80a`), including clean full-suite runs under all three
sanitizers (TSan/ASan/UBSan). `plan.sqlite3`'s `ticket` table has zero `blocked`, `todo`, or
`doing` rows; the `task` table has zero unclassified (`''`/`todo`) rows.

This session is running autonomously (per explicit user authorization). All four of NEXT.md's
original §8 tasks are done, plus a full post-pilot audit round (4 parallel find-only agents → 4
fix commits) and a full sanitizer-trio investigation (TSan/ASan/UBSan, 6+ real bugs found and
fixed — see §3) — §8 currently holds only natural follow-ons that haven't been started yet
(extending the performance pass to other hot-path types; a possible further audit round). Two
pre-session decisions from the user remain in effect: (1) no new benchmarking dependency —
`std::chrono`-based timing only, per `bench/StringBenchmark.cpp`; (2) push after each verified
task, same cadence as before.

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
- `NumberStyles`-aware `Parse`/`TryParse` (all 8 integer types) now supports `Integer`,
  `HexNumber`, `Number`, and `Currency` in full (`AllowThousands`/`AllowDecimalPoint`/
  `AllowCurrencySymbol`/`AllowParentheses`/`AllowTrailingSign` were added 2026-07-13, see §3) —
  the one remaining gap is `AllowExponent`, which real .NET's `Number`/`Currency` styles don't
  include either (only `Float`/`HexFloat` do, and those don't apply to integer types), so this is
  a non-gap, not a scope reduction.
- `Span<char>`/UTF-8 `ReadOnlySpan<byte>`-based `Parse`/`TryParse` overloads for the same 8 types
  — not implemented at all, explicitly out of scope for the ticket that added the NumberStyles
  overloads.
- `ImmutableList<T>` is missing `Sort`/`Reverse`/`ForEach`/`CopyTo`/`GetRange`/`ConvertAll`/the
  `Find` family/`ToBuilder`/comparer overloads — cataloged in a class-level doc-comment, not
  implemented.
- `TaskCompletionSource<TResult>.Task` property is missing entirely — an architectural gap, since
  this port's `Task` always launches immediately on construction with no "pending" bridge mode to
  hang a `TaskCompletionSource` off of.
- `System::Xml::Linq::XText`'s `WriteTo` doesn't distinguish `WriteWhitespace` vs `WriteString`
  the way real .NET does when the parent is an `XDocument` — needs a larger `XmlWriter` change to
  close correctly (a `WriteWhitespace` primitive doesn't exist in this port's `XmlWriter` at all).

**Needs verification (unknown status)**:
- No Windows or Emscripten build has ever been compiled for this repository. Every platform
  `#ifdef` branch for those targets is unverified beyond code review.
- Performance characteristics (allocation counts, algorithmic complexity, hot-path cost) have
  only been measured for `System::String`'s `Split(char)`/`Concat`/`Join` so far (2026-07-13, see
  §3). Every other type in this codebase — `List<T>`, `StringBuilder`, `Dictionary`, etc. — is
  still unmeasured.
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

**Completed this session** (2026-07-13/14, full detail in §3 / git history — compacted here to
keep this section actionable): build/test baseline re-verify; `TypedReference` classification
(false-alarm, no change needed); `Task::WhenAll`; `NumberStyles.Currency`/`AllowThousands` for
all 8 integer types; `Channel::CreateUnboundedPrioritized`; a `String` performance-audit pilot;
a full post-pilot audit round (resource-management fixes, `String::Format` validation,
`Task::Delay`/`Stream::Seek` fixes, 2 verified test-coverage additions); `Task::WhenAny` (the
last remaining documented `Task` gap); the project's first-ever full-suite sanitizer trio —
ThreadSanitizer (1 real production deadlock + 3 real test-only races), AddressSanitizer (1 real
heap-buffer-overflow + 5 real memory leaks), UndefinedBehaviorSanitizer (1 real, if practically
harmless, UB call) — all fixed, all three now run clean project-wide; a `-Wshadow` compiler-
warning audit (clean — see §3); a performance-audit extension to `List<T>`/`StringBuilder`
(honest no-significant-finding result — see §3, no code changed).

None of the tasks below are currently blocking anything — pick based on what's actually wanted
next, or ask the user first if unsure which to prioritize.

1. **`Dictionary<K,V>` performance pass**, if the performance-audit direction continues — the
   one hot-path collection type from the original `String`/`List<T>`/`StringBuilder` shortlist
   (§3) not yet looked at. Same "measure first" discipline: read for `stringstream`/missing-
   `reserve()`/O(n²) patterns, measure any real candidate found, only change with a genuine
   measured delta. Given `List<T>`/`StringBuilder` both came back clean, treat this as a
   lower-confidence lead, not an assumed win.

2. **Another audit round, different categories.** This session's post-pilot round covered
   TODO/FIXME markers (clean), weak tests (mostly false positives — see §3's lesson-learned
   note), resource-management/RAII (4 real bugs, fixed), `plan.sqlite3` drift (clean), memory
   safety / undefined behavior (the full TSan/ASan/UBSan trio — 8 real bugs total, fixed), and
   variable shadowing (`-Wshadow` — clean). Categories NOT yet covered: `-Wconversion`/
   `-Wsign-conversion` (skipped so far as likely too noisy given this codebase's pervasive
   intentional `intcs`/`size_t` conversions — would need a smarter triage approach, not a blind
   full-codebase run), duplicated-implementation search, or missing edge-case handling in
   recently-added code (the `NumberStyles`/`Channel::CreateUnboundedPrioritized`/`Task::WhenAny`
   work from this session is itself a good target for a fresh pair of eyes).

3. **`TaskCompletionSource<TResult>.Task` property.** Still missing (§5) — an architectural gap
   since this port's `Task` always launches immediately on construction with no "pending" bridge
   mode. Would need design thought about how to represent a Task that doesn't yet have a
   `std::async` backing it.

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
- **No speculative performance optimization without measurement first** — the `String` pilot
  (§3/§8) measured with a standalone script before changing anything; any further performance
  work should follow the same discipline, using `bench/StringBenchmark.cpp` as the template.
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
Read NEXT.md first. It reflects the repository state as of HEAD 1cdc80a (12378/12378 tests
passing, 0 errors/0 warnings, also clean under the full ThreadSanitizer/AddressSanitizer/
UndefinedBehaviorSanitizer trio, all verified at that commit) — re-verify first anyway:
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
