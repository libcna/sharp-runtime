<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# SortedSet&lt;T&gt; mutation-counter contract

*Design and implementation record for ticket #1786
(`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, size S). Recorded 2026-07-28 on
local branch `feature/remediation-coll-sortedset-version-overflow`. This ticket
carries **no `SR-AUD-*` identifier** — the audit numbering is frozen at 364 and
this defect was found during remediation, by ticket #1784's own required
overflow analysis (`docs/SortedSetLiveViewDesign.md` §31.6). It does **not**
reopen SR-AUD-361, which stays `remediated`, and it is **not** a regression from
#1783 or #1784: the counter, its type, and its increment all arrived with ticket
1713's fail-fast enumerator work.*

---

## 1. Executive decision

`SortedSet<T>`'s shared mutation counter becomes **64-bit unsigned**
(`SharpRuntime::ulongcs`) in `State` and in `Iterator`, and the per-view Count
cache keeps its 32-bit tag but stores it **biased by one and compares it
widened**, which makes the tag identify a counter value exactly and stops the
cache being written once the counter outgrows it.

This is **Alternative B (wider counter) for the counter, plus Alternative C
(checked exhaustion of the tag) for the cache**, and it is fully
**source-, symbol-, and object-layout-compatible**. It was therefore implemented
in full under this ticket; no approval was required and none was requested.

What it closes, all four reproduced against the real production header before
anything changed (§4):

| # | Defect | Threshold | Status |
|---|---|---|---|
| 1 | `++version` at `INTCS_MAX` is signed-integer overflow — **undefined behaviour** | 2^31 − 1 mutations | **eliminated** |
| 2 | A wrapped counter silently revalidates a stale `Iterator` snapshot | 2^32 mutations | **eliminated** |
| 3 | A wrapped counter silently revalidates a stale cached view `Count` | 2^32 mutations | **eliminated** |
| 4 | The cache's `-1` "never computed" sentinel is itself a reachable counter value, so a cold cache reads as warm | 2^32 − 1 mutations | **eliminated** |

Defect 4 was **not** in the ticket's description. It was found while tracing the
code for this design and is the most serious of the four, because unlike 2 and 3
it needs no prior observation: any view created on a state whose counter has
reached that value answers `0` immediately.

Measured cost: **none.** Every benchmarked operation is within run-to-run noise
of the pre-fix header, and warm view `Count` is slightly faster (§13).

---

## 2. Ticket and defect

Ticket #1786, key `REMED-COLL-VERSION-COUNTER-OVERFLOW`, priority **P3**, size
**S**, category `investigation`, area `Collections`, source path
`modules/collections/include/System/Collections/Generic/SortedSet.hpp`.

### 2.1 A deliberate narrowing of the recorded acceptance criteria

The ticket's stored `acceptance_criteria` says that if a change is selected "it
must be implemented repository-wide rather than for SortedSet alone". The
instruction governing this working session narrows that: #1786 is scoped to
`SortedSet<T>`, and any other collection carrying the same flaw is to be
recorded as a **separate inactive ticket** rather than folded in. That
instruction is followed, the divergence is recorded here rather than silently
absorbed, and the repository-wide sweep is opened as inactive ticket **#1787**
(§17). The inventory the criteria asked for — which collections carry a counter
and of what type — is delivered in full in §16; only the *implementation* for
the other fourteen is deferred.

---

## 3. Current representation, before this ticket

`modules/collections/include/System/Collections/Generic/SortedSet.hpp`, as
committed at `5fda45cb`:

```cpp
struct State {
    std::set<T> data;
    intcs version = 0;                       // int32_t
};

class Iterator {
    std::shared_ptr<const State> state_;
    SetIterator it_;
    SetIterator end_;
    intcs version_ = 0;                      // int32_t snapshot
    void checkVersion() const {
        if (version_ != state_->version) throw System::InvalidOperationException(...);
    }
};

static constexpr intcs kCountNotCached = -1;
mutable std::atomic<intcs> cachedCount_{0};
mutable std::atomic<intcs> cachedCountVersion_{kCountNotCached};
```

`intcs` is `int32_t` (`SharpRuntimeHelper.hpp:50`).

**Every increment site**, and there are exactly four, all reached through
`++state_->version`:

| Member | Condition | Increments |
|---|---|---|
| `Add(item)` | the `std::set::insert` reported an insertion | 1 |
| `Remove(item)` | the `std::set::erase` reported a removal | 1 |
| `Clear()` on an owning set | the set was non-empty | 1 |
| `Clear()` on a view | the range was non-empty | 1 |

Nothing else writes it. The two constructors do not (the initializer-list
constructor builds the `std::set` directly), `GetViewBetween` does not, and the
copy/move/assignment paths transfer or replace whole `State` objects rather than
touching the counter.

**Every read site**, and there are exactly three:

| Site | Comparison |
|---|---|
| `Iterator::Iterator` | snapshot: `version_(state_->version)` |
| `Iterator::checkVersion` | `version_ != state_->version` — **equality only** |
| `getCountProperty()` | `cachedCountVersion_.load(acquire) == currentVersion` — **equality only** |

### 3.1 Increments per public operation

Because the bulk and set-algebra members route through `Add`/`Remove`, their
increment counts are *per effective element mutation*, not per call — measured
and pinned by `SetAlgebraBumpsOncePerEffectiveElementMutation`:

| Operation | Increments |
|---|---|
| `Add` inserting / rejecting a duplicate | 1 / **0** |
| `Remove` erasing / absent | 1 / **0** |
| `Clear` with content / already empty | 1 / **0** |
| `UnionWith(other)` | one per element of `other` actually inserted |
| `ExceptWith(other)` | one per element actually erased (or 1, via `Clear`, when self-aliasing) |
| `IntersectWith(other)` | one per element actually erased |
| `SymmetricExceptWith(other)` | one per element actually inserted or erased |

The rejected-duplicate and absent-removal zeroes are a deliberate divergence
from .NET, which bumps its counter even when the element is not added because a
rotation may still have restructured the tree (`SortedSet.cs:323-324`,
`:397-398`). sharp-runtime delegates structure to `std::set`, has no rotations of
its own to invalidate, and has pinned the zero-bump behaviour since ticket 1713.
This ticket preserves it exactly.

---

## 4. Pre-fix evidence

All probes live in the repository-local, gitignored `build-probe-sortedset/`
tree. **No production or test source was modified before this evidence was
taken**, and the pre-fix binary is built from the *committed* header, extracted
by `git show HEAD:…SortedSet.hpp` into
`build-probe-sortedset/prefix-include/`, so the reproduction stays runnable now
that the working tree is repaired.

```
build-probe-sortedset/build_prefix.sh probe12_version_overflow   # pre-fix binary
build-probe-sortedset/build.sh probe12_version_overflow asan -fno-access-control
```

`probe12_version_overflow.cpp` is **one source used on both sides**: each mode
positions the shared counter at a chosen value and asks the public API what it
answers, so nothing is conditionally compiled and the two columns below are
directly comparable. Output separates `invariants-failed` (must be 0 on both
sides) from `defects-observed` (expected pre-fix, expected 0 post-fix).

The counter is positioned with GCC's **`-fno-access-control`**, which suppresses
access checking and nothing else: no macro is defined over a library header, no
declaration is edited, and the code generated for `SortedSet<T>` is what an
ordinary translation unit generates. (`#define private public` was tried first
and does **not** work here — `SortedSet<T>`'s members are private by *default*,
with no `private:` keyword for the macro to rewrite.)

