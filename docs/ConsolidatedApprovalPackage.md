<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Consolidated approval package — `System::Diagnostics` and `System::Text`

Written 2026-08-04 on branch `feature/remediation-batch-approval-packages-next-review`.

This is the **single place a decision is requested** for the twelve approval-sensitive tickets in
the two namespaces whose reviews are otherwise complete:

- **`System::Diagnostics`** — #2029, #2030, #2031, with **#2032 folded into #2029**;
- **`System::Text`** — #2013 … #2021.

It supersedes, **as the request**, `docs/SystemDiagnosticsNamespaceReviewPlan.md` §14 and
`docs/SystemTextApprovalPackage.md`. Both stay in place as the historical design records and are
not rewritten; where this document and either of them disagree, **this document is the measured
one** and §D.6 and §T.11 say exactly where.

**Nothing gated is implemented by writing this.** The only repository change made during the
verification is **#2033**, a documentation-plus-pins ticket split out under the compatible-subpart
rule (§D.5).

**Evidence base.** Every "now" row was re-measured on 2026-08-04 against the shipped library:

| Probe | Source | Log |
|---|---|---|
| Diagnostics reader-join doors | `build-probe/2033_probe1_reader_join_entry_points.cpp` | `..._entry_points.log` |
| Text approval re-verification | `build-probe/2033_probe2_text_approval_reverify.cpp` | `..._reverify.log` |

