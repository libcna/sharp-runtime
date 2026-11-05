<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
# XmlReader rejects malformed UTF-8 storage (GAP-22)

`XmlReader::CreateFromString`, both `Create` overloads (content and file routes),
and the forwarding `XmlTextReader` now refuse ill-formed UTF-8. Previously tinyxml2
could expose those bytes as text or attributes; writing that text then failed.

The complete input is checked before parsing or exposing nodes. Reader creation
succeeds for this encoding failure; the first read throws `XmlException` with
`Invalid character in the given encoding.` and sets `ReadState::Error`. Subsequent
reads return false. Closing before the first read suppresses the deferred error.
`CheckCharacters=false` cannot disable encoded-input validity.

No public names, signatures, exception specifications, object layouts or vtables
change. The changed state is behind the existing opaque pointer. Callers should
keep their exception handling around reads/deserialization, not only creation.
No data conversion or replacement of malformed sequences is performed.

The existing character/file BOM distinction remains: leading U+FEFF in character
input is invalid root content; a UTF-8 BOM on encoded files is consumed. Valid
scalar sequences, including supplementary characters, keep their exact UTF-8 bytes.
UTF-16 and other file encodings are not introduced by this repair. XML character
legality beyond encoded scalar validity and exact line/column/BaseURI diagnostics
remain separate concerns. Native invalid-encoding diagnostics omit position suffixes.

Oracle: .NET 11 preview reference snapshot
`/rv/tmp/runtime/src/libraries/System.Private.Xml/src/System/Xml/Core/XmlTextReaderImpl.cs:21-22`
selects strict UTF-8 decoding; `InvalidCharRecovery` at 3535-3562 raises
`Xml_InvalidCharInThisEncoding`, whose exact base message is in
`src/Resources/Strings.resx:183-184`. This is snapshot-specific corroboration;
the independent UTF-8 scalar rules and retained .NET 4/CNA MTH-SAFE-007 finding
settle the rejection contract. XNA Refresh `IntermediateSerializer.Deserialize<T>`
IL_0081..00a5 catches the read-time XML error, and `IntermediateReader`'s
`CreateInvalidContentException` IL_0080..0095 retains the inner exception.

Consumer integration: rebuild the sibling SharpRuntime used by CNA and relink its
content-pipeline tests/fuzzer. No public spelling migration or C API mapping change
is needed. Run the focused XML/CNA regressions, seed replay and the repository gate
before landing.
