<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# SR-AUD-098 — `AggregateException` causal diagnostics and `Flatten` ordering

*Review record for ticket **#2306**. This is the durable evidence for the
SR-AUD-098 decomposition and for the tickets it produced: **#2307** (clause C1,
landed), **#2308** (clause C5, landed with a defect), **#2310** (the corrective
ticket for that defect), and **#2309** (clauses C2/C3/C4/C6, deferred,
`needs_user`).*

**This document creates no `SR-AUD-*` identifier** — audit numbering is frozen at
364 — and it **does not mark SR-AUD-098 remediated**. SR-AUD-098 is a
conjunction; the clauses that close in this batch are C1 and C5, and C2/C3/C4
(and therefore C6) remain open, so the finding stays `confirmed`.

`/rv/tmp/runtime/src/libraries/` is **not present in this environment.** Every
.NET statement below is quoted from the frozen finding text itself, which is
repository-owned recorded evidence; nothing is recalled from memory. Where the
frozen text does not settle a question, this document says so and defers rather
than guessing.

---

## 1. The frozen finding, split into clauses

The finding
(`audit/modules/core/include/System/AggregateException.hpp.audit.md`,
§SR-AUD-098) is a single paragraph asserting several independent things. Split
by assertion, preserving its wording:

| Clause | Frozen assertion | Kind |
|---|---|---|
| **C1** | "The message-plus-inner/vector constructors neither set the base `Exception::innerException_` to the first inner exception…" | local contract |
| **C2** | "…nor append contained messages to `what()`. ….NET… includes each inner diagnostic in `Message`." | exact .NET text |
| **C3** | "`Handle` rebuilds unhandled errors with the generic default text" | message preservation |
| **C4** | "`Flatten` likewise discards the caller's message" | message preservation |
| **C5** | "Its recursive depth-first `collectLeaves` emits nested leaves before a later direct leaf (`a,b,c`), where current .NET's queue algorithm returns direct leaves before queued nested leaves (`c,a,b`)." | ordering |
| **C6** | "The green test `MessageAndSingle_Ctor_StoresInner` locks the incomplete C++ text rather than the .NET-shaped causal message." | test consequence of C2 |

These are **not** one root cause. C1 is a base-class contract violation, C5 is a
traversal-order defect, and C2/C3/C4 are one interlocking message problem. They
were therefore taken as separate tickets, which is what let C1 and C5 land while
C2/C3/C4 stayed blocked.

---

## 2. What is internally provable

### C1 — decidable with no .NET reference at all

`System::Exception` documents `getInnerExceptionProperty()` as "the exception
that caused the current exception". Every one of `AggregateException`'s six
constructors left that field null while `getInnerExceptionsProperty()` held the
causes: an aggregate that stored *N* causes reported none. That is a violation of
**this port's own base-class contract**, decidable from repository-owned
documentation, and needs no external authority. Ticket **#2307**.

*Premise extension, measured per constructor rather than inherited:* the frozen
text names only the "message-plus-inner/vector constructors". The plain vector
and initializer-list constructors had the identical gap. All six were measured;
the four that store a cause now name it, and the two that store none stay null.

### C5 — decidable from the frozen finding's own recorded evidence