| Mode | Pre-fix | Post-fix |
|---|---|---|
| `ub-increment` | **UBSan signed-integer-overflow report**, counter goes to −2147483648 | no report, counter goes to 2147483648 |
| `iterator-aba` | **stale iterator revalidated**, dereference yields 10 | `InvalidOperationException`, as it must |
| `count-cache-aba` | **stale count 4 returned** where the range holds 3 | 3 |
| `sentinel-collision` | **cold cache answers 0** where the range holds 5 | 5 |
| `horizon` | passes | passes |
| `arithmetic` | passes | passes |
| **`defects-observed` total** | **4** | **0** |
| `invariants-failed` total | 0 | 0 |

### 4.1 The exact UBSan diagnostic

`build-probe-sortedset/probe12_prefix_ub-increment.log`:

```
prefix-include/System/Collections/Generic/SortedSet.hpp:425:20: runtime error:
    signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
    #0 System::Collections::Generic::SortedSet<int>::Add(int const&)
    #1 modeUbIncrement  build-probe-sortedset/probe12_version_overflow.cpp:134
    #2 main             build-probe-sortedset/probe12_version_overflow.cpp:319
```

Line 425 of the pre-fix header is `if (added) ++state_->version;` inside `Add`.
The corresponding lines in `Remove` and `Clear` are the same expression and are
reached the same way; the probe exercises `Add` because it is the shortest path
to the boundary.

The report is a *recoverable* UBSan diagnostic, so execution continues and the
probe can go on to show the wrapped value — which is the point: the wrap is not
merely theoretical, it is what the following three modes then exploit.

### 4.2 Iterator ABA

`probe12_prefix_iterator-aba.log`. An iterator is taken at counter value 3;
`s.Add(20)` bumps it to 4 and the guard correctly fires. The counter is then
positioned at `3 + 2^32`, which on a 32-bit field lands back on 3 — exactly what
2^32 further effective mutations would do. The guard stops firing:

```
guard-fires-on-the-very-next-mutation=1
version-2^32-mutations-later=3
stale-iterator-dereference-value=10
defect:stale-iterator-revalidated-after-2^32-mutations=1
```

`std::set::insert` does not invalidate iterators, so the stale iterator does not
merely return a wrong answer — it enumerates a container that has changed under
it, which is the precise failure ticket 1713's guard exists to prevent.

### 4.3 Count-cache ABA

`probe12_prefix_count-cache-aba.log`. The cache is filled at a version, two
in-range elements are then removed without an intervening `Count` read, and the
counter is positioned 2^32 on from the fill:

```
true-count-after-the-second-removal=3
count-answered-2^32-mutations-later=4
defect:stale-count-returned-after-2^32-mutations=1
```

This is *deterministically* constructible, not probabilistic: `Add`+`Remove` is
two increments, so a loop of exactly 2^31 pairs lands precisely on the aliasing
value. It needs the handle to survive the window without a `Count` read, which is
what makes it rare rather than impossible.

### 4.4 The sentinel collision — found here, not in the ticket

`probe12_prefix_sentinel-collision.log`. `kCountNotCached` was `-1`, and the
header asserted that "the shared version counter starts at 0 and only
increments, so it never legitimately holds this value". That is true only until
it overflows: 2^32 − 1 effective mutations walk the counter
`INTCS_MAX → INTCS_MIN → … → -1`. A view that has **never** computed its Count
then reads its cache as warm and answers the zero it was constructed with:

```
cold-cache-tag-field=4294967295
cold-cache-count-field=0
version-positioned-at=-1
count-answered-by-a-never-filled-cache=0
defect:cold-cache-read-as-warm=1
```

Severity note: this one is strictly worse than defects 2 and 3. They need a
stale snapshot to have survived a 2^32 window; this needs nothing at all. Every
view over that state answers `0` for its whole life until the next mutation.

### 4.5 What the #1784 atomic cache did and did not change

It changed **only the memory ordering** of the accesses. The values stored, the
types, and the equality test are byte-for-byte the pre-#1784 ones, which is
exactly what §31.6 of `docs/SortedSetLiveViewDesign.md` recorded. `probe12`'s
`count-cache-aba` and `sentinel-collision` modes run against the #1784 header
and reproduce both defects, confirming that the atomics neither introduced nor
mitigated them.

---

## 5. .NET comparison

Read from the local current .NET sources, not from memory:
`/rv/tmp/runtime/src/libraries/System.Collections/src/System/Collections/Generic/SortedSet.cs`
and `SortedSet.TreeSubSet.cs`.

| Question | .NET |
|---|---|
| Counter type | `private int version;` — **`Int32`, signed** (`SortedSet.cs:56`) |
| Increment | `version++` / `++version` at `SortedSet.cs:310, 324, 398, 504` and `UpdateVersion()` at `:767` |
| Checked or unchecked? | **Unchecked.** C# arithmetic is unchecked by default and the runtime is not compiled with `/checked`, so `version++` at `int.MaxValue` **wraps to `int.MinValue` with fully defined two's-complement semantics** |
| Enumerator check | `if (_version != _tree.version) throw new InvalidOperationException(...)` — equality only (`SortedSet.cs:1898, 1964`) |
| View Count check | `if (_countVersion != _underlying.version) { recount; _countVersion = _underlying.version; }` — equality only (`TreeSubSet.cs:322-327`) |
| Sentinel | `TreeSubSet`'s constructor sets `version = -1; _countVersion = -1;` (`TreeSubSet.cs:48-49`) — **the same `-1` sentinel, with the same latent collision** |
| Formal ABA prevention | **None.** Nothing in `SortedSet.cs` or `TreeSubSet.cs` detects or prevents counter reuse |
| Bumps on a rejected duplicate? | **Yes** — deliberately, because a rotation may have restructured the tree even when nothing was added |

