<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Diagnostics` namespace review and remediation plan

Ticket **#2023**, written 2026-08-03 on branch
`feature/remediation-batch-text-approvals-next-review`.

This is the sixth namespace review in the post-audit remediation programme,
after `System::Threading` (#1950), `System::Threading::Tasks`/`Channels`
(#1964), `System::Runtime` (#1972), `System::Uri` (#1987) and `System::Text`
(#2006). It follows the same contract: **every confirmed finding in the
namespace gets exactly one disposition, no finding disappears between the audit
index and this plan, and every premise is re-measured against the shipped
library before it is relied upon.**

**Nothing in §§1–17 is implemented by writing them.** The measured
before-matrix is `build-probe/2023_probe1_before.log`, reproducible with
`build-probe/2023_probe1_diagnostics_before.cpp`. **All eight premises
reproduced**; §4 records the three the audit understates.

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays
frozen at **364**.

---

## 1. Why `System::Diagnostics` is next

Selected by measurement over `audit/AUDIT_FINDINGS_INDEX.md`, not
alphabetically, not by raw count, and **not** by inheriting the previous
handoff's recommendation — which named `modules/collections`,
`modules/buffers` and `modules/io` and explicitly told a fresh context to
re-derive the queue rather than trust that sentence. Re-derived:

| Module / namespace | Open | high | med | low | Existing plan? | Reviewed? | Cohesion |
|---|---|---|---|---|---|---|---|
| `modules/core` | 72 | 9 | 59 | 4 | many family plans (CCF-004/005/007, comparison contract, text subset) | partly, by family | **poor** — `System::` spans String, Convert, DateTime, Decimal, Math, Guid, Span, Exception |
| `modules/threading` | 17 | 6 | 11 | 0 | **yes** — `docs/ThreadingNamespaceReviewPlan.md` | **yes** (#1950) | good |
| `modules/buffers` | 11 | 3 | 8 | 0 | partial — `docs/Base64FamilyPlan.md` | no | good |
| `modules/io` | 11 | **0** | 11 | 0 | partial — `StreamCapabilityContractDesign.md`, `TextWrapperInputContractPlan.md` | no | medium |
| `modules/net` (+7 sibling Net modules) | 10 (39 across the family) | 3 | 7 | 0 | no | no | **poor** — 8 modules; #1962 blocked |
| **`modules/diagnostics`** | **8** | **5** | **3** | 0 | **no** | **no** | **excellent** — 1 module, 1 namespace |
| `modules/xml` | 8 | 2 | 6 | 0 | no | no | medium (+3 in `xml-linq`) |
| `modules/globalization` | 7 | 1 | 6 | 0 | no | no | good, but see below |
| `modules/time-zone` | 7 | 0 | 7 | 0 | no | no | good |

`System::Diagnostics` wins on every criterion the selection rule names:

- **Meaningful severity.** **Five of its eight open findings are `high`** —
  a 62.5 % high ratio, the highest of any un-reviewed namespace in the
  repository. `modules/io`, the nearest competitor by count, has **zero**.
- **No existing plan.** `docs/` contains no `Diagnostics` design or review
  document, and no open ticket referenced any of SR-AUD-268 … SR-AUD-275
  before this review.
- **Limited dependencies.** `modules/diagnostics` declares
  `PUBLIC_DEPENDENCIES Core.Base` and nothing else — the narrowest dependency
  set of any candidate. Four first-party files include it (§2.4).
- **Module cohesion.** One module, one namespace, and **seven of the eight
  findings are the same class** (`Process`), so the review has one subject
  rather than eight.
- **A useful compatible queue.** Measured, **five** of the eight repairs need
  no approval at all (§7.1) — including the two that today **abort the process**
  and **deadlock a forked child**.

Not chosen, with reasons recorded so they are not re-litigated: `modules/core`
is not a namespace (and is already being drained by cross-cutting families);
`modules/threading` already has a complete review and its remainder is the
blocked #1956–#1959; `modules/globalization`'s seven findings almost all need
culture/ICU data that does not exist in this container, so its compatible queue
would be nearly empty; the `Net` family spans eight modules and carries the
blocked #1962.

---

## 2. Scope and file inventory

Measured from the repository. `modules/diagnostics` is a `STATIC` target with
`PUBLIC_DEPENDENCIES Core.Base` and no private or test dependency.

### 2.1 Public headers (24 files, 1,536 lines)

| File | Lines | Public surface | Findings |
|---|---|---|---|
| `System/Diagnostics/Process.hpp` | 117 | pimpl handle; move-only; `getStartInfoProperty`/`set`, `Start()`, 3 static `Start`, `getIdProperty`, `getHasExitedProperty`, `getExitCodeProperty`, `getStandardOutputTextProperty`, `getStandardErrorTextProperty`, 2 × `Kill`, 2 × `WaitForExit`, `GetCurrentProcess` | 268, 269, 270, 271, 272, 273, 274 |
| `System/Diagnostics/ProcessStartInfo.hpp` | 95 | 3 ctors, `FileName`, `Arguments`, `ArgumentList` (mutable ref), `EnvironmentVariables` (mutable ref), `WorkingDirectory`, `RedirectStandardOutput`, `RedirectStandardError` | 274 |
| `System/Diagnostics/Debug.hpp` | 203 | all-static: `GetProvider`/`SetProvider`, `IndentSize`, `IndentLevel`, `Indent`/`Unindent`, `Write*`, `Assert`, `Fail` | **275** |
| `System/Diagnostics/Trace.hpp` | 131 | all-static: `IndentSize`, `IndentLevel`, `Write*`, `TraceInformation`/`Warning`/`Error`, `Assert` | **275** |
| `System/Diagnostics/DebugProvider.hpp` | 78 | 4 virtuals (`Write`, `WriteLine`, `Fail`, `OnIndentLevelChanged`…) | 275 (through `Debug`) |
| `System/Diagnostics/StackTrace.hpp` | 68 | frame accessors | — |
| `System/Diagnostics/StackFrame.hpp`, `StackFrameExtensions.hpp`, `Debugger.hpp`, `UnreachableException.hpp` | 214 | — | — |
| 14 attribute headers (`Debugger*Attribute.hpp`, `ConditionalAttribute.hpp`, `CodeAnalysisAttributes.hpp`, `StackTraceHiddenAttribute.hpp`) | 630 | marker types | — |

### 2.2 Implementation files (1 file, 371 lines)

`src/System/Diagnostics/Process.cpp` — findings 268, 269, 270, 271, 272, 273,
274. **Everything else in the component is header-inline**, which is the
central ABI fact of §8: `Process` is the *only* type here with an out-of-line
body, and it is a **pimpl**, so its internal state can change without touching
any consumer's layout. `Debug`/`Trace` have **no data members at all** (every
member is `static`), so they have no layout to change either. **This namespace
is unusually cheap to repair**, and §8 quantifies that.

### 2.3 Tests (7 files, 1,251 lines, one executable)

`SharpRuntimeTests_Diagnostics`: **159 tests** measured 2026-08-03 —
`CodeAnalysisTests.cpp` (326), `DebugTraceTests.cpp` (261),
`DiagnosticsRemainingTests.cpp` (396), `ProcessEnvironmentOverrideTests.cpp`
(57), `ProcessStartupFailureTests.cpp` (54), `ProcessTests.cpp` (105),
`TraceConditionalWriteTests.cpp` (52). **`Process` has 105 lines of test for
371 lines of the most failure-prone code in the module**, and no test covers
destruction, restart, EINTR, the process tree, or concurrency — which is
exactly the shape of the eight findings.

### 2.4 Cross-module callers

Measured by grep over `modules/` and `tests/`: `System/Diagnostics/` headers
are included by `modules/threading/include/System/TimeProvider.hpp`,
`modules/xml/src/System/Xml/XPath/XPathAstInternal.cpp`,
`tests/integration/Task42Tests.cpp` and
`tests/integration/System/ExceptionHResultPopulationTests.cpp` — **all four for
`Debug`/`Trace`/attributes, none for `Process`**. No first-party caller starts a
process, so every `Process` repair's in-repository blast radius is its own tests.

---

## 3. Confirmed finding inventory — all 8, with the measured current behaviour

Every row re-measured 2026-08-03 against the shipped
`libsharp_runtime_diagnostics.a`. "Measured" quotes `2023_probe1_before.log`.

| ID | Sev | Cause | Measured now | Disposition |
|---|---|---|---|---|
| **268** | med | **D-A** | `WaitForExit(-1)` on a 3 s child **returned `false` after 0 ms**; `WaitForExit(INTCS_MIN)` **returned normally after 0 ms** with no diagnostic | **#2024**, compatible |
| **269** | high | **D-B** | destroying an unredirected running `Process`: the child was **state `'Z'`** 500 ms later and the probe's own `waitpid(WNOHANG)` still returned it — **an unreaped zombie**. Destroying a *redirected* running `Process` instead **took 2005 ms** for a 2 s child — **destruction blocks** | **#2029**, DESIGN, blocked |
| **270** | high | **D-C** | `Start()` on a `Process` with a live reader thread: **`terminate called without an active exception`, KILLED by signal 6** | **#2025**, compatible |
| **271** | high | **D-D** | the same `const std::string&` returned by `getStandardOutputTextProperty()` observed **4 bytes mid-run and 8 bytes after exit** — the reader thread appends to a buffer the caller holds a reference into; `getHasExitedProperty() const` **mutates** `hasExited`/`exitCode` | **#2030**, DESIGN, blocked |
| **272** | med | **D-A** | with a non-`SA_RESTART` `SIGALRM` at 1 s, `WaitForExit()` on a 3 s child **returned after 1000 ms** and `getExitCodeProperty()` then **threw** "Process must exit before requested information can be determined" — the blocking form returned **without the child having exited** | **#2024**, compatible |
| **273** | med | **D-E** | after `Kill(true)`, a `setsid` grandchild was **ALIVE** | **#2031**, DESIGN, blocked |
| **274** | high | **D-F** | the forked child calls `::setenv` (allocating, not async-signal-safe) and `::execvp`; the parent is multithreaded exactly when a previous redirected `Start` left reader threads running | **#2026**, compatible |
| **275** | high | **D-G** | `Debug::providerStorage()` is a function-local `static std::shared_ptr<DebugProvider>` written by `SetProvider` and read by every `Write`, with no atomic and no lock; `Debug::indentSizeStorage()` and `Trace::indentSizeStorage()` are plain process-global scalars (`indentLevel` **is** `thread_local`) | **#2027**, compatible |

**Eight findings in, eight out.** None is a duplicate, none is a false premise,
none is already remediated, and none receives a "no action" disposition.

---

## 4. Corrections to the audit record

Historical audit text preserved; these are appended corrections.

### 4.1 SR-AUD-269 is two opposite defects, and the index row names both without
saying they are mutually exclusive

Measured: the outcome depends on **whether output was redirected**. Unredirected
→ the destructor returns immediately and leaves a **zombie**, permanently, for
the lifetime of the parent. Redirected → `~Impl` joins the reader threads, which
cannot finish until the child closes its stdout, so **destruction blocks for as
long as the child runs** — 2005 ms measured for a 2 s child, and unbounded in
general. A repair must therefore satisfy two requirements that pull in opposite
directions, which is why #2029 is a design ticket and not a bug fix.

### 4.2 SR-AUD-272 understates its own consequence

The row says EINTR "makes `WaitForExit` return without recording child exit
state, leaving ExitCode unavailable". Measured, the stronger statement is true:
**`WaitForExit()` — whose doc-comment says "Blocks the calling thread until the
associated process terminates" — returned while the child was still running**,
1000 ms into a 3 s child. The contract violated is the function's own, not just
`ExitCode`'s availability. The same function then joins the reader threads
(`Process.cpp:317-318`) even though the child is alive, so a redirected caller
blocks there instead — an EINTR-triggered *change of which line blocks*.

### 4.3 SR-AUD-268's second half is a distinct defect from its first

`WaitForExit(-1)` returning immediately is a **wrong constant** (.NET's `-1` is
`Timeout.Infinite`). `WaitForExit(-2)` not being rejected is a **missing
validation**. They share one expression, so #2024 repairs them together, but the
first changes a defined result and the second only closes an unvalidated domain;
§7.3 tabulates them separately rather than blending them.

### 4.5 SR-AUD-270's trigger is a joinable reader, not a running child (#2025, measured)

Appended by **#2025** from `build-probe/2025_probe1_before.log`. §5's D-C and §10's test
matrix both frame the abort as happening while the previous child is **still running**
("restarting a `Process` **with a live pipe-reader**"). Measured, the trigger is a **joinable
reader thread**, which outlives the child:

- **Probe case C** — the previous child had already **exited**, and the restart still aborted
  with `SIGABRT`, because nothing had joined the reader. Only `WaitForExit()` and
  `getHasExitedProperty()` join it. So §10's row *"restart after the previous child exited
  (must work)"* was **already broken today** in the redirected case, for any caller that had
  not happened to wait first.
- **Probe case B** — the captured text **accumulates** across a restart: `"first"` then
  `"firstsecond"`. §10 asked for "`stdoutText` state after a restart" to be decided; it is now
  **reset** at the commit point.
- **Probe case E** — an **unredirected** restart while the child runs neither aborts nor is
  refused today: it silently **abandons** the first child, which is never reaped
  (`waitpid(WNOHANG)` returned 0, i.e. still alive and still owned). §7.1's one-line
  justification ("changes only paths that today abort or deadlock") therefore **understates**
  #2025: this path today neither aborts nor deadlocks, and #2025 narrows it to an exception.
  That row is tabulated in §7.4 rather than absorbed silently.

A fourth measured fact reframes the whole ticket: **`Process`'s default constructor is
private**, so the only public ways to obtain one are the three static `Start` overloads and
`GetCurrentProcess()`. Every public call to the **instance** `Start()` is therefore a
**restart** by construction — the "first start" case exists only inside the static factories.

### 4.6 Three audit statements that are correct and are **not** corrected


- SR-AUD-274's async-signal-safety claim is exactly right, and the measured
  aggravating detail is that the parent becomes multithreaded **through this
  class's own** reader threads, so the hazard is reachable without the caller
  ever creating a thread.
- SR-AUD-271's TSan claim was **not** re-run here (no TSan build was made in
  this batch); the probe reproduces only the *observable* half (a live reference
  whose contents change). #2030's own sanitizer plan re-runs TSan.
- SR-AUD-275's TSan claim is likewise the audit's; §11 marks it deferred.

---

## 5. Root causes

### D-A — one timing function with two unvalidated domains

**Members: SR-AUD-268, SR-AUD-272.** `WaitForExit`'s two overloads neither
honour .NET's `-1 = infinite` constant, nor reject a nonsensical negative, nor
retry the blocking `waitpid` on `EINTR` — although the *same file* retries
`EINTR` correctly in three other places (`drainPipe`,
`reportChildStartupFailure`, and `Start`'s status read). As with `System::Text`'s
T-A, the repair is **transcribed from this file's own already-correct pattern**,
so it needs no reference tree. Compatible.

### D-B — a destructor with no reaping policy

**Member: SR-AUD-269.** `~Impl` joins threads and does nothing about the child.
Gated: the two failure modes require opposite fixes and the choice (detach,
reap asynchronously, kill, or block) is a policy.

### D-C — a restart path that assigns over a joinable `std::thread`

**Member: SR-AUD-270.** `Process.cpp:274` and `:278` assign into
`impl_->stdoutReader`/`stderrReader` unconditionally. `std::thread::operator=`
on a joinable target calls `std::terminate`. The class's own doc-comment
advertises the restart ("Starts (**or restarts**) the process"). Compatible —
a call that aborts the process has no defined result to preserve.

### D-D — public state shared with an internal thread, with no boundary

**Member: SR-AUD-271.** Structurally the same shape as CCF-009/T-G (shared
mutable state reachable from a public getter), but with an internal *thread*
rather than a process-wide singleton as the second party. Gated, because the
honest repair changes `getStandardOutputTextProperty`'s return type.

### D-E — a process-tree contract implemented as a process-group signal

**Member: SR-AUD-273.** `killpg` reaches the child's process group; `setsid`
leaves it. Gated: the alternatives (recursive `/proc` walk, `PR_SET_CHILD_SUBREAPER`,
cgroup) differ in portability and in **how many processes get killed**.

### D-F — non-async-signal-safe work between `fork` and `exec`

**Member: SR-AUD-274.** `::setenv` allocates; if another thread held the malloc
lock at `fork`, the child deadlocks and never execs — and the parent then blocks
in `Start`'s status-pipe read. Compatible: build the `envp` array **before**
forking and `execve`/`execvpe` it.

### D-G — global diagnostic state with no synchronisation

**Member: SR-AUD-275.** A non-atomic `shared_ptr` global plus two non-atomic
scalar globals. Compatible: `Debug` and `Trace` have **no data members**, so
adding a lock or an atomic changes no layout and no signature.

---

## 6. Findings and surfaces that are *not* in this namespace's queue

| Item | Why not |
|---|---|
| `System::Diagnostics::CodeAnalysis` attributes | audited, no evidence-backed finding |
| `StackTrace`/`StackFrame`/`Debugger` | audited, no confirmed finding; symbolisation is out of scope like reflection |
| `System::Diagnostics::Metrics`, `ActivitySource`, `EventSource`, `Stopwatch`'s siblings | **not ported**; absent features are not remediation |
| `Process` enumeration, priority/memory/module introspection, `UseShellExecute`, the `Exited`/`OutputDataReceived` event model | documented out of scope in `Process.hpp`'s own class comment; §15 |
| `PosixSignalRegistration` (#1985, #1986, #1979) | `System::Runtime`, already ticketed |
| `System::Diagnostics::Contracts` | not ported |

---

## 7. Compatible versus approval-sensitive classification

### 7.1 Compatible — no approval required

| Ticket | Cause | Findings | What changes observably |
|---|---|---|---|
| **#2024** | D-A | 268, 272 | `WaitForExit(-1)` waits indefinitely as .NET's `Timeout.Infinite` does; `milliseconds < -1` raises `ArgumentOutOfRangeException`; the blocking `waitpid` retries on `EINTR`, so `WaitForExit()` honours its own "blocks until the process terminates" contract |
| **#2025** | D-C | 270 | restarting a `Process` whose previous child is still being read no longer **aborts the process** |
| **#2026** | D-F | 274 | the forked child performs no allocating call before `exec`, so it cannot deadlock on an inherited malloc lock. **No observable behaviour change on any successful path** |
| **#2027** | D-G | 275 | `Debug::SetProvider`/`GetProvider` and the two indent-size globals become race-free. **No single-threaded behaviour change** |
| **#2028** | — | (docs) | **nothing executable** — `Process.hpp`, `Debug.hpp` and `Trace.hpp` stop promising what the gated tickets have not delivered |

Why each is compatible in one line: **#2025 and #2026 change only paths that
today abort or deadlock; #2027 changes only concurrent behaviour, which was
undefined; #2028 changes no code; #2024 is the only one that changes a
currently-defined result, and §7.3 tabulates every row it changes.**

### 7.2 Approval-sensitive — designed here, not implemented

| Ticket | Cause | Findings | Gate |
|---|---|---|---|
| **#2029** | D-B | 269 | destruction semantics: today it either leaks a zombie or blocks; every fix picks one to change |
| **#2030** | D-D | 271 | `getStandardOutputTextProperty`'s **return type** must change from `const std::string&` to `std::string` to be safe |
| **#2031** | D-E | 273 | `Kill(true)` starts killing processes it does not kill today |

### 7.3 The complete observable-change table for #2024

The only compatible ticket that changes a currently-defined result.

| Call | Before (measured) | After |
|---|---|---|
| `WaitForExit(0)` | polls once, returns `hasExited` | **identical** |
| `WaitForExit(500)` on a fast child | `true` | **identical** |
| `WaitForExit(500)` on a slow child | `false` after ~500 ms | **identical** |
| `WaitForExit(-1)` | **`false` after 0 ms** | **blocks until the child exits**, then `true` — .NET's `Timeout.Infinite` |
| `WaitForExit(-2)` … `WaitForExit(INTCS_MIN)` | **returns normally after 0 ms** | `ArgumentOutOfRangeException("milliseconds")` — **the one narrowed row** |
| `WaitForExit()` with no signals | blocks until exit | **identical** |
| `WaitForExit()` interrupted by a non-`SA_RESTART` signal | **returns early, child still running, `ExitCode` throws** | blocks until the child actually exits |
| `WaitForExit()` on `GetCurrentProcess()` | returns immediately | **identical** |

The `-1` row is a **behaviour change on a defined input** and is called that
rather than hidden: today it returns a meaningless `false` (the child has not
exited and the caller asked to wait). It is proposed as compatible on the
#2007 precedent — a defined result with no possible use — but a reviewer who
disagrees can split it into its own gated ticket without disturbing the other
three rows.

### 7.4 The complete observable-change table for #2025 (appended by #2025, measured)

§7.1's one-line justification for #2025 ("changes only paths that today abort or deadlock")
does not cover every row, so every row is listed. "Before" is
`build-probe/2025_probe1_before.log`; "after" is `build-probe/2025_probe1_after.log`.

| Call | Before (measured) | After |
|---|---|---|
| restart after exit, unredirected (case A) | works | **identical** |
| restart after exit, redirected, caller waited (case B) | works, but captured text **accumulates** (`"firstsecond"`) | works; captured text is **reset** (`"second"`) |
| restart after exit, redirected, caller did **not** wait (case C) | **`SIGABRT`** — `terminate called without an active exception` | works |
| restart while running, redirected (case D) | **`SIGABRT`** | `InvalidOperationException` |
| restart while running, unredirected (case E) | **succeeds**, silently abandoning the first child unreaped | `InvalidOperationException` — **the one narrowed row** |
| restart after a failed restart (case F) | works | **identical** |
| restart with an empty `FileName` | `InvalidOperationException` | **identical** (validation order unchanged) |
| `Start()` on a `GetCurrentProcess()` instance | forks, but leaves `isCurrentProcess` set, so `WaitForExit`/`Kill` silently no-op on a real child | the flag is cleared, so the instance describes the child it started |

Case E is the only row that removes a currently-working call. It is proposed as compatible
because what it removed was a **silent child leak**: the caller lost the pid, the object stopped
describing the abandoned process, and nothing could ever reap it. Case B's reset is the second
behaviour change on a working path, and is the answer to §10's open question about
`stdoutText` after a restart. A reviewer who disagrees with either can split it out without
disturbing cases C and D, which are the abort this ticket exists to remove.

---

## 8. Compatibility proofs and the source / ABI / layout consequence matrix

### 8.1 Declarations

| Ticket | Signature | `noexcept` | virtual / vtable | data members | mangled names |
|---|---|---|---|---|---|
| #2024 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2025 | unchanged | unchanged | unchanged | **`Impl` only** — pimpl, invisible | unchanged |
| #2026 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2027 | unchanged | unchanged | unchanged | **none exist** (`Debug`/`Trace` are all-static) | unchanged |
| #2028 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2029 | unchanged | unchanged | unchanged | `Impl` only — invisible | unchanged |
| #2030 | **`getStandardOutputTextProperty`/`getStandardErrorTextProperty` return `std::string` by value** | unchanged | unchanged | `Impl` only | **two change** |
| #2031 | unchanged | unchanged | unchanged | `Impl` only | unchanged |

**The single most important ABI fact in this namespace: `Process` is a pimpl.**
`sizeof(Process)` is one `std::unique_ptr` and stays so whatever happens to
`Impl` — so mutexes, atomics, extra pids, a reaper thread and a subreaper flag
are all **layout-invisible**, and a consumer relinks rather than rebuilds.
`Debug`, `Trace` and every attribute type have **no data members at all**.
**Only #2030 touches a public declaration**, and only by changing two return
types from `const std::string&` to `std::string` — which is source-compatible
for the ordinary spellings (`auto s = …`, `const auto& s = …`, which
lifetime-extends) and a break only for code that stores the reference and
expects it to track later output, which is exactly the unsafe use the ticket
exists to remove.

### 8.2 Recompilation

`Process.cpp` is the only body; #2024–#2026 and #2029–#2031 are **relink-only**
for consumers. #2027 and #2028 touch headers (`Debug.hpp`, `Trace.hpp`,
`Process.hpp`), so their consumers **recompile** — the ordinary consequence of
an inline change.

---

## 9. Downstream consumer impact

**Not estimated, by instruction.** `CNA` and `mobile-eggbert` were not read,
searched, built, tested or modified, and no filesystem search left this
repository. **#1773 stays `blocked`.** In-repository callers were measured
(§2.4): **no first-party file uses `Process` at all**, and the four that use
`Debug`/`Trace` use them single-threaded, so **no in-repository call site
changes behaviour** under the compatible batch.

---

## 10. Test matrix

Permanent, add-only, in `modules/diagnostics/tests/System/Diagnostics/`.

| Area | Cases required |
|---|---|
| **#2024 timing** | `WaitForExit(-1)` on a child that exits after a measured delay (returns `true`, elapsed ≥ the delay); `-2`, `INTCS_MIN` → exact exception type, `paramName`, message; `0` on a live and on an exited child; a positive timeout that expires and one that does not; **an EINTR case** using a non-`SA_RESTART` `SIGALRM`, asserting `WaitForExit()` returns only after the child exits and `ExitCode` is then available; `GetCurrentProcess()` unchanged |
| **#2025 restart** | restart after the previous child exited (must work); restart **while** it runs with and without redirection (must not abort — exact exception or documented semantics); restart twice; both readers live; `stdoutText` state after a restart |
| **#2026 fork safety** | a start issued **while two reader threads are live** (the reachable multithreaded-parent case), repeated N times; a start with 100 environment variables; a start whose `chdir` fails; a start whose exec fails — all still reporting synchronously through the status pipe |
| **#2027 diagnostics globals** | concurrent `SetProvider`/`Write` from two threads (TSan); `IndentSize` set concurrently; single-threaded behaviour byte-identical before and after; `SetProvider(nullptr)` still throws |
| **#2028 docs** | none executable; instead **pin the current gated behaviour** — the zombie, the blocking destructor, the surviving `setsid` grandchild and the live output reference — so #2029–#2031 cannot land silently (the #2012/#2022 precedent, and #2022 is why this row is mandatory rather than optional) |
| **layout pins** | `sizeof`/`alignof` of `Process` and `ProcessStartInfo` |

Every `Process` test must be leak-free **and zombie-free**: the suite should
assert at teardown that no child of the test process remains in state `Z`.

---

## 11. Sanitizer matrix

| Sanitizer | Applies to | What it must show |
|---|---|---|
| **TSan** | **#2027 (compatible), #2030 and #2029 (gated)** | the `Debug::SetProvider`/`Write` race and the `stdoutText` reader/getter race present **before** and absent **after**, with the changed body compiled into the probe. **TSan is genuinely applicable here** — unlike in `System::Text`, where it was correctly recorded as not applicable |
| **ASan** | #2025, #2026, #2029 | no use-after-free when a reader thread outlives a restarted or destroyed `Process` |
| **LSan** | #2024, #2025, #2026 | no fd leak and no thread leak across a failed start, a restart, or an interrupted wait |
| **UBSan** | #2024 | the timeout arithmetic (`now + milliseconds(INTCS_MIN)`) is defined. **Run by #2024 and NON-DISCRIMINATING, reported as such:** clean **before and after**, because converting `INTCS_MIN` milliseconds to nanoseconds stays inside `int64`. UBSan was proven live by a deliberate `chrono` overflow control |

`fork`-based tests need `ASAN_OPTIONS=detect_leaks=0` in the child or a
`_exit` path that does not run atexit handlers; record whichever is used.

---

## 12. Reference evidence actually available, per repair

`/rv/tmp/runtime/src/libraries/` re-verified **absent** 2026-08-03; no .NET
runtime is installed.

| Cause | Evidence available here | Sufficient? |
|---|---|---|
| **D-A** (EINTR half) | this file's **own** three correct `EINTR` retry loops (`drainPipe`, `reportChildStartupFailure`, `Start`'s status read); POSIX `waitpid(2)` semantics | **yes** — transcribed from the port |
| **D-A** (`-1` half) | `Timeout.Infinite == -1` is stated in this repository's own `System::Threading` port and used by `WaitHandle`/`Monitor` there | **yes** — in-repository precedent |
| **D-A** (rejection half) | the sibling guards in the same file raise `ArgumentOutOfRangeException(name, "Non-negative number required.")`; #1953's null-argument precedent | **yes** |
| **D-C** | `std::thread::operator=` on a joinable target calls `std::terminate` — the C++ standard, not .NET | **yes** |
| **D-F** | POSIX `fork(2)`'s async-signal-safety requirement; `execvpe`/`execve` are POSIX/GNU | **yes** |
| **D-G** | the race is a C++ data race, provable by TSan without any .NET reference | **yes** |
| **D-B, D-D, D-E** | .NET's exact `Process.Dispose`, `StandardOutput` and `Kill(true)` contracts | **no** — each is gated on approval *and* on evidence, and §14 says so per ticket |

**No repair in §7.1 depends on a .NET behaviour that could not be established
here.** That is the criterion separating the two columns, and it is the same
criterion the previous five reviews used.

---

## 13. Recommended execution order

1. **#2023** — this plan (no code).
2. **#2025** (D-C) — first, because it is the one defect that **aborts the
   process**, and because every other `Process` test needs restart to be safe.
3. **#2026** (D-F) — second, same file, same fork path, and it removes a
   deadlock that would otherwise make the concurrency tests flaky.
4. **#2024** (D-A) — the timing contract, once the lifecycle is safe.
5. **#2027** (D-G) — independent of `Process` entirely; may be done in parallel
   with any of the above, and is the only compatible ticket that needs TSan.
6. **#2028** — last, because it must describe what #2024–#2027 left true, and it
   must pin what #2029–#2031 have not changed.
7. **#2029, #2030, #2031** — design records only; none is implemented without
   its §14 approval sentence.

#2025 and #2026 may share one commit (both edit `Start`'s fork path in one run —
the #2007/#2008 and #1991/#1992 precedent). Every other ticket takes its own.

---

## 14. Approval package — the three gated causes

Requested **only** when this namespace's compatible half is done; **none is
requested by writing this**, and the consolidated request will follow the
`docs/SystemTextApprovalPackage.md` format.

### 14.1 #2029 — D-B, the destruction policy (SR-AUD-269)

**Now:** unredirected → a permanent zombie; redirected → destruction blocks for
the child's whole lifetime (measured 2005 ms). **.NET:** `Process.Dispose()`
neither waits for nor kills the child; the runtime reaps asynchronously through
a SIGCHLD-based child reaper. **Alternatives:** (A) a process-wide reaper thread
owning a SIGCHLD handler — matches .NET, but installs a process-wide signal
disposition, which collides with `PosixSignalRegistration` (#1975/#1979);
(B) double-fork so the child is reparented to init and never needs reaping —
changes the pid the caller observes and breaks `WaitForExit`; (C) reap
best-effort in the destructor with `WNOHANG` and **detach** the readers, so
destruction never blocks and a still-running child is left to the OS —
smallest, and leaves a zombie only for a child still running at destruction;
(D) document that a `Process` must be waited on before destruction. Recommended:
**C**, with (A) recorded as the .NET-parity option.

> Approve making `~Process` non-blocking — detaching any live pipe-reader
> threads instead of joining them, and reaping the child with `waitpid(WNOHANG)`
> when it has already exited — accepting that a child still running at
> destruction is left to the operating system rather than waited for, and that
> destroying a redirected `Process` no longer waits for its output. Ticket
> **#2029**.

### 14.2 #2030 — D-D, the concurrency boundary (SR-AUD-271)

**Now:** `getStandardOutputTextProperty()` hands out a `const std::string&` into
a buffer an internal thread appends to (measured: 4 bytes, then 8);
`getHasExitedProperty() const` mutates `hasExited`/`exitCode`.
**Proposed:** a mutex inside `Impl` (layout-invisible, §8.1) guarding all state,
and the two text getters **returning by value**. **Consequence:** two public
return types change — the only public declaration change in the namespace.
**Alternative:** keep the reference and document that it may only be read after
`WaitForExit`, which is what the current tests happen to do.

> Approve changing `System::Diagnostics::Process::getStandardOutputTextProperty`
> and `getStandardErrorTextProperty` to return `std::string` **by value** and
> guarding all `Process` state with an internal mutex, accepting the two changed
> public return types and the copy they introduce, so that reading captured
> output while the child is still running is no longer a data race. Ticket
> **#2030**.

### 14.3 #2031 — D-E, the process-tree contract (SR-AUD-273)

**Now:** `Kill(true)` is `killpg`, and a `setsid` descendant survives (measured).
**Proposed:** walk the descendant set (`/proc/*/stat`'s ppid, transitively) and
signal each, as .NET does on Linux. **Consequence:** `Kill(true)` starts killing
processes it does not kill today — which is what the contract promises, and is
still a change in blast radius; and the walk is **Linux-specific**, so other
POSIX platforms need a documented fallback to today's `killpg`.
**Alternative:** rename/redocument the parameter as "process group" and close the
finding as a documented reduction.

> Approve making `System::Diagnostics::Process::Kill(true)` terminate the
> transitive descendant set discovered from `/proc` on Linux rather than only
> the child's process group, accepting that it begins to kill descendants that
> called `setsid` and survive today, and that non-Linux POSIX platforms keep the
> process-group behaviour with a documented limitation. Ticket **#2031**.

---

## 15. Explicit exclusions

1. **Process enumeration and introspection** (`GetProcesses`, `GetProcessById`,
   priority, memory, modules, threads) — documented out of scope in
   `Process.hpp` and not invented as findings.
2. **The event model** (`Exited`, `OutputDataReceived`, `ErrorDataReceived`) and
   the `Stream`-based `StandardOutput`/`StandardError` — absent API, not wrong
   API. Note that #2030's return-by-value repair is *also* the shape that would
   have to change again if streams were ever added; recorded so the two are not
   done twice.
3. **`UseShellExecute`** — Windows-shell-specific.
4. **Windows and Emscripten `Process`** — the class throws
   `PlatformNotSupportedException` there by design (`CLAUDE.md`'s platform
   policy). Every repair here is POSIX-side and must keep the non-POSIX branches
   compiling and throwing.
5. **`StackTrace` symbolisation** — the same class of deviation as reflection.
6. **Downstream migration.** CNA and mobile-eggbert (§9).

---

## 16. Post-audit observations found by this review (no `SR-AUD-*` identifier)

Audit numbering stays frozen at 364. Each was measured by #2023 inside files an
existing finding already owns, and is folded into the owning ticket.

| # | Observation | Measured | Folded into |
|---|---|---|---|
| 1 | `WaitForExit()` (the **void** form) returns while the child is still running when interrupted — the doc-comment's own contract, not just `ExitCode`'s availability | returned after 1000 ms on a 3 s child | #2024 |
| 2 | after an interrupted `WaitForExit()`, the reader joins at `Process.cpp:317-318` still execute, so a redirected caller blocks *there* instead | inspection + §4.2 | #2024 |
| 3 | destroying a **redirected** running `Process` blocks for the child's whole lifetime, the opposite failure from the zombie the finding names | 2005 ms for a 2 s child | #2029 |
| 4 | the parent is multithreaded **through this class's own reader threads**, so SR-AUD-274's fork hazard needs no caller-created thread to be reachable | inspection | #2026 |
| 5 | `getHasExitedProperty() const` mutates observable state through the pimpl, so `const` gives no thread-safety guarantee here | inspection | #2030 |

---

## 16.1 A post-audit defect found by the compatible batch and FILED, not absorbed (#2032)

Found by **#2026** while making its regression tests deterministic. It belongs to no existing
finding and receives no `SR-AUD-*` identifier; numbering stays frozen at 364.

`Process::WaitForExit(intcs)` polls to a deadline, but every poll runs `reapIfNeeded()`, which
**joins the pipe-reader threads** once it reaps the child. A reader cannot finish until the pipe
reaches EOF, and EOF requires *every* holder of the write end to close it — including a
**grandchild** that inherited it. Measured
(`build-probe/2032_probe1_waitforexit_timeout_ignored.log`): `/bin/sh` is `dash`, and
`dash -c 'sleep 30'` **forks** rather than exec's, so `Kill()` terminated only `dash`;
`WaitForExit(5000)` then returned `true` after **29,951 ms** — about six times its own timeout,
and unbounded in general.

It is **not** SR-AUD-269 (that is the *destructor* blocking), **not** SR-AUD-272 (EINTR) and
**not** SR-AUD-268 (the argument domain): it is the timeout overload exceeding its own declared
bound. It is **not fixed here** because every repair has to decide what happens to a reader
thread that cannot finish — join, detach, or abandon the fd — and **detaching is exactly
§14.1's option C**, the gated destructor policy. Absorbing it into a compatible ticket would
pre-empt that approval. Ticket **#2032**, `blocked` on #2029.

Consequence for this namespace's own tests: `#2025` and `#2026` use `exec sleep 30` rather than
`sleep 30` for a killable redirected child, so no test depends on this defect. The shape is
preserved in the probe, not hidden.

---

## 17. Namespace completion criteria

`System::Diagnostics` is complete when **all eight** findings are `remediated`
or carry the `confirmed (design-complete)` qualifier with a blocked
implementation ticket, and:

1. #2024–#2028 are `done` and their tests are permanent;
2. #2029–#2031 each carry a durable design here **and** a blocked ticket whose
   notes name the approval sentence **and** a permanent behaviour-pinning test
   (mandatory, not optional — this is the #2022 lesson);
3. the whole 37-executable gate is green apart from the known
   environment/#1962 failures;
4. `SharpRuntimeTests_Diagnostics` has grown by the §10 matrix, add-only, from
   its measured baseline of **159**;
5. TSan is **run**, not recorded as inapplicable — this is the first namespace
   in the programme where it genuinely applies to the compatible half;
6. no `SR-AUD-*` identifier was created — numbering stays at 364.

---

## 18. Status

| Ticket | Cause | Findings | State |
|---|---|---|---|
| #2023 | — | maps all 8 | this document |
| #2024 | D-A | 268, 272 | **done** (2026-08-04), compatible, +13 tests |
| #2025 | D-C | 270 | **done** (2026-08-04), compatible, +15 tests |
| #2026 | D-F | 274 | **done** (2026-08-04), compatible, +13 tests |
| #2027 | D-G | 275 | `todo`, compatible |
| #2028 | — | (docs + gated pins) | `todo`, compatible |
| #2029 | D-B | 269 | **blocked**, design complete (§14.1) |
| #2030 | D-D | 271 | **blocked**, design complete (§14.2) |
| #2031 | D-E | 273 | **blocked**, design complete (§14.3) |
