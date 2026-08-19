<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `BoundedChannelOptions::FullMode` is validated on assignment (ticket #1969)

*2026-08-19.* `System::Threading::Channels::BoundedChannelOptions::FullMode` was a bare public
mutable data member. It is now private, behind `getFullModeProperty()` / `setFullModeProperty()`,
and an undeclared enumerator is rejected with `ArgumentOutOfRangeException` — as .NET's property
setter does.

**This is a public source break.** Landed under `docs/StandingApprovals.md` **SA-8**, whose first
bullet — *"making a public data member private and adding the rule-5 accessor pair"* — is exactly
the approval this ticket had been waiting for since 2026-08-03. SA-2's five conditions are
discharged below.

---

## 1. What was wrong, and why the shape was the obstacle

The obstacle was never the check's logic. A bare data member has **nowhere to put a check**, so
validation was unreachable without changing the shape — which is what the ticket was blocked on.

The consequence was not cosmetic:

```cpp
BoundedChannelOptions options(1);
options.FullMode = static_cast<BoundedChannelFullMode>(99);   // used to compile and stick
auto channel = Channel<int>::CreateBounded(options);
channel.Writer->TryWrite(1);
channel.Writer->TryWrite(2);   // used to SUCCEED -- Count reaches 2 on a channel bounded at 1
```

The writer asks `fullMode != Wait` to decide whether to take the drop path, and `99 != Wait`, so
it does. It then switches on the mode to decide *what* to drop, and `99` matches no arm — so
nothing is dropped and the item is appended anyway. A caller, or a deserialized value, could
defeat the bounded-memory contract outright.

## 2. What changed

| | Was | Is |
|---|---|---|
| `FullMode` | public mutable data member | **private** `fullMode_` |
| read | `opts.FullMode` | `opts.getFullModeProperty()` |
| write | `opts.FullMode = m` | `opts.setFullModeProperty(m)` |
| an undeclared value | stored silently | `ArgumentOutOfRangeException`, param name **`"value"`** |
| default | `Wait` | `Wait` — unchanged |
| `Capacity` | already correct | **untouched** |
| `SingleWriter`/`SingleReader`/`AllowSynchronousContinuations` | public data members | **still public data members** (§4) |

The setter is transcribed from `ChannelOptions.cs:80-97`: a four-arm switch over the declared
enumerators with a `default` that throws `ArgumentOutOfRangeException(nameof(value))`.

## 3. Two corrections to the ticket's premise

**The ticket names one validated member; .NET has two.** `BoundedChannelOptions.Capacity` is
validated as well — its setter and its constructor both throw
`ArgumentOutOfRangeException(nameof(value))` when the value is negative. **This port already got
that one right**: `capacity_` is private, and both the constructor and `setCapacityProperty` call
`ThrowIfNegative(..., "value")`. Nothing there needed changing, and this note records it so the
next reader does not "fix" a member that already matches.

**The capacity bound is `< 0`, not `< 1`.** .NET permits `Capacity == 0`, and this port does too.
That is worth stating because the intuitive repair — requiring at least one slot — would be a
divergence, and a zero-capacity channel is a working rendezvous shape with its own tests here.

## 4. The three base flags stay public data members, deliberately

.NET's `SingleWriter`, `SingleReader` and `AllowSynchronousContinuations` are plain auto-properties
with **no validation at all** (`ChannelOptions.cs:17,27,39`) — `{ get; set; }` and nothing else. A
public data member is observationally identical to that. SA-8 reaches a representation .NET keeps
*private, readonly or absent*; it does not reach one .NET publishes as freely as this.

Converting them would be a source break that buys no behaviour — the exact opposite of `FullMode`,
where the shape was the only thing preventing a check .NET actually performs. The boundary is
pinned by `Decl1969_TheThreeBaseFlagsStayPublicDataMembers` rather than left looking like an
oversight, the same way #2330 pinned `ValueTuple` when `Tuple` changed.

## 5. To migrate

```cpp
// before
BoundedChannelOptions options(2);
options.FullMode = BoundedChannelFullMode::DropOldest;
auto mode = options.FullMode;

// after
BoundedChannelOptions options(2);
options.setFullModeProperty(BoundedChannelFullMode::DropOldest);
auto mode = options.getFullModeProperty();
```

A value outside the four declared enumerators now **throws** where it used to be stored. If you
were relying on that — you were relying on the defect in §1.

## 6. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the setter stores without validating | `Fix1969_AnUndeclaredValueIsRejected`, `Fix1969_TheBoundedMemoryContractCanNoLongerBeDefeated`, `Fix1969_TheParameterNameIsValueNotFullMode` |
| M2 — one declared arm (`DropWrite`) dropped | `Fix1969_EveryDeclaredModeIsAccepted` **and two pre-existing tests** |
| M3 — parameter name is `"FullMode"` rather than .NET's `nameof(value)` | `Fix1969_TheParameterNameIsValueNotFullMode` — **only after the test was repaired**, see below |
| M4 — rejection half-applies before throwing | `Fix1969_AnUndeclaredValueIsRejected`, `Fix1969_TheBoundedMemoryContractCanNoLongerBeDefeated` |
| M5 — the default is no longer `Wait` | `Fix1969_TheDefaultIsWait`, `Fix1969_AnUndeclaredValueIsRejected`, `Fix1969_TheBoundedMemoryContractCanNoLongerBeDefeated` |

**M5 is also worth a note.** Beyond the three named failures, it makes a *pre-existing*
`ChannelTests` case **hang** — a writer that expected `Wait`-mode blocking takes a drop path
instead. A mutation caught only as a hang is not caught *by name*, which is why the three named
failures matter: they identify the change in under a second, before the suite reaches the case
that would otherwise just stop.

**M3 is worth recording, because the first version of its test was vacuous.** It searched
`what()` for the substring `"value"` — and `ArgumentOutOfRangeException`'s default message is
*"Specified argument was out of the range of valid values."*, which contains that substring
**whatever the parameter is named**. The test passed against the mutation. It now asserts
`getParamNameProperty() == "value"`, which is the thing actually under test.

Negative consumer fixture: `test/consumer/threading_channels_fullmode_private_negative.cpp`,
three sites, all rejected. Fixture set grows to **39 fixtures / 207 sites**. Site 3 is the
spelling that used to compile *and run*, storing a value no switch arm handles.

## 7. Downstream, measured

Per SA-2 condition 5: `FullMode` appears in **zero** places in `cna` and **zero** in
`mobile-eggbert`. Neither repository was modified, and no downstream ticket is needed.

First-party migration was six sites, all in this repository's own tests plus one read in
`Channel<T>::CreateBounded`.

## 8. The identical shape, one module over

`System::Threading::Tasks::ParallelOptions::MaxDegreeOfParallelism` is the same *shape* — a public
mutable data member where .NET has a validating property — and its header said so, citing #1969 as
the reason it was approval-gated. Ticket #1966 validated it at the **use site** instead, precisely
because the field is public. SA-8 now reaches it, and it is filed as **#2388** rather than bundled
here.

**One thing that is already right there and must not be "fixed" by analogy with this ticket**:
#1966 uses the parameter name `"MaxDegreeOfParallelism"`, and that matches .NET exactly —
`ArgumentOutOfRangeException.ThrowIfZero(value, nameof(MaxDegreeOfParallelism))`
(`Parallel.cs:87-88`). So the reference is **inconsistent between the two option types**:
`BoundedChannelOptions.FullMode` names `value`, `ParallelOptions.MaxDegreeOfParallelism` names the
property. Both are transcribed as they are rather than harmonised. The only divergence left at
#2388 is **where the guard sits** — .NET's setter refuses to store an invalid degree at all, while
this port stores it and rejects it when the loop runs.
