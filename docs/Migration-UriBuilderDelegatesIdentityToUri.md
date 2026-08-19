<!-- SPDX-License-Identifier: MIT -->
# Migration — `UriBuilder::Equals`/`GetHashCode` delegate to the built `Uri` (#2391)

Ticket **#2391**, landed 2026-08-19 on an explicit user decision (`docs/StandingApprovals.md` SA-13).

## What changed

```cpp
// before (#2004)
bool Equals(const UriBuilder& other) const { return ToString() == other.ToString(); }
intcs GetHashCode() const { return std::hash<std::string>{}(ToString()); }

// after (#2391) — .NET's UriBuilder.cs:277-279
bool Equals(const UriBuilder& other) const;   // via the built Uri, comparand parsed as a string
intcs GetHashCode() const;                    // getUriProperty().GetHashCode()
```

## Two behaviour changes, in opposite directions

**1. Identity became canonical (a widening of equality).** `UriBuilder` now compares through
`Uri`, which has had canonical identity since #1995 — folded scheme, folded host, resolved default
port, path and query, with fragment and user-info excluded. So these pairs, formerly **unequal**,
are now **equal**:

| | |
|---|---|
| `Host = "example.com"` vs `Host = "EXAMPLE.COM"` | host case is folded |
| `Host = "example.com"` vs the same with `Port = 80` | an explicit default port resolves away |

Identity is canonical, not blind: a different host, or a non-default port, is still unequal.

**2. `Equals` and `GetHashCode` can throw again (a narrowing of totality).** **This is the cost, it
was stated before the decision was taken, and it must not be quietly softened later.** For a builder
whose rendering does not parse, both members now throw `UriFormatException` — **including
`b.Equals(b)`**. Two measured routes survive, both ordinary setters:

* `setHostProperty("[::1")` — an unterminated IP literal, which #1996 G-1 deliberately does not wrap;
* `setHostProperty("")` — an empty host (#2000).

(`setHostProperty("h:abc")` and `("h:99999")` left this set with #1996 G-1: they are bracketed now
and parse.)

### Why that is not simply re-breaking what #2004 fixed

#2004 removed the delegation for a real reason: the pair disagreed at the worst possible place,
`b.Equals(b)` returning `true` while `b.GetHashCode()` **threw**, leaving an object that compared
equal to itself with no obtainable hash. Its repair made *hashing* stop parsing. #2391 closes the
same gap **from the other side**: both members now go through the built `Uri`, so they are total on
exactly the same set of objects and `Equals ⇒ equal hash` holds across the whole of it. What changed
is *which* set — the parseable builders rather than all of them.

### And #2004's own justification had already become false

#2004 argued the string hash was *value-identical* to delegating, because "`Uri::parse` assigns
`absoluteUri_ = uriString` on every branch it accepts, and `Uri::GetHashCode` hashes exactly that".
**#1995 made `Uri::GetHashCode` hash the canonical identity key instead**, earlier in the same
session, which silently falsified that argument. Keeping #2004 would have meant a builder and the
`Uri` it builds hashing **differently** — precisely the defect #2004 existed to prevent, one level
up. So this is a convergence, not a reversal.

## The asymmetry is .NET's and is deliberate

`UriBuilder.Equals(object)` is `rparam is not null && Uri.Equals(rparam.ToString())`:

* **this** builder goes through the `Uri` property, so an unparseable **self throws**;
* the **other** is handed over as a **string**, and `Uri.Equals(string)` runs
  `TryCreate(s, UriKind.RelativeOrAbsolute, out _)` and **returns false** when that fails.

So an unparseable *other* is merely unequal. Reproducing only one half would be tidier and would not
be .NET. Pinned by `Fix2391_TheComparandIsAStringSoAnUnparseableOTHERIsMerelyUnequal`.

Note `RelativeOrAbsolute`, **not** `Absolute` — the opposite of what #1997 A-3 chose for the
*creation* overloads. The two questions have different answers.

## Tests

Four shipped pins were **inverted, not deleted** — each had been written anticipating exactly this:

| Pin | Was | Now |
|---|---|---|
| `HashIsObtainableWhereverEqualsSucceeds_OtherUnparseableRenderings` | both members answer | `Fix2391_AnUnparseableBuilderNowThrowsFromBothMembers` |
| `Decl1995_BuilderHashNoLongerMatchesTheBuiltUrisHash` | the two hashes differ | `Fix2391_BuilderHashIsTheBuiltUrisHash` — they agree by construction |
| `DeliberatelyUnequalPairsStayUnequal_PinsTheGatedIdentityChange` | the pairs are unequal | `Fix2391_TheseFormerlyUnequalPairsAreNowEqual` |
| `EqualsImpliesEqualHashAcrossEveryEqualityClass` | included two unparseable shapes | domain shrank; the exclusion is asserted in place |

`HashIsObtainableWhereverEqualsSucceeds_MalformedPort` needed no change: `"h:abc"` parses since
#1996 G-1.

## Mutation testing

Five mutations, **all caught** — but M1 only after the test that was supposed to catch it was
rewritten, and the reason is worth keeping:

| # | Mutation | Caught by |
|---|---|---|
| M1 | comparand kind `Absolute` instead of `RelativeOrAbsolute` | `Fix2391_TheComparandKindIsRelativeOrAbsoluteAndItIsLoadBearing` |
| M2 | build the other side too, so both throw | `Fix2391_TheComparandIsAStringSoAnUnparseableOTHERIsMerelyUnequal` |
| M3 | revert `GetHashCode` to #2004's string hash | four cases |
| M4 | revert `Equals` to #2004's text compare | three cases |
| M5 | an unparseable other returns `true` | the asymmetry pin |

**M1 was first recorded as an unobservable equivalence, and that record was wrong.** The reasoning
was that `getUriProperty()` is always absolute, so a relative comparand can never be equal and both
spellings return `false`. Probing the premise instead of trusting it produced a measurement that
breaks it: **this port's `Uri(std::string)` constructor accepts `"://example.com/"` while its own
`TryCreate(s, UriKind::Absolute)` rejects it.** So `self` can be a `Uri` the strict comparand parse
would refuse, and then `emptyScheme.Equals(emptyScheme)` is `true` under the reference spelling and
`false` under the mutation — a builder unequal to itself. That is now the discriminating assertion.

The two-grammar inconsistency itself is out of scope here and is filed as **#2393**.
