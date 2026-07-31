<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Shared-PRNG concurrency plan — CCF-009

*Review plan for the next post-audit remediation family, selected 2026-07-31
after CCF-019's compatible portion completed. Audit finding **SR-AUD-010**
(high). Design record only — **no production file has been changed by this
plan**, and it creates no `SR-AUD-*` identifier: numbering stays frozen at
**364**.*

---

## 1. Why this family, and how it was selected

`prompt.md`'s Step 1 selects the next unclassified row from `plan.sqlite3`'s
`task` table. **That queue is empty** — measured, `SELECT COUNT(*) FROM task
WHERE status='' OR status='todo'` returns **0**, and the table is fully
classified at 14,979 `ignored` / 1,082 `ported` / 140 `ignore`. The namespace
review therefore has nothing to select, and the established fallback the
previous batches used applies: take the next **cross-cutting family** from
`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`.

Selection, from the 307 open findings (0 critical, **65 high**, 232 medium, 10
low):

| Candidate | Why not chosen |
|---|---|
| CCF-018 — enumerator lifecycle | overlaps #1889, which the user has just **declined**; it would immediately re-raise the same layout approval |
| CCF-020 — `ICollection::CopyTo` | its migration ticket **#1773** is blocked on downstream consumers this batch may not inspect |
| CCF-003 / CCF-006 / CCF-010 | large formatting/parsing families whose repairs are accepted-grammar changes, i.e. approval-gated before they start |
| **CCF-009 — shared PRNG state** | **chosen** |

CCF-009 is the right next family on four measurable grounds: its only member is
**high** severity; both defects are **TSan-confirmed**, so the repair has an
objective pass/fail gate; the family is **bounded at two implementation sites**;
and §6 shows the whole repair is achievable with **no public signature, layout,
vtable or ABI change** — so it can be finished without an approval round, unlike
every other open family of comparable severity.

---

## 2. File and type inventory

| File | Role |
|---|---|
| `modules/core/include/System/Random.hpp` | `System::Random` — public class, state **in the header** |
| `modules/core/src/System/Random.cpp` | generator bodies, `getSharedProperty()` |
| `modules/core/include/System/Guid.hpp` | `System::Guid` — declaration only for the affected paths |
| `modules/core/src/System/Guid.cpp` | `NewGuid()`, `CreateVersion7()` |
| `modules/core/tests/System/RandomTests.cpp`, `GuidTests.cpp` | existing coverage |
| `audit/modules/core/src/System/Random.cpp.audit.md`, `Guid.cpp.audit.md` | owning audit reports |

**Measured shape of the state**

```cpp
// Random.hpp — PUBLIC HEADER, so any new member is an object-layout change
class Random {
    std::array<int32_t, 56> seedArray_{};   // mutated by Next()
    int inext_ = 0;                          // mutated by Next()
    int inextp_ = 21;                        // mutated by Next()
public:
    static Random& getSharedProperty();      // Random.cpp: `static Random instance;`
```

```cpp
// Guid.cpp — function-local statics, entirely internal
Guid Guid::NewGuid() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint32_t> dist(0, 255);
    ...
}
```

## 3. Public surface the repair must preserve

`Random`: `Random()`, `Random(intcs seed)`, `getSharedProperty()`, `Next()` ×3,
`NextInt64()` ×3, `NextSingle()`, `NextDouble()`, `NextBytes()` ×2, `Shuffle()`,
`GetItems()`, `GetString()`, and the `protected` `Sample()` seam.
`Guid`: `NewGuid()`, `CreateVersion7()` ×2.

Two properties of that surface constrain every candidate design:

1. **`getSharedProperty()` returns `Random&`** — a *reference*, so whatever it
   names must outlive the call and be addressable.
2. **`Sample()` is `protected` and virtual-free but overridable-by-derivation** —
   a derived `Random` may reimplement it, so any repair that bypasses it for the
   shared instance would change what a derived type observes.

## 4. The two defects, and the one shared root cause

| Site | Defect | Severity |
|---|---|---|
| `Random::getSharedProperty()` | one process-wide `Random` whose `seedArray_`/`inext_`/`inextp_` are mutated by every `Next()` with no mutex, atomic or thread-local ownership | high |
| `Guid::NewGuid()` | one process-wide `std::mt19937_64` + distribution mutated by every call; `CreateVersion7()` reaches the same engine through it | high |

