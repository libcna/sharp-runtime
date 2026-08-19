<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Declaration — a processing instruction can be read back only before every other node (ticket #2202)

*2026-08-19.* This runtime writes a processing instruction correctly in any position and can
parse one back **only at the very start of the document**. That asymmetry is now documented at
both doors and pinned at both layers.

**This ticket changed no production statement.** It is a declaration and its evidence, the same
shape as #2015 and #2324. Nothing you can write, read or emit behaves differently.

---

## 1. The limitation

| Document | Loads? |
|---|---|
| `<?p d?><root/>` | **yes** |
| `<?xml version="1.0"?><?p d?><root/>` | **yes** — an XML declaration may precede it |
| `<root><?p d?></root>` | no — `XML_ERROR_PARSING_DECLARATION` |
| `<root/><?p d?>` | no |
| `<!--c--><?p d?><root/>` | no — not even a comment may precede it |

Both writer families — `System::Xml::XmlWriter::WriteProcessingInstruction` and
`System::Xml::Linq::XProcessingInstruction` — emit the instruction in **every** one of those
positions, and the text they emit is well-formed XML. So SR-AUD-349's closure property (whatever
this runtime writes, this runtime must be able to read) does **not** hold for this one node kind.

## 2. The cause is the substrate's node-type model

Not a check in the wrong place, and not a defect in the port. Measured directly against
`vendor/tinyxml2`, with no port code involved
(`build-probe/2202_probe1_tinyxml2_direct.cpp`):

```
<?p d?><root/>                      -> OK
<root><?p d?></root>                -> XML_ERROR_PARSING_DECLARATION
<root/><?p d?>                      -> XML_ERROR_PARSING_DECLARATION
<!--c--><?p d?><root/>              -> XML_ERROR_PARSING_DECLARATION
<?xml version="1.0"?><?p d?><root/> -> OK
first node of '<?p d?><root/>': Declaration=1 Unknown=0 Element=0 value=p d
```

Exactly the verdicts the port produces, and the last line is the reason: **tinyxml2 has no
processing-instruction node type at all.** Every `<?` becomes an `XMLDeclaration` — the leading
`<?p d?>` is a *Declaration* whose value is the string `p d` — so ordinary PIs inherit tinyxml2's
rule that a declaration is allowed only at document level and before anything else.

The port therefore neither loses nor rebases position information. There is no local projection
bug to repair.

## 3. .NET has no such limitation, and the difference is the same model distinction

`XmlLoader.cs:203-209` switches on `XmlNodeType.XmlDeclaration` and
`XmlNodeType.ProcessingInstruction` as **separate cases in the same general node loop** — a loop
that runs for element content, not only for the prolog. Two node types, two rules. tinyxml2 has
one node type and therefore one rule.

## 4. Why it is documented rather than repaired

Two routes exist and both are refused:

1. **Patch `vendor/tinyxml2` to add a PI node type.** `vendor/` is third-party source, kept
   unmodified from upstream, and CLAUDE.md forbids editing it.
2. **Pre-rewrite non-leading `<?...?>` before handing text to the substrate and map it back.**
   This forks the substrate's parsing semantics for every document that passes through
   `LoadXml`, to serve one node kind. Measured as mutation M3 (§5), a naive version of exactly
   this shim breaks `XPathSelectTests.ProcessingInstructionNodeTest_SelectsPI` — shipped,
   working functionality — which is what a semantics fork costs in practice.

The ticket's own acceptance criteria prescribe this outcome: *"if the substrate cannot express
it, the correct outcome is an explicit documented limitation on `XProcessingInstruction` and
`XmlWriter::WriteProcessingInstruction` plus a pin, not silence."* The substrate cannot express
it, and that had been confirmed; the documentation and the second pin were simply never written.

## 5. Evidence

Three mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the writer silently drops a PI written inside an element | `Decl2202_TheWriterEmitsWhatTheParserCannotReadBack` |
| M2 — `LoadXml` swallows the substrate's parse error | both new pins **and four pre-existing tests** |
| M3 — strip non-leading PIs before parsing (the realistic future "repair") | both new pins **and `XPathSelectTests.ProcessingInstructionNodeTest_SelectsPI`** |

Pins, at both layers as the acceptance criteria require:

* `System::Xml` — `XmlWriterValidationTests.Decl2202_*` (three cases, **new**: the four verdicts,
  the declaration exception, and the write-then-fail-to-read asymmetry end to end).
* `System::Xml::Linq` —
  `XLinqLexicalSerializationTests.ProcessingInstruction_ParserPositionLimitIsSubstrateNotSerialization`
  (pre-existing).

Doc-comments: `XmlWriter::WriteProcessingInstruction` and `XProcessingInstruction`, both stating
the limitation, its cause, and that .NET does not share it.

Gate: **17,424 run, 17,424 passed, 0 failed, 0 skipped** across 38 executables — `+3` on 17,421,
exactly the three new cases (`SharpRuntimeTests_Xml` 515 → 518). No other executable moved.

## 6. What would reopen this

A substrate with a real processing-instruction node type, or an upstream tinyxml2 that grows one.
The asymmetry pin is written so it fails the moment the limitation lifts:

> *"if this stops throwing, #2202's limitation is gone and the note on
> `WriteProcessingInstruction` and `XProcessingInstruction` must be removed"*

## 7. Downstream

No behaviour changed, so nothing downstream can break. Neither `cna` nor `mobile-eggbert` was
modified.