plus four temporary source mutations, each applied, built, run, and reverted with `git diff`
confirmed clean (§§D.2.3, D.3 and D.4, and #2033's own in §D.5).
`/rv/tmp/runtime/src/libraries/` re-verified **absent**; no .NET runtime is
installed in this container, so every claim about .NET is labelled **inferred** and never used as
the sole support for a recommendation.

---

# Part 1 — `System::Diagnostics`

## D.1 How to read this: three decisions, one of which absorbed a fourth

| Code | Tickets | Findings | One-line question |
|---|---|---|---|
| **D-A** | **#2029 + #2032** | SR-AUD-269 (+ one post-audit defect) | What happens to a pipe-reader thread that cannot finish, and to a child nobody waited for? |
| **D-B** | #2030 | SR-AUD-271 | May the two captured-output getters change their public return type? |
| **D-C** | #2031 | SR-AUD-273 | May `Kill(true)` start killing descendants that survive today? |

The three are **independent**: D-B changes a declaration and nothing else, D-C changes a signal's
blast radius and nothing else, and neither depends on D-A's policy. Approve, reject or defer them
separately.

---

## D.2 Approval D-A — the reader-thread and reaping policy (#2029 **and** #2032)

### D.2.1 Why #2032 is not a separate decision

`Process::reapIfNeeded` does two things in one place (`Process.cpp:110-129`): it reaps the child
with `waitpid(WNOHANG)`, and — the moment it succeeds — it **joins both pipe-reader threads**. A
reader cannot finish until the pipe reaches EOF, and EOF requires *every* holder of the write end
to close it, including a **grandchild** that inherited it. So "the child has exited" and "the
readers have finished" are welded together, and every caller pays for both.

#2032 was filed as `WaitForExit(milliseconds)` exceeding its own bound. **Measured, that is one of
five doors.** With `/bin/sh` (dash, which *forks* rather than exec's for `sleep N`) holding the
redirected pipe for 8 s, and the direct child killed out-of-band so it is dead but unreaped:

| Public call | Blocked | Declared bound |
|---|---|---|
| `WaitForExit(500)` | **7,502 ms** | 500 ms — **exceeded** |
| `getHasExitedProperty()` | **7,502 ms** | **none** — it takes no timeout |
| `Kill(false)` | **7,502 ms** | **none** — documented "immediately stops" |
| `Start()` (restart) | **7,503 ms** | **none** |
| `~Process` | **8,004 ms** | **none** — SR-AUD-269's blocking half |

`getExitCodeProperty()` reaches the join through `getHasExitedProperty()`, so **six public members
are affected by one cause**. A policy applied to `~Process` alone — which is exactly what
`SystemDiagnosticsNamespaceReviewPlan.md` §14.1's approval sentence says — would leave the other
four unbounded. **That is why #2032 belongs inside this decision and why §14.1's sentence is
replaced below.**

### D.2.2 Current behaviour, complete matrix

Measured except where marked. "Reaped" means the child is not left in state `Z`.

| Scenario | Blocking | Reaped | Captured text | Notes |
|---|---|---|---|---|
| Child exits, closes all redirected pipes | prompt everywhere | on the first reaping call | complete | the intended path; works today |
| Child exits, **grandchild holds stdout** | **all five doors block for the grandchild's life** | yes, once the join returns | complete when it returns | measured, §D.2.1 |
| Child exits, **grandchild holds stderr** | identical | identical | identical | **by construction** — the same two joins at `Process.cpp:126-127`; only the stdout case was timed |
| `WaitForExit(finite)` | **exceeds its bound** | yes | complete | #2032 |
| `WaitForExit()` (infinite) | blocks past the child, until pipe EOF | yes | complete | no bound to violate; the wait deliberately covers the readers |
| **Explicit disposal** | — | — | — | **there is no `Dispose()` in this port.** Destruction is the only disposal, so "explicit disposal" and `~Process` are the same row |
| `~Process`, unredirected | prompt | **no — permanent zombie** | n/a | SR-AUD-269 half one |
| `~Process`, redirected, child running | **blocks for the child's whole life** | no | n/a | SR-AUD-269 half two (2,005 ms for a 2 s child) |
| Restart (`Start()` again) | blocks as above | yes | **reset** to empty | #2025; a still-running child is refused with `InvalidOperationException` |
| **Asynchronous output readers** | — | — | — | **none exist.** `OutputDataReceived`/`ErrorDataReceived` and the `Stream`-based `StandardOutput` are out of scope (plan §15). The only readers are the two internal ones |
| Buffered unread output | never discarded today | — | complete | the join is what guarantees it |
| Reader join vs detach | always **join** | — | — | the subject of this decision |
| **Callback lifetime after destruction** | — | — | — | **no callbacks exist** in this port; there is no event model to outlive anything |
| Pipe ownership | the reader thread owns the **read** end and `close()`s it on exit (`drainPipe`, `Process.cpp:90`) | — | — | if a reader is abandoned rather than joined, that fd is never closed by anyone else |
| Reaping independent of reader completion | **not separated** — one function does both | — | — | separating them is the actual repair shape |

Three of the twelve scenarios the request template names (**explicit disposal**, **asynchronous
output readers**, **callback lifetime**) do not exist in this port. They are answered as
"not applicable, and here is why" rather than fabricated.

### D.2.3 The defect in §14.1's recommended option C, found by this verification

§14.1 recommends **option C**: "reap best-effort in the destructor with `WNOHANG` and **detach**
the readers". Measured against the code, **that is unsound as written**:

`drainPipe` is started as `std::thread(drainPipe, fd, &impl_->stdoutText)` (`Process.cpp:365`) — it
holds a **raw pointer into the `Impl` the `Process` owns**. Detaching it in `~Impl` and then
destroying `Impl` leaves a live thread appending to a destroyed `std::string`: a **guaranteed
use-after-free**, not a hypothetical one. Neither #2028's mutation check nor #2033's found it,
because both detached at points where the object stays alive, and neither was run under ASan.

The repair is available and cheap: give the reader **shared ownership** of its buffer
(`std::shared_ptr<std::string>` co-owned by the thread), so a detached reader writes into storage
that outlives the `Process`. `Impl` is behind a pimpl, so this is **layout-invisible**. But it is a
real requirement of option C, and an approval of option C that does not include it authorises a
use-after-free. **Any approval below that permits detaching must be read as including it.**

### D.2.4 What the approval must state, and the four candidate policies

The request template asks for six specific answers. Each candidate policy answers all six:

| Question | **A** — SIGCHLD reaper | **B** — double-fork | **C′** — detach + shared buffer | **D** — document only |
|---|---|---|---|---|
| Does a timeout bound only process exit? | yes | yes | **yes** | no — unchanged |
| Does it also bound reader completion? | no | no | **no** | no |
| Are readers detached after process exit? | no (still joined) | no | **yes** | no |
| May unread output be discarded? | no | no | **yes** — output arriving after the last read is not observable | no |
| May callbacks occur after `WaitForExit` returns? | n/a — no callbacks exist in this port | n/a | n/a | n/a |
| Does destruction wait for reader completion? | yes | yes | **no** | yes |

Descriptions, with the cost each carries:

- **A — a process-wide reaper thread owning a `SIGCHLD` handler.** Matches .NET (inferred). Installs
  a **process-wide signal disposition**, which collides directly with `PosixSignalRegistration`
  (#1975/#1979) — the repository already owns that surface and the collision is not theoretical.
  Fixes the zombie; fixes **nothing** about the reader join, so all five doors keep blocking.
- **B — double-fork so the child is reparented to init.** No reaping needed at all. Changes the pid
  the caller observes and **breaks `WaitForExit` outright** (the caller is no longer the parent).
  Rejected on those grounds.
- **C′ — option C, corrected.** Reap best-effort with `WNOHANG`; **detach** rather than join, in
  `reapIfNeeded` **and** `~Impl`; give the reader shared ownership of its buffer (§D.2.3). Every
  public door becomes prompt, a still-running child is left to the OS, and captured output is
  complete only up to the moment it was read. **Recommended.**
- **D — document and leave.** Already done: #2028 documented the destructor, #2033 documented the
  other four doors, and both are pinned. Leaves one `high` finding and one post-audit defect open
  permanently.

**The scope dimension, which is a second question inside the same decision.** Whatever policy is
chosen, it may be applied to `~Process` **only**, or to `reapIfNeeded` **as a whole**. §14.1 asked
for the former. Measured, the former leaves `getHasExitedProperty()`, `getExitCodeProperty()`,
`Kill()`, `Kill(bool)` and the restart `Start()` blocking without any bound at all. The
recommendation is **the whole of `reapIfNeeded`**.

### D.2.5 Consequences

| Dimension | Under C′ |
|---|---|
| Public signatures | **unchanged** |
| Object layout | **unchanged** — `Process` is a pimpl; `sizeof(Process) == sizeof(unique_ptr)`, pinned |
| Vtable | **unchanged** — `Process` has no virtuals |
| Mangled symbols | **unchanged** |
| `noexcept` | **unchanged** — none present, none added |
| Recompilation | `Process.cpp` only if the header comments are left alone; consumers **relink** |
| Platform | POSIX-only path; the Windows/Emscripten branches keep throwing `PlatformNotSupportedException` |
| Observable semantics | five doors stop blocking; a detached reader's late output is no longer observable; a child still running at destruction is left to the OS instead of being waited for |
| Resource | a detached reader is a leaked thread **and** a leaked fd for as long as the pipe holder lives; today it is a blocked caller instead. Neither is free, and this trades one for the other **deliberately** |

**Migration.** No first-party file uses `Process` at all (plan §2.4), so nothing in this repository
changes behaviour. Externally, a caller that relies on `WaitForExit()` returning only once all
output has been captured keeps that guarantee — the infinite form still joins under C′ *by design*
if the approval says so; a caller that reads the captured text after a **timeout** may see less
than it does today. Downstream (CNA, mobile-eggbert) **was not investigated; #1773 stays blocked.**

**Tests and sanitizers.** #2033's four pins and its control invert. New: a detached-reader
lifetime case run under **ASan** (this is where §D.2.3's use-after-free would appear and must not),
under **LSan** with the detached thread's allocation accounted for, and under **TSan** for the
buffer hand-off. A zombie-free teardown assertion already exists in every `Process` suite.

**Rollback.** Restore the joins and the raw buffer pointer. No persisted state, no file format, no
serialized data; the shared-buffer change is invisible to consumers in both directions.

> **Approval D-A.** Approve making `System::Diagnostics::Process` **detach** its pipe-reader
> threads instead of joining them, in **`reapIfNeeded` and in the destructor alike** — so that
> `getHasExitedProperty()`, `getExitCodeProperty()`, `Kill()`, `Kill(bool)`, the restart `Start()`,
> `WaitForExit(milliseconds)` and `~Process` all return promptly even while a grandchild still
> holds the redirected pipe — together with giving each reader **shared ownership of its capture
> buffer** so a detached reader cannot write into a destroyed `Process`, and reaping the child
> best-effort with `waitpid(WNOHANG)`; accepting that a child still running at destruction is left
> to the operating system rather than waited for, that output arriving after the last read is no
> longer observable, and that a reader whose pipe never reaches EOF becomes a leaked thread and
> descriptor rather than a blocked caller. Tickets **#2029** and **#2032**.
>
> *(Answer separately if you want `WaitForExit()` — the **infinite** overload only — to keep
> joining, so that its captured output stays complete on return. That is the one door where the
> current behaviour has a defensible contract.)*

---

## D.3 Approval D-B — the concurrency boundary (#2030, SR-AUD-271)

**Now, measured.** `getStandardOutputTextProperty()` returns `const std::string&` into a buffer an
internal thread appends to (the #2023 probe read 4 bytes mid-run and 8 after exit);
`getHasExitedProperty() const` **mutates** `hasExited`/`exitCode` through the pimpl.

**Proposed.** A mutex inside `Impl` guarding all state, and both text getters **returning by
value**.

| Dimension | Consequence |
|---|---|
| Public signatures | **two change** — `const std::string&` → `std::string` |
| Mangled symbols | **two change** |
| Object layout / vtable / `noexcept` | **unchanged** — the mutex is inside the pimpl |
| Source compatibility | `auto s = p.getStandardOutputTextProperty();` and `const auto& s = …;` both keep working (the latter lifetime-extends). **Breaks** only code that stores the reference and expects it to track later output — the unsafe use the ticket exists to remove |
| Recompilation | header change, so every consumer recompiles |

**Alternatives.** (A) return by value plus the mutex — recommended. (B) keep the reference and
document that it may only be read after `WaitForExit()` — which is what #2028 already did and what
the current tests happen to do; leaves a `high` finding open. (C) return by value **only** for the
getters and leave the rest unguarded — cheaper, but `getHasExitedProperty()`'s const mutation
stays, so `const` still conveys nothing.

**Migration.** No first-party caller. Externally, a runtime copy per call where there was none.

**Tests and sanitizers.** The two #2028 pins invert — one of them is a `static_assert`, so it
becomes a **compile error**, which is the intended trip-wire. **TSan** is the gate: a two-thread
probe (getter vs live reader) must report the race before and be silent after.

**Rollback.** Restore the two return types and remove the mutex; a second header change.

> **Approval D-B.** Approve changing `System::Diagnostics::Process::getStandardOutputTextProperty`
> and `getStandardErrorTextProperty` to return `std::string` **by value**, and guarding all
> `Process` state with a mutex inside its pimpl, accepting the two changed public return types and
> mangled symbols, the copy they introduce, and the recompilation the header change forces — so
> that reading captured output while the child is still running is no longer a data race. Ticket
> **#2030**.

---

## D.4 Approval D-C — the process-tree contract (#2031, SR-AUD-273)

**Now, measured.** `Kill(true)` is `::killpg`, and a `setsid` grandchild **survives**.

**Proposed.** Walk the transitive descendant set from `/proc/*/stat`'s ppid field and signal each,
as .NET does on Linux (inferred).

**New evidence from this verification.** §14.3's walk was **implemented temporarily** in
`Kill(bool)` (breadth-first from the child pid over `/proc`, `SIGKILL` each) and built and run: the
`setsid` grandchild **was killed** and the pin failed with its intended message. So the recommended
option is **demonstrably implementable in this container** — which §14.3 asserted but had not
shown. The mutation was reverted and `git diff` confirmed clean.

| Dimension | Consequence |
|---|---|
| Signatures / layout / vtable / symbols / `noexcept` | **all unchanged** |
| Observable semantics | `Kill(true)` begins killing processes it does not kill today — a change in **blast radius**, in the direction the parameter name already promises |
| Platform | the `/proc` walk is **Linux-specific**; other POSIX platforms need a documented fallback to today's `killpg` |
| Race | the walk is inherently racy (a descendant forked between the read and the signal escapes). .NET has the same property |

**Alternatives.** (A) the `/proc` walk with a documented non-Linux fallback — recommended.
(B) rename/redocument the parameter as "process group" and close the finding as a documented
reduction — what #2028 already did as a stopgap. (C) `PR_SET_CHILD_SUBREAPER` or a cgroup — larger,
Linux-specific too, and changes process reparenting for the whole program.

**Tests.** The #2028 pin inverts. New: the `setsid` grandchild **is** killed; a two-level
`setsid` chain is killed; a non-descendant with a coincidentally similar pid is **not**; the
non-Linux branch still compiles and documents its reduction.

**Rollback.** Restore the single `killpg` call.

> **Approval D-C.** Approve making `System::Diagnostics::Process::Kill(true)` terminate the
> transitive descendant set discovered from `/proc` on Linux rather than only the child's process
> group, accepting that it begins to kill descendants which called `setsid()` and survive today,
> that the walk is inherently racy against a concurrently forking descendant, and that non-Linux
> POSIX platforms keep the process-group behaviour with a documented limitation. Ticket **#2031**.

---

## D.5 The compatible portion split out and implemented — #2033

Tested against the compatible-subpart rule and **passed on every clause**: its root cause is a
**disclosure** defect (four public doors promised something they do not do), it needs no user
decision, it changes no public signature, layout, vtable, `noexcept` contract or mangled symbol, it
selects **no** reader-thread policy, it carries its own permanent tests, and it leaves the blocked
remainder — the policy (#2029) and the bound violation (#2032) — accurately scoped and still
blocked.

Shipped: `Process.hpp`'s class note and six member comments made true, plus
`ProcessReaderJoinBlockingPinTests.cpp` (+6: four pins, one control, one teardown check).
Mutation-checked. `SharpRuntimeTests_Diagnostics` **219 → 225**.

**No other separable compatible portion exists** in #2029, #2030 or #2031. Each was tested: #2029's
remainder *is* the policy; #2030's remainder is a public return type; #2031's remainder is a change
in which processes receive a signal.

## D.6 Corrections to `SystemDiagnosticsNamespaceReviewPlan.md` §14

| § | What §14 says | Measured 2026-08-04 |
|---|---|---|
| 14.1 | the approval changes `~Process` | the join lives in `reapIfNeeded` and is reached from **five** public doors; a destructor-only change leaves four unbounded (§D.2.1, plan §16.1.1) |
| 14.1 | option C is "detach the readers" | detaching in `~Impl` is a **use-after-free**: the reader holds a raw pointer into the destroyed `Impl`. Option C needs shared buffer ownership to be sound (§D.2.3) |
| 14.1 | option A "matches .NET" | still **inferred**; no .NET reference in this container. Recorded as such, and A is not recommended anyway |
| 14.3 | the `/proc` walk is the proposed repair | now **demonstrated** — implemented temporarily, the `setsid` grandchild was killed (§D.4) |
| §16.1 (#2032) | "the timeout overload exceeding its own bound" | true, and **one of five doors**; four of them have no bound at all |

---

# Part 2 — `System::Text`

## T.1 Status of the existing package

`docs/SystemTextApprovalPackage.md` (#2022, 2026-08-03) is **verified and stands**. Independently
re-measured 2026-08-04 (`build-probe/2033_probe2_text_approval_reverify.log`), and separately
confirmed that **no `modules/text` or `modules/core` production file has changed since that
package was written** (`git log 6abd16e..HEAD -- modules/text modules/core` is empty), so its
"now" rows are still the current behaviour and not a carried-forward measurement.

Three things are **added** here: the coupling analysis the request asks for (§T.2), a fifth and
sixth alternative for #2017 with a fact that changes their ranking (§T.5), and the CCF-012
re-enumeration (§T.6).

| Code | Ticket | Family | Recommendation |
|---|---|---|---|
| **A1** | #2013 | object identity | **approve**, option A′ |
| **A2** | #2021 | object identity | **approve** |
| **B** | #2015 | index unit | **approve as a permanent documented deviation** |
| **C1** | #2014 | conversion | approve **with C2 + C3** |
| **C2** | #2016 | conversion | approve **with C1 + C3** |
| **C3** | #2017 | conversion | approve **with C1 + C2**; answer the fallback-surface question |
| **D** | #2020 | format grammar | **approve** (option A) — the only thing that closes CCF-012 |
| **E** | #2019 | Web encoders | **defer** — reference behaviour unverifiable here |
| **F** | #2018 | Unicode tables | **defer** — no data source here |

## T.2 Which decisions are coupled, and which are not

**A1 + A2 must be atomic.** Measured: `EncodingInfo(20127,…).GetEncoding()` returns
`Encoding::UTF8()` **itself** — the shared, publicly mutable singleton. So A2 alone hands out a
different member of the same mutable family, and A1 alone leaves one public door
(`EncodingInfo::GetEncoding`) still handing out an object whose read-only-ness A1 just declared.
They are one decision about one object. **A2's own approval sentence says so explicitly**, and the
`Clone()` that A1 adds reuses the code-page dispatch A2 needs, so implementing them separately
duplicates work.

**C1 + C2 + C3 must be atomic.** Not for tidiness — because each changes the number the others'
tests assert:

- `Latin1Encoding` after C1 needs an encoder fallback for U+0100 and above, which is **inert until
  C3**. C1 alone makes the tests encode the hard-coded `'?'` that C3 exists to remove.
- C2 changes the byte **count** every C1 and C3 expectation is written against, for the two
  factories that emit a BOM.
- C3's "default output is unchanged" requirement is only checkable against a byte stream C2 has
  already settled.

Landing any one alone produces an internally inconsistent partial implementation of "what bytes
come out", which is worse than none.

**D is independent** and must not be grouped with anything. It touches a `modules/core` header, not
`modules/text`'s encodings; it changes a **format grammar**, not a byte encoding; and it is the
only member of the group that closes a cross-cutting family (CCF-012).

**B, E and F are independent of everything**, including each other.

**Not grouped, deliberately:** A with C (different objects, different questions — a read-only
factory and a Latin-1 algorithm have nothing in common but a base class); D with C (a format
grammar is not a character encoding); E with F (both are deferrals, but for different missing
evidence — a reference implementation versus a data table).

## T.3 Family A — object identity (#2013 A1, #2021 A2)

Re-measured 2026-08-04:

```
UTF8() == UTF8()                                  SAME OBJECT
after setDecoderFallbackProperty("<X>") on UTF8(): GetString({0xFF}) = "<X>"   -- for EVERY caller
EncodingInfo(20127,…).GetEncoding()->CodePage      65001   (requested 20127)
                     .GetEncoding() identity       IS the shared mutable singleton
sizeof(Encoding)=40 align=8   UTF8=40 ASCII=40 Unicode=48 UTF32=48 Latin1=40
```

The layout numbers are what make **option A′** the recommendation: there is no free padding byte,
so a `bool isReadOnly_` member costs `sizeof(Encoding)` **40 → 48** for every encoding, and a
virtual `Clone()` costs a vtable slot. Implementing `getIsReadOnlyProperty()` by **identity
comparison against the seven factory instances** and `Clone()` by **code-page dispatch**, both
non-virtual and both defined in `Encoding.cpp`, costs **no data member, no vtable slot and no
layout change**. The full before/after tables, the migration analysis and the rollback are in
`SystemTextApprovalPackage.md` §2 and are not restated.

> **Approval A1.** Approve making `System::Text::Encoding`'s seven factory instances read-only — so
> `setDecoderFallbackProperty` and `setEncoderFallbackProperty` raise
> `System::InvalidOperationException` when called on an object returned by `UTF8()`, `ASCII()`,
> `Unicode()`, `BigEndianUnicode()`, `UTF32()`, `UTF7()`, `Latin1()` or `Default()` — and adding a
> non-virtual `getIsReadOnlyProperty()` and a non-virtual `Clone()` implemented by identity
> comparison and code-page dispatch, so that **no data member, vtable slot or object layout
> changes**; accepting that this repository's own `TextNamespaceTests` migrates to `Clone()` and
> that any downstream caller which configures a factory encoding begins to receive a runtime
> exception rather than a compile error. Ticket **#2013**.

> **Approval A2.** Approve making `System::Text::EncodingInfo::GetEncoding` return the encoding
> matching its own code page — 65001, 20127, 1200, 1201, 12000, 12001, 28591 and 65000 — and raise
> a `System` exception for any other code page, accepting that it no longer returns UTF-8
> unconditionally and that an unrecognised code page begins to throw. **Approve together with A1**:
> measured, the object it returns today *is* `Encoding::UTF8()` itself, so A2 without A1 keeps
> handing out a process-wide mutable singleton under a new name. Ticket **#2021**.

## T.4 Family B — the unit of a public index (#2015)

Unchanged from `SystemTextApprovalPackage.md` §3, which the request does not disturb. The decisive
measured fact stands: `System::String` is a **static helper class over `std::string`**, every index
it takes or returns is a byte offset, and there is no adapter layer in which a unit could be
reinterpreted. Changing the unit inside `System::Text` alone would make `Encoding::GetCharCount`
and `StringBuilder::Length` disagree with `std::string::size()` and with every `System::String`
static — an internal contradiction inside one program, in place of an honest divergence from .NET.

> **Approval B.** Approve **closing #2015 as a permanent, documented deviation** rather than
> implementing it: every public index, length and count in `System::Text` (and in `System::String`,
> `System::Text::StringBuilder` and the rune enumerators) remains a **UTF-8 storage-byte**
> position, accepting that `Encoding::GetCharCount` and `StringBuilder::Length` permanently report
> different numbers than .NET as their doc-comments now state, and that the deviation is recorded
> in `CLAUDE.md`'s "Known permanent deviations" list. Ticket **#2015**.
>
> *(If instead you want .NET's unit, say so and #2015 stays blocked pending a whole-runtime UTF-16
> decision — do not approve a `System::Text`-only change.)*

## T.5 Family C — conversion semantics (#2014 C1, #2016 C2, #2017 C3)

Re-measured 2026-08-04, all four load-bearing rows reproduced:

```
BigEndianUnicode  GetBytes("A") = feff0041          <-- BOM as payload
UTF32             GetBytes("A") = fffe000041000000  <-- BOM as payload
UTF16LE.GetString(FF FE 41 00)  = 41                <-- a real U+FEFF is DROPPED
Latin1.GetBytes(u8"é") = c3a9   (ISO-8859-1 is e9)
Latin1.GetString({0xE9}) = e9   (ill-formed UTF-8)
ASCII + EncoderReplacementFallback("!"): GetBytes(u8"é") = 3f   (hard-coded '?')
UTF8  + EncoderReplacementFallback("!"): GetBytes("\xFF")  = 21   (the one that works)
```

C1 and C2's analyses stand as written in `SystemTextApprovalPackage.md` §4.1 and §4.2. Their
approval sentences are reproduced verbatim at §T.9 below.

### T.5.1 #2017's fallback-surface question — six alternatives, with a fact that reorders them

The encoder fallback surface is byte-shaped and cannot carry a non-ASCII scalar:

```cpp
virtual bool Fallback(char charUnknown, intcs index) = 0;             // EncoderFallbackBuffer
virtual std::vector<bytecs> GetFallbackBytes(char unknownChar) const = 0;  // EncoderFallback
char EncoderFallbackException::getCharUnknownProperty() const;
```

**Two measured facts the previous package does not state, and both narrow the problem:**

1. **The stock replacement path ignores the argument entirely.** Both
   `EncoderReplacementFallback::GetFallbackBytes(char)` and
   `EncoderReplacementFallbackBuffer::Fallback(char, intcs)` take an **unnamed** parameter, and
   measured, `GetFallbackBytes('A')` and `GetFallbackBytes('\xC3')` return the same `21`. The
   replacement text is stored as a `std::string`, so a **multi-byte replacement already works**.
   So the `char` defect affects only `EncoderExceptionFallback`'s message and
   `getCharUnknownProperty()`, plus any user subclass that inspects the argument — measured, a
   custom subclass today sees the raw storage bytes `c3 a9`, never the scalar.
2. **The decoder direction is already correctly shaped.** `DecoderFallback::GetFallbackString`
   takes `(const bytecs*, intcs)` and `DecoderFallbackBuffer::Fallback` takes
   `(const std::vector<bytecs>&, intcs)`. Routing ill-formed **byte sequences** through the decoder
   fallback therefore needs **no signature change of any kind**.

| # | Alternative | Public source impact | Vtable / ABI | Migration | Representable input domain | Exception behaviour | Tests required |
|---|---|---|---|---|---|---|---|
| 1 | **Keep `char`, document the limit** | none | none | none | one storage byte | unchanged | the existing pins only |
| 2 | **Add an overload** taking a scalar (`char32_t`), the `char` one delegating | additive only; existing subclasses still compile | **new vtable slots** (one per added virtual) | none required; subclasses opt in | full U+0000–U+10FFFF via the new overload | an exception fallback can report the true scalar when it overrides the new form | both overloads per fallback kind; a subclass overriding only the old form still works |
| 3 | **Replace the virtual signature** (`char` → `char32_t`) | **source break** for every external subclass | vtable **and** mangled-symbol change | every subclass edits its override | full | correct scalar everywhere | full matrix; a negative consumer fixture proving the old spelling no longer compiles |
| 4 | **Encode the scalar as a surrogate pair** and report it in two `Fallback` calls | none | none | none | full **only for BMP+supplementary via two calls**; a subclass that handles one call in isolation still sees half a character | an exception fallback throws twice or on the high half — ill-defined | a case per plane; this is .NET's `CharUnknownHigh`/`CharUnknownLow` shape, which the port already recorded as a deliberate reduction |
| 5 | **Replace internally, never exposing the scalar** — the encoding consults `getMaxByteCountProperty()`/the replacement bytes without reporting a character | none | none | none | n/a — nothing is reported | an **exception** fallback can no longer say *what* failed, only *that* it did | the replacement matrix; the exception path loses its message content |
| 6 | **Decoder direction only** (ill-formed sequences), encoder direction unchanged | **none** — the decoder surface is already byte-vector-shaped (fact 2) | **none** | none | full, for the decoder | a configured decoder exception fallback starts throwing where it was inert | the truncated-trailing-unit matrix per encoding |

**Recommendation, changed from the previous package by fact 1.** The previous package recommended
"B if a source break is acceptable, otherwise C" — i.e. alternatives 3 or 6 here. Given that the
**default** replacement path is unaffected by the `char` width, alternative **1 + 6** is now the
better bounded first step: route everything through the configured fallbacks in both directions,
keep the `char` reporting surface, and **document** that the reported character is a storage byte.
That closes SR-AUD-293 entirely and SR-AUD-292 for every default configuration, with **no vtable
change, no source break and no migration**, leaving only "what an exception fallback's message
says" for a later, smaller decision. Alternative **2** is the correct end state if the reported
character must be right; alternative **3** is not recommended (a source break for a message
string); **4** and **5** are recorded and not recommended.

## T.6 CCF-012 — independently re-enumerated

The request asks for this to be re-derived, not restated. Done by exhaustive search of `modules/`
on 2026-08-04.

**Every file that scans a brace character** (16 files), classified:

| File | What it scans | Composite format? |
|---|---|---|
| `core/include/System/detail/CompositeFormat.hpp` | `runCompositeFormat` | **YES — implementation 1** |
| `text/include/System/Text/CompositeFormat.hpp` | `countPlaceholders` | **YES — implementation 2** |
| `core/src/System/Guid.cpp` | `{…}` Guid format specifiers | no — a different grammar |
| `text-regular-expressions/.../Match.hpp` | `${name}` substitution | no |
| `xml-linq/.../XName.hpp` | `{ns}local` | no |
| `collections/.../DictionaryEntry.hpp` | writes `"{" + … + "}"` | no — output only |
| `core/include/System/String.hpp` | a **comment** about `{` | no — not code |
| `globalization/.../IdnMapping.cpp`, `net/.../WebHeaderCollection.cpp`, `text/.../UTF7Encoding.hpp` | `{` inside an ASCII character-class range | no |
| `xml-linq/.../XAttribute.hpp`, `XElement.cpp`, `XStreamingElement.cpp` | comments about `{`/`}` not being legal XML Names | no |
| `text-json/.../JsonTokenType.hpp`, `Utf8JsonWriter.cpp` | JSON object braces | no |

**Production scanners: 2. Formatting engines: 1** (`runCompositeFormat`). **Validation-only
parsers: 1** (`countPlaceholders`). **Call sites:** every `String::Format` (×22),
`StringBuilder::AppendFormat` (×11), `Console::Write`/`WriteLine` (×11) and
`FormattableString::ToString` delegates to `runCompositeFormat` — verified, the only other
reference to that symbol in `modules/text/include/System/Text/CompositeFormat.hpp` is a **comment**.
`CompositeFormat::Parse` is the sole caller of `countPlaceholders`.

**Acceptance differences, re-measured with grammar rejections separated from index-out-of-range**
(the previous package could not separate them through the public door; this probe reads the
exception message):

| Format | `Parse` today | Shared grammar | Direction |
|---|---|---|---|
| `{0,not-a-width}` | ok | **grammar reject** ("Expected an ASCII digit") | **narrows** |
| `{0,-}` | ok | **grammar reject** | **narrows** |
| `{0 }` | **FormatException** | **accepted** | **WIDENS** |
| `{0  ,5}` | **FormatException** | **accepted** | **WIDENS** |
| `{999999}`, `{1000000}`, `{9999999}`, `{000000000001}` | ok | **grammar-accepted** (only index-out-of-range) | same |
| `{10000000}`, `{2147483646}` | ok | **grammar reject** ("Format item ends prematurely") | **narrows** |
| `{-1}`, `{0:{1}}`, `Hello {`, `a } b` | FormatException | grammar reject | same |

So `SystemTextNamespaceReviewPlan.md` §14.8's *"any index at or above 1,000,000 begins to throw"*
is confirmed **false**, and adoption genuinely changes acceptance in **both** directions.

**Exception ordering and messages.** Both engines raise `System::FormatException` and both check
the grammar left-to-right, so **ordering does not change**. **Messages do**: `Parse` emits the bare
`"Input string was not in a correct format."` while the shared engine appends
`" Failure to parse near offset N. <reason>"` — measured above. That is a message change on every
rejected input, and it is an improvement, but it must be stated.

**Can the shared scanner be non-rendering?** It must be. `runCompositeFormat` takes an `argCount`
and a `render` callback, raises index-out-of-range against `argCount`, and **executes alignment
padding while it runs** — `Parse("{0,1000000}")` through it would allocate a megabyte of spaces to
validate a string. So #2020 is **not** "call `runCompositeFormat`": it must extract a
`scanCompositeFormat(format, onItem)` into `System::detail` that neither renders nor pads, and have
both the formatter and `Parse` use it. That is a refactor of a `modules/core` header on which all
45 formatting entries depend, and its behaviour-preservation must be proved by re-running #1884's
3,675-case `String::Format` corpus unchanged.

**Does #2020 close CCF-012?** **Yes, and only under option A.** The population is exactly the two
implementations above; CCF-012's own closing condition requires *"a shared parsed-token model or a
deliberately narrow documented formatter"*, which hand-aligning two engines (option B) does not
satisfy, and which documenting the divergence (option C) satisfies only as a declared deviation.
**CCF-012 is not marked closed by this document**, and must not be marked closed by design work.

> **Approval D.** Approve closing CCF-012 by extracting the composite-format **scanner** from
> `System::detail::runCompositeFormat` into a shared, **non-rendering**
> `System::detail::scanCompositeFormat` used by both the formatter and
> `System::Text::CompositeFormat::Parse`, so that one grammar is stated once — accepting that
> `Parse` begins to **reject** `{0,not-a-width}`, `{0,-}`, `{10000000}` and every index above
> 9,999,999 (including `{2147483646}`, which succeeds today), begins to **accept** `{0 }` and
> `{0  ,5}`, which it rejects today, and begins to emit the shared engine's richer
> `"…Failure to parse near offset N…"` message text on every rejection; and accepting a
> behaviour-preserving refactor of a `modules/core` header on which all 45
> `String::Format`/`AppendFormat`/`Console::Write` entries depend, proved by re-running #1884's
> 3,675-case corpus unchanged. Ticket **#2020**.

## T.7 Family E — the Web encoders (#2019)

Unchanged: **defer**. .NET's exact default allow-list, the exact escape spelling (`&#xNNNN;` vs
`&#NNNN;`) and the supplementary-scalar spelling are **not verifiable in this container**, and this
package does not treat prior knowledge of .NET as evidence. Either take the bounded, self-evidenced
subset (escape non-ASCII only, leave the ASCII set alone) or leave #2019 blocked until a reference
tree exists.

> **Approval E.** *(Recommended answer: **defer**.)* Approve giving `System::Text::Encodings::Web`'s
> default HTML and JavaScript encoders a Basic-Latin allow-list that escapes every non-Basic-Latin
> scalar, plus a `Create(UnicodeRange…)` opt-in for the current pass-through — **and state which
> allow-list**, because .NET's exact default set cannot be verified here and must not be
> implemented from memory. Ticket **#2019**.

## T.8 Family F — Unicode classification (#2018)

Unchanged: **defer**, or take the bounded half. There is no Unicode data source in this container,
and a hand-written approximation is worse than a declared reduction. The bounded half is real and
self-evidenced: this port's white-space table contains **U+FEFF**, which .NET excludes.

> **Approval F.** Approve implementing real Unicode category and simple case mapping in
> `System::Text::Rune` — including the generated table, its data source, its attribution, and a
> **stated Unicode version with a policy for updating it** — accepting that every non-ASCII
> classification and casing result changes, and that `Rune::IsWhiteSpace` also changes because this
> port's table wrongly contains U+FEFF. Ticket **#2018**.
> *(Recommended alternative if no Unicode data source is available: approve only the U+FEFF
> correction plus a class-level statement of the ASCII scope, and keep #2018 blocked.)*

## T.9 Family C's approval sentences, verbatim

> **Approval C1.** Approve making `System::Text::Latin1Encoding` convert Unicode scalar values
> rather than UTF-8 storage bytes — `GetBytes` emitting one ISO-8859-1 byte per scalar
> U+0000–U+00FF and the configured encoder fallback for every other scalar, `GetString` decoding
> each byte to the scalar of the same value — accepting that every non-ASCII conversion produces
> different bytes than today and that data previously produced by this class cannot be
> round-tripped through the new code. Ticket **#2014**.

> **Approval C2.** Approve (a) removing the byte-order mark from `UnicodeEncoding::GetBytes` and
> `UTF32Encoding::GetBytes`, changing the bytes **both** `Encoding::BigEndianUnicode()` and
> `Encoding::UTF32()` produce for every input; (b) making `GetString` stop consuming a leading
> U+FEFF, so a real ZERO WIDTH NO-BREAK SPACE is decoded instead of discarded; and (c) adding a
> **virtual** `GetPreamble()` to `System::Text::Encoding`, accepting the resulting vtable layout
> change and the full recompilation it forces on every consumer. Ticket **#2016**.
> *(Answer (c) separately if you prefer the non-virtual per-class spelling, which avoids the vtable
> change but cannot be reached through an `Encoding&`.)*

> **Approval C3.** Approve making every `System::Text` encoding route unencodable characters and
> ill-formed byte sequences — including an incomplete trailing UTF-16 or UTF-32 unit — through its
> configured `EncoderFallback`/`DecoderFallback`, so a configured exception fallback throws, and
> requiring each concrete encoding's constructor to install the replacement its loop hard-codes
> today so that default output is unchanged. **Answer separately, choosing from §T.5.1's six
> alternatives:** what happens to the reported character, given that the stock replacement path
> ignores it entirely and the decoder surface is already byte-vector-shaped, so the recommended
> **1 + 6** needs no vtable change and no source break at all. Ticket **#2017**.

## T.10 Compatible portions in the Text package

**None found**, re-tested this batch against every one of #2013–#2021: each changes a **defined,
currently-observable** result on valid input, except #2013 which changes a thread-safety guarantee.
The one that did exist — the missing behaviour pins — was found and shipped by #2022. This
conclusion is unchanged and was re-derived, not inherited.

## T.11 Where this document differs from `SystemTextApprovalPackage.md`

| Topic | Previous package | Here |
|---|---|---|
| #2017's alternatives | four (A–D), recommending B-or-C | **six**, and the recommendation changes to **1 + 6** because the stock replacement path measurably **ignores** the `char` argument and the decoder surface is already byte-vector-shaped |
| CCF-012 acceptance table | grammar rejection and index-out-of-range not separable through the public door | **separated by exception message**; every row confirmed, and the message-text change on rejection is added to Approval D |
| A1 + A2 coupling | stated in A2's sentence | promoted to an explicit **atomicity** requirement (§T.2) |
| Everything else | — | **verified unchanged and adopted** |

---

# The grouped checklist

Answer at whatever granularity you like; each line stands alone unless it says otherwise.

```
=== System::Diagnostics ===

D-A  #2029 + #2032   Detach the pipe readers in reapIfNeeded AND the destructor, with
                     shared buffer ownership; reap best-effort with WNOHANG. Five public
                     doors stop blocking; late output stops being observable.  RECOMMENDED
                     (sub-question: should the INFINITE WaitForExit() keep joining?)
D-B  #2030           Both captured-output getters return std::string BY VALUE, plus an
                     internal mutex. TWO PUBLIC RETURN TYPES CHANGE.            RECOMMENDED
D-C  #2031           Kill(true) walks /proc for the transitive descendant set on Linux;
                     killpg elsewhere, documented. Kills more than today.       RECOMMENDED

=== System::Text ===

A1   #2013           Read-only factory encodings, by identity (no layout/vtable change),
                     plus IsReadOnly and Clone.                    RECOMMENDED — with A2
A2   #2021           EncodingInfo::GetEncoding resolves its own code page and throws for
                     an unimplemented one.                         RECOMMENDED — with A1
B    #2015           Public indices stay UTF-8 bytes, permanently and by decision,
                     recorded in CLAUDE.md.                        RECOMMENDED
C1   #2014           Latin-1 converts scalars, not storage bytes.  with C2 + C3
C2   #2016           No BOM in GetBytes (BigEndianUnicode AND UTF32), no BOM stripping in
                     GetString, virtual GetPreamble.               with C1 + C3
C3   #2017           Every encoding routes through its configured fallback.
                     SEPARATE QUESTION: pick one of the six alternatives in T.5.1
                     (recommended: 1 + 6 — no vtable change, no source break).
                                                                   with C1 + C2
D    #2020           One shared, non-rendering composite-format scanner; Parse both
                     narrows and widens, and its messages change.  CLOSES CCF-012.
                                                                   RECOMMENDED
E    #2019           Default Web encoders' allow-list.             DEFER — no evidence
F    #2018           Real Unicode tables in Rune.                  DEFER — no data source
```

Example answers this checklist is designed to accept:

```
Approve Diagnostics D-A and D-C.  Reject D-B.
Approve Text A1+A2 and D.  Reject Text C.  Defer E and F.
Approve Text C3 alternative 2 rather than 1+6.
```

**Nothing in this document is implemented by writing it, and no approval is assumed.** Every ticket
named here remains `blocked` in `plan.sqlite3`.
