<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Runtime` namespace review

**Ticket #1972** (`REVIEW-SYSTEM-RUNTIME-NAMESPACE`, P1, size L, **review/design**).
Written 2026-08-03 on branch `feature/remediation-batch-system-runtime-review`.

This is the evidence-based namespace review that converts the **21 open audit
findings** in `modules/runtime/` into a bounded, ordered ticket queue. It issues
**no new `SR-AUD-*` identifier** — audit numbering stays frozen at **364** — and it
marks **no finding remediated merely because it is planned**.

It is the successor deliverable to `docs/ThreadingNamespaceReviewPlan.md` (#1950)
and `docs/ThreadingTasksChannelsReviewPlan.md` (#1964) and follows their structure.
Where a cause here is a **new site of an already-closed cross-cutting family**, this
document says so and reuses that family's selected policy rather than inventing a
second one.

---

## 1. Why `System::Runtime` is next, and why not `System::Uri`

The queue selection is not alphabetical and not by raw finding count.

`plan.sqlite3` contains exactly **one `todo` ticket** — **#1963**, which is
deliberately deferred because it needs the .NET reference tree this environment does
not have. Every other active row is `blocked` (approval or downstream) or
`needs_user`. The `task` table's mechanical porting queue is **exhausted**, so
`prompt.md` Step 1 selects nothing and the namespace question has to be answered
from the audit inventory instead.

`NEXT.md` §14 and `plan.md` both record the previous batch's recommendation:
*"`System.Runtime` or `System.Uri` is the recommended next review — neither has a
`docs/*Plan.md`, and both are small enough for one batch."* That recommendation is
**not stale**; it was written by the immediately preceding batch and nothing since
has changed the inventory. Both candidates were re-measured here before choosing.

Open confirmed findings by owning module, re-measured from
`audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-03:

| Module | Open | high | medium | low |
|---|---|---|---|---|
| `core` | 72 | — | — | — |
| **`runtime`** | **21** | **3** | **16** | **2** |
| `uri` | 14 | **0** | 14 | 0 |
| `text` | 14 | — | — | — |

`System::Runtime` is selected over `System::Uri` because:

1. **Severity, not count.** `runtime` carries **three high-severity findings**;
   `uri` carries **none**. All three highs sit in one file
   (`PosixSignalRegistration.cpp`) and two of them — a hang inside a raw signal
   handler and the destruction of process-wide signal policy — are availability
   defects that no `uri` finding matches.
2. **No family plan exists** for either, so nothing would be re-planned; but
   `runtime`'s findings collapse into **twelve** root causes covering **one**
   dominant file, which is the "one structural fix closes a family" shape a review
   is supposed to find.
3. **Dependency readiness.** `modules/runtime` declares only
   `PUBLIC_DEPENDENCIES Core.Base Collections.Core`. Every compatible repair below
   is internal to the module, so the module graph stays at **41 / 91**.
4. **Independent validation.** `SharpRuntimeTests_Runtime` is a single executable.
5. **Not blocked on anything.** No `runtime` finding depends on #1773, on the #1929
   date/time approval chain, or on any declined layout approval.

Explicitly **not** selected, and why: `core` (72) is already carved up by seven
`CCF-*` family plans; `uri` (14) and `text` (14) are lower-severity and remain the
natural successors to this review.

**One selection premise in `docs/ThreadingNamespaceReviewPlan.md` §1 is corrected
here.** That document dismissed this namespace with *"`runtime` (21) is dominated by
reflection and serialization surfaces that CLAUDE.md already classifies as permanent
deviations."* Measured against the index, that is **wrong**: of the 21 open findings,
**one** (SR-AUD-168, interop attributes) falls under a CLAUDE.md permanent deviation,
**zero** are reflection findings, and **zero** are serialization findings — neither
`Serialization/SerializationInfo.hpp` nor `Serialization/StreamingContext.hpp` carries
a single finding. The three highest-severity findings are POSIX signal-handling
defects, which no permanent deviation covers. The historical §1 text is retained; this
correction is appended to it in §21 of that document.

---

## 2. Scope and file inventory

Component **`Runtime`** → target `sharp_runtime_runtime`,
`modules/runtime/CMakeLists.txt`, `TYPE STATIC`,
`PUBLIC_DEPENDENCIES Core.Base Collections.Core`.

| Kind | Count | Location |
|---|---|---|
| Public headers | 34 | `modules/runtime/include/System/Runtime/**/*.hpp` |
| — `System::Runtime` | 2 | `AmbiguousImplementationException.hpp`, `GCSettings.hpp` |
| — `System::Runtime::CompilerServices` | 19 | `CompilerServices/*.hpp` |
| — `System::Runtime::ExceptionServices` | 1 | `ExceptionServices/ExceptionDispatchInfo.hpp` |
| — `System::Runtime::InteropServices` | 9 | `InteropServices/*.hpp` |
| — `System::Runtime::Serialization` | 2 | `Serialization/*.hpp` |
| — `System::Runtime::Versioning` | 1 | `Versioning/VersioningAttributes.hpp` |
| Implementation bodies | 2 | `src/.../PosixSignalRegistration.cpp` (240 POSIX + 20 fallback lines), `src/.../RuntimeInformation.cpp` (94 lines) |
| Public types (`class`/`struct`/`enum class`) | 66 | across the 34 headers |
| Test translation units | 5 | `tests/System/Runtime/{RuntimeTests}.cpp` + four subdirectory units |
| `TEST(...)` cases | 126 | 82 + 20 + 11 + 9 + 4 |
| Audit reports | 43 | `audit/modules/runtime/**` — **1 : 1 with the 43 tracked files**, no gap |

The module is **overwhelmingly header-only**: 32 of 34 public headers carry their
whole implementation inline, and only two `.cpp` bodies exist. That is the single
most important structural fact for §11 — a change to a private data member in this
module has almost no `.cpp`-only containment, so every layout question is a
consumer-visible question. It is also why the two `.cpp` bodies concentrate the
severity: they are the only places where real OS state is touched.

### 2.1 Public-surface inventory by defect-bearing type

| Type | Header / body | Public surface touched by a finding |
|---|---|---|
| `PosixSignalRegistration` | `PosixSignalRegistration.hpp` + `.cpp` | `Create(PosixSignal, Handler)`, `Dispose()`, destructor, and the process-global registry/watcher behind them |
| `PosixSignal` | `PosixSignal.hpp` | the enum's accepted value domain (via `Create`) |
| `ExceptionDispatchInfo` | `ExceptionDispatchInfo.hpp` | `Capture(exception_ptr)`, static `Throw(exception_ptr)`, instance `Throw()`, implicit move ctor / move assignment |
| `GCSettings` | `GCSettings.hpp` | `setLatencyModeProperty`, `setLargeObjectHeapCompactionModeProperty` |
| `FormattableStringFactory` | `FormattableStringFactory.hpp` | `Create(std::string, std::vector<std::string>)` doc-comment |
| `OSPlatform` | `OSPlatform.hpp` | absence of a public default constructor |
| `RuntimeInformation` | `RuntimeInformation.hpp` + `.cpp` | absent `FrameworkDescription`/`RuntimeIdentifier`; `getOSArchitectureProperty()` on Windows |
| `AmbiguousImplementationException` | `AmbiguousImplementationException.hpp` | base class, sealing, absent message+inner constructor |
| `ExternalException` | `ExternalException.hpp` | absent `(string, int)` constructor, `ErrorCode`, specialised `ToString()` |
| `CompilerFeatureRequiredAttribute` | `CompilerFeatureRequiredAttribute.hpp` | `setIsOptionalProperty` |
| `ConditionalWeakTable<TKey,TValue>` | `ConditionalWeakTable.hpp` | `Enumerator::Reset()`, enumerator snapshot retention, template parameter domain |
| seven versioning attributes | `VersioningAttributes.hpp` | absent `OSPlatformAttribute` base; constructor-only `Url`/`Message` |
| fifteen interop attributes/enums | `InteropAttributes.hpp` | `UnmanagedType::LPStruct`, `StructLayoutAttribute::Pack`, `DllImportAttribute::PreserveSig`/`BestFitMapping`, `MarshalAsAttribute` fields, absent `ComInterfaceType`/`ClassInterfaceType` |

### 2.2 Files carrying **no** finding

Recorded so a future session does not re-derive it: 21 of the 34 headers and both
test units for `NativeMemory` and `RuntimeHelpers` are clean. `RuntimeHelpers.hpp`
(247 lines) and `NativeMemory.hpp` (234 lines) were both fully audited with **no
confirmed source defect** — `NativeMemory`'s POSIX `AlignedRealloc` old-size repair
is explicitly recorded as already correct. `Serialization/SerializationInfo.hpp` and
`Serialization/StreamingContext.hpp` carry no finding at all.

---

## 3. Confirmed finding inventory (all 21 open + 1 remediated)

Status is the **verified** status as of 2026-08-03, re-derived from
`audit/AUDIT_FINDINGS_INDEX.md` **and re-checked against current source** — not
copied from the audit prose. Every open one is still `confirmed`; **none** has been
silently remediated since the audit, **none** is a duplicate, and **none** rests on a
false premise (three carry premise *corrections*, recorded in §4).

| ID | Sev | Type | One-line defect | Cause |
|---|---|---|---|---|
| SR-AUD-169 | high | `PosixSignalRegistration` | the pre-existing process disposition is discarded on install and forced to `SIG_DFL` on the last dispose | **R-A** |
| SR-AUD-172 | high | `PosixSignalRegistration` | the self-pipe is blocking, so the raw handler's `write()` can block once the pipe fills | **R-B** |
| SR-AUD-171 | high | `PosixSignalRegistration` | a non-cancelled job-control signal runs the OS default and **stops the process** | **R-C** |
| SR-AUD-170 | medium | `PosixSignalRegistration` | positive raw Unix signal numbers cast to `PosixSignal` are rejected | **R-D** |
| SR-AUD-155 | medium | `ExceptionDispatchInfo` | a null `exception_ptr` is accepted and deferred to an undefined rethrow | **R-E** |
| SR-AUD-156 | medium | `GCSettings` | both setters retain values outside their declared enum domain | **R-F** |
| SR-AUD-152 | medium | `OSPlatform` | the valid `default(OSPlatform)` value cannot be expressed | **R-G** |
| SR-AUD-153 | medium | `RuntimeInformation` | `FrameworkDescription` and `RuntimeIdentifier` are absent | **R-G** |
| SR-AUD-158 | medium | `AmbiguousImplementationException` | wrong base, not sealed, no message+inner constructor | **R-G** |
| SR-AUD-159 | medium | `ExternalException` | no `(string, int)` constructor, no `ErrorCode`, no specialised `ToString()` | **R-G** |
| SR-AUD-160 | low | `CompilerFeatureRequiredAttribute` | `IsOptional` is freely mutable where .NET has `init` | **R-G** |
| SR-AUD-163 | medium | versioning attributes | no public `OSPlatformAttribute` base for the five platform attributes | **R-G** |
| SR-AUD-164 | medium | versioning attributes | nullable/mutable metadata collapsed into constructor-only strings | **R-G** |
| SR-AUD-165 | medium | `UnmanagedType` | `LPStruct = 48` collides with `LPUTF8Str`; `Currency`/`IDispatch` absent | **R-G** |
| SR-AUD-166 | medium | `StructLayout`/`DllImport` | `Pack=8`, `PreserveSig=true`, `BestFitMapping=true` vs managed `0`/`false`/`false` | **R-G** |
| SR-AUD-167 | medium | `MarshalAs`/COM attributes | typed fields and two public enums lost to untyped integers | **R-G** |
| SR-AUD-161 | medium | `ConditionalWeakTable` | `Reset()` rewinds a snapshot and the snapshot strongly retains every value | **R-H** |
| SR-AUD-162 | medium | `ConditionalWeakTable` | scalar generic arguments are admitted where the managed API forbids them | **R-I** |
| SR-AUD-059 | low | `FormattableStringFactory` | the doc-comment promises an exception the body never throws | **R-J** |
| SR-AUD-154 | medium | `RuntimeInformation.cpp` | Windows `OSArchitecture` aliases `ProcessArchitecture` | **R-K** |
| SR-AUD-168 | medium | interop attributes | no declaration attachment, ABI effect, P/Invoke or COM consumer exists | **R-L** |
| SR-AUD-157 | medium | two exception types | **already `remediated`** by #1875 (HResults assigned and pinned) | — |

---

## 4. Corrections to the audit record

Every correction below was **measured** on 2026-08-03, not inferred. The historical
audit text is preserved verbatim in each per-file report; corrections are appended
there and summarised here. Reproduction sources and logs:
`build-probe/1972_probe1_runtime_boundaries.cpp`, `build-probe/1972_probe2_posix_signal.cpp`,
`build-probe/1972_probe1_before.log`, `build-probe/1972_probe2_before.log`.

### 4.1 SR-AUD-155 has **four** undefined-behaviour routes, not the two it names

The finding names `Capture(null)` and static `Throw(null)`. Both reproduce
(`capture_null=died_signal_11`, `static_throw_null=died_signal_11`). Two further
routes reach the identical `std::rethrow_exception(nullptr)` through **ordinary
well-formed C++ that never passes a null anywhere**:

```
moved_from_source_is_null=1     moved_from_throw=died_signal_11
move_assigned_source_is_null=1  move_assign_throw=died_signal_11
```

`ExceptionDispatchInfo` holds a `std::exception_ptr` by value and declares no move
operations, so the **implicitly declared** move constructor and move assignment
operator leave the source's `exception_` null. A consumer who writes
`auto sink = std::move(source);` and then calls `source.Throw()` gets the same
SIGSEGV as a consumer who passed null to `Capture`.

**This decides the repair.** A check placed only at `Capture` and static `Throw`
— which is what the finding literally asks for — leaves both moved-from routes
open, because they never cross either entry. The instance `Throw()` needs its own
guard. This is the **same shape** as `docs/ThreadingNamespaceReviewPlan.md` §18.5's
correction to SR-AUD-199, where a moved-from `CancellationToken` was a second route
to an identical crash the finding never named. Third occurrence of the pattern in
this repository; §5 R-E records it as a standing check for future reviews.

### 4.2 SR-AUD-169's consequence is sharper than "receives the default disposition"

The finding says the process *"receives the default disposition after the last
registration is disposed"*. Measured, that phrasing understates two of the three
cases, because **for most catchable signals the default disposition terminates the
process**:

```
sighup_before_create=SIG_IGN
sighup_during_registration=port-handler
sighup_after_dispose=SIG_DFL      <-- SIGHUP's default is *terminate*
```

A process that deliberately set `SIG_IGN` on `SIGHUP` — the standard daemon idiom —
is **killed by the next SIGHUP** after an unrelated component's last
`PosixSignalRegistration` is disposed. The audit's own SIGWINCH reproduction cannot
show this, because SIGWINCH's default *is* ignore. Both routes are now reproduced;
the SIG_IGN → terminate route is the one that justifies the `high` severity, and it
is recorded here because the repair must restore `SIG_IGN` exactly, not merely
"restore something".

### 4.3 SR-AUD-170 rejects the positive spelling of **named** members too

The finding is about *"a positive native signal number"* such as `SIGUSR1`. Measured,
the rejection is wider than that:

```
raw SIGUSR1(10)=rejected     raw SIGUSR2(12)=rejected
raw SIGPIPE(13)=rejected     raw SIGALRM(14)=rejected
raw SIGWINCH(28)=rejected    <-- a signal the port *does* support, spelled positively
```

`static_cast<PosixSignal>(SIGWINCH)` is rejected even though
`PosixSignal::Sigwinch` is accepted, because `toNativeSignalNumber` maps **only**
the ten negative enumerators. So the defect is not "raw numbers are unsupported"
but "the enum is accepted in exactly one of its two valid spellings". Any repair
must make the two spellings agree, not merely add an unnamed-signal path.

### 4.4 The audit's own SR-AUD-171 and SR-AUD-172 reproductions are order-dependent, and the first version of this review's probe got it wrong

Recorded because it is a methodology trap, not a defect: the registry's watcher is a
**process-global thread started by the first `Create()`**, and `fork()` duplicates
only the calling thread. A child forked *after* the parent has registered anything
inherits `watcherRunning_ == true` with **no watcher thread behind it** —
`ensureWatcherStarted()` returns early, nothing drains the pipe, `dispatchSignal()`
never runs, and both forked cases report a confident **false negative**:

```
(first probe version, forked after the parent registered)
  child_still_running_after_sigtstp callbacks=0     <-- SR-AUD-171 "did not reproduce"
  watcher_never_parked                              <-- SR-AUD-172 "did not reproduce"

(corrected order, forked before any parent registration)
  child_stopped_by=20 (WIFSTOPPED) ... callbacks=1  <-- SR-AUD-171 reproduces
  passed_pipe_capacity ... died_signal_14 (SIGALRM) <-- SR-AUD-172 reproduces
```

This is `docs/ThreadingNamespaceReviewPlan.md` §19.4's rule in a new form: *a probe's
negative result is evidence about the probe until the probe has been shown capable of
reporting something.* Every forked case in `1972_probe2` now prints a liveness marker
(`callbacks=`, `parked`) so a future silent pass is visible rather than trusted, and
the fork-based cases run **first**, before the parent registers anything.

### 4.5 SR-AUD-172 is a hang, and the blocked writes carry no information

The finding is source-proven and says *"a flood reproducer is not run in the test
process"*. One is now run, deterministically:

```
plain_pipe read_O_NONBLOCK=0 write_O_NONBLOCK=0
passed_pipe_capacity
saturation_child=died_signal_14 (SIGALRM: the raw handler blocked)
```

With the watcher parked inside a user callback, delivery number ~65,537 blocks
**inside the raw signal handler** and never returns; `alarm(10)` is what ends the
child. Two things the finding does not state follow from the shape of the code and
sharpen the case for the repair: the blocking writes are **pure loss** — `pending_`
is a per-signal *flag*, already set before the `write()`, so the byte that blocks
carries no information the watcher does not already have — and the thread that
blocks is **whichever thread the OS chose for delivery**, which is precisely the
"interrupted while holding a lock the watcher needs" deadlock the finding describes.

### 4.6 SR-AUD-162's premise does not survive translation to C++

Recorded as a premise correction rather than a repair (see §5 R-I). The finding says
the C++ template *"admits scalar generic arguments that the managed API forbids"*
and cites CS0452. Measured against the actual C++ declaration, the port does **not**
instantiate a weak reference to a scalar: `ConditionalWeakTable<TKey,TValue>` keys on
`std::weak_ptr<TKey>` and stores `std::shared_ptr<TValue>`, and
`std::weak_ptr<int>` is a perfectly well-defined reference to a heap-managed control
block. The managed `where TKey : class` constraint exists because the **CLR** cannot
create a weak GC handle to a value type; that is a CLR limitation with no C++
counterpart. Adopting the constraint would delete working, well-defined
functionality to imitate a restriction whose cause does not exist here. §5 R-I
therefore proposes **documenting a deliberate widening**, not narrowing the domain,
and §17 states what evidence would reopen it.

### 4.7 SR-AUD-168's root cause is an already-classified permanent deviation

`CLAUDE.md`'s "Known permanent deviations" lists **"P/Invoke / interop — out of
scope"**. SR-AUD-168's central claim — that the interop attributes have no
declaration attachment, no ABI transformation, no DLL binding and no COM marshaller
— is therefore a **restatement of an accepted project boundary**, not an open design
question. Its *actionable* residue is the one thing the audit adds and CLAUDE.md
does not cover: the header **does not say so**, unlike the compiler-service marker
headers which do. §5 R-L splits the finding accordingly.

### 4.8 `docs/ThreadingNamespaceReviewPlan.md` §1's dismissal of this namespace

Corrected in §1 above and appended to that document as §21.

### 4.9 One handoff wording correction, carried forward from the previous batch

The previous batch's report stated that **#1967** involved *"no exception-contract
change"*. That sentence conflates three separate consequences that this repository
keeps distinct, and the correction belongs in the record rather than in conversation.
For #1967 the accurate statement is:

- **No public signature or `noexcept` declaration change** — true.
- **No ABI or mangled-symbol change** — true.
- **A deliberate, observable change in the exception type crossing an API
  boundary** — also true, and it was the *point* of the ticket: SR-AUD-234's repair
  wraps the close error in `ChannelClosedException` at `ChannelReader<T>::ReadAsync`
  and `ChannelWriter<T>::WriteAsync`, so a caller now catches a different type than
  before.

#1967 is **not reopened**: its implementation and tests are complete and correct.
Only the summary wording is corrected, and the historical text is preserved. The
correction is also appended to `docs/ThreadingTasksChannelsReviewPlan.md` §18 and to
`NEXT.md`.

---

## 5. Shared root causes

Twelve causes cover all 21 open findings. Three are new sites of policies this
repository has already settled; the rest are local to this namespace.

### R-A — process-global signal disposition is taken over and never given back (1 finding)

**SR-AUD-169** (high). `installIfNeeded` calls `sigaction(signo, &sa, nullptr)` —
the third argument is the *oldact* out-parameter, and passing `nullptr` throws the
pre-existing disposition away. `uninstallIfUnused` then calls
`std::signal(signo, SIG_DFL)`, which is not "undo" but "impose the OS default".

Root cause: the registry stores *which* signals it has installed
(`installedSignals_`, a `std::vector<int>`) but not *what was there before*. The
repair is a data-structure change inside the anonymous namespace — remember the
`struct sigaction` alongside the signal number, and restore it instead of forcing
`SIG_DFL`.

**Selected policy:** save on the transition to installed, restore on the transition
to uninstalled, byte for byte including `sa_flags` and `sa_mask`. Nothing about
*chaining* to the saved handler during delivery belongs here — that is R-C, and it
is approval-gated. Keeping them apart is what makes R-A compatible.

### R-B — an async-signal handler can block (1 finding)

**SR-AUD-172** (high). `ensureWatcherStarted` uses `::pipe(selfPipe_)`, whose
descriptors are blocking by definition, and `onNativeSignal` then writes to
`selfPipe_[1]` with its own comment asserting that a full pipe *"returns `EAGAIN`
and is safe to ignore"*. `EAGAIN` is returned only by a **non-blocking** descriptor.

Root cause: the code documents the non-blocking self-pipe idiom and implements the
blocking half of it. **Selected policy:** make the **write end** non-blocking so the
comment becomes true. The **read end stays blocking** on purpose — the watcher's
`read()` is its parking mechanism, and making it non-blocking would convert the
watcher into a spin loop. This is the whole repair; nothing else changes.

### R-C — the non-cancelled default-disposition path is unconditional (1 finding, approval-gated)

**SR-AUD-171** (high). `dispatchSignal` ends every non-cancelled delivery with
`std::signal(signo, SIG_DFL); ::raise(signo);` regardless of *which* signal it is.
For `SIGTSTP`/`SIGTTIN`/`SIGTTOU` the default action stops the process, so merely
**observing** a job-control signal suspends the observer.

This cause also owns the **second half of SR-AUD-169** — whether a saved non-default,
non-ignore handler should be *chained* on a non-cancelled delivery — because both
are the same decision: *what should happen after the callbacks return?* See §10.

### R-D — a valid public input domain is rejected (1 finding)

**SR-AUD-170** (medium). `toNativeSignalNumber` indexes a ten-entry table by
`-value - 1`, so anything outside `[-10, -1]` becomes `0` and `Create` throws
`PlatformNotSupportedException`. §4.3 shows this rejects the positive spelling of
supported signals as well as genuinely unnamed ones.

**Selected policy:** treat a **positive** value as a raw native signal number and
accept it when the OS can actually catch it, keeping every existing rejection that
is *correct* (`SIGKILL`, `SIGSTOP`, out-of-range values). This **widens** accepted
input: no call that works today stops working, which is what makes it compatible.

### R-E — a null/empty handle crosses a public boundary and defers to undefined behaviour (1 finding)

**SR-AUD-155** (medium). **This is the fourth module to reach the CCF-011 policy**
(`docs/EmptyCallableBoundaryPlan.md`), after `core` (#1866–#1870), `threading`
(#1951) and `threading-tasks` (#1965). The handle here is a `std::exception_ptr`
rather than a `std::function`, but the shape is identical: an empty handle is
accepted at a public entry and the failure surfaces later as a native fault the
caller cannot catch.

**Selected policy — CCF-011's, unchanged:** decide emptiness **at the public
boundary, before anything is done with the input**, and choose the exception by API
shape. Here .NET's answer is recorded directly in the finding: `Capture(null)` and
static `Throw(null)` throw `ArgumentNullException`. §4.1's two extra routes are
**not** argument errors — nothing was passed — so they take
`InvalidOperationException`, which is .NET's answer for "this object is not in a
state where the operation is meaningful". Three of the thirty-plus sites this policy
has now reached needed a non-default exception type; this is the fourth, and it is
the reason §8 of `EmptyCallableBoundaryPlan.md` says the policy selects *.NET's
answer for that shape of API*, not one blanket spelling.

### R-F — public setters retain values outside their declared enum domain (1 finding)

**SR-AUD-156** (medium). Both `GCSettings` setters are bare assignments, so
`static_cast<GCLatencyMode>(99)` becomes persistent, observable global state
(measured: `latency(99): retained=99`, `latency(-1): retained=-1`,
`loh(0): retained=0`, `loh(3): retained=3`).

**Selected policy:** reject at the boundary with
`ArgumentOutOfRangeException`, over the domains the finding records from the .NET
source — `LatencyMode` from `Batch` (0) through `SustainedLowLatency` (3), and LOH
mode from `Default` (1) through `CompactOnce` (2). `NoGCRegion` (4) is a **read-only
runtime-owned state**: it is a legal value to *observe* and an illegal value to
*write*, so the getter's domain and the setter's domain deliberately differ.

### R-G — the public shape itself diverges (10 findings, approval-gated)

**SR-AUD-152, 153, 158, 159, 160, 163, 164, 165, 166, 167.** This is the exact
analogue of `docs/ThreadingNamespaceReviewPlan.md`'s cause **T-H**, and it is
handled the same way: every member changes a declaration a consumer can already
name — a base class, a sealing decision, a constructor set, a public enumerator
value, a field's default, a field's type, or a property's presence.

Nothing in R-G is implemented without approval. §10 carries the design and the exact
approval sentence.

### R-H — a snapshot enumerator retains and replays what the source has released (1 finding, approval-gated)

**SR-AUD-161** (medium). `ConditionalWeakTable::Enumerator` copies every `Entry`
— including its **strong** `ValuePtr` — into a snapshot vector, and `Reset()` sets
the snapshot index back to zero. .NET's enumerator promises to retain **only**
`Current`, and its `Reset()` is an empty method.

Related to CCF-018 (enumerator lifecycle) and CCF-019 (borrowed handles outliving
owners) but a member of neither: CCF-018 is about missing *checks* before storage
access and CCF-019 about *borrowed* pointers, whereas this enumerator's defect is
**over-retention** — it is too strong, not too weak. Repairing it changes the
enumerator's data members, so it is approval-gated (§10).

### R-I — the C++ template domain is wider than the managed generic domain (1 finding)

**SR-AUD-162** (medium). §4.6 shows the finding's premise does not survive
translation. **Selected disposition:** document the widening as a deliberate,
permanent native adaptation in the header, and do **not** add a constraint. §17
states what would reopen it.

### R-J — documentation states a contract the code does not implement (2 findings, one shared)

**SR-AUD-059** (low) and the *documentable residue* of **SR-AUD-168** (medium).
`FormattableStringFactory::Create`'s `@throws std::invalid_argument if @p format is
empty` is false — measured `empty_format=no-throw args=0 text_len=0` — and .NET
rejects only null, which `std::string` cannot represent. `InteropAttributes.hpp`
describes layout/marshalling *effects* it cannot produce and, unlike the
compiler-service marker headers, never says so.

**Selected policy:** make the documentation true. Do **not** add an empty-format
rejection: the behaviour is correct and the *claim* is the defect. This is the same
disposition the audit itself recommends ("not a reason to add an empty-string
rejection").

### R-K — a platform branch that cannot be exercised or verified here (1 finding)

**SR-AUD-154** (medium). The Windows `getOSArchitectureProperty()` branch returns
`getProcessArchitectureProperty()`. The repair needs `IsWow64Process2` /
`GetNativeSystemInfo`, a Windows toolchain to compile it, and a mixed-bitness
Windows process to observe the difference. **None of the three exists here.**
Deferred verification ticket (§17), not a guess.

### R-L — an accepted permanent deviation that the header does not disclose (1 finding, split)

**SR-AUD-168** (medium). §4.7 splits it: the **structural** half is
`CLAUDE.md`'s permanent "P/Invoke / interop — out of scope" deviation and needs no
ticket; the **disclosure** half joins R-J.

---

## 6. Findings that are *not* in this namespace's queue

- **SR-AUD-157** — already `remediated` by #1875. Not re-opened, not re-counted.
- Every `core`, `threading`, `threading-tasks`, `threading-channels`, `uri` and
  `text` finding — different modules, different reviews.
- **#1963** (SR-AUD-200) — a `threading` deferral, unchanged by this review.

---

## 7. Dependency graph

```
R-B (self-pipe non-blocking)          independent — smallest, land first among the signal work
   |
R-A (save/restore disposition)        independent of R-B; both touch installIfNeeded/uninstallIfUnused
   |
   +--> R-C (chaining + job control)  APPROVAL-GATED; *requires* R-A's saved disposition to exist
   |
R-D (raw signal numbers)              independent; touches toNativeSignalNumber only

R-E (ExceptionDispatchInfo)           fully independent — different header, different namespace
R-F (GCSettings)                      fully independent
R-J (documentation truth)             fully independent
R-I (template-domain disclosure)      fully independent

R-G (public shapes)                   APPROVAL-GATED, ten independent members
R-H (weak-table enumerator)           APPROVAL-GATED, independent
R-K (Windows OS architecture)         DEFERRED — no toolchain, no host, no reference
```

The only hard ordering constraint is **R-A before R-C**: chaining cannot invoke a
saved handler that is not saved. Everything else may land in any order; §13 gives
the recommended one.

---

## 8. Compatible versus approval-sensitive classification

| Cause | Findings | Classification | Ticket |
|---|---|---|---|
| R-E | SR-AUD-155 | **compatible** | #1973 |
| R-B | SR-AUD-172 | **compatible** | #1974 |
| R-A | SR-AUD-169 | **compatible** | #1975 |
| R-F | SR-AUD-156 | **compatible** | #1976 |
| R-D | SR-AUD-170 | **compatible** (widening only) | #1977 |
| R-J | SR-AUD-059 + SR-AUD-168 disclosure | **compatible** (doc only) | #1978 |
| R-C | SR-AUD-171 + SR-AUD-169's chaining half | **approval-gated** | #1979 |
| R-G | ten findings | **approval-gated** | #1980 |
| R-H | SR-AUD-161 | **approval-gated** | #1981 |
| R-I | SR-AUD-162 | compatible, but a *classification* rather than a repair | #1982 |
| R-K | SR-AUD-154 | **deferred verification** | #1983 |
| R-L | SR-AUD-168 structural half | **no ticket** — CLAUDE.md permanent deviation | — |

"Compatible" here means all four of: no public signature, no object layout, no
vtable, no `noexcept` specification changed; **and** no call that succeeds today
begins to fail, except where the call was already producing undefined behaviour or
retaining an out-of-domain value that .NET rejects.

---

## 9. Source / ABI / layout consequence matrix for the compatible work

| Ticket | Public signature | Object layout | vtable | `noexcept` | Mangled symbols | Component edge |
|---|---|---|---|---|---|---|
| #1973 | unchanged | unchanged (`ExceptionDispatchInfo` stays 8 bytes) | none exists | unchanged (nothing was `noexcept`) | unchanged | none |
| #1974 | unchanged | anonymous-namespace state only | none | unchanged | unchanged | none |
| #1975 | unchanged | anonymous-namespace state only | none | unchanged | unchanged | none |
| #1976 | unchanged | `GCSettings` has no instances (`= delete`) | none | unchanged | unchanged | none |
| #1977 | unchanged | anonymous-namespace state only | none | unchanged | unchanged | none |
| #1978 | unchanged | unchanged | none | unchanged | unchanged | none |

Every compatible repair is either inside `PosixSignalRegistration.cpp`'s anonymous
namespace — invisible to any consumer — or an inline body whose declaration is
untouched. The module graph stays **41 / 91**.

**Observable behaviour consequences, stated rather than waved away:**

- #1973: four calls that produced a **SIGSEGV** now throw a catchable
  `System::Exception`. Nothing that worked stops working.
- #1974: a signal flood that previously **hung** a delivery thread now completes.
  Coalescing is unchanged — the byte that used to block carried no information.
- #1975: after the last `Dispose`, the process's disposition is what it was before
  the first `Create` instead of `SIG_DFL`. A consumer that (absurdly) relied on
  `PosixSignalRegistration` to *clear* a pre-existing handler loses that. No such
  consumer can exist deliberately; the current behaviour is not documented anywhere.
- #1976: `setLatencyModeProperty(static_cast<GCLatencyMode>(99))` now throws instead
  of storing 99. **This is the one compatible ticket that makes a currently
  succeeding call fail** — and .NET throws for the identical call. Every in-domain
  write, including `NoGCRegion` **reads**, is unaffected.
- #1977: strictly additive acceptance.
- #1978: no runtime change whatsoever.

---

## 10. Approval package — the three gated causes

Nothing in this section is implemented. Each entry gives the exact current
behaviour, the exact proposed behaviour, the consequences, alternatives, and **the
single approval sentence** the user would be answering.

### 10.1 #1979 — R-C: what happens after the callbacks return

**Current, measured** (`1972_probe2_before.log`): every non-cancelled delivery runs
`std::signal(signo, SIG_DFL); ::raise(signo);`. For `SIGTSTP` a child that merely
*observes* the signal stops: `child_stopped_by=20 (WIFSTOPPED) ... callbacks=1`.
A saved original handler — once #1975 saves one — is never invoked.

**Proposed:** mirror .NET's `HandleNonCanceledPosixSignal`, per the reference basis
recorded in the audit report: for `SIGTSTP`/`SIGTTIN`/`SIGTTOU`, do **nothing** after
notification; for a signal whose saved disposition is neither `SIG_DFL` nor
`SIG_IGN`, **chain** to the saved handler instead of raising; otherwise keep the
current restore-and-raise.

**Consequences.** No public signature, layout, vtable or `noexcept` change. One
deliberate observable change: a non-cancelled job-control registration no longer
suspends the process, and there is **no way to opt back in** — that is mandatory
migration for any consumer relying on it. A second: a saved handler starts running
where it previously did not.

**Why it is gated rather than landed with #1975.** The evidence classes differ. The
port's behaviour is reproduced here; .NET's is not. The audit report's reference
basis is a **reading of `pal_signal.c` and `PosixSignalRegistration.Unix.cs`** taken
when `/rv/tmp/runtime/src/libraries/` was present — **it is absent in this
environment** — and carries **no managed probe**. That is precisely the distinction
the repository already drew between **#1968** (SR-AUD-233 *had* a managed probe, so a
behaviour-incompatible repair landed) and **#1963** (SR-AUD-200 had none, so nothing
was changed). SR-AUD-171 is on #1963's side of that line.

**Alternatives, in increasing cost:**
1. **Adopt as proposed** — closest to .NET, revertible, one `.cpp` body.
2. **Job control only** — skip the stop for the three job-control signals, leave
   chaining out. Smaller change; leaves half of SR-AUD-169 open.
3. **Opt-in switch** — a new parameter or global toggle. Adds public surface for a
   behaviour .NET does not make configurable; not recommended.
4. **Defer until the reference tree is available**, pinning current behaviour with a
   test. Costs nothing and loses nothing but time.

> **Approval sentence for #1979:** *"Make a non-cancelled `SIGTSTP`/`SIGTTIN`/`SIGTTOU`
> delivery a no-op instead of stopping the process, and chain a saved non-default,
> non-ignore disposition instead of restoring `SIG_DFL` and re-raising — accepting
> that a consumer relying on the current job-control stop has no opt-in replacement,
> and that .NET's behaviour here is evidenced by the audit's reading of
> `pal_signal.c` rather than by a managed probe."*

### 10.2 #1980 — R-G: the ten public-shape divergences

Grouped by what each would break, so the answer can be partial:

| Group | Members | Change | Breaks |
|---|---|---|---|
| **G-1 additive only** | SR-AUD-152, 153, 159 | add `OSPlatform`'s public default constructor; add `RuntimeInformation::getFrameworkDescriptionProperty`/`getRuntimeIdentifierProperty`; add `ExternalException(std::string, intcs)`, `getErrorCodeProperty()`, specialised `ToString()` | nothing — no existing declaration changes |
| **G-2 value change** | SR-AUD-165, 166 | `LPStruct` 48 → 43; add `Currency=15`, `IDispatch=26`; `Pack` 8 → 0; `PreserveSig`/`BestFitMapping` true → false | any consumer reading the current defaults; **`StructLayoutAttributeTests.DefaultPack_IsEight` pins the wrong value and would have to be rewritten** |
| **G-3 hierarchy change** | SR-AUD-158, 163 | reparent `AmbiguousImplementationException` to `Exception` and seal it; introduce `OSPlatformAttribute` and reparent five attributes | `catch (const SystemException&)`; any derivation; **vtable and layout** |
| **G-4 mutability change** | SR-AUD-160, 164 | remove `setIsOptionalProperty`; move `Url` from constructor parameter to settable property | every current caller of the removed/moved members — **mandatory migration** |
| **G-5 typed metadata** | SR-AUD-167 | retype `MarshalAs` fields; add `ComInterfaceType`/`ClassInterfaceType` | any consumer passing raw integers |

**Recommendation, if only one group is approved: G-1.** It is purely additive, closes
three medium findings, and cannot break a consumer. G-2 is next-cheapest but requires
rewriting a test that deliberately pins the wrong value. G-3 changes vtables. G-4
mandates migration. G-5 is the largest.

> **Approval sentence for #1980 (G-1 only, the recommended minimum):** *"Add
> `OSPlatform`'s public default constructor (empty name, unequal to every named
> platform), `RuntimeInformation::getFrameworkDescriptionProperty()` and
> `getRuntimeIdentifierProperty()`, and `ExternalException`'s `(message, errorCode)`
> constructor, `getErrorCodeProperty()` accessor and specialised `ToString()` — all
> strictly additive, changing no existing declaration, layout, vtable or mangled
> symbol."*

### 10.3 #1981 — R-H: the weak-table enumerator

**Current:** `Enumerator` holds `std::vector<Entry>` — a strong `shared_ptr` per
value — and `Reset()` rewinds its index. **Proposed:** hold weak references in the
snapshot and resolve at `MoveNext`, so only `Current` is retained; make `Reset()` a
no-op, matching .NET.

**Consequences.** `Enumerator`'s data members change → **object layout change** in a
header-only template. `Reset()` becoming a no-op is a **deliberate observable
change**: measured `after_reset=1` today, `after_reset=False` in the managed probe
the audit ran. Any consumer calling `Reset()` to re-enumerate loses that.

> **Approval sentence for #1981:** *"Change `ConditionalWeakTable<TKey,TValue>::Enumerator`
> to retain only `Current` and make `Reset()` a no-op — accepting an enumerator object-layout
> change in a header-only template and the loss of re-enumeration via `Reset()`."*

---

## 11. Test matrix

| Ticket | New permanent tests | Where |
|---|---|---|
| #1973 | `Capture(null)` throws `ArgumentNullException`; static `Throw(null)` likewise; moved-from instance `Throw()` throws `InvalidOperationException`; move-assigned-from likewise; the moved-**to** instance still rethrows correctly; every existing capture/rethrow path unchanged | `ExceptionDispatchInfoTests.cpp` |
| #1974 | a flood past the pipe capacity with the watcher parked completes instead of hanging, bounded by a timed thread so a regression **fails** rather than wedges the suite | `PosixSignalTests.cpp` |
| #1975 | a custom `sigaction` is exactly restored after the last `Dispose`; `SIG_IGN` is restored as `SIG_IGN`; nested registrations restore only on the last one; `sa_flags`/`sa_mask` survive | `PosixSignalTests.cpp` |
| #1976 | out-of-domain latency (`99`, `-1`) and LOH (`0`, `3`) writes throw `ArgumentOutOfRangeException` and retain the previous value; `NoGCRegion` is rejected as a **write** but remains readable; every in-domain write still succeeds | `RuntimeTests.cpp` |
| #1977 | a positive raw `SIGUSR1` is accepted and delivers; `static_cast<PosixSignal>(SIGWINCH)` is accepted and behaves like `PosixSignal::Sigwinch`; `SIGKILL`/`SIGSTOP` stay rejected; out-of-range stays rejected | `PosixSignalTests.cpp` |
| #1978 | an empty format is accepted and yields zero arguments and empty text — pinning that the **behaviour** is correct and the doc was the defect | `RuntimeTests.cpp` |

Every new test is **add-only**. No existing test is deleted, disabled, weakened or
recategorised by any compatible ticket. (#1980 G-2 would require rewriting
`StructLayoutAttributeTests.DefaultPack_IsEight`; that is called out in §10.2 and is
one reason G-2 is not recommended first.)

---

## 12. Sanitizer matrix

| Ticket | Sanitizer | What it must discriminate |
|---|---|---|
| #1973 | ASan + UBSan + LSan | the pre-fix SIGSEGV on all four routes; no leak on the new throwing paths |
| #1974 | ASan + UBSan + LSan; **TSan** | the pre-fix hang is a *liveness* failure TSan cannot see — recorded as a known non-discriminator; TSan's job here is to prove the repair introduces no race between the handler's write and the watcher's read |
| #1975 | ASan + UBSan + LSan | no leak or invalid access from the added saved-disposition storage |
| #1976 | ASan + UBSan | no UB from the added enum-range comparisons |
| #1977 | ASan + UBSan + LSan | no out-of-bounds on the widened table lookup — this is the ticket where an off-by-one is most likely |

**Rule inherited from `docs/ThreadingNamespaceReviewPlan.md` §19.4 and applied
throughout:** a "sanitizer reported nothing" result is evidence about the *probe*
until the probe has been shown capable of reporting something. §4.4 is this
review's own example of that rule catching a false negative before it became a
conclusion. Honest non-discriminators are recorded as such, not dressed up.

Every sanitizer binary must be compiled **from source** with the sanitizer flags.
`GCSettings.hpp`, `ExceptionDispatchInfo.hpp` and `FormattableStringFactory.hpp` are
header-only, so instrumenting a probe recompiles them; `PosixSignalRegistration.cpp`
must be added to the probe's command line rather than linked from
`build/libsharp_runtime_runtime.a`, which is **not** instrumented.

---

## 13. Recommended execution order

1. **#1973** (R-E) — undefined behaviour reachable from public input, smallest, no
   OS interaction. Highest priority by the standing severity order.
2. **#1974** (R-B) — a reproducible hang; small, self-contained, and it makes the
   signal tests safe to extend.
3. **#1975** (R-A) — process-global state; must precede any R-C work.
4. **#1976** (R-F) — permanently invalid object state, no OS interaction.
5. **#1977** (R-D) — rejects valid input; lowest risk of the signal work but the
   most arithmetic, so it goes after the other two have stabilised the file.
6. **#1978** (R-J) — documentation truth; no runtime change.

Then, only with approval: **#1980 G-1** (cheapest, additive) → **#1979** →
**#1981** → the remaining #1980 groups.

---

## 14. Deferred verification work

- **#1983** (SR-AUD-154, R-K). Blocked on three independent absences: no Windows
  toolchain, no mixed-bitness Windows host, and no `/rv/tmp/runtime/src/libraries/`
  to confirm the `IsWow64Process2` mapping. The audit's secondary observation — that
  the unknown-compile-target fallback *fabricates* `X64` rather than refusing — is
  recorded with it. **Nothing is implemented from recollection.**
- **#1982** (SR-AUD-162, R-I). The disposition is a documented widening (§4.6). It
  would be reopened by evidence that this port's `weak_ptr<TKey>` keying has a
  semantic defect for scalar `TKey` — not merely by the existence of CS0452.

---

## 15. Explicit exclusions

- **SR-AUD-157** — already remediated (#1875).
- **The structural half of SR-AUD-168** — CLAUDE.md permanent deviation (§4.7).
- **Reflection, GC internals, P/Invoke, serialization infrastructure** — CLAUDE.md
  permanent deviations. `RuntimeHelpers`'s CLR-dependent operations correctly throw
  `PlatformNotSupportedException` and the audit classifies no defect there.
- **`System::GC`** — a separate module; `GCSettings::isServerGC_` being hardwired
  `false` is that adapter's documented boundary, not a finding.
- **Anything in `core`, `threading`, `uri`, `text`** — other reviews.
- **Adding `O_CLOEXEC` to the self-pipe.** Discovered while reproducing SR-AUD-172:
  the two descriptors are inherited across `exec()`. It is a real, separate defect,
  it has **no `SR-AUD-*` identifier** (numbering frozen at 364), and folding it into
  #1974 would change `exec()` inheritance under cover of a liveness repair. Tracked
  as inactive ticket **#1985**.

---

## 16. Namespace completion criteria

`System::Runtime` is complete when all of:

1. Every one of the 21 open findings is `remediated`, or carries a recorded
   disposition (approval-gated with a design, deferred with stated missing evidence,
   or classified as a permanent deviation).
2. `SharpRuntimeTests_Runtime` passes with no failure and no skip.
3. `cmake --build build --parallel 2` is clean — zero errors, zero warnings.
4. The module graph is still **41 / 91**, seams **2 / 18**, negative fixtures
   **10 / 81**.
5. No `SR-AUD-*` identifier was issued; the total stays **364**.
6. Every premise correction is appended to the owning per-file report with the
   historical text preserved.

---

## 17. Status

Written 2026-08-03. Implementation status is tracked per ticket in `plan.sqlite3`
and appended to this document as each lands.