### 5.1 The four things this comparison settles

1. **Matching .NET's integer width does not make the C++ code correct.** The
   CLR *defines* signed overflow as wrapping; C++ makes it undefined behaviour.
   Identical source-level arithmetic has different standing in the two
   languages, and this is precisely the trap #1784's §31.1 identified in a
   different guise for the racing `int` write.
2. **.NET has defects 2, 3 and 4 too** — as defined-but-wrong behaviour rather
   than UB. Its 2^32 ABA window and its `-1` sentinel collision are real; they
   are simply never hit in practice and nobody has cared.
3. **The four concerns must be separated**, and this design separates them:
   *(a)* eliminating C++ UB — mandatory, non-negotiable, and cheap; *(b)*
   preventing practical snapshot ABA — worth doing and achievable for free;
   *(c)* preserving source and ABI compatibility — a hard constraint here;
   *(d)* matching observable .NET behaviour — satisfied, because none of this is
   observable to a program that does not perform billions of mutations.
4. **sharp-runtime should exceed .NET's robustness here, and now does.** .NET's
   ABA horizon is 2^32; this port's is 2^64 for iterators and *unreachable* for
   the Count cache, which stops caching rather than guessing. Exceeding the
   reference is justified because the C++ consequence of the shortfall (UB) is
   categorically worse than the managed one (a stale number), and because
   nothing observable to a conforming program changes.

---

## 6. The required contract, stated exactly

The eighteen points the ticket requires, answered without hedging.

1. **Increment behaviour for every representable prior value.** `bumpVersion()`
   is `++state_->version` on a `SharpRuntime::ulongcs`. Unsigned arithmetic is
   modulo 2^64 by definition ([basic.fundamental]), so the operation is defined
   for **every** value the counter can hold, including `ULONGCS_MAX`, where it
   yields 0.
2. **No signed-overflow UB.** There is no signed arithmetic on the counter
   anywhere. Verified by UBSan (§12).
3. **Iterator and enumerator invalidation.** Unchanged in behaviour and
   strengthened in reach: `Iterator` snapshots `state_->version` at construction
   and `checkVersion()` throws
   `InvalidOperationException("Collection was modified; enumeration operation
   may not execute.")` from `operator*`, `operator->`, and `operator++` whenever
   the snapshot differs. `operator==`/`operator!=` still compare positions only,
   so range-`for` termination is untouched.
4. **Count-cache invalidation.** A view's cached count is returned only when the
   widened cache tag equals `state_->version + 1`. Any effective mutation moves
   the counter and therefore invalidates it. `invalidateCountCache()` resets the
   tag to `kCountNotCached` on every path that rebinds a handle.
5. **At or near counter exhaustion.** *Cache tag:* once
   `state_->version > kMaxCacheableVersion` (2^32 − 2), no tag can match and the
   cache is no longer written; a view's `Count` becomes an O(k) recomputation on
   every call — slower, never wrong, and permanent for that state. *Counter:*
   exhaustion is at 2^64 effective mutations, where it wraps to 0. That is the
   only point at which a stale snapshot could be revalidated, and it is not
   reachable (§6.1).
6. **Existing wrappers and live views.** Unaffected. There is still exactly one
   counter per shared `State`, so a mutation through an owning set, a view, a
   nested view, or an overlapping view invalidates every outstanding iterator
   and every view's cached Count, exactly as before.
7. **Copy.** Copying an owning set deep-clones the `State`, counter value
   included, after which the two counters advance independently. Copying a view
   yields another handle onto the same `State` and therefore the same counter,
   with a **cold** Count cache.
8. **Assignment.** Rebinds the handle to the other object's `State` and counter
   and invalidates this handle's Count cache. Never mutates a counter another
   handle observes.
9. **Thread-safety interaction with the #1784 atomic Count cache.** The
   publication protocol is preserved verbatim — count stored first `relaxed`,
   tag stored last `release`, tag loaded first `acquire` — and both fields keep
   their width, alignment, and lock-free status. `state_->version` deliberately
   stays a plain non-atomic field for the reason #1784 gave: nothing writes it
   during a legal concurrent read window, and making it atomic would falsely
   suggest concurrent mutation had become defined.
10. **May mutation throw at exhaustion?** **No.** `bumpVersion()` is `noexcept`
    and unconditional. Adding a throw would change the exception contract of
    `Add`, `Remove`, and `Clear` for a condition no program can reach.
11. **May mutation terminate?** **No.** Nothing aborts, asserts, or traps.
12. **A new State identity or generation?** **No.** §8 explains why State
    renewal cannot be made compatible with the #1783 live-view contract.
13. **Can stale snapshots ever become valid again?** For an `Iterator`: only if
    the counter returns to the snapshot value, which requires exactly 2^64
    effective mutations on one `State`. For the Count cache: **never** — past
    the tag's reach the cache is not consulted at all, and within it the tag
    identifies the counter exactly. The residual iterator case is stated, not
    hidden (§15).
14. **Performance cost.** None measurable (§13).
15. **Source compatibility.** Unaffected. No public signature, return type,
    parameter, or `const` qualification changed (§11.1).
16. **Symbol compatibility.** Unaffected; the mangled `GetViewBetween` symbol is
    byte-identical to #1784's (§11.2).
17. **Object-layout compatibility.** Unaffected; `sizeof`, `alignof`, and every
    member offset of `SortedSet<T>` and `Iterator` are identical (§11.3).
18. **Migration or rebuild requirements.** None on this revision's account
    (§11.5).

### 6.1 Why 2^64 is treated as unreachable rather than handled

A `std::set<int>` `Add`+`Remove` pair measures ~80 ns on the benchmark machine
(§13), i.e. ~2.5 × 10^7 increments per second. At an implausibly generous 10^9
increments per second, 2^64 increments take 1.8 × 10^10 seconds — **over 580
years of uninterrupted mutation of a single collection instance**. A checked or
saturating increment to guard it is evaluated and rejected in §7 (Alternative
C/F): both put a branch on every mutation, and saturation would silently stop
invalidating snapshots, which is worse than the state it guards.

---

## 7. Alternatives evaluated

The compatibility ceiling that decides most of this: `sizeof(SortedSet<int>)`
must stay **40** and `sizeof(SortedSet<std::string>)` **104**, both approved by
the user under #1783 and re-verified under #1784. Measured member offsets
(`probe13_member_offsets.cpp`) show there is **no spare byte**:

```
int:    state_@0 (16)  lower_@16 (8)  upper_@24 (8)  cachedCount_@32 (4)  cachedCountVersion_@36 (4)  = 40
string: state_@0 (16)  lower_@16 (40) upper_@56 (40) cachedCount_@96 (4)  cachedCountVersion_@100 (4) = 104
```

