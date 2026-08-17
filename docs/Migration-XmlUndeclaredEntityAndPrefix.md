<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — an undeclared entity or namespace prefix is rejected (tickets #2082, #2083)

*2026-08-17.* `XmlDocument::LoadXml` accepted two shapes of malformed XML that .NET rejects. It
rejects them now, and so does `XDocument::Parse`, which shares the loader.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change.

---

## 1. What changed

| Input | Was | Is |
|---|---|---|
| `<r>&nope;</r>` | accepted, **round-tripped as `<r>&amp;nope;</r>`** | `XmlException` |
| `<r a='&nope;'/>` | accepted | `XmlException` |
| `<p:r/>` | accepted, prefix never resolved | `XmlException` |
| `<r><p:child/></r>`, `<r p:a='1'/>` | accepted | `XmlException` |
| `<p:r xmlns:p='urn:x'/>` and any declared prefix | accepted | **unchanged** |
| `<r xml:lang='en'/>`, `<r xml:space='preserve'/>` | accepted | **unchanged** |
| the five predefined entities, `&#65;`, `&#x41;` | accepted | **unchanged** |
| a **declared** entity (`<!DOCTYPE r [<!ENTITY g "hi">]><r>&g;</r>`) | accepted, inert | **unchanged** |
| `&` inside a comment, CDATA section or processing instruction | accepted | **unchanged** |

## 2. Why

.NET throws for both:

* `XmlException("Reference to undeclared entity '{0}'.")` — `XmlTextReaderImpl.cs:3829`;
* `XmlException("'{0}' is an undeclared prefix.")` — `XmlTextReaderImpl.cs:7787`.

The entity case is the worse of the two, and not because of the acceptance: the reference was
reinterpreted as literal text **and re-escaped**, so a caller who loaded and saved a document
silently rewrote it. `&nope;` became `&amp;nope;`.

## 3. Two implementation notes worth knowing

**The entity check runs on the raw text, and it has to.** tinyxml2 *decodes* the five predefined
entities during parsing, so once the tree exists, `&amp;nope;` (legal — the text `&nope;`) and
`&nope;` (undeclared) are the same five characters in the same text node. A post-parse walk
cannot tell them apart.

**"Undeclared" is not "not predefined".** The first cut of the check rejected anything outside
the predefined five, and the repository's own billion-laughs pin caught it immediately — that
document *declares* two entities and then references one. The check now reads the DOCTYPE
internal subset for `<!ENTITY name …>` declarations. A declared entity is accepted and, as
before, **not expanded**; that parity gap is pre-existing and is what keeps this port free of the
billion-laughs exposure.

## 4. Two limits, recorded rather than hidden

* **`XmlDocument::Load(filename)` does not run the entity check.** It hands the path straight to
  tinyxml2 and never holds the raw bytes. The *prefix* check runs at both doors, because it works
  on the parsed tree. That asymmetry is ticket **#2361**.
* **A DTD-declared entity is still not expanded.** Unchanged, pre-existing, and out of scope
  here.

## 5. `System.Xml.Linq` follows, and its pins said it should

`XElement::Parse` and `XDocument::Parse` share the loader, so an undeclared prefix now throws
there too. The Linq pins that recorded the old behaviour said exactly why they existed: *"#2083
already owns [this] at the DOM layer; answering it from the Linq side would settle it by
accident. Pinned so that a later change to it is a decision rather than a side effect."* The
decision was made at the DOM layer, and this change updates them.

## 6. To migrate

A document with an undeclared entity or prefix is not well-formed XML and .NET never accepted
it. If you were loading one, you were also silently rewriting it on save.

If you need the prefix, declare it: `<p:r xmlns:p="urn:x"/>`.

## 7. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `XmlDocument` or `System::Xml` — **zero sites in
both**. Neither repository was modified.
