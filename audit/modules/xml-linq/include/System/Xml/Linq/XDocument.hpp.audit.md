# Audit: `modules/xml-linq/include/System/Xml/Linq/XDocument.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

Document structure validation, declaration, and normal serialization paths are covered; namespace parsing and lexical DTD edge cases remain limited.

### Correction appended by ticket #2197 (2026-08-10) — remediated

This finding is **remediated**. Measured corrections to the record above, all additive:

- **It emitted MALFORMED XML, not only namespace-lossy XML.** Two attributes differing only by
  namespace serialized as `<r x="1" x="2"/>`, and re-reading that throws
  `XML_ERROR_PARSING_ATTRIBUTE` from this module's own parser. Silent corruption at a public
  door.
- **It destroyed namespace declarations built the way .NET builds them.**
  `XAttribute(XNamespace::Xmlns + "p", "urn:x")` — the exact spelling `XAttribute.cpp`'s own
  `ValidateAttribute` exists to validate — serialized as `<e p="urn:x"/>`: the declaration became
  an **ordinary attribute named `p`**, silently unbinding the prefix for the whole subtree.
- **`getIsNamespaceDeclarationProperty()` was wrong for parsed input, inconsistently** — `0` for
  `xmlns:p`, `1` for `xmlns`. The default form was right by accident: its literal local name *is*
  `"xmlns"`. The prefixed form was misclassified because its name was never split.
- **The surface is six files, not five.** `XStreamingElement::WriteTo` wrote local names only,
  by the same reasoning and with the same in-code comment.
- **The DOM layer already resolved namespaces correctly** — `XmlNode::getLocalNameProperty`/
  `getPrefixProperty`/`getNamespaceURIProperty` and `XmlAttribute::getNamespaceURIProperty`,
  including the two rules easiest to get wrong (an unprefixed attribute takes **no** namespace;
  the `xml` prefix is built in). The converter simply never called them. This is family **X-C**,
  a public door bypassing a resolver the module already ships — the same family as SR-AUD-335,
  in the same module.

**The repair.** Parsing builds `XName` from the DOM's resolved parts; serialization gives both
doors a namespace scope rebuilt from the tree, reusing a declared prefix where one is in scope
and generating `p1`, `p2`, … with a declaration where none is. `XElement::GetDefaultNamespace()`
and `GetPrefixOfNamespace()` are added (purely additive). **Nothing is cached in any object** —
that is what keeps the repair object-layout compatible, and it is deliberate: caching would need
a field on `XObject`, which is the approval #1896 was refused.

**+48 permanent regressions** (`XLinqNamespaceTests.cpp`). **Five mutations; four discriminated
immediately and the fifth did not, which is recorded rather than hidden**: allowing an attribute
to take the default namespace's prefix — the XML Namespaces rule most often got wrong — passed
the whole suite, so a test that discriminates it was added and the mutation re-run against it.
ASan+UBSan+LSan over the four changed production bodies (60-deep rebinding chain, 200-attribute
allocator stress with `p1`–`p50` pre-taken, undeclaration, reserved prefixes, and every
rejecting door): **exit 0, zero reports**; non-recovering UBSan **exit 0**; a deliberate
out-of-bounds control in the same build **did** report.

**Behaviour change, documented in `docs/Migration-XmlLinqNamespaces.md`:** namespaced trees
parse, query and serialize differently; `xmlns:p=""` and a genuine duplicate expanded attribute
name are now rejected; an unqualified tree is unchanged byte for byte. Two pre-existing tests
asserted the *defect* (a local-name lookup finding a namespaced attribute, and
`XAttribute::ToString()` dropping the namespace) and were updated to the stronger contract, with
both layers of their history kept in the comment.

**Deliberately unchanged:** an **undeclared** prefix is still accepted and still unresolved
(#2083 owns that narrowing question at the DOM layer);
`SaveOptions::OmitDuplicateNamespaces` is still inert. No `SR-AUD-*` identifier was issued;
numbering stays frozen at **364**.

## Missing assertions and diagnostics

Add namespace, declaration, DTD lexical, retained-child lifetime, and file-path error regressions.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
