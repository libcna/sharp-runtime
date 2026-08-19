<!-- SPDX-License-Identifier: MIT -->
# Migration — `MarshalByRefObject::InitializeLifetimeService()` (#2374)

Ticket **#2374**, landed 2026-08-19 on an explicit per-action approval
(`docs/StandingApprovals.md` SA-13 batch). **Decided against the recommendation on the record.**

## What changed

```cpp
[[noreturn]] virtual void* InitializeLifetimeService();   // throws PlatformNotSupportedException
```

matching .NET's `public virtual object InitializeLifetimeService()`
(`MarshalByRefObject.cs:22-26`), which throws
`PlatformNotSupportedException(SR.PlatformNotSupported_Remoting)`.

Its absence turned an **observable runtime diagnostic** into a **compile error at an unrelated
place** for anyone writing the .NET call.

## Why it needed an ask, and why it had to be `virtual`

`System::MarshalByRefObject` already has a vtable (the virtual destructor), so adding a virtual
**inserts a slot** — here and in **both** derived classes, `System::AppDomain` and
`System::ContextBoundObject`. SA-3 authorises private data members and **excludes vtable changes
explicitly**; SA-10 covers signatures and does not reach a vtable. Silent binary break: nothing
fails to compile and **every consumer must rebuild**.

Measured at the time of the grant — **zero** sites for all three types in `cna` and **zero** in
`mobile-eggbert`.

**It must not be added non-virtually as a shortcut.** A non-virtual member of the same name would
compile, satisfy any presence check, look correct at every call site, and **silently defeat the one
thing the .NET member exists for**: letting a derived type override the lease policy. #2297 left it
*absent* rather than add it that way, and that judgement stands — the approval bought the slot, not
a workaround.

## The return type is `void*`, deliberately

.NET returns `object`. This port's `System::Object` is an abstract class a lease object would have
to derive from, which is surface this port does not have and #2374 does not invent. It matches the
shape `GetLifetimeService()` already uses, and since the body never returns, **no caller can observe
the difference**.

## Tests

`Fix2297_…TheVirtualOneIsStillAbsent` is **inverted** — it had said *"this assertion is what makes
its arrival visible rather than silent"* — and renamed to
`Fix2297_GetLifetimeServiceThrows_And2374_AddedTheVirtualOne`. The **detection idiom is kept**
rather than replaced by a direct call, because it is what makes presence and absence expressible in
the same form.

`Fix2374_ADerivedTypeCanOverrideTheLeasePolicy` is the case that matters: it calls through a **base
reference**, so what is asserted is *dynamic dispatch*, not a same-name member found by static
lookup. A non-virtual member runs the base body there and throws. It also asserts that a plain
derived type **still throws**, so the override is a real replacement rather than the base having
gone quiet.

## Mutation testing

Three mutations, all caught:

| # | Mutation | Caught by |
|---|---|---|
| M1 | make the member non-virtual | compile error — the `override` in the test's derived type |
| M2 | remove the member again | compile error — the presence `static_assert` |
| M3 | the base body stops throwing | two cases |

A first run of this set was **invalid and is recorded rather than counted**: the restore step
copied a *pre-change* backup, so M2 and M3 reported ANCHOR MISSING against a file that no longer
had the member. The baseline was re-taken and the set re-run.