40 and 104 are both multiples of the 8-byte alignment, so **there is no tail
padding to widen the cache tag into**. The Count cache's budget is exactly 64
bits, and an exact count needs 31 of them, so an exact 64-bit tag *cannot* fit.
That is a proof, not a preference, and it is what rules out the obvious answer.

`Iterator`, by contrast, measured `state_@0 (16) it_@16 (8) end_@24 (8)
version_@32 (4)` = 36 rounded to **40**: it has **four bytes of tail padding**,
so widening its snapshot to 64 bits is free. `State` is heap-allocated behind a
`shared_ptr` and is not part of any object's layout; widening its counter leaves
`sizeof(State)` at 56 anyway.

### Alternative A — unsigned 32-bit modular counter

Change `intcs` to `uintcs`. Increment becomes defined; the object layout,
symbols, and signatures are untouched; the diff is one word.

*Rejected.* It closes defect 1 and **nothing else**. ABA at 2^32 survives
unchanged, and — the point the ticket insists on addressing — it makes that ABA
*easier to reach*, because today a program must pass through UB at 2^31 before
it can get there, whereas under A the path to a silently stale iterator is
entirely well-defined. Replacing undefined behaviour with defined-but-wrong
behaviour is a real improvement in one respect and a mild regression in
another, and it is not enough when a strictly better option costs the same.
Defect 4 (the `-1` sentinel) also survives A in a new form: with an unsigned
counter, `static_cast<uintcs>(-1)` is `UINTCS_MAX`, still reachable.

### Alternative B — wider 64-bit counter ***(SELECTED for the counter)***

`State::version` and `Iterator::version_` become `SharpRuntime::ulongcs`.

- *Practical wrap horizon:* 2^64, over 580 years (§6.1).
- *Does ABA remain theoretically possible?* **Yes**, at exactly 2^64
  mutations. Stated honestly rather than claimed away.
- *Object layout:* `SortedSet<T>` unchanged — the counter is not a member of it.
  `Iterator` unchanged at 40 bytes, because the wider field lands in padding it
  already had. Measured, not assumed.
- *Atomic alignment:* not applicable; neither field is atomic. The two atomic
  cache fields are untouched by B.
- *ABI and rebuild:* no mangled name changes; `Iterator`'s and `State`'s
  internal field *widths* change, but both are private implementation details of
  a header-only template (§11.4).
- *Platform differences:* `ulongcs` is `uint64_t`, available on every supported
  toolchain. On a 32-bit target the counter is still 64-bit and still correct;
  `Iterator` would grow there, which is not an ABI concern because the published
  layout figures are explicitly LP64-only.
- *Performance:* none measurable (§13). A 64-bit increment and a 64-bit compare
  cost the same as 32-bit ones on x86-64.

### Alternative C — checked exhaustion

Keep the width and refuse to increment past a terminal value: throw before
mutating, or rebuild the `State`, or invalidate everything permanently.

*Rejected for the counter, adopted in spirit for the cache tag.* For the
counter: a check is a branch on **every** mutation, paid forever to guard a
condition reachable in 20 seconds of hot looping at 32 bits and never at 64.
Throwing would also break the strong exception guarantee story — `Add` and
`Remove` currently throw only `ArgumentOutOfRangeException` and only *before*
touching the set, whereas an exhaustion throw would have to happen after the
`std::set` was already modified, leaving the counter and the container
disagreeing. No repository evidence supports inventing an exception type for it
(§7.1). For the **cache tag**, where the reachable threshold really is 2^32,
checked exhaustion is exactly right and is what shipped: the tag simply stops
being written and `Count` recomputes.

### Alternative D — version plus independent State identity or generation

Pair the counter with identity information a wraparound cannot recreate.

