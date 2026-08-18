<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `XmlDocument::Load(filename)` rejects an undeclared entity too (ticket #2361)

*2026-08-18.* `XmlDocument::Load` now runs the same undeclared-entity check that
`XmlDocument::LoadXml` has run since #2082. A document loaded **from a file** used to accept
`&nope;` and then silently rewrite it as `&amp;nope;` on save.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Load(path)` on a file containing `&nope;` | accepted; saved back as `&amp;nope;` | `XmlException` |
| `LoadXml("<r>&nope;</r>")` | `XmlException` | `XmlException` (unchanged, since #2082) |
| `Load(path)` on any legal document | — | **unchanged** |
| `Load(path)` on a missing file | `XmlException` | `XmlException`, same text |
| `Load(path)` on malformed content | `XmlException` | `XmlException`, same text |
| `Load(path)` on an undeclared **prefix** | `XmlException` | unchanged — #2083 always ran here |

**The rewrite is the half that lost data.** Accepting the reference was recoverable; reinterpreting
it as literal text *and re-escaping it* meant loading and saving a document changed the document.

## 2. Why the check could not run here before

`ThrowIfUndeclaredEntityReference` scans the **raw** text, because tinyxml2 decodes the five
predefined entities during parsing — after which `&amp;nope;` and `&nope;` are indistinguishable.
`LoadXml(std::string)` has the raw text. `Load(filename)` handed the path straight to
`tinyxml2::XMLDocument::LoadFile` and never held the bytes.

`Load` now reads the file itself and parses the buffer, which is what `LoadFile` does internally
anyway (read whole file, call `Parse`).

## 3. The failure path deliberately still goes through `LoadFile`

`LoadFile` is what produces `XML_ERROR_FILE_NOT_FOUND`, `XML_ERROR_FILE_COULD_NOT_BE_OPENED` and
`XML_ERROR_FILE_READ_ERROR`, and the `ErrorStr()` this exception message has always carried.
Reproducing those categories from an `ifstream` failure would be **inventing diagnostics** rather
than keeping them, so when the read fails the code lets tinyxml2 categorise its own failure and
throws the unchanged message.

The file is read in **binary** mode: a text-mode read on Windows would collapse CRLF and move the
offsets the scanner walks.

## 4. Two mutations that are not mutations

Recorded rather than counted as passes.

**Text mode instead of binary** is a no-op *on this platform*. On Linux the two modes are
identical, so the mutation is unobservable here by construction; the flag is a correctness measure
for a platform the gate does not run on.

**`Parse(c_str())` without the length** is semantically equivalent, and that was verified by probe
rather than assumed. Three NUL placements were measured through both doors:

| Document | `LoadXml` | `Load` |
|---|---|---|
| `<r>a\0b</r>` — NUL mid-document | throws | throws |
| `<r>a</r>\0junk` — NUL after a complete document | accepted, `<r>a</r>` | accepted, `<r>a</r>` |
| `<r>a</r>\0<x>&nope;</x>` | throws | throws |

The two doors agree in every shape, with and without the explicit length, because tinyxml2 stops
at the NUL either way. The explicit length is kept because it is the clearer spelling, not because
a test defends it. (The probe binaries were deleted afterwards, per the build-resource policy.)

| Real mutation | Caught |
|---|---|
| Drop the entity check from `Load` (the pre-#2361 code) | ✅ |
| Treat an unreadable file as empty content | ✅ (3 tests) |
| `ReadWholeFile` keeps only the first line | ✅ |

## 5. To migrate

A file containing an undeclared entity reference was never valid XML — .NET rejects it with
`XmlException("Reference to undeclared entity '{0}'.")` (`XmlTextReaderImpl.cs:3829`). Declare the
entity in a `<!DOCTYPE>` internal subset, or escape the ampersand as `&amp;`.

## 6. Downstream, measured

Per SA-2 condition 5: neither `cna` nor `mobile-eggbert` references `XmlDocument` outside their
audit notes — **zero live sites in both**. Neither repository was modified.
