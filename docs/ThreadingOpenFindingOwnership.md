<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/threading` — strict ownership of every open audit finding

> **Historical ownership snapshot (2026-08-12).** The open/blocked counts and owner states below
> describe that checkpoint, not current HEAD. All audit findings were subsequently reconciled;
> current dispositions are in `docs/AuditFindingsReconciliation.md` and current ticket state is
> authoritative only in `plan.sqlite3`.

Ticket **#2342**, 2026-08-12. Companion to `docs/ThreadingNamespaceReviewPlan.md`; it does
not replace that plan, it records who owns the *remaining* work after the plan's own tickets
#1947–#1955, #1971 and #2341 have run.

## 1. Method

Counted directly from `audit/AUDIT_FINDINGS_INDEX.md`, by locating each row's `SR-AUD-###`
identifier and its link target rather than by a fixed table column, and keeping the rows whose
link target lives under `modules/threading/`. Ownership was then taken from `plan.sqlite3`,
searching every ticket's title, description, acceptance criteria and notes for the identifier.

A ticket mentioning a finding is **not** its owner. Excluded on that ground:

| Excluded mention | Why |
|---|---|
| #1766 | the repository-wide audit that *discovered* every finding |
| #1950 | the namespace review that converted the findings into a ticket queue; completed |
| #1949, #1951, #1953, #1954, #1955, #1971, #2341 | completed remediation tickets that closed a *different* finding or a *different half* of a split one |
| #2134 (`net-sockets`), #1979 (`runtime`) | cross-references naming a threading finding as the same *cause* in another module |

## 2. The count

At the start of this batch: **17** open. `SR-AUD-202` was remediated by #2341 in this same
batch, so **16** remain open. Every one of the 16 has exactly one meaningful current owner;
**none is unclaimed**, and **none is implementation-ready**.

| Finding | Sev | Owner | Owner state | What actually blocks it |
|---|---|---|---|---|
| SR-AUD-187 | high | #1959 | blocked | §9 q3 — taking ownership of a borrowed `IThreadPoolWorkItem*` is a **public source break** |
| SR-AUD-221 | high | #1959 | blocked | §9 q3 — same, for `SetSynchronizationContext` |
| SR-AUD-203 | high | #1956 | blocked | §9 q1 — dispose-while-held must start throwing; the *race* half was closed by #1955 |
| SR-AUD-204 | high | #1957 | blocked | §9 q2 — new waiting-writer field, plus the writer-preference fairness change |
| SR-AUD-210 | high | #1957 | blocked | lock discipline around the post-phase action; **the layout half is now answered** — see §4 |
| SR-AUD-191 | medium | #1956 | blocked | §9 q1 — `ITimer::Change` after `Dispose` must return `false` |
| SR-AUD-208 | medium | #1956 | blocked | §9 q1 — three `Close()` bodies start throwing, and each gains a flag that may grow `sizeof` |
| SR-AUD-219 | medium | #1956 | blocked | §9 q1 — `IsValueCreated` after `Dispose`; the *factory* half was closed by #1951 |
| SR-AUD-201 | medium | #1957 | blocked | §9 q2 — new in-flight-consumer field, **and** an unverified .NET question (`/rv` absent) |
| SR-AUD-193 | medium | #1958 | blocked | group B — distinct `ManagedThreadId`s for external threads |
| SR-AUD-194 | medium | #1958 | blocked | group B — a second callback shape → layout |
| SR-AUD-196 | medium | #1958 | blocked | group B — non-public constructors: a source break that retires this repository's own tests |
| SR-AUD-220 | medium | #1958 | blocked | group B — `ThreadLocal::Values` → layout plus a cross-thread lifetime contract |
| SR-AUD-215 | medium | #1958 | blocked | needs a `Capture()` that returns a real context first — an ownership/lifetime design decision (#1971 §21.1) |
| SR-AUD-209 | medium | #1958 | blocked | §9 q4 — a vtable on two types that have none, **and** a return-type change |
| SR-AUD-200 | medium | #1963 | todo (inactive) | reference-dependent: `/rv/tmp/runtime/src/libraries/` is absent, and #1963 forbids guessing |

Severity of the 16: **5 high, 11 medium**. Row status of all 16: plain `confirmed`.

## 3. The stale historical claim, checked

An earlier context recorded that `modules/threading` had **six HIGH findings, all already
claimed by blocked design tickets**. Re-derived from the live repository, that claim was
**true**, and is now one finding out of date:

- the six highs were SR-AUD-187, 202, 203, 204, 210 and 221;
- their owners were #1959, #1957, #1956, #1957, #1957 and #1959 — all blocked, all
  design-complete on 2026-08-03;
- SR-AUD-202 was the single member any of those designs had *pre-authorised* for a split
  (`ThreadingNamespaceReviewPlan.md` §20.2 item 1), and #2341 took it. **Five highs remain.**

So the correction is not that the claim was wrong, but that it was never a statement that
nothing could be done: one of the six carried its own written permission, and reading the
design record rather than the ticket status is what found it.

## 4. How many are implementation-ready? **Zero.**

After #2341 there is no threading finding whose repair can land under existing authority.
Every remaining one crosses at least one of: a §9 approval question, a public source break, a
new private field on a type with real object layout, an unresolved ownership/lifetime design,
or an unavailable .NET reference.

The nearest miss is **SR-AUD-210**, and #2341 narrowed it by measurement rather than leaving it
"to be determined":

- `sizeof(Barrier)` / `alignof` are **160 / 8** and would stay there — the "phase is finishing"
  state §20.2 asks for already exists as `actionCallerId_`;
- so §9 q2's *layout* rationale does not reach `Barrier` at all;
- what remains is a pure **synchronisation-guarantee** question — may the barrier's lock be
  observably released for the duration of a user callback — which §20.2's approval sentence
  asks by name and which is the user's to answer.

That is the difference between it and `Monitor`: SR-AUD-202's repair only turned a call that
never returned into one that returns, and changed no guarantee any working program could
observe.

## 5. One bookkeeping observation, deliberately not acted on

All 16 open threading rows carry the plain `confirmed` status, although #1956–#1959 — which
own 15 of them — each record "DESIGN COMPLETE 2026-08-03". By the index's own definition those
rows would qualify for the `confirmed (design-complete)` qualifier. Re-labelling them would
move the repository buckets from 90/62 to roughly 75/77, which is an accounting change with
repository-wide consequences and a judgement about whether a design ticket that *is* its own
blocked implementation ticket satisfies the qualifier's second clause. It is recorded here and
left alone.
