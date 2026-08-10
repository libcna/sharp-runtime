# Audit: `modules/xml-linq/include/System/Xml/Linq/XCData.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-335 — medium — direct serializers emit or corrupt XML special lexical content

XCData SerializeTo writes the literal CDATA delimiter without splitting a
value containing ]]>. The probe serializes left]]>right as
<![CDATA[left]]>right]]>; parsing through the local XML path returns
leftright]]>, losing the delimiter. XComment and XProcessingInstruction direct
serializers similarly concatenate unvalidated -- and ?> sequences.

### Correction appended by ticket #2196 (2026-08-10) — remediated

This finding is **remediated**. Measured corrections to the record above, all additive:

- **The surface is five doors, not three node kinds.** The processing-instruction **target** is
  a fifth door and it failed differently from the other four: measured,
  `XProcessingInstruction("a?>b", "d")` emitted `<?a?>b d?>` through the direct door while the
  sibling `WriteTo()` door **threw** `Invalid XML name: 'a?>b'.`
- **The writer doors were already correct before this ticket.** `System::Xml::XmlWriter` has
  always shipped `sanitizeCDataText` / `sanitizeCommentText` /
  `sanitizeProcessingInstructionText` and always called `XmlConvert::VerifyName` on the PI
  target. The defect was confined to `SerializeTo` — that is, `ToString()`,
  `ToString(SaveOptions)` and `Save(fileName)`, plus every containing element/document that
  recurses into them. This is family **X-C**: a public door bypassing a validator the module
  already ships, exactly as `docs/SystemXmlNamespaceReviewPlan.md` §17 predicted for this module.
- **The three consequences were three different shapes.** CDATA was **lossy** (`left]]>right`
  re-read as the value `leftright]]>`, one node having become a CDATA node plus a text node);
  the processing instruction **silently dropped data** (measured
  `build-probe/2196_probe4_pi.log` case P09: the emitted `<?p left?>right?>` re-read with
  `data == "left"`, `right` gone); the comment was **silent** — this runtime's parser accepts
  the invalid `<!--left--right-->`, so nothing signalled the corruption at all.
- **The repair shares one definition rather than copying one.** The three transforms moved to
  `modules/xml/include/System/Xml/detail/XmlLexicalSanitizer.hpp`, so the writer door and the
  direct door cannot drift apart. `XmlWriter`'s behaviour is unchanged character for character
  (483/483 `SharpRuntimeTests_Xml` pass unmodified).
- **Validation was added at serialization, not at construction.** `XProcessingInstruction`'s
  constructor and `setTargetProperty` still accept a malformed target — the class doc-comment
  records non-validation there as a deliberate scope decision, and narrowing it is a wider
  accepted-input change than this finding calls for. That boundary is now pinned by a test.

**+28 permanent regressions** (`XLinqLexicalSerializationTests.cpp`), **four mutations, all
discriminating** (removing the CDATA split fails 8, the comment protection fails 6, the PI
target validation fails 2, the PI data protection fails 2). ASan+UBSan+LSan and non-recovering
UBSan over the four changed production bodies: **exit 0, zero reports**, with a deliberate
out-of-bounds control in the same build proving the instrumentation is live. No public
signature, object layout, vtable or `noexcept` change.

**One post-audit defect was found and is NOT part of this finding: #2202** — this runtime's
parser accepts a processing instruction only before every other node, so a PI written inside an
element or after the root cannot be read back at all. Measured with the emitted text held
constant (`<root><?p d?></root>`, no special character anywhere), so it is independent of the
delimiter repair. **No `SR-AUD-*` identifier was issued**; numbering stays frozen at **364**.

Two adjacent doors are deliberately **not** repaired here and carry their own tickets, each
waiting on an already-open `modules/xml` decision: **#2200** (`XDocumentType::SerializeTo`'s
three unvalidated quoted literals, twin of #2084) and **#2201** (an embedded NUL crossing the
direct serializers verbatim, twin of #2085 and its mirror image — the Linq door *emits* the NUL
where the writer door *truncates* at it).

## Missing assertions and diagnostics

Add exact CDATA delimiter, comment double-hyphen/trailing-hyphen, processing
instruction target/data, and XML 1.0 control-character validation tests for
ToString, Save, and WriteTo.

## Final assessment

Confirmed XML lexical preservation/validation defect: SR-AUD-335.
