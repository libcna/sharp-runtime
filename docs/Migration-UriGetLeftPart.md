<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Uri::GetLeftPart(UriPartial)` (ticket #1997 group A-1)

*2026-08-19.* `System::UriPartial` has documented `Uri::GetLeftPart` since it was ported, and the
member did not exist. It does now.

#1997's own acceptance criterion calls A-1 *"strictly additive and touches no existing
declaration"*, which makes it ordinary **SA-5** work. Purely additive: nothing was removed, no
signature changed, no `sizeof` or vtable moved, and the module graph is unchanged at 41/93.

---

## 1. What it returns

Transcribed from `Uri.cs:1343-1383`. For `http://user:pw@example.com:8080/a/b?q=1#frag`:

| `UriPartial` | Result |
|---|---|
| `Scheme` | `http://` |
| `Authority` | `http://user:pw@example.com:8080` |
| `Path` | `http://user:pw@example.com:8080/a/b` |
| `Query` | `http://user:pw@example.com:8080/a/b?q=1` |

The fragment is never included — `Query` is the rightmost part `UriPartial` names. A default port
is omitted, which is this port's existing `getAuthorityProperty()` rule and .NET's.

`InvalidOperationException` for a relative URI, `ArgumentException` naming `part` for a value
outside the four — and the relative check comes **first**, which is .NET's order.

## 2. Two details that are only visible in the reference

**The scheme delimiter comes from the source text, not from a rule.** `UriPartial::Scheme` maps to
`GetParts(UriComponents.Scheme | KeepDelimiter)`, and that case is
`_string.Substring(Offset.Scheme, Offset.User - Offset.Scheme)` (`Uri.cs:2940-2944`) — the raw run
up to the userinfo. So a URI written with `//` keeps it and one without keeps its bare colon:
`mailto:someone@example.com` gives `"mailto:"`, not `"mailto://"`. Synthesising `"://"` is caught.

**`Authority` is the empty string when there is no authority** — and .NET's comment three lines
above that statement says the opposite of the statement:

```csharp
// It not return an empty string but instead "scheme:" because it is a LEFT part.
...
return string.Empty;
```

The code is what runs, so the code is what is transcribed. `mailto:a@b` gives `""`.

## 3. A-2's cost estimate was wrong, and it stays with #1997

The same criterion pairs A-1 with A-2 (`Uri::CheckHostName`) as *"strictly additive"* and *"the
recommended minimum"*. **A-2 is not additive in that sense.**

`Uri.CheckHostName` (`Uri.cs:1286-1325`) classifies through `IPv6AddressHelper.IsValid` and
`IPv4AddressHelper.IsValid`, and `modules/uri` has neither: it depends on `Core.Base` alone, and
validating an IPv6 literal's **content** is an explicitly declared out-of-scope boundary here
(`docs/SystemUriNamespaceReviewPlan.md` §15.4) — `parse()` checks only the bracket structure.

So A-2 costs either **a new public module edge** to reach `System::Net::IPAddress`, or **a second
address-literal parser** inside this module — the duplication #2354 spent a whole ticket removing.
Neither "touches no existing declaration". It is left with #1997, and the reason is recorded in
`Uri.hpp` next to where the member would go, so the estimate is not re-made from the ticket text.

A first cut of this change *did* write `CheckHostName` against `IPAddress` and was withdrawn when
the module graph rejected it — which is how the cost was measured rather than guessed.

## 4. Evidence

Six mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the delimiter is always synthesised as `://` | 2 cases |
| `Authority` returns the scheme when there is none | its own case |
| the relative-URI check is dropped | its own case |
| `Query` omits the query | the prefix table |
| the authority drops the userinfo | the prefix table |
| an undefined part returns an empty string instead of throwing | its own case |

## 5. Downstream

`cna` and `mobile-eggbert` reference `GetLeftPart` or `UriPartial` in **zero** places. The change
is additive, so there is nothing to migrate.
