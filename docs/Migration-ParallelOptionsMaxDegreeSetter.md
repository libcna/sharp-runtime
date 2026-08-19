<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ParallelOptions::MaxDegreeOfParallelism` is validated in its setter (ticket #2388)

*2026-08-19.* `System::Threading::Tasks::ParallelOptions::MaxDegreeOfParallelism` was a bare
public mutable data member. It is now private, behind
`getMaxDegreeOfParallelismProperty()` / `setMaxDegreeOfParallelismProperty()`, and an invalid
degree is rejected **at assignment** — where .NET rejects it.

Landed under `docs/StandingApprovals.md` **SA-8**, with SA-2's five conditions discharged.
Split out of #1969 rather than bundled with it, because the approval question there was about
`BoundedChannelOptions` and the consumer measurement is per-type.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| read | `opts.MaxDegreeOfParallelism` | `opts.getMaxDegreeOfParallelismProperty()` |
| write | `opts.MaxDegreeOfParallelism = n` | `opts.setMaxDegreeOfParallelismProperty(n)` |
| `0` or `< -1` | **stored**, rejected later when a loop ran | rejected **at assignment** |
| exception & parameter name | `ArgumentOutOfRangeException("MaxDegreeOfParallelism")` | unchanged |
| default | `-1` | unchanged |
| valid values | `-1` and every `>= 1` | unchanged |
| `ParallelOptions` aggregate-ness | aggregate | **no longer an aggregate** |

**No accepted value changed meaning, and no rejected value became accepted.** Only the moment of
rejection moved.

## 2. Why the move matters

Ticket #1966 landed this validation already — but at the entry of every `Parallel` method,
because a public data member has nowhere to put a check. Its doc-comment recorded that as a
forced choice awaiting an approval, and named #1969 as the gating ticket.

The difference is observable, not cosmetic:

```cpp
ParallelOptions opts;
opts.MaxDegreeOfParallelism = 0;                 // used to succeed
auto d = opts.MaxDegreeOfParallelism;            // reads 0 -- a value .NET can never hold
// ...and if no loop is ever run, no diagnostic is ever produced.
```

.NET's setter (`Parallel.cs:85-90`) is two guards:

```csharp
ArgumentOutOfRangeException.ThrowIfZero(value, nameof(MaxDegreeOfParallelism));
ArgumentOutOfRangeException.ThrowIfLessThan(value, -1, nameof(MaxDegreeOfParallelism));
```

so `_maxDegreeOfParallelism` can never hold an invalid number in the first place.

## 3. The parameter name is `"MaxDegreeOfParallelism"`, and that is not a slip

#1969 landed the sibling change on `BoundedChannelOptions::FullMode` with the parameter name
`"value"`. This one keeps `"MaxDegreeOfParallelism"`. **Both match their own reference**:

| Type | .NET writes |
|---|---|
| `BoundedChannelOptions.FullMode` | `nameof(value)` — `ChannelOptions.cs:96` |
| `ParallelOptions.MaxDegreeOfParallelism` | `nameof(MaxDegreeOfParallelism)` — `Parallel.cs:87-88` |

The reference is inconsistent between its two option types. Both are transcribed as they are;
harmonising them would be inventing a reference where SA-5 says to derive one. `#1966` had
already got this right, and this ticket deliberately does not "fix" it.

## 4. One test moved rather than being left to pass for the wrong reason

#1966 asserted that an invalid degree beats an empty body — the degree error must be reported
first, because in .NET the setter runs before `Parallel.For` is called at all.

**With the guard in the setter there is no longer an ordering to assert.** The invalid degree
cannot reach `Parallel::For`; it is refused at the assignment. Leaving the old test would have
made it pass for the wrong reason — the throw would come from the assignment, *outside* the
`EXPECT_THROW` — which is exactly the trap #2359 hit. It is replaced by
`Fix2388_AnInvalidDegreeCannotReachParallelForAtAll`, which asserts the stronger property and
then shows the options object is still usable, because a rejected assignment leaves the previous
value in place.

## 5. The use-site guard is kept, and its mutation is an equivalence

`requireValidMaxDegreeOfParallelism` still runs at every `Parallel` entry point, and it is now
**unreachable through the public surface**: the field is private, its only mutator validates, and
the private member makes `ParallelOptions` a non-aggregate, so brace initialisation cannot reach
it either.

It is kept — one comparison per loop — as the only thing that would catch a future constructor or
friend that sets the field directly. Its mutation is therefore recorded as an **equivalence**
rather than counted as a caught mutation, at the site and here.

## 6. Evidence

Mutations on the setter, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the setter stores without validating | `Fix2388_ZeroAndBelowMinusOneAreRejectedAtAssignment` and the `ParallelDegreeBoundaryTests` family |
| M2 — the `value == 0` arm dropped | the same, on the `0` row specifically |
| M3 — the bound becomes `value < 0`, rejecting the valid `-1` | `Fix2388_MinusOneAndEveryPositiveAreAccepted` |
| M4 — parameter name becomes `"value"` (i.e. #1969's) | `Fix2388_TheParameterNameIsThePropertyNotValue` |
| M5 — rejection half-applies before throwing | `Fix2388_ZeroAndBelowMinusOneAreRejectedAtAssignment` |
| E1 — remove the now-unreachable use-site guard | **not caught — an equivalence**, §5 |

Negative consumer fixture: `test/consumer/threading_tasks_maxdegree_private_negative.cpp`, four
sites, all rejected. Fixture set grows to **40 fixtures / 211 sites**. Site 3 is the spelling that
used to compile *and stick*; site 4 is the aggregate-ness a consumer loses silently.

## 7. Downstream, measured

Per SA-2 condition 5, and measured separately from #1969's rather than assumed to match it:
`MaxDegreeOfParallelism` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`.
Neither repository was modified, and no downstream ticket is needed.
