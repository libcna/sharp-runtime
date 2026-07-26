# Audit: `modules/text/src/System/Text/UTF8Encoding.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

Malformed UTF-8 replacement behavior is implemented through fallbacks. But
negative raw count returns empty and a negative index is converted to `size_t`;
setting decoder fallback to null then decoding malformed data produces the
confirmed ASan/UBSan null dereference.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-287 — high — null fallback settings are dereferenced later.

## Other missing assertions and diagnostics

- Test negative index/count, null/empty distinctions, null fallback setters,
  fallback object lifetime, and exception indexes after nonzero offsets.

## Final assessment

SR-AUD-286 and SR-AUD-287 apply.
