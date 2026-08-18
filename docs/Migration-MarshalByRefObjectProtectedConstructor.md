<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `MarshalByRefObject` is no longer directly constructible (ticket #2297)

*2026-08-18.* `System::MarshalByRefObject obj;` compiled. .NET declares the class `abstract` with
a `protected` constructor, so C# rejects the equivalent with `CS0144`.

Landed under `docs/StandingApprovals.md` **SA-8** and **SA-9** with SA-2's five conditions.
**Two of the ticket's three parts landed; the third needs a vtable approval** — see §3.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `System::MarshalByRefObject obj;` | compiled | **rejected** |
| `new System::MarshalByRefObject()` | compiled | **rejected** |
| `MarshalByRefObject sliced = derived;` | compiled | **rejected** |
| `GetLifetimeService()` | **absent** | present, throws `PlatformNotSupportedException` |
| `class X : public MarshalByRefObject {}` | — | **unchanged**, and the only intended use |
| `AppDomain`, `ContextBoundObject` | — | **unchanged** |

`GetLifetimeService()`'s absence turned an **observable runtime diagnostic into a compile error at
an unrelated place** — .NET keeps it precisely so a caller gets
`PlatformNotSupportedException("Remoting is not supported on this platform.")`. It is **not**
`virtual` in .NET, so adding it costs no vtable slot.

## 2. To migrate

Derive:

```cpp
// before
System::MarshalByRefObject obj;

// after
class MyRemotable : public System::MarshalByRefObject {};
MyRemotable obj;
```

A reference or pointer to the base is unaffected.

## 3. What did NOT land, and why

**`InitializeLifetimeService()` is still absent.** .NET's is `virtual`, and this class already has
a vtable (the destructor), so adding it inserts a **slot** — a vtable change here **and in both
derived classes**, `AppDomain` and `ContextBoundObject`. `docs/StandingApprovals.md` SA-3 excludes
vtable changes explicitly, so it needs its own approval: **ticket #2374**.

It is left absent rather than added **non-virtually**, because a non-virtual member of that name
would silently defeat the one thing the .NET member exists for — letting a derived type override
the lease policy. A test asserts its absence through a detection idiom, so #2374's arrival is
visible rather than silent.

**`MemberwiseClone(bool)` is still absent.** .NET's calls `Object.MemberwiseClone()`, and
`System::Object` in this port declares no such member — measured. Adding it would be an invention
rather than a port, which SA-5 forbids.

**No `[[deprecated]]`.** Both .NET members carry `[Obsolete(RemotingApisMessage)]`. Whether .NET's
`Obsolete` becomes C++ `[[deprecated]]` anywhere in this repository is the undecided ticket
**#2289**, and answering it incidentally here would settle it in the wrong place.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `MarshalByRefObject` — **zero sites in both**. Neither
repository was modified.
