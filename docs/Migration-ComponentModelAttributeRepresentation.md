<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — six `System::ComponentModel` attributes get .NET's shape (ticket #2403)

*2026-08-20.* Six attributes published a **bare mutable public data member** where .NET publishes a
**get-only property**, and most were missing their `Yes`/`No`/`Default` statics and their equality
members entirely.

Landed under **SA-8** (*"where this port publishes a mutable or public representation and .NET's is
private, readonly or absent, match .NET and migrate the first-party sites"*) with **SA-2**'s five
conditions, since removing a public data member is a public source break.

**No vtable change**: `Equals`, `GetHashCode` and `getIsDefaultAttributeProperty` are already
`virtual` on `System::Attribute`, so these are overrides of existing slots, not new ones.

**Downstream, measured:** **zero** sites of any kind in `cna` and in `mobile-eggbert`. Downstream
record: **#2404**.

---

## 1. What made this more than a style point

**The port was inconsistent with itself inside one file.** `CategoryAttribute`,
`BrowsableAttribute`, `DisplayNameAttribute` and `DescriptionAttribute` — in that very header and
its neighbour — already had the correct shape: private member, `getXxxProperty()`, the static set,
and `Equals`/`GetHashCode`/`getIsDefaultAttributeProperty`. The other six did not.

| Type | Was | Now (= .NET) |
|---|---|---|
| `ReadOnlyAttribute` | `public bool IsReadOnly`; `Yes`/`No`, **no `Default`** | `getIsReadOnlyProperty()`; `Yes`/`No`/`Default` |
| `ImmutableObjectAttribute` | `public bool Immutable`; **no statics** | `getImmutableProperty()`; `Yes`/`No`/`Default` |
| `LocalizableAttribute` | `public bool IsLocalizable`; no statics | `getIsLocalizableProperty()`; `Yes`/`No`/`Default` |
| `MergablePropertyAttribute` | `public bool AllowMerge`; no statics | `getAllowMergeProperty()`; `Yes`/`No`/`Default` |
| `NotifyParentPropertyAttribute` | `public bool NotifyParent`; no statics | `getNotifyParentProperty()`; `Yes`/`No`/`Default` |
| `RefreshPropertiesAttribute` | `public Refresh RefreshProperties_`; no statics | `getRefreshPropertiesProperty()`; `All`/`Repaint`/`Default` |

All six are now `final`, matching .NET's `public sealed class`, and all six gained
`Equals`/`GetHashCode`/`getIsDefaultAttributeProperty`.

## 2. `RefreshProperties` is a top-level enum

.NET declares `public enum RefreshProperties` in its **own file** (`RefreshProperties.cs`). This
port nested it as `RefreshPropertiesAttribute::Refresh`, differing in **both the name and the
scope**. It is now top-level, with .NET's values (`None = 0`, `All = 1`, `Repaint = 2`).

**Migration:** `RefreshPropertiesAttribute::Refresh::All` becomes `RefreshProperties::All`.

## 3. The defaults are not uniform, and that is .NET's

**`MergablePropertyAttribute::Default` is `Yes`** (`MergablePropertyAttribute.cs:12`), where the
other four boolean attributes default to `No`. `BrowsableAttribute::Default` is `Yes` too
(`BrowsableAttribute.cs:32`), and this port already had that right.

A repair that "harmonised" the five would be wrong **here and nowhere else**, which is why the
asymmetry carries a pin of its own — asserted in one case, so it reads as an asymmetry rather than
as five unrelated literals.

## 4. One divergence is deliberate: `GetHashCode`

**.NET's `GetHashCode` for all six is `base.GetHashCode()` — identity — while its `Equals` is
value-based.** Two equal .NET instances can therefore hash differently: a hash-contract violation in
the reference itself.

**This port does not reproduce it, and the reason is written down rather than chosen.**
`System/Attribute.hpp`'s own doc-comment states the house rule in terms:

> A subclass that needs value equality must override **both** `Equals` and `GetHashCode` … Overriding
> only one breaks the equals/hashCode contract.

and the four already-correct siblings in this very header use a value-based hash. Reproducing .NET
here would contradict the port's own stated rule *and* its own neighbours. The divergence is pinned:
mutation M7 replaces the value hash with .NET's identity hash and **is caught**.

## 5. What a caller has to change

| Was | Now |
|---|---|
| `attr.IsReadOnly` | `attr.getIsReadOnlyProperty()` |
| `attr.Immutable` | `attr.getImmutableProperty()` |
| `attr.IsLocalizable` | `attr.getIsLocalizableProperty()` |
| `attr.AllowMerge` | `attr.getAllowMergeProperty()` |
| `attr.NotifyParent` | `attr.getNotifyParentProperty()` |
| `attr.RefreshProperties_` | `attr.getRefreshPropertiesProperty()` |
| `RefreshPropertiesAttribute::Refresh::X` | `RefreshProperties::X` |

**There is no replacement for *writing* one, deliberately.** .NET publishes no setter, and a caller
that needs a different value constructs a different attribute — or uses the `Yes`/`No`/`Default`
statics, which #2403 also completed. A metadata attribute a caller can retarget after construction
is not metadata, and **the write is the spelling that would have survived a careless migration
longest**, because it compiled silently against the old shape while changing the attribute's
meaning. It gets its own negative-fixture site for that reason.

## 6. Testing

First-party migration: **8 sites**, all in this module's own test file, every one named by the
compiler. Negative consumer fixture
`test/consumer/component_model_attribute_representation_negative.cpp`, **8 sites** — a read and a
**write** for `ReadOnlyAttribute`, a read for each of the other five, and one for the enum's scope.
Fixture set **49 / 248 → 50 / 256**.

**The module's whole prior coverage for these six was constructor round-trips through the public
field.** Nothing asserted the statics, the equality members or the defaults — so .NET's `Default`
values, which are the actual contract of a metadata attribute, were unpinned. Six cases now pin
them.

Seven mutations, **all caught**, three of them at compile time (`static_assert` for the seals, and
the enum's scope) — which is the only way C++ reports a shape.

**A process note worth keeping.** An early build of this change appeared to report **zero errors**
and was very nearly taken at face value. The cause: `cmake --build … -k 200` is not keep-going —
`-k` must follow `--`, and CMake was printing its usage banner and exiting. This is `CLAUDE.md` rule
2's recorded #2395 trap in a new form: *the build stops at the first failing translation unit unless
`-k` is passed*, and a mis-passed `-k` is worse than none, because it looks like success.
