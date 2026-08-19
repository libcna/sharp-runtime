<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriTypeConverter::ConvertFrom` returns `std::optional<Uri>` (ticket #1999)

*2026-08-19.* `System::UriTypeConverter::ConvertFrom` returns `std::optional<Uri>` instead of
`Uri`, so an **empty** input returns the empty state rather than throwing.

**This is a public virtual signature change**, landed under **SA-10** with SA-2's five conditions
discharged.

---

## 1. What was wrong

The return type was a by-value `Uri`, which **cannot express .NET's `null`**. So an empty string
was forwarded straight to the `Uri` constructor and threw `UriFormatException`. .NET
short-circuits it:

```csharp
if (value is string uriString)
{
    if (string.IsNullOrEmpty(uriString))
    {
        return null;
    }

    // Let the Uri constructor throw any informative exceptions
    return new Uri(uriString, UriKind.RelativeOrAbsolute);
}                                                     // UriTypeConverter.cs:40-51
```

## 2. The widening is exactly one input wide

.NET's own comment — *"Let the Uri constructor throw any informative exceptions"* — says the empty
case is the **only** one it short-circuits. A malformed string still throws.

That boundary is pinned by its own test, because "return the empty state on any failure" is the
plausible over-correction, and it is mutation M3.

## 3. Why SA-10 and not an approval

The design record (`docs/SystemUriNamespaceReviewPlan.md` §14.5) called this *"a vtable-slot
signature change, plus mandatory migration for every override"* and wrote an approval sentence.
Two things have changed since:

* **SA-10 was granted afterwards**, and its list names **"return type"** explicitly, routing such
  a change through SA-2's five conditions rather than a fresh ask. SA-3's exclusion is a change to
  the vtable's **shape** — adding or removing a virtual, or changing the base class — not a
  signature within an existing slot.
* **The migration cost is measurably zero.** There are **no overrides and no derivations** of
  `UriTypeConverter` anywhere: not in `modules/`, not in `test/` or `tests/`, and not in either
  downstream consumer. The only call sites were four in this repository's own tests.

What a *future* override would lose is pinned as site 3 of the negative fixture.

## 4. To migrate

```cpp
Uri uri = converter.ConvertFrom(text);              // was
auto uri = converter.ConvertFrom(text);             // now
if (uri) { /* … uri->getHostProperty() … */ }
```

An empty `text` no longer throws — it yields an empty optional.

## 5. One detail made explicit rather than changed

The kind is now spelled `UriKind::RelativeOrAbsolute`, matching the reference. It is
**behaviourally identical** to the one-argument `Uri(text)` this used to call: with that kind, both
guards in `Uri(string, UriKind)` are inert and it simply parses. The change is documentation, not
behaviour — stated so a reader does not look for an effect that is not there.

## 6. Evidence

Four mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the empty input is no longer short-circuited | `Fix1999_ConvertFromEmptyReturnsTheEmptyStateInsteadOfThrowing` |
| M2 — every input returns the empty state | four cases |
| M3 — a *malformed* input is short-circuited too | `Decl1999_OnlyTheEmptyInputIsShortCircuited` |
| M4 — the kind becomes `Absolute` | `Fix1999_ARelativeUriIsAccepted` — **only after that case was added** |

**M3** is the plausible over-correction — "return the empty state on any failure" — and the reason
§2's boundary has a test of its own.

**M4 is the one worth recording.** It went **uncaught** at first: every case converted an
*absolute* URI, so requiring `UriKind::Absolute` never failed. A relative input is the only one
the kind discriminates, which is precisely why .NET passes `RelativeOrAbsolute` — and the new case
also shows the relative URI round-trips, which is why `ConvertTo` uses `OriginalString`.

M2 was invalid as first written — it left `text` unused and `-Werror=unused-parameter` rejected it
— and was reformulated rather than counted.

Negative consumer fixture: `test/consumer/uri_typeconverter_optional_negative.cpp`, three sites,
all rejected. Fixture set grows to **44 fixtures / 226 sites**. Site 2 is the spelling most likely
to survive a careless migration — calling straight through the result, which used to be a `Uri`.

Site 3's diagnostic is **"invalid covariant return type"** rather than the "does not override" I
predicted, and it is the more precise of the two: gcc reads the old signature as an *attempted
covariant override* of the new one and rejects it on the spot, naming the reason.

Gate: **17,517 run, 17,517 passed, 0 failed, 0 skipped** across 38 executables — `+2` on 17,515
(`SharpRuntimeTests_Uri` 287 → 289; one pin inverted in place, two cases added). No other
executable moved. Module graph unchanged at 41/93.

## 7. Downstream, measured

`UriTypeConverter` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither
repository was modified, and no downstream ticket is needed.
