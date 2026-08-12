<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/runtime` — strict ownership of every open audit finding

Companion to `docs/ThreadingOpenFindingOwnership.md`, produced by the same method on
2026-08-12. It answers one question for `modules/runtime`: of the open findings, how many
carry work a session could implement **now**, without a user decision and without evidence
this environment cannot supply?

The answer is **zero**, and the two repairs this batch landed (#1986, #1985) are *not*
audit findings — see §5.

---

## 1. Method

Findings are attributed to a module by the **report path** recorded in
`audit/AUDIT_FINDINGS_INDEX.md`, parsed by identifier rather than by column position, not
by the free-text `Source` column (which carries bare file names like
`RuntimeInformation.cpp` that are ambiguous across modules).

Ownership is **strict**. A ticket does not own a finding merely because its text contains
the `SR-AUD-*` identifier. Discovering-audit mentions, cross-references, sequencing notes,
"not a family with …" disclaimers and completed historical verifications are excluded; the
owner is the ticket that carries the **remaining** work.

---

## 2. The count

`modules/runtime` has **14** open findings — 1 high, 12 medium, 1 low; 12 of them
`confirmed (design-complete)` and 2 plain `confirmed`. Every one is individually owned and
**none is unclaimed**:

| Owner | State | Findings | Count |
|---|---|---|---|
| **#1979** (cause R-C) | blocked — approval-gated | SR-AUD-171 | 1 |
| **#1980** (cause R-G) | blocked — approval-gated | SR-AUD-152, 153, 158, 159, 160, 163, 164, 165, 166, 167 | 10 |
| **#1981** (cause R-H) | blocked — approval-gated | SR-AUD-161 | 1 |
| **#1983** (cause R-K) | blocked — deferred verification | SR-AUD-154 | 1 |
| *no ticket* (cause R-L) | CLAUDE.md permanent deviation | SR-AUD-168 (structural half) | 1 |
| | | **total** | **14** |

`docs/SystemRuntimeNamespaceReviewPlan.md` §8 is the authority for the cause map; §10 holds
the three approval sentences, §14 the deferred-verification blockers, §15 the exclusions.

---

## 3. Why none of them is implementation-ready

* **#1979, #1980, #1981** each terminate in a written **approval sentence** in §10 that the
  user has not answered. #1979 additionally rests on a reference basis — a reading of
  `pal_signal.c` taken when `/rv/tmp/runtime/src/libraries/` was present — that is **absent
  in this environment** and carries no managed probe. The repository has already drawn that
  line: #1968 landed a behaviour-incompatible repair because SR-AUD-233 *had* a managed
  probe; #1963 changed nothing because SR-AUD-200 had none. SR-AUD-171 is on #1963's side.
* **#1983** is blocked on three independent absences — no Windows toolchain, no
  mixed-bitness Windows host, and no reference tree to confirm the `IsWow64Process2`
  mapping. Nothing is implemented from recollection.
* **SR-AUD-168's structural half** is an already-classified permanent deviation (P/Invoke
  and COM interop are out of scope). Its *disclosure* half was closed by #1978.

---

## 4. The `ThreadingNamespaceReviewPlan.md` §22 candidate, checked and collapsed

§22 (2026-08-03) corrected §1's reason for deferring `runtime` and described what the
namespace actually contains as **"three high-severity POSIX signal-handling defects in one
`.cpp` body"**: the destruction of process-wide signal policy, a blocking write inside a
raw signal handler, and a job-control stop on mere observation.

Re-measured on 2026-08-12, that grouping **no longer describes an open family**:

| §22 member | Identifier | Status now | Closed by |
|---|---|---|---|
| destruction of process-wide signal policy | SR-AUD-169 | **remediated** | #1975 |
| blocking write inside a raw signal handler | SR-AUD-172 | **remediated** | #1974 |
| job-control stop on mere observation | **SR-AUD-171** | **confirmed (design-complete)** | — approval-gated, #1979 |

Two of the three were repaired between §22 being written and this review. The single
survivor is precisely the member that needs the user's decision. §22 is historically
accurate and is **not** edited; it simply predates #1974 and #1975.

The grouping was also **topical rather than causal**. The review plan had already split
these into three separate causes — R-A (SR-AUD-169), R-B (SR-AUD-172), R-C (SR-AUD-171) —
which is why they were remediable independently and on different dates. They shared a
translation unit, not an invariant.

---

## 5. What was implementable in that body, and why it is not an audit finding

Two **post-audit** defects were recorded against `PosixSignalRegistration.cpp` while #1974
was being written, both with ordinary ticket numbers and **no `SR-AUD-*` identifier**
(numbering is frozen at 364). Both were `todo` and unblocked, and both are repaired by this
batch:

* **#1986** — `dispatchSignal()` copies each handler into a snapshot, releases the mutex,
  then invokes; `Dispose()` erases the entry but the *pending* invocation still runs, so a
  handler capturing the caller's frame touched dead stack. ASan reported
  `stack-use-after-scope` before and nothing after. See §26 of the review plan.
* **#1985** — the self-pipe descriptors were inherited across `exec()`. See §27.

Neither closes an audit row. `modules/runtime` still has **14** open findings and the
repository buckets are unchanged at **213 remediated / 89 confirmed / 62 confirmed
(design-complete)**.

The distinction matters for the ranking in §2: a module whose audit findings are all
approval-gated is *not* necessarily a module with no work in it. It was the post-audit
ticket list, not the audit index, that held this batch's work.

---

## 6. One number worth carrying forward

Counting only what a session can act on without a user decision:

| | open | owned | unclaimed | blocked (ticket) | permanent deviation | todo/deferred | **implementation-ready** |
|---|---|---|---|---|---|---|---|
| `modules/runtime` (audit findings) | 14 | 14 | 0 | 13 | 1 | 0 | **0** |
| `modules/runtime` (post-audit tickets) | — | — | — | 0 | 0 | 0 | **0** *(both cleared by this batch)* |

The 13 ticket-blocked rows are #1979 (1), #1980 (10), #1981 (1) and #1983 (1). The
fourteenth is SR-AUD-168's structural half, which has **no** ticket because a CLAUDE.md
permanent deviation covers it — neither ready nor blocked on anything that will change.
