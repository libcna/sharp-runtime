<!-- SPDX-License-Identifier: MIT -->
# Migration — `System::Timers::Timer` derives from `System::Object` (#2155)

Ticket **#2155** (SR-AUD-239, cause TM-B), landed 2026-08-19 on an explicit per-action approval
(`docs/StandingApprovals.md` SA-13). **Decided against the recommendation on the record**; the
reasoning is preserved below so the trade stays visible rather than relitigated.

## What changed

```cpp
class Timer : public System::Object { ... };   // was: class Timer { ... };
Elapsed.Raise(this, args);                     // was: Elapsed.Raise(nullptr, args);
```

`Elapsed` now reports **the raising timer** as its sender, matching .NET's
`intervalElapsed(this, elapsedEventArgs)` (`Timer.cs:313`).

## Why it was `nullptr`, and why the fix had to be a base class

Nothing was wrong with the *intent*. `EventHandler<T>::Raise` types its sender as
`System::Object*`, `Timer` had no such base, and `std::is_convertible_v<Timer*, Object*>` was `0` —
so **`nullptr` was the only value that compiled**. The defect was structural, not a mistake at the
call site.

**The obvious alternative does not exist here.** .NET's `Timer` derives from `Component`
(`Timer.cs:15`), **not** from `Object` directly. This port has **no `ComponentModel` `Component`
class at all** — measured: no `Component.hpp` anywhere under `modules/`. So the `Object` base is the
only available route. The divergence from .NET is therefore in the **base**, not in the observable
sender, which now matches exactly. Pinned by `Decl2155_TheDivergenceIsInTheBaseNotTheSender`, so a
future `Component` would be a deliberate re-basing rather than a silent one.

## The cost — a silent binary break

| | before | after |
|---|---|---|
| `sizeof(Timer)` | 104 | **112** |
| `alignof(Timer)` | 8 | 8 |
| polymorphic | no | **yes** — new vtable |

Nothing fails to compile. **Every consumer must rebuild completely.** `docs/StandingApprovals.md`
SA-3 authorises private data members and **explicitly excludes** vtable and base-class changes,
which still ask per action; SA-8 does not reach it either. The approval was granted on this
measurement:

* **zero** `System::Timers` sites in `cna`;
* **zero** in `mobile-eggbert`;
* one first-party site outside `modules/timers` itself.

The recommendation was to decline — a new vtable on a public type is the most expensive break
available, and `Timer` is not polymorphic for any other reason. It was granted anyway. Recorded, not
reopened.

## Consequences a subscriber and a subclasser should know

* `Timer` now inherits `ToString()`, `Equals(const Object*)` and `GetHashCode()` from
  `System::Object`, and implements the base's pure virtual `GetTypeName()` as
  `"System.Timers.Timer"`.
* `Timer` is **still non-copyable** — asserted by a pin written *before* this change precisely
  because a base-class edit is the kind that quietly reintroduces a copy constructor.
* The `Object` subobject is at **offset 0**, so an `Object*` obtained from a `Timer*` compares equal
  to it. Pinned, because that is the half `sizeof` cannot express.

## Tests

Two shipped pins were **inverted**, and both had said in terms *"#2155 landed without updating its
pin"* — **both failed the build**, at compile time, which is the evidence they were load-bearing
rather than decorative:

| Pin | Now |
|---|---|
| `ElapsedStillReportsANullSender_SeeBlockedTicket2155` | `Fix2155_ElapsedReportsTheRaisingTimerAsItsSender` |
| `TimerRemainsANonPolymorphicNonObjectType_SeeBlockedTicket2155` | `Fix2155_TimerIsNowAPolymorphicObjectType` |

Two pins added: `Fix2155_TheLayoutCostIsExactlyOneVptr` (the price, asserted as a relationship as
well as a literal, plus the offset-0 check) and `Decl2155_TheDivergenceIsInTheBaseNotTheSender`.

The sender assertion is `EXPECT_EQ(seen, static_cast<Object*>(&timer))`, **not** merely non-null: a
mutation passing some other `Object*` satisfies a null check and fails this one. That is M2.

## Mutation testing

Four mutations, **all caught**:

| # | Mutation | Caught by |
|---|---|---|
| M1 | revert the sender to `nullptr` | `Fix2155_ElapsedReportsTheRaisingTimerAsItsSender` |
| M2 | pass some other `Object*` | the same case — which is why it asserts identity, not non-nullness |
| M3 | `GetTypeName()` returns `"System.Object"` | `Fix2155_TheLayoutCostIsExactlyOneVptr` |
| M4 | drop the base | compile error |
