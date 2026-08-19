<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Decision — `System::Uri` keeps an embedded NUL, and the proposed repair was backwards (ticket #2003)

*2026-08-19.* **No production statement changed.** This is a decision and its evidence, in the
shape of #2015 and #2324: the ticket asked for approval to make `Uri` reject an embedded NUL, and
the reference — which the ticket could not read — says .NET does not reject it.

---

## 1. The approval must not be sought

#2003's recorded gate is a sentence to be approved:

> *"Make `System::Uri` reject an embedded NUL anywhere in the URI string with
> `UriFormatException`, **accepting that the .NET behaviour it matches could not be re-measured
> in this environment**."*

`/rv/tmp/runtime` is present, so it was re-measured, and it points the other way:

* **`UriHelper.s_notSafeForUnescapeChars` lists U+0000–U+001F explicitly.** A NUL is never
  unescaped back from `%00`; it is carried, not refused.
* **`Uri.TryCreateThis` (`UriExt.cs:30-70`) has no whole-string character precheck at all.** The
  only rejection paths are the scheme, the host and the port.
* A control character in the path, query or fragment is percent-**escaped** during
  canonicalisation. The data survives; only its rendering differs.

So the proposed repair points **away** from the reference. The ticket closes with no change.

## 2. This port already matches, and the one difference is an existing declared boundary

| Position | .NET | this port |
|---|---|---|
| host | rejected — `DomainNameHelper.IsValid` excludes it | rejected (#2359) |
| port | rejected — not a digit | rejected |
| scheme | not a scheme char → the reference is **relative** | relative, whole text becomes the path |
| path, query, fragment, userinfo | carried, as `%00` | carried, **raw** |

The last row is the only difference, and it is the already-declared **no-percent-encoding**
boundary (`docs/SystemUriNamespaceReviewPlan.md` §15.1), not a second divergence. Both runtimes
preserve the byte; they render it differently.

## 3. Two premise corrections, both measured

1. **The ticket's title says a NUL crosses "into every component".** It does not any more.
   **#2359** gave the host .NET's DNS character set, and a NUL is outside it exactly as a space
   is — so the host has rejected it since that ticket landed, four days after #2003 was written.
2. **"No truncation" was asserted of the string and never of the component accessors.** It holds
   for those too, and every row now states a **length**, because a truncating parser and a
   preserving one differ by a byte count that no `operator<<` would show — a first probe of this
   ticket printed the accessors with `%s` and appeared to show truncation that was not there.

## 4. Evidence: the decision is enforced, not merely written down

Two changes were **built and run** before being reverted, which is what makes this a measurement
rather than an assertion:

| Change | Result |
|---|---|
| truncate the input at the first NUL | **caught by 4 tests** |
| **the repair the ticket proposed** — reject anywhere with `UriFormatException` | **caught by 3 tests** |

The second is the one that matters: if the approval were ever granted and implemented, the suite
fails immediately and names this decision. That is the signal these pins exist to produce.

`Decl2003_RejectionIsConfinedToTheThreePlacesDotNetRejects` states the three rejecting positions
and seven carrying ones, each with an asserted length.

## 5. Downstream

No behaviour changed, so nothing to migrate.