*Rejected as infeasible within the compatibility boundary, for a reason worth
recording.* The `State` pointer identity **is** already available to an
`Iterator` (it holds `shared_ptr<const State>`), so an iterator could compare
identity as well as version. But identity does not change when the counter
wraps — the `State` is the same object — so it adds nothing against ABA. Making
it change means replacing the `State`, which §8 shows cannot be done without
splitting the live-view graph. Storing a generation *inside* `State` and a copy
of it in each snapshot would work, but the Count cache has no room for it
(§7's layout proof), so it would fix iterators only — and B fixes iterators
better, for free.

### Alternative E — never-reused monotonic token object

Allocate a generation object per mutation and compare identity.

*Rejected.* It puts a heap allocation on every `Add`, `Remove`, and `Clear` — a
40× cost on the measured 80 ns mutation path — to replace a counter that is
already exact for 2^64 steps. The ticket's own instruction not to "introduce
allocation per ordinary mutation without compelling evidence" is decisive, and
there is no such evidence.

### Alternative F — saturating version

Stop incrementing at the maximum.

*Rejected, and it must be, because it is silently invalid as a standalone
scheme.* Once saturated, every subsequent mutation leaves the counter unchanged,
so an iterator snapshotted at the saturated value **never fail-fasts again** and
a cached Count is **never invalidated again**. It converts a rare wrong answer
into a permanent one. Saturation is only viable paired with a separate
"exhausted" flag consulted on every check — i.e. Alternative C's branch on the
hot path — which is why the two were considered together and both rejected for
the counter.

### 7.1 A note on exception types

Alternatives C and F, in the variants that throw, would need an exception type.
The repository offers no precedent: no collection in `modules/collections/`
throws on a counter condition, and .NET has no equivalent throw either. Rather
than inventing one, the design avoids needing it — which is the outcome the
ticket's "do not choose an exception type without repository evidence"
instruction points at.

### 7.2 The Count cache tag: how exactness was recovered without a branch

The first implementation used an explicit horizon test on the read path:

```cpp
if (currentVersion > kMaxCacheableVersion)
    return static_cast<intcs>(std::distance(rangeBegin(), rangeEnd()));
```

Correct, but **measured +1 ns on every `Count` call** — including an owning full
set's, which cannot even reach that branch, because the larger inlined body
changed how the whole member was scheduled. That was isolated rigorously rather
than guessed (§13.2).

The shipped version removes the branch by biasing the tag:

> **The tag is the counter plus one, computed in 64-bit arithmetic, stored only
> while it still fits 32 bits, and compared by widening the tag back to 64 bits.**

That single bias makes one equality test exact for every counter value:

| State | Tag | `version + 1` | Match? | Correct? |
|---|---|---|---|---|
| never filled | `0` | ≥ 1 always | no | ✅ cold cache can never read warm — closes defect 4 |
| filled at `V` ≤ 2^32 − 2 | `V + 1` | `version + 1` | only when `version == V` | ✅ exact, no truncation |
| counter past the tag's reach | any 32-bit value | ≥ 2^32 | no | ✅ recompute, and the cache is no longer written |

The only value at which the first row could fail is `version == ULONGCS_MAX`,
where `version + 1` wraps to 0 — the 2^64 exhaustion point, already covered by
§6.1 and §15.

---

## 8. Why State renewal was investigated and rejected

The ticket asks explicitly whether counter exhaustion could be handled by
replacing the shared `State` with a fresh one while preserving the elements. It
cannot, and the reason is structural rather than a matter of effort:

- A full owning set **can** rebind its own `state_`. Nothing else can.
- Every view derived from it holds its own `shared_ptr<State>` to the **old**
  object. Ticket #1783's contract (`docs/SortedSetLiveViewDesign.md` §12, §16)
  says those views stay live and keep observing mutations. Rebinding only the
  owning set would leave the views writing to the abandoned state: the live-view
  graph splits in two, silently, and propagation — the entire content of
  SR-AUD-361's remediation — breaks.
- Mutating the `State` object in place cannot change its address identity, so
  "renewal" that preserves the graph is not renewal at all.
- A generation token *inside* `State` avoids the split, but existing iterators
  hold `shared_ptr<const State>` and must become **invalid**, not silently
  observe a new valid generation — which means every `checkVersion()` must
  compare the generation too, and every snapshot must carry it. `Iterator` has
  room (§7); the Count cache does not.
- Count caches store a tag value and would have to be prevented from mistaking a
  fresh generation for the old one — the same 64-bit-in-32-bits problem, one
  level removed.

So a compatible State-renewal strategy is **not** possible without adding layout
to `SortedSet<T>` or `Iterator`, and it is not needed: the selected solution
reaches the same guarantee arithmetically.

---

## 9. Selected contract and exact declarations

```cpp
namespace SharpRuntime::Testing {
/** Test-only access seam; declared here, never defined in production code. */
template<typename T> struct SortedSetVersionAccess;
}

template<typename T>
class SortedSet {
    struct State {
        std::set<T> data;
        SharpRuntime::ulongcs version = 0;          // was: intcs version = 0;
    };

    static constexpr SharpRuntime::ulongcs kMaxCacheableVersion =
        static_cast<SharpRuntime::ulongcs>(SharpRuntime::UINTCS_MAX) - 1u;   // 2^32 - 2
    static constexpr SharpRuntime::uintcs kCountNotCached = 0u;              // was: intcs, -1

    [[nodiscard]] static constexpr SharpRuntime::uintcs countCacheTag(
        SharpRuntime::ulongcs version) noexcept {
        return static_cast<SharpRuntime::uintcs>(version + 1u);
    }

    mutable std::atomic<intcs> cachedCount_{0};                              // unchanged
    mutable std::atomic<SharpRuntime::uintcs> cachedCountVersion_{kCountNotCached};

    void bumpVersion() noexcept { ++state_->version; }                       // new, private

    friend struct SharpRuntime::Testing::SortedSetVersionAccess<T>;          // new

public:
    class Iterator {
        std::shared_ptr<const State> state_;
        SetIterator it_;
        SetIterator end_;
        SharpRuntime::ulongcs version_ = 0;         // was: intcs version_ = 0;
        void checkVersion() const {
            if (version_ != state_->version)
                throw System::InvalidOperationException(
                    "Collection was modified; enumeration operation may not execute.");
        }
    };
};
```

**No public member's signature, return type, parameter list, or
`const`-qualification changes.** The whole diff is private.

### 9.1 Version transition algorithm

```
bumpVersion():
    version <- version + 1          (mod 2^64, always defined, noexcept)
```

called from, and only from, `Add` when it inserted, `Remove` when it erased, and
both `Clear` paths when they erased something.

### 9.2 Count-cache publication rules

```cpp
[[nodiscard]] intcs getCountProperty() const {
    if (!getIsViewProperty()) return static_cast<intcs>(state_->data.size());
    const SharpRuntime::ulongcs currentVersion = state_->version;
    if (static_cast<SharpRuntime::ulongcs>(
            cachedCountVersion_.load(std::memory_order_acquire)) == currentVersion + 1u)
        return cachedCount_.load(std::memory_order_relaxed);
    const auto computed = static_cast<intcs>(std::distance(rangeBegin(), rangeEnd()));
    if (currentVersion <= kMaxCacheableVersion) {
        cachedCount_.store(computed, std::memory_order_relaxed);
        cachedCountVersion_.store(countCacheTag(currentVersion), std::memory_order_release);
    }
    return computed;
}
```

#1784's protocol is preserved exactly: the count is stored first with `relaxed`,
the tag is stored last with `release`, and the tag is loaded first with
`acquire`, so a reader that observes a tag also observes the matching count and
the pair can never be read torn. The only additions are the `+ 1u` bias on the
comparison and the range guard on the **fill** path, which is the slow path.
`invalidateCountCache()` is unchanged apart from the sentinel's new value and
still writes the tag last with `release`.

### 9.3 Iterator invalidation rules

Unchanged in every observable respect. An `Iterator` is invalid exactly when the
shared counter differs from its snapshot; it throws
`InvalidOperationException` from `operator*`, `operator->`, and `operator++`;
`operator==`/`operator!=` never check. A rejected duplicate `Add` and an absent
`Remove` still do not invalidate anything.

### 9.4 View propagation rules

Unchanged. One counter per `State`; every handle — owning set, view, nested
view, overlapping view — shares it; any effective mutation through any of them
invalidates every outstanding iterator and every handle's cached Count.

### 9.5 Copy, move, and assignment rules

Unchanged from #1783/#1784 and re-pinned by tests. Copying an owning set clones
the counter into independent state; copying a view shares it and starts with a
cold cache; move leaves the source a valid empty owning set whose counter is 0;
all three rebinding paths call `invalidateCountCache()`.

---

## 10. The test-only access seam

Near-exhaustion behaviour cannot be reached through the public API without
billions of real mutations, and the ticket forbids performing them. The
permanent regressions therefore position the counter through

```cpp
namespace SharpRuntime::Testing { template<typename T> struct SortedSetVersionAccess; }
```

which `SortedSet.hpp` **declares and befriends** and which is **defined in
exactly one translation unit**, the test file. Properties that make this
acceptable rather than a dangerous hook:

- It grants *access* and defines *no behaviour*. Production code cannot call it,
  because nothing defines it in production.
- A friend declaration changes no object layout, no signature, and no mangled
  symbol — confirmed by the layout and symbol probes, which are byte-identical
  to #1784's.
- It is portable ISO C++, unlike the `-fno-access-control` the throwaway probes
  use, so it works on every toolchain this repository builds for.
- It is narrower than the alternative that was considered and rejected —
  `#define private public` in a permanent test file — which is ill-formed and, as
  §4 records, does not even work on this class.

### 10.1 The consumer-side guard — added by ticket #1803, 2026-07-29

*Appended after this document's ticket closed. Everything above is #1786's own
record and is unedited; nothing about the seam, the counter, the layout or the
live views changed.*

The four properties above were **argued** when #1786 closed, and proved only for
the repository's own build. Ticket #1800 later pinned the "defined in exactly
one translation unit" half by checking the repository's source text; ticket
#1803 pinned the "a consumer cannot reach it" half by **compilation**, which is
the only way that claim can be checked at all.

`test/consumer/collections_sorted_set_version_negative.cpp` compiles fifteen
outlawed spellings, one per translation unit, against the declared
`Collections.Core` public include surface, under
`-Wall -Wextra -Wpedantic -Werror`, and requires each to be rejected for its own
declared reason. Five sites pin that the seam is an **incomplete type** to a
consumer — `version`, `positionVersion` and `cachedTag` calls, an object
definition, and the `::Set` member type. Nine pin that the state it reaches has
no second route: `state_`, the nested `State` type, `bumpVersion()`,
`cachedCount_`, `cachedCountVersion_`, `countCacheTag()`,
`kMaxCacheableVersion`, `kCountNotCached`, and `Iterator::version_` are each
rejected with `is private within this context`. One pins that the defining
translation unit is not reachable through any public include path.
`scripts/check_negative_consumer_fixtures.py` runs it from
`scripts/local_ci_check.sh`; ten temporary header mutations, each shadowed in a
mirror tree and never committed, prove every site load-bearing.

One limitation, measured rather than assumed: a consumer that reopens
`namespace SharpRuntime::Testing` and writes its **own** explicit specialisation
of `SortedSetVersionAccess<int>` does obtain the access the friend declaration
grants, and compiles clean. That is well-formed ISO C++ — no C++ mechanism stops
a third party defining a class a header befriends by name — it is equally true
of `CollectionVersionAccess`, and it is therefore not expressible as a
compile-rejection site. It is unsupported, and it does not weaken the first
bullet above: production still defines nothing, so nothing this library or a
consumer *links against* can observe or call the seam. Full record:
`docs/NegativeConsumerFixtureValidation.md` §18, in particular §18.5.

---

## 11. Compatibility, measured

### 11.1 Public source compatibility — ✅ unaffected

No signature, return type, parameter, or `const` qualification changed. The
permanent suite asserts the exact pointer-to-member type of seven public members
plus the seven `SortedSetCountCacheTests` already asserts, so a change is a
compile error rather than a silent break. `getCountProperty()` still returns a
plain `intcs` by value; no 64-bit counter and no atomic leaks into the API.

### 11.2 Symbol compatibility — ✅ unaffected

`nm` on `probe8_postfix_layout_symbols.o`, diffed against #1784's stored output:

```
W _ZN6System11Collections7Generic9SortedSetIiE14GetViewBetweenERKiS5_
```

**identical**. Nothing this ticket touches appears in a mangled name: the
counter's type is not part of any signature, and the friend declaration is not
encoded.

### 11.3 Object-layout compatibility — ✅ unaffected

`probe8_postfix_layout_symbols` output is **byte-identical** to #1784's stored
log:

| Measurement | #1783 | #1784 | #1786 |
|---|---:|---:|---:|
| `sizeof(SortedSet<int>)` | 40 | 40 | **40** |
| `sizeof(SortedSet<std::string>)` | 104 | 104 | **104** |
| `sizeof(SortedSet<int>::Iterator)` | 40 | 40 | **40** |
| `alignof` (both) | 8 | 8 | **8** |
| `is_polymorphic` | 0 | 0 | **0** |
| `is_trivially_copyable` | 0 | 0 | **0** |
| `is_nothrow_move_constructible` | 1 | 1 | **1** |
| `is_copy_assignable` | 1 | 1 | **1** |

`probe13_member_offsets.cpp` goes further and compares every member **offset**
against the pre-#1786 header, which `sizeof` alone would not prove:

```
int:    state_@0  lower_@16 upper_@24 cachedCount_@32 cachedCountVersion_@36   identical
string: state_@0  lower_@16 upper_@56 cachedCount_@96 cachedCountVersion_@100  identical
Iterator: state_@0 it_@16 end_@24 version_@32                                  identical
```

### 11.4 The one difference, stated plainly

The probe's full diff is two lines:

```
< iterator-sizeof-version=4      > iterator-sizeof-version=8
< state-sizeof-version=4         > state-sizeof-version=8
```

`Iterator::version_` occupies bytes 32–39 instead of 32–35, taking four bytes
that were previously padding, and `State::version` widens inside a heap object
whose `sizeof` stays 56. Both are **private members of a header-only class
template**. `Collections.Core` is an `INTERFACE` target that produces no
archive, so there is no pre-built object file anywhere in this repository that
could disagree, and mixing translation units compiled against two different
revisions of a header is an ODR violation regardless of this change. This is
recorded as a difference, not hidden, because "sizeof is unchanged" alone would
be a weaker claim than the one being made.

### 11.5 Practical rebuild requirement

**None on this revision's account.** Consumers that recompile normally pick the
change up; nothing links against a stale symbol. This is the same standing
#1784 had, and unlike #1783, which required a full rebuild.

---

## 12. Sanitizer results

| Check | Result |
|---|---|
| UBSan, pre-fix `probe12 ub-increment` | **signed integer overflow reported** at `SortedSet.hpp:425` in `Add` |
| UBSan, post-fix, all six `probe12` modes | **0 diagnostics** |
| ASan + UBSan + LSan over all three permanent SortedSet suites | **105/105 pass**, 0 diagnostics, 0 leaks |
| LeakSanitizer actually active | confirmed by deliberate-leak self-test: 4,112 bytes in 102 allocations, exit 1 |
| TSan `probe10` (#1784's, 10 modes) | **0 races** in all nine real modes; `known-race` self-test still 2 |
| TSan `probe9` (#1783's, unmodified) | `shared-view-count` **0**, `distinct-handles` **0**, `known-race` **2** |
| TSan `probe14` (new, 6 modes incl. past-horizon) | **0 races** in all five real modes; `known-race` self-test 1 |

`probe14_tsan_horizon.cpp` exists because this ticket adds exactly one new read
path — the recompute-every-call path once the counter outgrows the tag — and
#1784's TSan campaign predates it. It covers that path below, on, and past the
transition, through nested, overlapping, and per-thread copied handles, and with
`Count` mixed with `Contains`, `Min`, `Max`, `ToVector`, and iteration. As in
#1784, **no mode ever mutates concurrently**: that is unsupported before and
after this ticket, and a report produced by it would say nothing about this
contract.

Post-fix coverage includes, as required: full-set mutation near the transition,
view mutation near the transition, parent and view iterators, nested views,
overlapping views, Count-cache publication, copy, move, assignment, destruction
with surviving views and iterators, and non-trivial element types
(`std::string`, and a `LessOnly` type providing only `operator<`).

---

## 13. Performance

`build-probe-sortedset/probe16_perf.cpp`, same source compiled against both
headers at `-O2 -DNDEBUG`, median of seven timed runs, repeated three times.
Every `Count` loop carries a compiler barrier — without it GCC hoists the whole
call out and the benchmark measures nothing (§13.1).

| Operation | Pre-fix (ns/op) | Post-fix (ns/op) | Verdict |
|---|---:|---:|---|
| `Add`+`Remove` pair, owning set | 83.0 – 89.5 | 80.4 – 82.7 | within noise |
| `Add`+`Remove` pair, view | 71.0 – 75.7 | 70.8 – 77.6 | within noise |
| Enumerated element (`checkVersion` per step) | 3.44 – 4.05 | 3.65 – 4.63 | within noise |
| Warm view `Count` (cache hit) | 1.01 – 1.04 | **0.75 – 0.77** | slightly faster |
| Mutate-then-`Count` (cache miss per version) | 479 – 511 | 477 – 492 | within noise |
| Owning-set `Count` | 0.50 – 0.51 | 0.50 – 0.52 | unchanged |

Memory: **unchanged** per `SortedSet<T>` (40/104 bytes) and per `Iterator`
(40 bytes). No atomic was added, none widened, and no allocation appears on any
path. The exhaustion path — a view's `Count` once the counter outgrows the tag —
costs one O(k) walk per call instead of an O(1) cache hit, for a state that has
taken more than 2^32 effective mutations.

### 13.1 A benchmark that measured nothing, and the correction

The first run of `probe16` reported warm view `Count` at 2.46 ns post-fix
against 1.03 pre-fix, and owning-set `Count` at 3.81 against 1.38 — on code the
change does not touch. Both loops were loop-invariant, so GCC hoisted the call
out entirely and the numbers recorded how lucky the hoisting had been. Adding an
`asm volatile` barrier per iteration made the measurement real and stable.

### 13.2 A regression that was real, found, and removed

With the barrier in place, the *first* implementation — the explicit horizon
branch of §7.2 — showed a reproducible regression: owning-set `Count`
0.47 → 1.46 ns and warm view `Count` 0.71 → 2.16 ns, stable across runs and
unaffected by `-falign-functions=64 -falign-loops=32`.

It was isolated by building two variant headers rather than by reasoning:

| Variant | Owning `Count` | Warm view `Count` |
|---|---:|---:|
| pre-fix header | 0.477 | 0.711 |
| **A** — 64-bit counter, original cache code | 0.480 | 0.711 |
| **B** — shipped cache, horizon branch removed | 0.483 | 0.725 |
| first implementation — 64-bit counter **and** horizon branch | 1.459 | 2.163 |
| **shipped** — 64-bit counter and biased tag | 0.463 | 0.699 |

So the counter widening costs nothing (A), and the whole regression came from
the single extra branch — which slowed the owning-set path too, a path that
cannot execute it, because it enlarged the inlined body. The biased tag (§7.2)
delivers the same exactness with no branch and no regression. This is recorded
because the regression was real and was measured, not because it survived.

---

## 14. Permanent tests

`modules/collections/tests/System/Collections/Generic/SortedSetVersionOverflowTests.cpp`,
**29 cases**, joining `SortedSetLiveViewTests` (47) and
`SortedSetCountCacheTests` (29). No existing assertion was edited.

**Ordinary counter behaviour (7):** a fresh set starts at 0 and the
initializer-list constructor bumps nothing; `Add` increments exactly once per
insertion and not for a rejected duplicate; `Remove` only when it erases; `Clear`
only when something was removed, for both an owning set and a view; view,
nested view, and parent share exactly one counter, with out-of-range `Add`/`Remove`
bumping nothing; set algebra bumps once per effective element mutation; and
copy, move, and assignment rebind the counter correctly.

**The old int32 boundary (4):** the counter's type and signedness are pinned;
the increment at `INTCS_MAX` moves forward instead of backwards and the set
keeps answering; mutation at and past seven distinct boundary values never
throws; and the counter's own exhaustion at `ULONGCS_MAX` wraps to 0 without UB
while still invalidating a snapshot taken before it.

**Iterator invalidation across the old wrap point (4):** an iterator is not
revalidated at `+2^32`, `+2·2^32`, `+7·2^32`, or at the old UB boundary, and
`operator*`, `operator++`, and `operator->` all throw; an iterator taken at the
boundary is invalidated by the next mutation; parent, view, and nested-view
iterators invalidate each other past the boundary while a rejected duplicate
invalidates nothing; and a range-`for` over a view still fail-fasts while the
counter crosses the old boundary mid-loop.

**The Count cache (8):** the sentinel cannot be produced by any counter value
and the tag is exactly the counter plus one; a never-filled cache is not
mistaken for a warm one at the old `-1` collision point or at four other values;
a cached count is not revalidated at `+2^32`, `+2·2^32`, or `+5·2^32`; the cache
is exact and used up to and including the last cacheable version; past it the
cache is neither read nor written and every mutation is tracked exactly; 300
interleaved mutations far past it each produce the exact count; nested and
overlapping views stay exact past it; an owning full set is unaffected at every
counter value; rebinding carries no stale cache across the transition; and a
view outliving its parent stays exact past it.

**Element types (2):** `std::string` and a `LessOnly` type providing only
`operator<` across the boundary.

**Compatibility (4 + 12 `static_assert`s):** the exact pointer-to-member type of
seven public members; six value-semantics traits; the cache fields' widths and
alignments; the published `sizeof` figures behind a 64-bit guard, matching this
repository's rule against permanent architecture-specific `sizeof` assertions;
and that no counter or atomic is visible through the public surface.

No test performs billions of operations; the longest runs 300 mutations. The
whole suite executes in ~1 ms.

---

## 15. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | Iterator ABA at exactly 2^64 effective mutations on one `State` | Negligible | **Real and not eliminated.** Over 580 years of uninterrupted mutation (§6.1). Guarding it costs a branch on every mutation (§7, Alternatives C/F). Stated, not hidden. |
| 2 | A view's `Count` degrades from amortized O(1) to O(k) after 2^32 effective mutations | Low | Deliberate. Correctness is preferred to speed, and the regime is one that was previously undefined behaviour. Lifting it needs a wider tag, i.e. a layout change, i.e. user approval — recorded in §17 as a possible future item, not requested. |
| 3 | The `+ 1` tag bias is a clever trick a future reader could "simplify" away | Medium | Mitigated by the invariant table in §7.2, a comment at the declaration, and four permanent tests that fail if the bias is dropped. |
| 4 | The test-only friend seam is a new name in a public header | Low | §10. Grants access only; never defined in production; no layout, signature, or symbol effect, all measured. |
| 5 | The counter's width differs from .NET's `int` | Low | Deliberate and argued in §5.1. Nothing observable to a conforming program differs. |
| 6 | Fourteen other collections still carry the pre-fix pattern | Medium | Out of this ticket's scope by explicit instruction; inventoried in §16 and opened as inactive ticket #1787 (§17). **Not fixed by this ticket.** |
| 7 | `probe12`/`probe13`/`probe14` depend on `-fno-access-control`, a GCC/Clang extension | Low | Probes only, never the permanent suite, which uses the portable friend seam. |

---

## 16. Repository-wide inventory

Delivered as the ticket's acceptance criteria require, even though the
implementation for the other collections is deferred (§2.1).

**Fifteen** types in `modules/collections/include/` carry a mutation counter.
All fifteen declare it `intcs` — `int32_t` — and all fifteen compare it for
**equality only** in their iterator guard, so all fifteen had the pre-#1786
arithmetic. Only `SortedSet<T>` additionally has a Count cache, so defects 3 and
4 are unique to it; defects 1 and 2 apply to all.

| Type | Header | Increment sites | Counter | After #1786 |
|---|---|---:|---|---|
| `SortedSet<T>` | `Generic/SortedSet.hpp` | 4 | `ulongcs` | ✅ **fixed** |
| `List<T>` | `Generic/List.hpp` | 14 | `intcs` | ❌ #1787 |
| `HashSet<T>` | `Generic/HashSet.hpp` | 14 | `intcs` | ❌ #1787 |
| `Dictionary<K,V>` | `Generic/Dictionary.hpp` | 9 | `intcs` | ❌ #1787 |
| `SortedDictionary<K,V>` | `Generic/SortedDictionary.hpp` | 6 | `intcs` | ❌ #1787 |
| `SortedList<K,V>` | `Generic/SortedList.hpp` | 8 | `intcs` | ❌ #1787 |
| `OrderedDictionary<K,V>` | `Generic/OrderedDictionary.hpp` | 10 | `intcs` | ❌ #1787 |
| `LinkedList<T>` | `Generic/LinkedList.hpp` | 7 | `intcs` | ❌ #1787 |
| `Queue<T>` | `Generic/Queue.hpp` | 6 | `intcs` | ❌ #1787 |
| `Stack<T>` | `Generic/Stack.hpp` | 6 | `intcs` | ❌ #1787 |
| `ArrayList` | `ArrayList.hpp` | 16 | `intcs` | ❌ #1787 |
| `Hashtable` | `Hashtable.hpp` | 8 | `intcs` | ❌ #1787 |
| `ListDictionaryInternal` | `ListDictionaryInternal.hpp` | 5 | `intcs` | ❌ #1787 |
| `Collections::Queue` | `Queue.hpp` | 4 | `intcs` | ❌ #1787 |
| `Collections::Stack` | `Stack.hpp` | 4 | `intcs` | ❌ #1787 |

Nothing outside `modules/collections/` carries such a counter.

`build-probe-sortedset/probe15_collection_counters.cpp` measures each type's and
each iterator's `sizeof`/`alignof` so #1787 does not start from zero. Every
container and every iterator measured is already a multiple of 8 bytes, which
means **none of them can be assumed to have spare tail padding** the way
`SortedSet<T>::Iterator` did: #1787 must measure member offsets per type, and
some of the fourteen may need the same layout approval #1786 did not. The
figures are indicative — a few of these types expose their iterator through a
name other than `begin()`'s return type — and #1787 must re-derive them.

---

## 17. Follow-up tickets

**#1787 — `REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M, `todo`
(inactive, not begun).** Apply #1786's counter analysis to the fourteen
collections in §16. It is a separate ticket rather than an extension of #1786
because the governing instruction for this session scoped #1786 to
`SortedSet<T>` (§2.1) and because each of the fourteen needs its own
object-layout measurement before a compatible repair can be promised.

Not created, and deliberately: a ticket to widen `SortedSet<T>`'s Count-cache
tag. That would lift risk 2 in §15, but it requires an object-layout change and
therefore explicit user approval, and there is no evidence any consumer performs
2^32 mutations on one collection. If such evidence appears, this document's §7
layout proof and §7.2 invariant table are what a future ticket needs.

**#1785 stays `todo` and untouched.** The nested-view exception-ordering
divergence is a semantic decision, not a defect, and this ticket changed no
exception behaviour whatsoever.

---

## 18. Implementation status

**Complete.** Implemented, tested, sanitizer-validated, layout-verified, and
committed on local branch
`feature/remediation-coll-sortedset-version-overflow`. No approval was required
because nothing public, symbolic, or layout-visible changed.

| Gate | Result |
|---|---|
| `cmake --build build --parallel 4` | 0 errors, 0 warnings |
| `SharpRuntimeTests_Collections_Core` | **1,841** passed (1,812 before, +29) |
| `scripts/local_ci_check.sh build` | **13,127** tests across 37 executables (13,098 before) |
| `scripts/validate_module_boundaries.py --root .` | 41 modules / 90 edges — no new edge |
| `test/validate_module_boundaries_test.py` | 7 tests OK |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| `git diff --check` | clean |
| `scripts/check_doxygen_warnings.sh` | **1,937** warnings, unchanged, ceiling 1,942 |
| `scripts/check_selective_components.sh` | all ten components pass |
| `check_selective_components.sh Collections.Core collections_sorted_set_view.cpp` | passes in isolation, 1,841 tests |
| Positive consumer fixture, `-Wall -Wextra -Wpedantic -Werror` | compiles, exits 0 |
| Negative consumer fixture (`const` caller) | still correctly rejected |
| ABI/layout probe vs #1784 baseline | byte-identical; symbols unchanged |

### 18.1 Rollback

`git revert` of the implementation commit restores the `int32` counter and, with
it, all four defects. A revert must be validated by re-running
`build-probe-sortedset/probe12_version_overflow.cpp` under UBSan against both
headers, not by CTest alone — the permanent suite would still pass, because the
near-boundary cases position the counter through the seam and would simply
observe the old behaviour as correct if the seam's expectations were reverted
with it.
