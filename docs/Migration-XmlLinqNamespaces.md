<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Xml::Linq` namespace semantics (ticket #2197, SR-AUD-334)

*2026-08-10.* This change is **source-, ABI-, layout-, vtable- and `noexcept`-compatible**, and
**behaviour-incompatible on purpose** for trees that use XML namespaces. Code that uses no
namespace at all is unaffected, byte for byte — that property is pinned by
`XLinqNamespaceTests.Serialize_UnqualifiedTreesAreByteIdenticalToBefore`.

## What was wrong

`System::Xml::Linq` exposed a full namespace model — `XName` carries a URI, `XNamespace` builds
qualified names, `XAttribute` validates namespace declarations — and then discarded it at both
ends:

- **Parsing** built every `XName` from the raw qualified tag text, so `<p:root xmlns:p="urn:a"/>`
  produced a name whose **local** part was the whole string `p:root` and whose URI was empty.
- **Serialising** wrote only local names.

That was not only a fidelity gap. Measured:

| Input | Emitted before | Consequence |
|---|---|---|
| `{urn:a}x="1"` and `{urn:b}x="2"` on one element | `<r x="1" x="2"/>` | **this runtime's own parser rejects it** (`XML_ERROR_PARSING_ATTRIBUTE`) |
| `XAttribute(XNamespace::Xmlns + "p", "urn:x")` | `<e p="urn:x"/>` | the **declaration became an ordinary attribute**, unbinding the prefix for the whole subtree |
| `{urn:a}root` | `<root/>` | the namespace is gone from a round trip |

and `getIsNamespaceDeclarationProperty()` answered **false** for every parsed `xmlns:prefix`
attribute, while answering true for a bare `xmlns` only by accident.

## What changed

1. **Parsed names carry their URI.** `<p:root xmlns:p="urn:a"/>` now yields the name
   `{urn:a}root`. Resolution uses the resolvers `modules/xml` already ships, including the two
   rules that are easiest to get wrong: an **unprefixed attribute takes no namespace** (a
   default `xmlns` governs element names only), and the **`xml` prefix is built in**.
2. **Declarations survive as declarations.** `xmlns:p="…"` parses to the name
   `{http://www.w3.org/2000/xmlns/}p` and `xmlns="…"` to the unqualified name `xmlns`, so
   `getIsNamespaceDeclarationProperty()` is now correct for both.
3. **Serialisation emits prefixes and declarations.** A prefix already in scope is reused; a
   namespace with none gets a generated `p1`, `p2`, … and its declaration on the same start tag.
   An unqualified element under an inherited default namespace emits `xmlns=""`.
4. **Two additive members:** `XElement::GetDefaultNamespace()` and
   `XElement::GetPrefixOfNamespace(const XNamespace&)`.
5. `XAttribute::ToString()`, `XElement::WriteTo` and `XStreamingElement::WriteTo` follow the
   same rules.

## What callers must change

### 1. Look names up by their qualified name, not their local name

```cpp
auto root = XElement::Parse("<cfg xmlns=\"urn:game\"><level/></cfg>");

// Before — worked, because the URI was thrown away:
auto level = root->Element(XName("level"));           // now nullptr

// After:
XNamespace ns = XNamespace::Get("urn:game");
auto level = root->Element(ns + "level");             // found
```

The same applies to `Elements(name)`, `Descendants(name)`, `Attribute(name)` and
`getAttributeValue(name)`. **This is .NET's behaviour**, and it is the point of the change: a
local-name lookup matching a namespaced element was the defect.

If a document has no namespaces, nothing changes.

### 2. Expect prefixes in serialised output

`ToString()`, `Save()` and `WriteTo()` now emit qualified names and declarations. Byte-for-byte
comparisons of serialised namespaced XML will differ. Compare parsed trees
(`XNode::DeepEquals`) rather than text where you can.

The **spelling** of a generated prefix (`p1`, `p2`, …) is this port's choice and is not
guaranteed to match .NET's; the reference source is not available in this environment. Declare
your own prefix if you need a specific one:

```cpp
auto e = std::make_shared<XElement>(XNamespace::Get("urn:game") + "cfg");
e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "g", "urn:game"));
// -> <g:cfg xmlns:g="urn:game"/>  instead of <p1:cfg xmlns:p1="urn:game"/>
```

### 3. Two inputs are now rejected that were accepted before

- **`xmlns:p=""`** — undeclaring a prefix is forbidden by XML Namespaces 1.0 (only 1.1 allows
  it). `XAttribute`'s declaration validator has always enforced this; before this change no
  parsed declaration ever reached it. It now throws `System::ArgumentException` from `Parse`.
- **A document whose two prefixes bind to the same URI and carry the same local attribute name**
  (`<r xmlns:a="u" xmlns:b="u" a:x="1" b:x="2"/>`) is a genuine duplicate-attribute document
  under XML Namespaces, and `XElement::Add` now rejects the second one with
  `System::InvalidOperationException`.

### 4. A programmatically built tree is not `DeepEquals` to its own round trip

A tree built in code carries no declaration attribute; serialising adds one, so the reparsed
tree has one attribute more. Namespace declarations **are** attributes and `DeepEquals` compares
attributes, so the two differ — in .NET too. A **second** round trip is stable, and that is the
property to assert.

## What deliberately did **not** change

- **An undeclared prefix is still accepted and still unresolved.** `<p:r/>` with no `xmlns:p` in
  scope keeps the local name `p:r` and an empty URI. .NET rejects such input; narrowing what
  this runtime accepts is the open question ticket #2083 owns at the DOM layer, and this change
  does not settle it from the Linq side.
- **`SaveOptions::OmitDuplicateNamespaces` is still inert.** A declaration is emitted only where
  one is needed, which is close to the flag's effect, but the flag itself is not consulted.
- **`XName`/`XNamespace` remain value-compared**, not interned.
- **No object grew.** The namespace scope is rebuilt from the tree on every serialisation entry
  point and cached nowhere; caching it would need a field on `XObject`.