**Shared root cause:** *process-wide singleton generator state that every caller
shares whether it knows or not, mutated without an ownership boundary.* Both
rely on thread-safe **static initialisation** (which C++11 does guarantee) and
then mutate the initialised object (which it does not). No caller can opt out,
which is what separates this from ordinary "do not share an object across
threads" advice — and is exactly why `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`
records the `SortedSet<T>` Count-cache race (#1784) as **related but not a
member**: that state was per-object and caller-owned.

## 5. Reference behaviour — cited, not remembered

Read 2026-07-31 from
`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Random.cs`:

- **`Random.Shared`** (line 57) is `new ThreadSafeRandom()`, *"Random
  implementation that delegates all calls to a **ThreadStatic** Impl instance"*
  (line 752), holding `[ThreadStatic] private static XoshiroImpl? t_random`
  (768) reached through `LocalRandom` (773) on **every** call.
- The comment at lines 755–759 is the decisive one and rules out the obvious
  C++ shortcut: *"It's also possible that one thread could retrieve Shared and
  pass it to another thread, so Shared **can't just access a ThreadStatic to
  return a Random instance stored there**. As such, we need the instance … by
  itself accessing a ThreadStatic on every access: we've chosen the latter."*

**Consequence for the port, and the false premise this corrects.** The tempting
one-word repair — `static thread_local Random instance;` inside
`getSharedProperty()` — is **wrong**, and .NET rejected it explicitly for the
same reason: it makes `&Random::getSharedProperty()` differ per thread, so a
caller that obtains the reference on one thread and uses it on another gets a
*different generator* than the one it asked for, silently. The contract must be
**one stable object whose calls route to thread-local state**, not a per-thread
object.

`Guid.NewGuid` has no such constraint: nothing is handed out, so per-thread
engine state is directly equivalent.

## 6. Compatibility boundary — the whole repair is compatible

| Change | Signature | Layout | vtable | ABI | Accepted input |
|---|---|---|---|---|---|
| `Guid::NewGuid`'s statics become `thread_local` | none | none | none | none | none |
| `Random::getSharedProperty` routes to thread-local state via a `this == &shared` test inside the `.cpp` | none | **none** | none | none | none |

The second row is the load-bearing claim, so it is spelled out. The shared
instance stays a single `static Random` with a stable address; the generator
bodies in `Random.cpp` — which are already out of line — gain a check of the
form *"is `this` the shared instance? then use the calling thread's engine
state"*. That is a **pointer comparison**, not a member: `sizeof(Random)` and
`alignof(Random)` are unchanged, no header declaration moves, and no symbol name
changes. A flag member (`bool isShared_`) would have been an **object-layout
change** and is therefore rejected here rather than discovered mid-implementation.

**Two consequences that must be decided before coding, not after** (§9):
whether the per-thread engines are seeded independently (they must be, or every
thread produces the identical stream), and what `Random::Shared` seeded-
reproducibility means once it is per-thread — .NET's answer is that `Shared` was
never reproducible, since it has no seeded constructor.

## 7. Dependency order

1. **#1901 — `Guid::NewGuid`/`CreateVersion7`.** No public-surface interaction,
   smallest blast radius, and it establishes the TSan gate the other ticket
   reuses. Land first.
2. **#1902 — `Random::getSharedProperty`.** Depends on #1901 only for the
   harness, not for code.
3. **#1903 — SR-AUD-010 closure.** Flip the finding to `remediated` once *both*
   sites pass; the cross-cutting record is explicit that repairing one leaves
   the other independently unsafe, so neither ticket may close it alone.

## 8. Test and sanitizer matrix

| Gate | Requirement |
|---|---|
| **TSan** | a dedicated probe under `build-tsan/` with N threads hammering `Random::getSharedProperty().Next()` and `Guid::NewGuid()`; **must report the race before the repair and be clean after** — the before-run is mandatory, so the gate is proved to be able to fail |
| ASan / UBSan / LSan | existing `RandomTests`/`GuidTests` clean |
| Determinism | a *seeded* `Random(seed)` produces the identical stream before and after, value for value — the repair must not touch the non-shared path |
| Per-thread independence | two threads' first 1,000 `Shared` draws are not identical (they would be if every thread seeded the same way) |
| Reference stability | `&Random::getSharedProperty()` is the **same address on every thread** — the clause §5 says .NET deliberately preserves |
| Cross-thread handoff | a reference obtained on thread A and used on thread B still produces valid, race-free values |
| Uniqueness | 100,000 `Guid::NewGuid()` across 8 threads yield no duplicate and no nil GUID; version/variant nibbles correct |
| `CreateVersion7` | monotone timestamp prefix preserved under concurrency |
| Mutation | removing the ownership boundary at either site must fail the TSan gate |
| Layout | `static_assert` on `sizeof`/`alignof` of `Random` before and after |

## 9. Ticket breakdown

| # | Scope | Status to create |
|---|---|---|
| **#1900** | this plan (design only) | `done` on landing |
| **#1901** | `Guid::NewGuid`/`CreateVersion7` per-thread engine + tests | `todo` |
| **#1902** | `Random::getSharedProperty` thread-local routing + tests | `todo` |
| **#1903** | SR-AUD-010 closure once both land | `blocked` on #1901, #1902 |

**One decision may surface during #1902 and must not be taken silently:** if a
`this == &shared` test proves unworkable against the `protected Sample()` seam —
because a derived type's override would then see thread-local state it did not
create — the fallback is a mutex **inside `Random.cpp`** guarding only the shared
instance. That is still compatible (no member, no layout), costs uncontended
lock/unlock per shared draw, and would be recorded rather than chosen quietly.

## 10. Completion criteria

CCF-009 is complete when **all** hold:

1. `Guid::NewGuid`, `Guid::CreateVersion7` and `Random::getSharedProperty()` have
   a real ownership boundary, and the TSan probe that reported races before the
   repair is clean after.
2. `sizeof`/`alignof` of `Random` and `Guid` are unchanged, asserted in tests;
   no public signature, vtable or symbol change; no consumer source migration.
3. A seeded `Random(seed)` stream is byte-identical to today's.
4. `&Random::getSharedProperty()` is stable across threads.
5. The repository gate passes with no test-count regression and no weakened test.
6. **SR-AUD-010 flips `confirmed → remediated`**, taking the post-audit tally from
   57/306/364 to **58 remediated / 305 confirmed / 364**, with numbering still
   frozen at 364 and no new `SR-AUD-*` issued.
