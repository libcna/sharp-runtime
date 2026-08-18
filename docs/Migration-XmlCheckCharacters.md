<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the writer doors reject characters outside XML's `Char` production (ticket #2349)

*2026-08-18.* `XmlWriter::WriteString("a\x01b")` and `XText("a\x01b").ToString()` now raise
`ArgumentException`. They used to emit the byte raw, producing a document that is not well-formed
XML.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Value | Was | Is |
|---|---|---|
| `0x01`, `0x08`, `0x0B`, `0x0C`, `0x0E`–`0x1F` in content | emitted raw | `ArgumentException` |
| `U+FFFE`, `U+FFFF` | emitted raw | `ArgumentException` |
| a lone surrogate in UTF-8 (`ED A0 80`) | emitted raw | `ArgumentException` |
| `\t`, `\n`, `\r` | emitted | **unchanged** — they *are* `Char` |
| any other text, including multi-byte | — | **unchanged, byte for byte** |
| `NUL` | `XmlException` (#2085) | **unchanged** — §4 |

Measured before: **28 of the 29 non-`Char` bytes in `0x00`–`0x1F`** went through both door
families.

## 2. The ticket recorded five priced options and called it a user decision

`docs/SystemXmlNamespaceReviewPlan.md` SS22 carries the table: *A reject unconditionally, B honour
the flag, C keep today's permissiveness, D writer only, E widen the reader too.* The blocker was
that `/rv` was absent.

**The reference collapses the table to one option.**
`XmlWriterSettings.CheckCharacters` defaults to `true` (`XmlWriterSettings.cs:513`) and is
enforced:

```csharp
if (_checkCharacters) throw XmlConvert.CreateInvalidCharException((char)ch, '\0');
else { … entitize or write raw … }               // XmlEncodedRawTextWriter.cs:1635-1653
```

.NET rejects by default; the flag is what turns that off. That is option B, derived rather than
chosen.

## 3. Both of the ticket's pricing complications are dissolved, not accepted

**(1) "Enforcing with the validator we already ship buys C0 controls only."** True of
`XmlConvert::VerifyXmlChars`, which iterates `char` and so checks **bytes** — it accepts `U+FFFE`,
`U+FFFF` and a lone surrogate. Ticket **#2354**, earlier the same day, moved a **code-point**
decoder into `Core.Base`, and both `modules/xml` and `modules/xml-linq` already depend on it. A
code-point-correct check now costs one call and **no new component edge**; the graph stays at
41/93. The ticket priced this when that decoder was not there.

**(2) "Option B is not a same-shaped change on both sides, because the Xml.Linq direct doors
cannot see the flag."** **.NET's cannot either.** `XNode.GetXmlWriterSettings` constructs a
*default* `XmlWriterSettings` and touches only `Indent` and `NamespaceHandling`
(`XNode.cs:681-687`), so it inherits `CheckCharacters = true` and checks exactly as any
default-settings writer does. The Linq side needs **no** new settings channel, no new
`SaveOptions` value and no ambient default: it checks unconditionally, and the two door families
agree **by construction** rather than by coordination.

## 4. Two exception types, deliberately

`NUL` keeps `XmlException`, because that is this port's own truncation guard (#2085) — a length
boundary at the tinyxml2 `const char*` API, not a transcription of a .NET check. Every other
non-`Char` code point raises `ArgumentException`, because that is what
`XmlConvert.CreateInvalidCharException` produces (`XmlConvert.cs:1614-1622`), with .NET's text:
*"'…', hexadecimal value 0x01, is an invalid character."*

## 5. What is still open, and it is stated rather than left implicit

`XmlReaderSettings::CheckCharacters` also defaults to `true` and is also unenforced. This port's
reader **accepts** these characters, so writing and reading now disagree — options A, B and D all
leave that asymmetry and only C and E avoid it. .NET has no such asymmetry, because its reader
enforces too.

The reader here is tinyxml2, which this port does not drive character by character, so closing it
is not a matter of adding a call. It is recorded here rather than silently tolerated.

## 6. To migrate

A document containing these characters was never valid XML — the `Char` production excludes them
and no character reference can carry them either, since a reference must itself match `Char`.
Strip or replace them before writing.

## 7. Evidence

| Mutation | Caught |
|---|---|
| The check is byte-wise again (the shipped `VerifyXmlChars` shape) | ✅ (both door families) |
| `U+FFFE` and `U+FFFF` become `Char` | ✅ (both) |
| Tab, LF and CR stop being `Char` | ✅ (4 tests, including pre-existing ones) |
| The C0 controls become `Char` (the pre-#2349 behaviour) | ✅ (both) |

## 8. Downstream

Neither `cna` nor `mobile-eggbert` references `XmlWriter`, `XDocument` or `XElement` — zero sites
in both.
