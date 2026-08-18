<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `StringContent` takes an `Encoding`, not a charset string (ticket #2070)

*2026-08-18.* `System::Net::Http::StringContent`'s second parameter changed from a charset
**string** to a `std::shared_ptr<System::Text::Encoding>`, and the body is now **serialised
through** that encoding.

**This is a public source break.** Landed under `docs/StandingApprovals.md` SA-2, with all five
conditions discharged — §7. No layout, vtable or `noexcept` concern; the type is header-only.

---

## 1. What was wrong

The charset was a **label only**. The bytes emitted were always the string's UTF-8 storage bytes,
so the declared charset and the payload could contradict each other:

```cpp
StringContent body("\xc3\xa9", "utf-16", "text/plain");
// Content-Type: text/plain; charset=utf-16
// body bytes:   c3 a9
```

`c3 a9` is `é` in UTF-8. A conforming server reads two octets under a `utf-16` label as **one**
UTF-16 code unit and gets `U+A9C3` — a different character, silently, with no diagnostic anywhere.

## 2. What .NET does, and why it is a different shape

.NET does not validate against the contradiction. It makes the contradiction **unrepresentable**:

```csharp
public StringContent(string content, Encoding? encoding, string? mediaType)
    : base(GetContentByteArray(content, encoding))          // serialise through it
{
    encoding ??= DefaultStringEncoding;                      // null means UTF-8
    …
    Headers.ContentType = new MediaTypeHeaderValue(mediaType, encoding.WebName);   // label from it
}
```
*(`StringContent.cs:48-73`; `GetContentByteArray` is `:90-98`.)*

One `Encoding` object is both the serialiser and the label, so there is no second source of truth
for them to disagree about. That is why the repair is a **signature** change and not a validation
check: a check would still let a caller name a charset the body was not encoded in, and would
merely refuse the ones the port happened to recognise.

## 3. What changed

| Call | Was | Is |
|---|---|---|
| `StringContent("é")` | `c3 a9`, `charset=utf-8` | **unchanged** |
| `StringContent("é", "utf-8", …)` | compiled | **does not compile** — pass `Encoding::UTF8()` |
| `StringContent("é", "utf-16")` | `c3 a9` under a `utf-16` label | does not compile |
| `StringContent("é", Encoding::Unicode())` | — | `e9 00`, `charset=utf-16` |
| `StringContent("é", nullptr, …)` | — | UTF-8, matching `encoding ??= DefaultStringEncoding` |
| `getCharSetProperty()` | whatever the caller said | the encoding's own `WebName`, always |

`ReadAsString()` returns the **encoded** bytes as a `std::string`. Under a non-UTF-8 encoding
those are not UTF-8 storage bytes and must not be treated as text; `ReadAsByteArray()` is the
honest accessor for a non-UTF-8 body.

## 4. To migrate

```cpp
StringContent body(text, "utf-16", "text/plain");                          // before
StringContent body(text, System::Text::Encoding::Unicode(), "text/plain"); // after

StringContent json(text, "utf-8", "application/json");                          // before
StringContent json(text, System::Text::Encoding::UTF8(), "application/json");   // after

StringContent plain(text);   // unchanged — still UTF-8, still text/plain
```

The web names are .NET's and were verified by probe: `utf-8`, `utf-16`, `us-ascii`,
`iso-8859-1`, `utf-32`.

## 5. One check became unnecessary and is kept anyway

#2063 rejects a CR/LF/NUL in the charset, because it is concatenated into a `Content-Type` field.
The charset can no longer carry one, since it now comes from an `Encoding`'s own web name — the
state is unrepresentable rather than rejected, which is the stronger of the two. The check is kept
because it costs nothing and a future encoding is not obliged to have a well-formed name.

## 6. A new component edge

`Net.Http` now depends on `Text`. The module graph goes **41 modules / 92 edges → 41 / 93**, and
`docs/ComponentCatalog.md` is regenerated. There is no alternative that keeps the edge count: the
whole point is to encode through `System::Text::Encoding`, and a private copy of even one encoder
would be the duplication #2354 has just finished removing six of.

## 7. SA-2's five conditions

1. **Migration note** — this document.
2. **Negative consumer fixture** — `test/consumer/net_http_stringcontent_encoding_negative.cpp`,
   three sites, including the two-argument `StringContent(body, "utf-8")` most likely to survive a
   careless migration. The fixture set grows to **36 fixtures / 197 sites**.
3. **Downstream ticket** — #2379.
4. **Full gate** — 17,320 run, 17,320 passed, 0 failed.
5. **Measured impact** — neither `cna` nor `mobile-eggbert` references `StringContent` at all:
   **zero sites in both**. Neither repository was modified.

## 8. Evidence

| Mutation | Caught |
|---|---|
| Store the raw string instead of encoding it (the pre-#2070 behaviour) | ✅ (2 tests) |
| Label the header `utf-8` regardless of the encoding | ✅ (2 tests) |
| A null encoding means UTF-16 rather than UTF-8 | ✅ (3 tests) |