C5 is the one ordering question that does **not** need `/rv`, because the frozen
finding already records both the algorithm ("current .NET's queue algorithm
returns direct leaves before queued nested leaves") **and** the exact expected
output (`c,a,b` for `{ Aggregate{a,b}, c }`). That is authoritative recorded
evidence, used exactly as scoped: the repair implements a FIFO queue and the
tests pin `c,a,b` for that shape and nothing wider. Ticket **#2308**.

Ordering was **not** guessed. Had the frozen text recorded only "different leaf
order" with no expected output, C5 would have been deferred alongside C2/C3/C4.

---

## 3. What is blocked on unavailable reference semantics — #2309

C2, C3 and C4 cannot be closed here, and C6 follows C2.

- **C2** requires the exact composed `Message` text .NET produces — wording,
  punctuation, separator and ordering of the contained diagnostics. The frozen
  finding states only *that* .NET "includes each inner diagnostic in `Message`",
  never what the composition looks like. This port's own default composition
  (`"One or more errors occurred. (a) (b)"`) is a local invention; whether .NET's
  custom-message form appends in the same shape is exactly the unavailable fact.
- **C3/C4** require `Handle` and `Flatten` to preserve the caller's message. The
  blocker is structural, not textual: this class has **no raw-message field**. It
  stores only the composed `what()` string, so a constructor that composed
  `"m (a) (b)"` cannot later be asked for `"m"` to rebuild a derived aggregate
  from. Preserving the message through `Handle`/`Flatten` and composing contained
  messages into it are therefore not independent — satisfying both needs a new
  member holding the raw message, which is a public representation change.

C2/C3/C4 are consequently **one decision, not three**, and that decision is
approval-bound on two counts: it invents message text with no available
authority, and it changes the object's representation. Ticket **#2309**,
`needs_user`.

**What must not be guessed:** the composed text, the separator, and whether the
raw message is retained in .NET's derived aggregates. Writing a plausible string
here would present invention as parity.

---

## 4. Ticket #2310 — the defect #2308 shipped

**#2308 landed a `Flatten()` that does not terminate.** It is recorded here
because the ordering repair itself is correct and worth keeping; only the
subscript was wrong.

The queue is walked by index rather than popped, which is fine. But #2308 read
it as:

```cpp
const std::vector<std::exception_ptr> current = pending[pending.size() - 1 - head];
```

`pending` grows by one entry for every list enqueued. On any aggregate holding a
nested aggregate, `pending.size()` and `head` therefore advance in lockstep and
the index stays pinned at `0`: the seed list is re-walked forever, appending its
leaves to the result and a fresh copy of the nested list to the queue on every
pass. The loop condition `head < pending.size()` never fails, because the body
keeps extending what it is testing against.

Measured directly (`build-probe/2309_flatten_defect.cpp`, run under `timeout 25`
and `ulimit -v`):

| Shape | #2308 as shipped | After #2310 |
|---|---|---|
| `{a, b}` — already flat | `a,b` | `a,b` |
| `{a, Aggregate{b,c}}` — the *control* | **did not terminate** | `a,b,c` |
| `{Aggregate{a,b}, c}` — the finding's shape | not reached | `c,a,b` |

The second row is the notable one: that shape is the control #2308's own test
named "IsUnchanged". The hang reaches **every caller of `Flatten()` on a nested
aggregate**, which is the only case `Flatten()` exists for.

The repair is `pending[head]`. Termination then holds because each iteration
consumes exactly one list and each nested aggregate contributes exactly one, over
a finite tree.

**Why the tests did not catch it:** they would have. #2308's own order tests
cover the hanging shapes, and the test executable hangs rather than passes. The
tests were never run — #2308's commit message asserts a validation that did not
happen. #2310 adds two assertions on exact leaf multiset and count, so that a
future queue which terminates but re-walks a list fails fast instead of hanging.

---

## 5. Compatibility

Nothing in C1 or C5 changes a public signature, overload, base, member, layout,
vtable, `noexcept` specification, exported symbol or module edge.
`AggregateException` is header-only; `sizeof`/`alignof` stay 192/8.

Behavioural transitions, both required by the frozen finding:

1. **C1** — an aggregate that stores causes now reports its first cause through
   the inherited `getInnerExceptionProperty()` instead of null. No first-party
   production code reads that property on an aggregate; every use in the
   repository is in a test tree, and none pinned it as null. The two
   in-repository producers, `CancellationTokenSource` and `Parallel`, use the
   plain vector constructor.
2. **C5** — `Flatten()` returns the same leaves, with the same identities and the
   same count, in queue order rather than depth-first order. Shapes on which the
   two agree — an already flat aggregate, and any aggregate whose nested entries
   all follow its direct leaves — are byte-identical. Because the default message
   is composed from the leaves in leaf order, a flattened aggregate's default
   message lists the same leaves in the new order; that is a consequence of C5,
   not an independent message decision, and C2/C3/C4 stay untouched.

C1's validation order moved earlier in fact and is unchanged in effect: the
null-element scan now runs in the base-class initializer, sequenced before every
member initializer, so ticket #1807's `ArgumentException` /
`ArgumentNullException` split, both message texts and the `paramName` survive
verbatim.

---

## 6. Disposition

SR-AUD-098 stays **`confirmed`**. C1 and C5 are closed; C2, C3, C4 and C6 are
not, and the finding is a conjunction. Marking it remediated would claim a
message parity this repository cannot currently establish.
