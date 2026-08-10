# Audit: `modules/xml/src/System/Xml/XmlWriter.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-349 — medium — XmlWriter emits malformed XML because names and writer state are not validated

The writer forwards element names directly to tinyxml2 and exposes no writer-state validation.  The direct probe writes `WriteStartElement("1bad")`, obtains `<1bad/>`, and the project reader rejects that result with `XML_ERROR_PARSING`.  A successful writer call can thus produce a document its paired reader cannot consume.

## Remediation record — #2076 (2026-08-04)

**REMEDIATED.** The finding's evidence is retained above; this section appends what
implementation measured. Plan: `docs/SystemXmlNamespaceReviewPlan.md` §4.3 and §20.4.
Reproduction: `build-probe/2076_probe1_writer_doors.cpp`, logs
`build-probe/2076_probe1_before.log` → `build-probe/2076_probe1_after.log`.

**The surface is wider than the one example.** The report names
`WriteStartElement("1bad")`. Measured against `1b65f0f`, **four** public doors take an XML
name and validated **none** of them:

| Door | Before | After |
|---|---|---|
| `WriteStartElement("1bad")` | `<1bad/>`, rejected by this module's own reader | `XmlException` |
| `WriteAttributeString("1bad","v")` | `<e 1bad="v"/>`, rejected by the same reader | `XmlException` |
| `WriteProcessingInstruction("a?>b","d")` | `<?a?>b d?>` — the `?>` **closed the instruction early** and spilled `b d?>` into document-level text | `XmlException` |
| `WriteDocType("1bad",…)` | `<!DOCTYPE 1bad>` | `XmlException` |

`WriteElementString` inherits the first. The PI row is the sharpest: the writer already
sanitized the PI **data** for exactly this hazard, and the **target** had no door at all.

**The state clause was also wider.** Every one of the twelve `Write*` members stayed callable
after `Close()` and silently discarded its argument; an unbalanced `WriteEndElement()` and an
attribute written with no element open were discarded as well.

**The corrected premise held.** `XmlConvert::VerifyName` already existed and
`XmlDocument::CreateElement` already used it. All four name doors now route through it, so the
writer door and the DOM door report an **identical** diagnostic for identical input — including
`ArgumentException` for an empty name, which is the validator's own pre-existing choice and is
deliberately not re-mapped. No name grammar was invented, which is what keeps the repair
evidence-backed with `/rv/tmp/runtime/` absent.

**Writer state** is enforced with `System::InvalidOperationException` — this port's choice,
recorded as such. `Close()` marks the writer closed **before** flushing, so a writer whose save
failed is terminally closed; it stays idempotent because `~XmlWriter()` calls it
unconditionally, and `ToString()`/`Flush()` stay usable because `ToString()` is the only way to
read an in-memory writer's result back.

**+27 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Xml` 400 → 427, `SharpRuntimeTests_Xml_Linq` 184 unchanged even though
`modules/xml-linq` is a real consumer of every repaired door.

**Nine mutations, each reverted from an exact backup with `git diff --stat` identical on both
sides.** M1 (`ThrowIfClosed` emptied) fails exactly 3 tests; M2 (unbalanced `WriteEndElement`
silent again) 3; M3 (element name) 7; M4 (attribute name) 4; M5 (PI target) 2; M6 (DOCTYPE
name) 1; M7 (no-open-element guard reverted to the original silent drop) 1; M8 (`Close()`
idempotency) 1; M9 (`closed` set after the flush instead of before) 1. **One false pass is
recorded rather than hidden:** M7's first attempt left `ThrowIfNoOpenElement` unused, the build
aborted on `-Werror=unused-function`, and the run reported the **stale** previous binary's
`427 PASSED`. It was re-run with the build output unsuppressed and the helper marked
`[[maybe_unused]]`, and only then did it discriminate.

**Sanitizers: a measured non-result.** §13 of the plan predicts no sanitizer relevance for
this ticket. Confirmed rather than assumed: ASan, UBSan and LSan are clean over **3,700**
rejections and the abandoned-mid-document destructor path, with `XmlWriter.cpp`,
`XmlConvert.cpp` and `vendor/tinyxml2/tinyxml2.cpp` compiled **from source** (`Xml` is a
`STATIC` component, so the archive would have been uninstrumented), and instrumentation proven
by a control heap-use-after-free in the same binary
(`build-probe/2076_probe4_asan.cpp`, log `build-probe/2076_probe4_asan.log`).

**Measured and deliberately NOT repaired here**, each recorded with evidence rather than
absorbed into this ticket:

- **#2084** — `WriteDocType`'s `publicId`/`systemId`/`internalSubset` are quoted-literal doors
  with no validation: `systemId = "s\">x<!--"` emitted
  `<!DOCTYPE r PUBLIC "p" "s">x<!--">` and an `internalSubset` containing `]` emitted
  `<!DOCTYPE r []><evil/><!--]>`; **both are rejected by this module's own reader**, so this is
  the same closure defect on a *literal* rather than a *name* door
  (`build-probe/2076_probe3_injection.log`).
- **#2085** — an embedded NUL silently truncates content at three doors: `WriteString("a\0b")`
  emitted `<e>a</e>`, an attribute value `"x\0y"` emitted `a="x"`, and
  `WriteCData("c\0d")` emitted `<![CDATA[c]]>` (`build-probe/2076_probe2_content.log`).
- **Recorded, no ticket.** Two roots (`<a/><b/>`) and document-level text are accepted; both are
  governed by `XmlWriterSettings::ConformanceLevel`, which the settings header already documents
  as *"Not currently enforced"*, so narrowing them is a settings-semantics decision, not this
  finding. A PI target of `xml` is accepted; `XmlReader.cpp` documents a **deliberate**
  handling for that target, so rejecting it would contradict a stated in-module contract.

## Missing assertions and diagnostics

- Tests do not assert rejection of invalid XML names, invalid call ordering, or operations after `Close()`.
- Add validation diagnostics naming the invalid element/attribute and current writer state before native serialization.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
