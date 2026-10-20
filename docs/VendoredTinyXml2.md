<!-- SPDX-License-Identifier: MIT -->

# Vendored tinyxml2: namespace isolation and processing instructions

`vendor/tinyxml2` is the one vendored library in this repository that carries a local
modification. It carries namespace isolation and, with explicit owner approval
on 2026-10-20, the narrow processing-instruction repair described below.

## What changed

| Upstream | Here |
|---|---|
| `namespace tinyxml2` | `namespace SharpRuntime::Vendor::tinyxml2` (written as three nested blocks, so the files still compile as C++11) |
| include guard `TINYXML2_INCLUDED` | `SHARP_RUNTIME_VENDOR_TINYXML2_INCLUDED` |
| global `TIXML2_MAJOR/MINOR/PATCH_VERSION`, `TINYXML2_MAX_ELEMENT_DEPTH` | the same constants, inside the namespace above |
| macros `TINYXML2_MAJOR/MINOR/PATCH_VERSION` | `SHARP_RUNTIME_TINYXML2_MAJOR/MINOR/PATCH_VERSION` |

`tinyxml2.h` states the same list in a comment below the upstream licence, as the zlib licence
requires of an altered copy. The remaining macros (`TINYXML2_LIB`, `TIXMLASSERT`,
`TINYXML2_CONSTANT`) are left as upstream spells them.

The build side matches: `sharp_runtime_tinyxml2` publishes no include directory. Headers reach the
copy as `<tinyxml2/tinyxml2.h>` through the `vendor/` root every component already carries, so a
consumer's own `#include <tinyxml2.h>` can no longer resolve to this copy.

Implementation files (`modules/xml/src`, `modules/xml/tests`) say `tinyxml2::` through a local
`namespace tinyxml2 = ::SharpRuntime::Vendor::tinyxml2;` alias. Public headers spell the full name,
because a consumer may have a `::tinyxml2` of its own.

## Why

`Xml` exposes tinyxml2 types in public headers (`XmlDocument::getNativeDocument()`,
`XmlNode::getNativeNode()`), so `sharp_runtime_tinyxml2` is a public link dependency of every
program that uses `System::Xml`. While the copy used upstream's names, a program that also carried
its own tinyxml2 linked two definitions of the same classes. The two need not agree. MeshCraft's
`Mc3` used tinyxml2 10.0.0, while this copy reports 11.0.0 and has a different `MemPoolT` layout.
Static archives do not report duplicate symbols, so the linker bound some of `Mc3`'s calls to this
copy's code. `MeshCraft --export` then crashed in `XMLDocument::~XMLDocument` with no diagnostic
at build time (2026-09-27).

Renaming the namespace gives each copy its own symbols. The two can be linked into one program,
and included into one translation unit, without affecting each other.
`modules/xml/tests/System/Xml/VendoredTinyXml2IsolationTests.cpp` pins this: it defines upstream's
include guard and its own `::tinyxml2` before including `System/Xml/XmlDocument.hpp`, which could
not compile before the change.

## Updating the vendored copy

1. Replace `vendor/tinyxml2/tinyxml2.{h,cpp}` with the new upstream files. Keep CRLF line
   endings, as upstream ships them.
2. Apply the table above again, including the modification comment in `tinyxml2.h`,
   and preserve the processing-instruction repair or verify upstream now implements it.
3. Build and run `SharpRuntimeTests_Xml` and `SharpRuntimeTests_Xml_Linq`. The isolation
   test fails to compile if any global name was missed; the processing-instruction tests
   protect the parser change and both writer-to-reader round trips.

The copy vendored in `8703129c` (2026-06-10) reports version 11.0.0, but it is not byte-identical
to upstream's `11.0.0` tag. Namespace isolation and the following parser patch are the
local changes since then; do not describe this as pristine upstream parser behavior.

## Processing-instruction placement (CNA-DEMOS-ISSUE-148)

The owner explicitly authorized a narrow vendor repair with regression tests.
`XMLNode::ParseDeep` previously applied the XML declaration placement restriction
to every `XMLDeclaration` node. That node also represents ordinary processing
instructions; no new node kind or ABI change is needed.

The local patch checks the target token. Ordinary processing instructions may
occur before, inside and after the root and after comments. The case-insensitive
reserved target `xml` is permitted only with lowercase spelling and as the first
document node. Invalid target tokens are refused rather than becoming newly
accepted nested markup. `xml-stylesheet` is not the reserved target.
The [XML 1.0 grammar](https://www.w3.org/TR/xml/#sec-pi) distinguishes these cases.

The patch does not rewrite source text, strip instructions, decode their data as
entities, change the node representation, or alter byte-stream BOM processing.
Comment/CDATA/attribute literals remain literals. It is not a claim to implement
every remaining XML conformance rule. The native regression
`VendoredTinyXml2ProcessingInstructionTests` covers placement, reserved declaration
controls, exact instruction data and adjacent text, and literal/byte-BOM controls.
The old #2202 limitation tests are replaced by positive round-trip expectations
for the same inputs, not removed.

Validation on Linux, using the existing `build/` and `build-probe/` trees with
at most two compilation jobs and shared ccache:

- Before the parser repair, three of the four new vendor tests fail; all 564
  pre-existing XML tests pass. The full baseline XML run is 565 PASS / 3 FAIL.
- The repaired XML suite passes 569/569, XML.Linq 350/350. This includes all four
  vendor tests and the new reader position/data/depth test, plus the corrected
  old limitation tests for their unchanged inputs.
- Actual vendored parser compiled with ASan/UBSan and leak detection: all four
  regressions pass without diagnostics. Linked GoogleTest itself is not instrumented.
- Full incremental build: zero errors/warnings after correcting a test-literal
  trigraph warning. The failed initial build is retained.
- All 41 component executables: 18,151 PASS, zero FAIL/SKIP, using the original
  Yacht SOAP fixture on a private loopback port. An independently reproduced
  pre-existing test scheduling race was corrected separately; see
  [the test synchronization record](ProcessReaderTestSynchronization.md).

CNA-DEMOS-ISSUE-148 is not yet closed: character-input BOM behavior, error timing
and downstream canonical gates remain separate work. No byte-BOM behavior was
changed by this patch. CNA campaign evidence lives in
`../cna-demos/docs/repair-evidence/content/` and its repair handoff.
