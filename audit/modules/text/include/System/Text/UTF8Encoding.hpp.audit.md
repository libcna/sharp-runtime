# Audit: `modules/text/include/System/Text/UTF8Encoding.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The declaration accurately documents the working UTF-8 fallback conversion,
but it inherits unsafe public fallback setters and participates in the mutable
factory singleton contract.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-287 — high — null fallback setters cause a later null dereference.
- SR-AUD-288 — high — shared factory encodings are mutable and racy.

## Other missing assertions and diagnostics

- Test raw negative ranges, null fallback setters, static factory read-only
  behavior, cross-thread fallback mutation, and fallback text with non-ASCII.

## Final assessment

SR-AUD-286 through SR-AUD-288 apply.
