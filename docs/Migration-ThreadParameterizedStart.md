<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Thread::Start(void*)` forwards its parameter (ticket #1958, SR-AUD-194)

*2026-08-19.* `System::Threading::Thread` gains a constructor taking
`std::function<void(void*)>`, and `Start(void*)` now delivers its argument to it — or throws if
the thread was built with the parameterless shape.

**`sizeof(Thread)` grows 104 → 136, so consumers must be recompiled.** Landed under **SA-5** with
**SA-3**'s layout condition discharged.

---

## 1. What was wrong

`Start(void*)` captured its argument and then discarded it with a literal `(void)parameter;`,
while the member's own doc-comment said the value was *"forwarded to the thread function"*. There
was no way for a caller to receive it and no diagnostic saying so — because the only accepted
callback shape had **no parameter slot**.

## 2. The recorded cost estimate was wrong

#1958's record listed SR-AUD-194 as *"a signature change"*, which would route it through SA-10
and SA-2's five conditions. **Measured, no existing signature changes.** The repair is:

* **additive** — a second constructor, the counterpart of .NET's
  `Thread(ParameterizedThreadStart start)` (`Thread.cs:152`);
* **a behaviour change** to `Start(void*)`, which is SA-5's ordinary territory.

So it landed as ordinary work with a pinned layout, not as a source break. Nothing that compiled
before compiles differently: a no-argument lambda converts only to `std::function<void()>`, a
`void*`-taking lambda only to `std::function<void(void*)>`. The one spelling that would become
ambiguous is `Thread(nullptr)`, and it exists in **zero** places across `modules/`, `test/`, `cna`
and `mobile-eggbert`.

## 3. What changes

| | Was | Is |
|---|---|---|
| `Thread(std::function<void(void*)>)` | **absent** | present; empty callable → `ArgumentNullException` |
| `Start(p)` on a **parameterized** thread | — | the body receives `p` |
| `Start(p)` on a **parameterless** thread | silently ignored `p`, ran the body | `InvalidOperationException` |
| `Start()` on a **parameterized** thread | — | runs with `nullptr`, **no exception** (§4) |
| `Start()` on a parameterless thread | unchanged | unchanged |
| `sizeof(Thread)` | **104** | **136** |

The message is .NET's verbatim: *"The thread was created with a ThreadStart delegate that does not
accept a parameter."*

No separate shape flag was needed — exactly one of the two callables is ever set, and which one
**is** the shape record. That is precisely .NET's `startHelper._start is ThreadStart` test.

## 4. Two asymmetries, both .NET's, both pinned

**`Start()` does not reject a parameterized thread.** .NET's private `Start(bool)` sets
`startHelper._startArg = null` and performs **no** delegate-shape check at all
(`Thread.cs:239-253`) — only `Start(parameter)` guards. "Reject it for symmetry" is the plausible
wrong answer, so a test pins the permissive behaviour.

**The shape check applies only before the first start.** .NET wraps it in
`if (startHelper != null)` (`Thread.cs:204-214`), and the comment above says why: *"In the case of
a null startHelper (second call to start on same thread) StartCore method will take care of the
error reporting."* So a **second** `Start(void*)` reports the **restart** error, not the
wrong-shape error.

**That second pin was written asserting the opposite, and it failed.** The reference then showed
that the *test* was wrong rather than the code: this port gets .NET's rule from the same fact,
because `fn_` is moved from on the first successful start and is empty afterwards, exactly as
.NET's `startHelper` becomes null. The test now asserts both halves — shape error before the
first start, restart error after it.

## 5. Evidence

Five mutations, all caught:

| Mutation | Caught by |
|---|---|
| M1 — the parameter is discarded again | `Fix1958_TheParameterActuallyReachesTheBody` — **by name, after repair** |
| M2 — the shape guard is removed | **as a crash only** — see below |
| M3 — `Start()` rejects a parameterized thread | `Decl1958_ParameterlessStartOnAParameterizedThreadPassesNull` — **by name, after reformulation** |
| M4 — an empty parameterized callable is accepted | `Fix1958_AnEmptyParameterizedCallableIsRejectedAtConstruction` |
| M5 — the new constructor consumes no managed id | `Fix1958_BothShapesGetDistinctManagedThreadIds` |

M5 matters because SR-AUD-193's uniqueness contract covers **every** `Thread` object regardless of
shape.

**M1 was caught only as a SEGV at first**, because the test dereferenced the pointer and the
mutation passes `nullptr`. The assertion is now null-safe, so it fails by name.

**M3 was caught only as a SIGABRT at first**, because my mutation threw from inside the *thread
body*, where no handler exists. That is not a realistic regression; reformulated as the plausible
one — rejecting at the call site "for symmetry" — it is caught by name.

**M2 is caught only as a crash, and that is inherent rather than a test defect.** Removing the
guard lets `Start(void*)` on a parameterless thread hand an **empty** `std::function` to a new OS
thread, whose call to it raises `std::bad_function_call` with no handler and reaches
`std::terminate` — exactly the failure mode SR-AUD-192 documented for the constructor. The
`EXPECT_THROW` does record its failure first, but the process aborts before gtest prints its
summary. No assertion can observe a `std::terminate` on another thread, which is the same
inherent limit #2215 recorded.

Gate: **17,497 run, 17,497 passed, 0 failed, 0 skipped** across 38 executables — `+7` on 17,490,
exactly the seven new cases (`SharpRuntimeTests_Threading` 514 → 521). No other executable moved.
All 514 pre-existing cases passed unchanged before the new ones were added. Module graph
unchanged at 41/93.

## 6. Downstream, measured

`System::Threading::Thread` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`,
so the rebuild requirement is recorded here for future consumers rather than acted on. Neither
repository was modified.

## 7. Scope

#1958 now has **two** findings left: **SR-AUD-209** (make `AutoResetEvent`/`ManualResetEvent`
derive from `WaitHandle` — a vtable *and* base-class change, which SA-3 explicitly excludes) and
**SR-AUD-196** (`ThreadStartException` publishes constructors .NET makes internal).
