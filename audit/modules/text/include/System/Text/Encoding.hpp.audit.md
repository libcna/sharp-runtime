# Audit: `modules/text/include/System/Text/Encoding.hpp`

## Metadata

- Audit status: AUDITED.
- References: [.NET Encoding.GetCharCount](https://learn.microsoft.com/en-us/dotnet/api/system.text.encoding.getcharcount?view=net-10.0),
  [.NET Encoding.IsReadOnly](https://learn.microsoft.com/en-us/dotnet/api/system.text.encoding.isreadonly?view=net-10.0).

## Assessment

The base contract defines the unsafe signed raw decode shape, blindly accepts
null fallback objects, counts bytes in the UTF-8 representation as characters,
and returns mutable shared factory instances. The direct ASan, TSan, and
semantic probes establish these separately.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-287 — high — null fallback setters cause a later null dereference.
- SR-AUD-288 — high — shared factory encodings are mutable and racy.
- SR-AUD-290 — medium — `GetCharCount` reports UTF-8 bytes, not UTF-16 chars.
- SR-AUD-299 — medium — `EncodingInfo` cannot resolve its advertised code page.

## Other missing assertions and diagnostics

- Test `IsReadOnly`-equivalent semantics, factory isolation, fallback nulls,
  supplementary Unicode counts, and all invalid argument cases.

## Final assessment

SR-AUD-286 through SR-AUD-290 and SR-AUD-299 apply.
