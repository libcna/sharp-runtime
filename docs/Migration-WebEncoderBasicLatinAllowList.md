<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the default Web encoders escape outside Basic Latin (ticket #2019)

*2026-08-17.* `System::Text::Encodings::Web::HtmlEncoder::Encode` and
`JavaScriptEncoder::Encode` now escape every scalar outside U+0000..U+007F. They previously
passed non-ASCII text through unchanged.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept` change.

---

## 1. What changed

| Input | Was | Is |
|---|---|---|
| `HtmlEncoder::Encode(u8"é")` | `é` (unchanged) | `&#xE9;` |
| `HtmlEncoder::Encode(u8"€")` | `€` | `&#x20AC;` |
| `HtmlEncoder::Encode(u8"😀")` | `😀` | `&#x1F600;` |
| `JavaScriptEncoder::Encode(u8"é")` | `é` | `é` |
| `JavaScriptEncoder::Encode(u8"😀")` | `😀` | `😀` |
| ASCII text, and the five markup escapes | unchanged | unchanged |

`UrlEncoder` is unaffected: it already percent-encoded every byte outside the unreserved set,
which is the same policy expressed for its own syntax.

## 2. Why

.NET's default encoders are built with an **allow-list**, not a deny-list:

```csharp
DefaultHtmlEncoder.BasicLatinSingleton =
    new DefaultHtmlEncoder(new TextEncoderSettings(UnicodeRanges.BasicLatin));   // :13
DefaultJavaScriptEncoder.BasicLatinSingleton =
    new DefaultJavaScriptEncoder(new TextEncoderSettings(UnicodeRanges.BasicLatin));   // :11
```

Everything outside U+0000..U+007F is escaped unless the caller builds a relaxed encoder. The
target set was the half the pre-#2019 approval package recorded as **unverifiable**; the
reference tree settles it.

The escape forms are .NET's too: `&#x` + the **scalar** in uppercase hex with no padding + `;`
(`DefaultHtmlEncoder.cs:98-126`), and `\uXXXX` with four uppercase hex digits — a **surrogate
pair** for a supplementary scalar, because that is what a JavaScript string literal can express
(`DefaultJavaScriptEncoder.cs:129-150`).

## 3. This is a narrowing, and it is the safe direction

An allow-list escapes a character nobody thought about; a deny-list ships it. That is why .NET
chose one, and it is why this change makes output **longer and more conservative** rather than
shorter.

**To migrate:** nothing, if your output goes into HTML or JavaScript — the escaped form renders
identically and is safer. If you were using these encoders to pass text through unchanged, they
were the wrong tool and now say so.

A relaxed encoder — .NET's `HtmlEncoder.Create(UnicodeRanges.All)` — is **not** provided by this
port. If you need one, that is new API and needs its own ticket.

## 4. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched: neither `cna` nor `mobile-eggbert`
names `HtmlEncoder` or `JavaScriptEncoder` — **zero sites in both**. Neither repository was
modified.
