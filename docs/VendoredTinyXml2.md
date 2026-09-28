<!-- SPDX-License-Identifier: MIT -->

# Vendored tinyxml2: namespace isolation

`vendor/tinyxml2` is the one vendored library in this repository that carries a local
modification. It changes names only, never parser behaviour.

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
2. Apply the table above again, including the modification comment in `tinyxml2.h`.
3. Build and run `SharpRuntimeTests_Xml`. The isolation test fails to compile if any global name
   was missed.

The copy vendored in `8703129c` (2026-06-10) reports version 11.0.0, but it is not byte-identical
to upstream's `11.0.0` tag. The namespace isolation described here is the only change made to it
in this repository since then.
