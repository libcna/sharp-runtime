<!-- SPDX-License-Identifier: MIT -->
# Migration — `Uri(std::string)` requires an absolute URI (#2393)

Ticket **#2393**, landed 2026-08-19 under SA-5 (*"aligning to the reference is ordinary work …
including where a call that succeeds today starts throwing"*).

## The defect

This port had **two absolute-URI grammars in one type**:

```cpp
Uri("://example.com/")                                    // SUCCEEDED
Uri::TryCreate("://example.com/", UriKind::Absolute, u)   // returned FALSE
```

`Uri::Uri(const std::string&)` called `parse()` and **never checked `isAbsoluteUri_`**, while the
`(string, UriKind)` overload did — and `TryCreate` goes through that overload. So a caller could
construct a `Uri` that this port's own `TryCreate` says is not an absolute URI.

**Reachable from ordinary code**: a `UriBuilder` with an empty scheme (accepted since #1996 G-3)
renders `"://example.com/"`, and `UriBuilder::getUriProperty()` is `Uri(ToString())`.

## The direction was derived, not chosen

.NET has **one** grammar, by construction:

* `new Uri(s)` is `CreateThis(s, false, UriKind.Absolute)` (`Uri.cs:424-429`);
* `TryCreate(s, UriKind.Absolute, out u)` is `CreateHelper(s, false, UriKind.Absolute)`
  (`UriExt.cs:223-227`);
* `CreateThis` throws exactly what `CreateHelper` returns `null` for (`UriExt.cs:18-26`).

So the constructor was wrong and `TryCreate` was right. The repair is one line: delegate.

## The narrowing is wider than the ticket described — say so plainly

The reported symptom was one odd string. The actual change is that **the one-argument constructor no
longer accepts *any* relative reference**:

```cpp
Uri u("/relative/path");    // used to construct silently; now throws UriFormatException
Uri u("a?b");               // likewise
Uri u("#fragment");         // likewise
```

**Migration**: name the kind.

```cpp
Uri u("/relative/path", UriKind::RelativeOrAbsolute);
```

Relative-URI support is **not** removed — every component accessor, the query/fragment split, and
resolution against a base all behave exactly as before. Only the *default kind* of the
one-argument constructor moved, and it moved onto .NET's.

**Measured impact**: **zero** real sites in `cna` (its single `System::Uri` match is inside a
comment) and **zero** in `mobile-eggbert`. First-party, **51 constructions across 24 tests** needed
the kind spelled out; every one of those tests keeps its coverage through the two-argument
constructor, because they were testing *relative-URI behaviour*, not the constructor's default.

Two shipped assertions were **inverted** rather than adapted, because they asserted the defect:

* `Decl1997A3_BothOverloadsResolveAgainstAbsoluteNotRelativeOrAbsolute` ended with
  `EXPECT_NO_THROW(Uri("/relative/path"))` and called that *"what makes the distinction observable
  rather than theoretical"*. It was observable, and it was the defect. The case now asserts the
  throw, and keeps its real subject by contrasting against `UriKind::RelativeOrAbsolute`.
* `Fix2391_TheComparandKindIsRelativeOrAbsoluteAndItIsLoadBearing` — see below.

## A consequence worth following, because it moved twice in one day

#2391 mutation **M1** (the comparand kind `Absolute` instead of `RelativeOrAbsolute`) has now
changed status **three times**, and the sequence is the point:

1. First recorded as an **unobservable equivalence**, reasoning that `getUriProperty()` is always
   absolute, so a relative comparand can never compare equal and both spellings return `false`.
2. Then measured **wrong**, because of *this* defect: the constructor accepted `"://example.com/"`
   while `TryCreate(Absolute)` refused it, so `self` could be a `Uri` the strict comparand parse
   would reject, and the two spellings disagreed. The case was rewritten to pin that.
3. Now an **equivalence again** — #2393 removed the second grammar, so `self` can no longer be such
   a `Uri`; the builder throws before any comparand is parsed.

The original reasoning was right; it was defeated only by a bug one layer down. The case is
renamed `Decl2391_TheComparandKindIsRelativeOrAbsoluteAndIsNowAnEquivalence` and records all three
steps, so the line is kept because .NET writes it, not because it is load-bearing.

## Tests

`Fix2393_TheConstructorAndTryCreateShareOneGrammar` asserts the **equivalence over an 11-row
corpus** — empty scheme, absolute-path, relative-path and network-path references, query-only,
fragment-only, `":"`, `""`, and three genuinely absolute URIs — rather than on the one reported
string, because a repair that fixed only the reported example would pass a single-row test.

## Mutation testing

Four mutations, all caught:

| # | Mutation | Caught by |
|---|---|---|
| M1 | revert to the two grammars | the equivalence pin + `Decl1997A3` + `Decl2391` |
| M2 | delegate to `RelativeOrAbsolute` | the same |
| M3 | delegate to `Relative` | broadly, including `UriBuilderTest.ToUri` |
| M4 | the kind overload stops enforcing `Absolute` | the same set |
