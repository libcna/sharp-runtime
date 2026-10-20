<!-- SPDX-License-Identifier: MIT -->
# XmlWriter embedded NUL exception category

CNA-DEMOS-ISSUE-147: XmlWriter's content-writing doors now reject embedded NUL
with `System::ArgumentException`, rather than `System::Xml::XmlException`.
The existing message still names the writing member and NUL; validation stays
before the native DOM receives the content, avoiding silent truncation.

Affected doors: WriteString, WriteAttributeString, WriteCData, WriteComment,
WriteProcessingInstruction, WriteElementString (through WriteString), and the
internal subset of WriteDocType. Callers that catch XmlException specifically
for invalid character data should catch ArgumentException. Malformed XML names
and other XmlException paths are not reclassified by this repair.

Evidence: `cna-demos/docs/repair-evidence/content/XmlWriterNulOracle.cs` passes
21/21 on both genuine Framework4.0.30319.1/Win32NT and .NET8.0.31/Unix. It checks
two NUL placements at seven doors and seven ordinary-content controls. The local
.NET11 source agrees: System.Private.Xml/System/Xml/Core/XmlEncodedRawTextWriter.cs
lines1631–1655 calls XmlConvert.CreateInvalidCharException when character checking
is enabled; System.Private.Xml/System/Xml/XmlConvert.cs lines1624–1627 selects
ArgumentException. This is SA-5 reference alignment, with no public spelling,
layout, vtable, ownership or noexcept change.

LINQ direct serialization has the same correction for XText, XCData, XComment,
processing-instruction data, document-type internal subsets, XAttribute values,
and XElement attribute/namespace values. The shared character-data guard retains
pre-write rejection and useful diagnostics; the separate XML-name/declaration
guard is unchanged. `XLinqNulOracle.cs` passes74/74 on genuine Framework4 and
.NET8.0.31: four
NUL placements and ordinary content at14 doors, plus four standalone attribute
namespace controls. The reference omits a standalone XAttribute's namespace URI;
this port emits it, so its existing URI guard remains necessary. This repair does
not claim to align that separate formatting difference. The earlier expanded
oracle's71/75 result disproved a proposed namespace expectation and is retained.

Local .NET11 XNode.cs:689–718 and XStreamingElement.cs:226–246 create default
XmlWriterSettings (CheckCharacters=true). Before the LINQ repair, the corrected
native19-test NUL suite gives7PASS/twelve FAIL. Names/metadata expectations are
not changed merely because character-data expectations changed.

The port's existing unconditional NUL rejection remains a documented limitation.
With CheckCharacters=false, .NET entitizes NUL in text/attributes and emits literal
NUL in several other doors; tinyxml2's string boundary cannot retain the latter.
This narrow repair does not implement that mode or claim complete XML parity.
WriteElementString is a compound start/write/end operation: this change does not
promise atomicity for its already-started element on a rejected value.

Before repair, the evidence-backed native XmlWriterValidationTests run has46PASS/
three FAIL of49: exact all-door category, retained useful diagnostic, and a new
unchanged-output guard for the six direct mutation doors. After repair49/49 pass;
LINQ20/20 pass, including pre-output namespace rejection. Both builds are warning-free.
Full post-repair gate passes18146/18146 across41 executables, zero failures/skips,
with the original isolated Yacht service and supported XNA_SAMPLES_ROOT fixture
directory. Report: build-probe/cna-repair-xml-linq-components-final.log.
An earlier invocation omitted that fixture environment and had five skips, not
behavior failures; its complete log remains *-components-missing-fixtures.log.
The two preceding failed gates and watcher stress findings are retained separately
in Testing-WatcherRenameBarrier.md. No public API/layout change; all assertions
still require rejection before invalid data reaches the native XML substrate.
