# Audit: `modules/xml-linq/include/System/Xml/Linq/XElement.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-334 — medium — namespace URI semantics are discarded by Xml.Linq parse and serialization

XElement exposes XName namespace-bearing construction and query APIs, but the
implementation only writes local names. The direct probe constructs
{urn:audit}root with a namespaced attribute and receives
namespaced-serialized=<root attribute="value"/>; reparsing reports an empty
namespace URI. Parsing a prefixed XML input likewise produces XName p:root
with an empty namespace URI instead of resolving urn:audit.

### Correction appended by ticket #1890 (2026-07-31) — the approved core repair landed; finding still `confirmed`

`XContainer` now clears the parent link of every child node it still owns in its
own destructor, and `XElement` clears every owned attribute's parent link and its
`next_` sibling link. 19 of this finding's 21 ASan `heap-use-after-free` cases
stop producing one; the XObject faulting-access count falls 48 → 4; **all eight**
public entry points that aborted with `pure virtual method called` stop doing so.
`sizeof`, class/vtable layout and allocation counts are unchanged (measured). The
full before/after table and the complete residual list — X15/X17 (#1892), X20
(#1891), X27c/X27d (#1893), X21 (permanent deviation) — are recorded once, on
`audit/modules/xml-linq/include/System/Xml/Linq/XObject.hpp.audit.md`, and in
`docs/OwnedTreeLifetimeContractPlan.md` §34. **SR-AUD-333 stays
`confirmed (design-complete)`**; numbering stays frozen at 364.

## Missing assertions and diagnostics

Add default/prefixed namespace parse, namespace-aware query, nested scope,
attribute namespace, save/reload, and OmitDuplicateNamespaces regressions.

## Final assessment

Confirmed namespace identity/round-trip defect: SR-AUD-334.
