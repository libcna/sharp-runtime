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

---

# Implementation record — 2026-07-31

*Sections 1–10 above are the plan as written before any code changed. Everything
below is measured, on branch `feature/remediation-ccf019-final-compatible`.
Tickets **#1901**, **#1902** and **#1903** are all `done`; **CCF-009 is complete**
and **SR-AUD-010 is `remediated`**. Probe binaries have been deleted per
`CLAUDE.md` build-resource rule 11 — this section is the durable evidence.*

## 11. The TSan gate, before and after

One probe, `build-probe/1901_ccf009_tsan_probe.cpp`, 8 threads × 2,000 draws per
site, selectable per site with `argv[1]` so each defect is attributed to its own
source. Built by `build-probe/1901_tsan_cc.sh` driven with `xargs -P 3` — the
40 `Core.Base` translation units the probe needs, compiled with
`-fsanitize=thread`, **not** a `build-tsan/` tree (which was never created: the
targeted set costs ~30 MB and 11 s instead of hundreds of MB).

| Probe mode | Before | After |
|---|---|---|
| `guid` | **13** data races, all at `Guid.cpp:344` (`dist(rng)`) | **0**, exit 0 |
| `random` | **6** data races, all inside `Random::internalSample()` — lines 72, 78, 83, 84, 85, i.e. `inext_`, `inextp_` and `seedArray_` | **0**, exit 0 |
| `both` | — | **0**, exit 0 |

The before-run is the mandatory half: it proves the gate can fail, and it is also
the **per-site mutation evidence** §8's last row asks for, because it *is* the
code with the ownership boundary removed at that site.

## 12. What was actually changed

Both repairs are confined to `.cpp` bodies. **No header was touched** — `git
status` over the change lists exactly `Random.cpp`, `Guid.cpp`, `RandomTests.cpp`,
`GuidTests.cpp`.

**#1901 — `Guid::NewGuid`.** The function-local `static std::mt19937_64` and
`static std::uniform_int_distribution` became a per-thread engine reached through
a file-local `newGuidEngine()`, plus a `thread_local` distribution. Nothing is
handed to the caller here, so per-thread state is exactly equivalent — §5's
constraint applies only to `Random`. `CreateVersion7` inherits the repair through
`NewGuid` with no edit of its own.

**#1902 — `Random::getSharedProperty`.** As §6 predicted, the shared instance
stays one `static Random` with a stable address, and `internalSample()` gains a
pointer comparison: if `this` is the shared instance, the draw is taken from the
calling thread's own generator instead. `internalSample()` is the **only** method
that writes `seedArray_`/`inext_`/`inextp_`, and every entry point on the class
funnels through it — including the header-inline templates (`NextInteger`,
`Shuffle`, `GetItems`) and the `protected Sample()` seam — so one test covers the
whole public surface. After construction the shared object is never written
again.

Three things were decided during implementation rather than left implicit:

1. **The `Sample()` seam concern in §9 did not materialise, and the mutex
   fallback was not needed.** A derived `Random` can never *be* the shared
   instance — `getSharedProperty()` constructs a `Random` — so `this == &shared`
   is false for every derived object and derived types are untouched. Pinned by
   `RandomTests.Shared_DerivedInstancesAreNotAffectedByTheBoundary`.
2. **Per-thread seeds are made distinct by construction, not by entropy.**
   §6 flagged that identically-seeded threads would produce identical streams;
   deriving the seed from `std::random_device` alone would do exactly that on a
   platform whose `random_device` is deterministic, as MinGW-w64's historically
   was. Both sites therefore mix a once-per-process entropy `base` with an atomic
   counter through an odd multiplier, which is a bijection on the modulus and so
   guarantees distinctness even where the entropy is absent. `Random`'s seed is
   reduced to 31 bits because `initializeSeed()` takes `|seed|`, so `+n` and `-n`
   would otherwise collide.
3. **The per-thread `Random` is placement-constructed in thread-local storage and
   deliberately never destroyed.** A plain `thread_local Random` is destroyed at
   thread exit, which would leave code that draws from the shared instance
   *during* teardown — another `thread_local`'s destructor — holding a reference
   to a destroyed object. The process-wide instance had no such window, so the
   repair must not open one. Nothing leaks: the storage is thread-local, not
   heap, and `Random` owns no resource (LSan confirms, §14). It also keeps the
   translation unit free of `__cxa_thread_atexit`, measured: that undefined
   reference appears with a plain `thread_local Random` and is **absent** from the
   shipped object, which matters for the compile-only MinGW and Emscripten
   targets.

**One cost, recorded rather than hidden:** the boundary check is one acquire load
and one predictable branch *per draw*, which `NextBytes` pays per byte. Hoisting
it out of the loops would require a new private member function — a public-header
edit — so it was not done.

**One behavioural note:** move-assigning *to* the shared instance now has no
effect on the values it produces, where before it corrupted the shared stream.
Both are pathological and the second was already a data race; no test pins either.

