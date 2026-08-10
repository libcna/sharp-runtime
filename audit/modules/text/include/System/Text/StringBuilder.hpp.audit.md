# Audit: `modules/text/include/System/Text/StringBuilder.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The class advertises .NET character length/indexes but stores raw UTF-8 bytes.
It therefore exposes byte positions and permits byte-splitting mutations. Its
raw `CopyTo` capacity argument is signed and reaches unchecked arithmetic in
the implementation.

## Finding references

- SR-AUD-295 — high — CopyTo capacity arithmetic can signed-overflow before
  validation.
- SR-AUD-296 — medium — StringBuilder character positions are UTF-8 bytes and
  can produce invalid text.

## Other missing assertions and diagnostics

- Test non-ASCII and supplementary length/index/insert/remove/copy behavior,
  invalid capacity, null destination, overflow, mutation during enumeration,
  and capacity narrowing.

## Final assessment

SR-AUD-295 and SR-AUD-296 apply.
