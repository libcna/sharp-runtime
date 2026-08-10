# Audit: `modules/text/src/System/Text/StringBuilder.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

`CopyTo` calculates `destinationLength - count` before validating a negative
capacity. The UBSan probe with `INT_MIN` capacity and count 1 reports signed
overflow at line 146. All indexing, length, insertion, removal, and copying
operate on UTF-8 bytes, so removing byte 1 from `éA` yields invalid `c341`.

## Finding references

- SR-AUD-295 — high — signed capacity arithmetic overflows before validation.
- SR-AUD-296 — medium — byte-based StringBuilder positions split UTF-8 text.

## Other missing assertions and diagnostics

- Test negative capacity, extreme signed values under UBSan, multibyte
  length/index/CopyTo/Insert/Remove, and managed exception types.

## Final assessment

SR-AUD-295 and SR-AUD-296 apply.