## 13. Determinism — measured, not argued

`build-probe/1902_determinism_dump.cpp` dumps **4,928 lines** — 8 seeds (`0`, `1`,
`7`, `42`, `-1`, `-12345`, `INTCS_MAX`, `INTCS_MIN`) across `Next()` ×3,
`NextInt64()` ×3, `NextDouble`, `NextSingle`, `NextBytes` ×2, `GetString`,
`GetHexString` ×2, `Shuffle`, `GetItems` and `NextInteger<T>` ×3 — compiled twice,
once against `HEAD`'s `Random.cpp` and once against the repaired one, and
**`cmp` reports the two dumps byte-identical**. The seeded path is untouched,
value for value. The pre-existing `RandomTests.Parity_*` suite, pinned against
live Mono output, is unchanged and still passes.

## 14. Test, sanitizer and ABI results

| Gate (§8) | Result |
|---|---|
| TSan | §11 — races before, **0** after, at both sites |
| ASan + UBSan + LSan | `build-asan/SharpRuntimeTests_Core_Base`, `GuidTests.*:RandomTests.*` — **168 passed, 0 diagnostics** |
| Determinism | §13 — byte-identical, 4,928 lines |
| Per-thread independence | `Shared_DistinctThreadsDoNotShareAStream`, `NewGuid_DistinctThreadsDoNotShareAStream` |
| Reference stability | `Shared_ReferenceIsTheSameAddressOnEveryThread` — 8 threads, all `==` the main thread's address |
| Cross-thread handoff | `Shared_ReferenceObtainedOnOneThreadStaysUsableOnAnother` |
| Uniqueness | `NewGuid_ConcurrentAcrossThreads_NoDuplicateNoNil` — **100,000** GUIDs, 8 × 12,500, zero duplicates, zero nil, version 4 and variant nibbles checked on every one |
| `CreateVersion7` | monotone prefix within a thread; every concurrent prefix inside the `[before, after]` window; 16,000 concurrent values, zero duplicates |
| Mutation | §15 |
| Layout | `sizeof(Random) == 240`, `alignof == 8`; `sizeof(Guid) == 16`, `alignof == 1` — `static_assert` **and** runtime `EXPECT` in both suites |

**ABI.** `nm -C --defined-only --extern-only` over `Random.o` and `Guid.o`,
before versus after: **identical, both files** — 58 and 87 external symbols, none
added, removed or renamed. The only new symbols are internal-linkage (`t`/`b`):
the file-local helpers and their TLS guards. The only new undefined reference is
`_GLOBAL_OFFSET_TABLE_`.

**Test count.** 14,731 → **14,745** (+14: 6 in `GuidTests`, 8 in `RandomTests`),
37 executables, zero failures. `scripts/local_ci_check.sh` passes.

## 15. Mutation gate — what the tests actually pin

TSan alone cannot distinguish a correct repair from a plausible wrong one: both
the `.NET`-rejected shortcut and an identically-seeded per-thread engine are
race-free. Each mutation below was applied to the shipped code, the `Core.Base`
test binary rebuilt at three jobs, the concurrency tests run, and the file
restored (`build-probe/1901_mutation_check.sh`).

| Mutation | Caught by |
|---|---|
| **A** — the boundary removed at either site (i.e. `HEAD`) | the TSan gate, per site, §11 |
| **B** — `static thread_local Random instance;` in `getSharedProperty()`, the shortcut .NET rejects at `Random.cs:755–759` | `Shared_ReferenceIsTheSameAddressOnEveryThread` **fails**; the other 15 pass, and TSan stays clean — which is the whole point |
| **C** — `Random`'s per-thread seed made a constant | `Shared_DistinctThreadsDoNotShareAStream` and `Shared_ConcurrentDrawsAreInRangeAndVaried` **fail** |
| **D** — `Guid`'s per-thread seed made a constant | `NewGuid_DistinctThreadsDoNotShareAStream`, `NewGuid_ConcurrentAcrossThreads_NoDuplicateNoNil` and `CreateVersion7_ConcurrentAcrossThreads_NoDuplicateAndWellFormed` **fail** |

Mutation **B** is the load-bearing one: it is the repair a future contributor is
most likely to reach for, it passes every sanitizer, and it silently breaks the
contract §5 exists to protect.

The 18 concurrency tests were also run under `--gtest_repeat=20` (and again ×10
after the §12.3 refinement) with no failure and ~100 ms per pass, so they are
neither flaky nor a meaningful addition to gate time.

## 16. Completion

All six criteria in §10 hold. **CCF-009 is complete**; **SR-AUD-010 flips
`confirmed → remediated`**, taking the post-audit tally to **58 remediated / 305
confirmed / 364**, numbering still frozen at **364**, no new `SR-AUD-*` issued and
no `CCF-*` cause added. `CNA` and `mobile-eggbert` were not inspected, built or
modified.
