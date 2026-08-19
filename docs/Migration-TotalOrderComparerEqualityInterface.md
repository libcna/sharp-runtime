<!-- SPDX-License-Identifier: MIT -->
# Migration — `TotalOrderIeee754Comparer<T>` implements `IEqualityComparer<T>` (#2170)

Ticket **#2170** (SR-AUD-042 remainder), landed 2026-08-19 under a per-action approval.

## What changed

`System::Numerics::TotalOrderIeee754Comparer<T>` — all three specializations (`float`, `double`,
`System::Half`) — now derives from `System::Collections::Generic::IEqualityComparer<T>` in addition
to `IComparer<T>`. `Equals` and `GetHashCode` became `override`s of that interface.

## Why it needed an ask, and what it costs

`sizeof` grows **8 → 16** on every specialization, with the `IEqualityComparer<T>` subobject at
**offset 8** — a second vptr. `alignof` stays 8.

This is a **silent binary break**: nothing fails to compile, but a consumer that was built against
the 8-byte type and links against the 16-byte one has a layout mismatch. **Every consumer must
rebuild.** `docs/StandingApprovals.md` SA-3 and SA-8 both say in terms that a vtable or base-class
change still asks per action; SA-10 covers signature changes and does not reach a base class. The
approval was granted on 2026-08-19 on this measurement:

* header-only and stateless;
* **zero** users in this repository outside its own test file;
* **zero** sites in `cna` and **zero** in `mobile-eggbert`;
* precedent: #1788 (`LinkedList<T>` 40 → 48) and #1789 (`BitArray::Enumerator` 32 → 40) were each
  granted the same way.

## What a caller gains

Exactly one thing, and it is the thing #2169 could not deliver: **polymorphic binding.** A
specialization can now be passed where `const IEqualityComparer<T>&` is required. Before this
change that was a *compile error*, not a wrong answer — so no existing call site can have depended
on the old behaviour.

`Compare`, `Equals` and `GetHashCode` all return exactly what they returned before. No answer moved.

## Three things the reference settles, recorded because they are easy to get wrong

1. **The growth has no .NET counterpart.** .NET's comparer is a `readonly struct`
   (`TotalOrderIeee754Comparer.cs:16`), and implementing an interface costs a C# struct no storage
   at all. .NET pays nothing for the same surface. In C++ polymorphic binding requires a base class
   and a base class with virtual members requires a vtable pointer. The 8 → 16 is a C++ artifact.

2. **`IEquatable<TotalOrderIeee754Comparer<T>>` is deliberately not reproduced.** .NET implements it
   as `Equals(TotalOrderIeee754Comparer<T> other) => true` (`:204`) — every instance of a stateless
   comparer equals every other, which a defaulted `operator==` on an empty C++ type already says.
   A third base would cost a third vptr to express nothing new. Pinned absent by
   `Decl2170_IEquatableIsNotReproduced`.

3. **`GetHashCode` diverges from .NET, and #2170 deliberately does not close it.** See below.

## Known divergence carried forward, not introduced

.NET's `GetHashCode(T obj)` is `obj.GetHashCode()` (`TotalOrderIeee754Comparer.cs:198-202`) — the
*value's own* hash. `Double.GetHashCode` normalizes so that, in its own comment, "all NaNs and both
zeros have the same hash code", and **this port's `Double::GetHashCode` already matches .NET exactly**,
normalization included. This port's comparer instead hashes the **bit pattern**, so:

| | .NET | this port |
|---|---|---|
| `GetHashCode(-0.0) == GetHashCode(+0.0)` | `true` | `false` |
| two distinct NaN payloads hash equal | `true` | `false` |

Both satisfy the hash contract — equality here *is* bit-pattern identity, so equal always implies
equal hash — and .NET's is simply coarser. But the difference is **directly observable** through a
public member. It is filed as its own ticket rather than folded into #2170 because closing it would
invert **five shipped pins** (`Float_SignedZerosAreDistinct`, `Float_NaNPayloadsAreDistinguished`,
`Double_EqualityAndHash`, `Double_HashFoldsBothHalvesRatherThanTruncating`, `Half_EqualityAndHash`),
which is a behaviour decision of its own and not a consequence of adding a base class.

Mutation **M4** — adopting .NET's value hash — is caught by `Float_SignedZerosAreDistinct`, which is
the evidence that those pins are load-bearing rather than incidental.

## Module graph

**Unchanged at 41 modules / 93 edges.** `IEqualityComparer<T>` lives in
`modules/core/include/System/Collections/Generic/`, i.e. `Core.Base`, which `Numerics` already
listed in `PUBLIC_DEPENDENCIES`. No new public component edge was needed.

## Mutation testing

Five mutations, **all caught**:

| # | Mutation | Caught by |
|---|---|---|
| M1 | drop the `IEqualityComparer` base from the `double` specialization | compile error (the polymorphic-binding test cannot bind) |
| M2 | swap the two bases' order | `Fix2170_TheSecondBaseCostsExactlyOneVptr` — the offset-8 assertion |
| M3 | give `Equals` a by-value signature, making it an overload rather than an override | compile error — the class stays abstract |
| M4 | adopt .NET's value hash | `Float_SignedZerosAreDistinct` |
| M5 | invert `Equals` | `Fix2170_BindsPolymorphicallyAsIEqualityComparer` + two pre-existing cases |

M2 is the one worth keeping: a bare `sizeof == 16` passes against it, because a single base that
merely grew would give the same number. Only the subobject offset discriminates.
