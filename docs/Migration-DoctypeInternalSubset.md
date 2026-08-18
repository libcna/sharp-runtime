<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a DOCTYPE internal subset must not close its own declaration (ticket #2348)

*2026-08-19.* `WriteDocType("r", "", "", "]><evil/><!--")` now raises `XmlException`. It used to
emit

```xml
<!DOCTYPE r []><evil/><!--]>
```

which is malformed XML **and injects an element**.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. The ticket's premise was corrected twice, in opposite directions

**#2084 corrected it once.** The finding blamed a `]` terminating the internal subset; the
terminator is `>`. That measurement is what split this ticket out.

**The reference corrects it again, the other way.** The ticket recorded F2 as *"a genuine WRITER
defect rather than a reader limitation"*, implying .NET does better. It does not:
`XmlWellFormedWriter.WriteDocType` validates the subset with `XmlCharType.IsOnlyCharData` alone —
a **character** check, which this port also performs since #2349 — and then
`XmlEncodedRawTextWriter` writes it with `RawText(subset)` between a literal `[` and `]`
(`XmlEncodedRawTextWriter.cs:281-286`). **.NET emits the same malformed document.**

So this is a **deliberate narrowing past the reference**. It is also exactly the rule #2084
already applied *in this same function* to the `ExternalID` literals, where .NET likewise checks
only characters. A writer that emits a declaration its own delimiters terminate early is producing
an injection vector, and this repository has consistently refused to.

## 2. What changed

| Subset | Was | Is |
|---|---|---|
| `]><evil/><!--` | emitted, injecting `<evil/>` | `XmlException` |
| `]  ><evil/>`, `]\n><evil/>` | emitted | `XmlException` |
| `<!ENTITY a "b">]><evil/>` | emitted | `XmlException` |
| `<!ENTITY a "b">` | emitted | **unchanged** |
| `]` alone | emitted | **unchanged** |
| `<!ENTITY a "]>">` | emitted | **unchanged** — §3 |
| empty | emitted | **unchanged** |

## 3. The rule is not "reject any `>`", and it is quote-aware

#2084 considered rejecting every subset containing `>` and rejected that, correctly:
`<!DOCTYPE r [<!ENTITY a "b">]><r/>` is well-formed XML that this port emits correctly, and only
this runtime's `>`-terminated DOCTYPE node mis-reads it on the way back — a **reader** limitation
`XmlDocumentType`'s own doc-comment already concedes. Narrowing a spec-valid writer to fit a known
reader limitation would be a regression.

The scope line is therefore: **reject what makes the emitted document malformed, never what merely
fails to survive this reader.** The test is a `]` followed, after optional whitespace, by `>`.

And it tracks quotes: `<!ENTITY a "]>">` carries `]>` inside a literal, where XML permits it and a
conforming parser reads it correctly. A scan for the two characters without tracking quotes would
reject that, which is the easy way to get this rule wrong — and a mutation removing the
quote-tracking is caught.

## 4. Both doors

`XmlWriter::WriteDocType` and `XDocumentType::SerializeTo`, which #2200 already made share this
policy. `XmlDocument::CreateDocumentType` needs nothing: it **ignores** the subset entirely and
never emits it.

## 5. Evidence

| Mutation | Caught |
|---|---|
| The check goes away entirely | yes (both door families) |
| The check stops being quote-aware | yes (both) |
| Whitespace between `]` and `>` is no longer skipped | yes (both) |
| The rule becomes "any `>`" — what #2084 rejected | yes (4 tests, two of them pre-existing) |

The fourth is the useful one: it is caught by tests that predate this ticket, which is what shows
the narrowing stopped where #2084 said it must.

## 6. Downstream

Neither `cna` nor `mobile-eggbert` references `WriteDocType` or `XDocumentType` — zero sites in
both.
