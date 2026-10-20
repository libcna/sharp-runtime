<!-- SPDX-License-Identifier: MIT -->

# XML character input is not an encoded byte stream

## CNA-DEMOS-ISSUE-148 (2026-10-20)

`XmlReader::CreateFromString` and `XmlDocument::LoadXml` accept already-decoded
character strings, represented here as UTF-8. tinyxml2's parser instead accepts
bytes and automatically consumes a leading UTF-8 preamble. Passing the string
straight through therefore silently accepted a leading U+FEFF that .NET rejects
as invalid root-level character content.

The character boundary now recognizes this invalid preamble without changing the
vendored parser's byte handling. `LoadXml` raises `XmlException` directly.
`CreateFromString` returns a reader in `Initial`; its first `Read` raises
`XmlException("Data at the root level is invalid. Line 1, position 1.")` with
line/position properties 1/1 and enters `ReadState::Error`. Later reads return
false, preserving Error. `Close` is terminal and suppresses the pending error
if called before reading.

The existing convenience `Create` heuristic also recognizes BOM-prefixed XML
before treating `/` in a closing tag as a path separator. It delegates character
input to the same boundary. A filename beginning with U+FEFF is not blindly
stripped or redirected: it must also have the existing XML-content `<` signal.
File loading still accepts a byte preamble. Embedded U+FEFF in text/attributes is
retained. XDocument/XElement Parse inherit the character rule through LoadXml;
their Load paths retain byte-stream behavior.

This does not change public signatures or public object layout. The reader flag
lives in its existing opaque implementation state. The private helper is shared
by reader and DOM; CNA does not emulate this System.Xml rule in a caller.

## Scope and timing limitation

This is the leading-character-preamble repair, not a replacement streaming parser.
The DOM-backed reader still reports other tinyxml2 parse failures during factory
creation rather than at .NET's streaming cursor position. Its public class
documentation now says so explicitly. No claim of full malformed-document
streaming parity is made.

## Evidence

The permanent CNA campaign `XmlReaderEnvelopeOracle.cs` passes 61/61 against
genuine Framework 4 and .NET 8.0.31. The XNA intermediate corpus additionally pins
the exact wrapped InvalidContentException message for a character BOM; rejecting
at reader creation would bypass that wrapping boundary.

Before repair, five new native controls yield three passes/two failures:
character reader/DOM acceptance is wrong, while byte-file acceptance, embedded
character preservation and close-before-first-read already pass. After repair,
the whole XML suite passes 574/574 and XML.Linq passes 352/352, including two new
Parse/Load controls. The full 41-executable component gate passes 18,158/18,158
with zero failures/skips and the unchanged private Yacht SOAP fixture.

The existing `build/` was rebuilt incrementally with shared ccache, maximum two
jobs, zero errors/warnings. Module boundaries pass (44 modules/109 edges).
Actual XmlReader.cpp and XmlDocument.cpp plus the five new controls were compiled
with ASan/UBSan in existing `build-probe/`, using one compiler; 5/5 pass with leak
detection and halt-on-error enabled, no diagnostics. Other linked dependencies
are the normal archives, not claimed instrumented.

Reports live in the CNA campaign under
`../cna-demos/build/linux-full/reports/repair-xml-bom-*`. Downstream CNA validation
is recorded separately when complete; this document does not close the entire
CNA issue on the dependency tests alone.
