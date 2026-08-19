<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Uri` equality and hashing are canonical (ticket #1995, SR-AUD-142)

*2026-08-19.* `System::Uri::operator==` and `GetHashCode` compare and hash a **canonical
identity** instead of the raw input string.

**Equality semantics change**, so the behaviour of any container keyed by `Uri` changes. Nothing
that was equal becomes unequal — the change is a widening. Landed under **SA-5**; no signature,
layout, vtable, mangled-symbol or `noexcept` change, and **no rendered string moves**.

---

## 1. What was wrong

`operator==` compared `absoluteUri_` verbatim and `GetHashCode` hashed it. Measured:
`HTTP://EXAMPLE.COM:80/Path` and `http://example.com/Path` were **unequal, with different
hashes**, although they name the same resource.

## 2. What .NET compares — and the half the design record missed

Both members feed from the **same** component set, which is why they cannot disagree:

```csharp
UriComponents components = UriComponents.HttpRequestUrl;                  // Uri.cs:1835, 1539
if (_syntax.InFact(UriSyntaxFlags.MailToLikeUri)) components |= UriComponents.UserInfo;
string selfUrl = ... GetParts(components, UriFormat.SafeUnescaped);
```

and `HttpRequestUrl = Scheme | Host | Port | Path | Query` (`UriEnumTypes.cs:43`).

**The design record proposed only "case-fold scheme and host, treat an explicit default port as
absent".** The reference *also* excludes the **fragment** and the **user-info**, and says so in a
comment of its own — *"Fragment AND UserInfo (for non-mailto URIs) are ignored"* (`Uri.cs:1833`).
So:

```cpp
Uri("http://example.com/p#one") == Uri("http://example.com/p#two")   // true
Uri("http://user@example.com/p") == Uri("http://example.com/p")      // true
```

Neither follows from §14.1's wording. Both are now asserted.

## 3. A trap measurement found, not the record

`defaultPortForScheme` matches **lower-case** scheme names only, so this port parses
`HTTP://example.com/` with port **−1** and `http://example.com/` with port **80**. Comparing the
*stored* numbers would therefore have left those two **unequal even after folding the scheme** —
defeating the repair on exactly the input it exists for.

The identity key resolves the default from the **folded** scheme. The parse itself is untouched:
`getPortProperty()` still returns −1 there, and the pre-existing pin asserting that still passes.

.NET has no such trap because it lower-cases the scheme *while parsing* and then resolves the
default, so its `Port != other.Port` (`Uri.cs:1822`) already compares resolved numbers.

## 4. Why fold at comparison rather than at parse

This port deliberately performs **no parse-time canonicalisation** — an explicit exclusion of the
review plan (§15). Folding at comparison reaches .NET's answer without touching `AbsoluteUri`,
`OriginalString`, `getSchemeProperty()` or `getHostProperty()`, all of which still return the raw
input. A test asserts that.

## 5. What is *not* reproduced, stated rather than discovered later

.NET compares the **path** case-insensitively for UNC and DOS paths
(`IsUncOrDosPath ? OrdinalIgnoreCase : Ordinal`, `Uri.cs:1849`) and hashes a `file:` URI
case-insensitively (`Uri.cs:1548-1551`). This port models **neither** `IsUncOrDosPath` nor
`IsFile`, so the path is always compared case-sensitively.

That is a **narrower** equality than .NET's — it never calls equal two URIs .NET would call
unequal — which is the safe direction. A test pins it.

## 6. `UriBuilder` is deliberately not changed, and that is a conflict, not an omission

§14.1 also says *"`UriBuilder` delegates to `Uri`"*, and .NET does exactly that
(`GetHashCode() => Uri.GetHashCode()`, `UriBuilder.cs:279`). **Delegating would reintroduce a
defect this repository already measured and removed.** .NET's `Uri` property builds the Uri and
can **throw**; ticket #2004 moved `UriBuilder::GetHashCode` off that route precisely because of
it, listing four routes through *ordinary setters* where it throws.

So the two landed decisions conflict, and the reference does not settle it *for this port*: .NET
is consistent because **both** its members go through `Uri` and throw together; this port is
consistent because **neither** does. Delegating one alone restores the inconsistency #2004
removed.

That is **ticket #2391**, not something to resolve silently here. The gap #1995 widens — a builder
identity that is raw text while `Uri`'s is canonical — is pinned by
`Decl1995_BuilderHashNoLongerMatchesTheBuiltUrisHash`.

## 7. Evidence

Six mutations, **all caught**: no case folding; the fragment included; the user-info included;
the default port not resolved; the hash reverting to `absoluteUri_`; the query dropped.

M2, M3 and M6 were **invalid as first written** — their anchors spelled `\x01` as an escape
rather than as the literal backslash text in the source — and were re-run rather than counted.

Two pre-existing pins were **inverted rather than deleted**, because both existed to make this
change visible: `DocumentedContract_CaseDifferingUrisAreNotEqualYet` said *"this test must be
updated when identity changes"*, and `UriIdentityItselfIsUnchangedByThisTicket` was written by
#2004 so a reader would not mistake **that** ticket for an identity change. A third,
`HashIsValueIdenticalWhereTheOldRouteSucceeded`, asserted a #2004 compatibility claim this ticket
invalidates, and was rewritten to assert the new relationship and name #2391.

Gate: **17,528 run, 17,528 passed, 0 failed, 0 skipped** across 38 executables — `+6` on 17,522
(`SharpRuntimeTests_Uri` 294 → 300: seven cases added, two pins inverted in place, and one
obsolete pin removed rather than left disabled). No other executable moved. Module graph unchanged
at 41/93.

## 8. Downstream, measured

`System::Uri` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`. The one grep
hit is a **comment** in `cna/modules/media/src/Xna/Song.cpp` explaining that it deliberately does
*not* use `System::Uri`. Neither repository was modified.
