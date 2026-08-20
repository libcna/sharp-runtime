<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — an unrecognised culture name is refused (ticket #2410)

*2026-08-20.* `CultureInfo(name)` and `CultureInfo::GetCultureInfo(name)` now reject a name that is
not a well-formed BCP 47 tag, with `CultureNotFoundException`.

Landed under **SA-14 decision 3** (throw from both doors) with the boundary set by **SA-15.2**.
**Downstream, measured: zero `CultureInfo` sites** in `cna` and in `mobile-eggbert`.

---

## 1. What it used to do, which was worse than "falls back to invariant"

`CultureInfo("xx-YY")` **succeeded**. `getNameProperty()` returned `"xx-YY"`, while the number and
date format objects came from the **invariant** culture. **The object claimed to be `xx-YY` and
behaved as invariant** — it lied about what it was, and nothing surfaced that.

And `CultureNotFoundException` **already existed and was already thrown** — but only from the
**LCID** path (`ValidateLcidStub`, five specific numeric values), never from a name. **One door of
one type rejected what the other accepted**, which is #2393's shape.

## 2. The boundary, and why it is deliberately wider than .NET's

**Accepted**: `""`, `"und"` (any casing), and any well-formed tag — a **2-3 letter** primary subtag
followed by subtags of 1-8 alphanumeric characters. `"en"`, `"de-DE"`, `"zh-Hant-TW"`.

**Rejected**: `"process-default"`, `"main thread"`, `"de_DE"`, `"123"`, `"de-"`, `"-de"`,
`"en-abcdefghi"`, `"en-D.E"`.

**This is NOT .NET's invariant-globalization rule, and the header says so rather than implying it.**
Measured: `CultureData.cs:660-675` short-circuits only `""` and `"und"`, and `GlobalizationMode.cs:19`
makes `PredefinedCulturesOnly` default to `GlobalizationMode.Invariant` — so .NET **in this port's
own mode** accepts those two and throws for **every** other name, **`"de-DE"` included**.

The reason for taking the wider rule is recorded rather than assumed: **this port has no culture
database at all**, so .NET's rule would leave `CultureInfo` unable to represent *any* named culture.
The syntactic check catches the lying-object class and nothing else.

**What a caller must not read into an accepted name**: it means *this is a well-formed tag*, never
*this runtime has data for that culture*. It has none for any of them, and a test asserts exactly
that — `CultureInfo("de-DE")`'s month names are still `"January"`.

### 2.1 One narrowing past BCP 47, and it is what makes the check work

RFC 5646 also allows a **5-8 letter registered** primary subtag. Under that reading
**`"process-default"` is well formed** — measured: a first cut of this check **accepted it**, which
would have missed the exact case the decision names. Restricting the primary subtag to the 2-3
letters every real culture name uses rejects it. **What is lost is the rare registered form**, and
that is stated in the header rather than discovered by whoever needs one.

## 3. What a caller has to change

Any name that is not a well-formed tag now throws. In practice that means **placeholder or
identifier-shaped names** — `"process-default"`, `"main-thread"`, `"default"` — which is exactly the
class the change exists to catch. Real culture names are unaffected.

## 4. Testing

Seven mutations after reformulation, six caught. Both doors have their own mutation, because
guarding one and not the other is precisely the state this ticket removes.

**Two mutations were NOT caught at first and both were genuine gaps**: a first cut of the malformed
list exercised only the **primary** subtag, so the over-long and non-alphanumeric checks on **later**
subtags were untested and removing either went undetected. Three rows were added (`"en-abcdefghi"`,
`"en-D.E"`, `"en-US-x y"`) and both mutations are now caught.

**One is a proven equivalence**: breaking the `"und"` short-circuit's case-insensitivity changes
nothing, because `"und"` is a well-formed three-letter primary subtag that the general rule accepts
in any casing. It is kept for being .NET's and for documenting why `"und"` means invariant, and the
site says so.

**One mutation was invalid as first written** — it left a lambda unused and `-Werror` rejected it,
so the verdict said nothing about the tests — and was reformulated rather than counted.
