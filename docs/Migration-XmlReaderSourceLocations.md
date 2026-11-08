<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# XmlReader source locations (CNA HND-TQ-01)

`XmlReader::getBaseURIProperty()` now exposes an absolute, escaped file URI for a
reader created from a local filename. String input has an empty base URI. Closing
the reader clears its reported base URI.

Line information now identifies the source token, including separate closing tags
and attributes. Columns are one-based UTF-16 code-unit positions, with CRLF counted
as one newline. Entity decoding does not move subsequent source locations. Before
the first node, after EOF, and after Close, line and column are zero.

This is an additive nonvirtual member. XmlReader still contains one owning pointer;
its size, alignment, base classes and vtable are unchanged. Existing source spellings,
exception specifications and accepted inputs are unchanged. Consumers linking the
new getter must rebuild/link the updated Xml archive. Existing callers observing
line information will now receive real columns instead of the old constant zero.
The internal event state grows; original source bytes are discarded after locations
have been attached, rather than retained for the reader's entire lifetime.

The independent reference is the local .NET snapshot
`System.Private.Xml/src/System/Xml/Core/XmlTextReaderImpl.cs`: BaseURI lines933–939,
IXmlLineInfo lines1906–1921, element positioning line4315, text positioning line5418,
and processing-instruction positioning line6217. This snapshot is .NET11 preview;
CNA's corresponding identity format is independently pinned by XNA4 Refresh
`IntermediateReader.CreateInvalidContentException` IL0006–0033 and IL0047–007b.

The tinyxml2 parser remains eager and keeps its existing grammar/diagnostic
boundaries. Its parse exceptions now carry its own error line, but no precise
error column; this does not claim .NET-equivalent malformed-XML error locations.
XmlReaderSettings line offsets remain outside this change.

Validation is recorded in the CNA HND-QUALITY lane REPORT.md. The existing CNA
headless build disables standalone SharpRuntime tests, so the new runtime behavior
is exercised through device-free CNA pipeline regressions as well as the updated
XmlReaderNavigationTests expectation. The standalone Xml gate remains an integrator
check; no full-runtime test-count claim is made.
