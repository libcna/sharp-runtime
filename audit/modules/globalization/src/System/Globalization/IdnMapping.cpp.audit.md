# Audit: `modules/globalization/src/System/Globalization/IdnMapping.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: malformed UTF-8, Punycode vectors, STD3, and output-length tests
  pass in the module target; the standalone U+0378 probe is reproducible.

## Assessment

The UTF-8 decoder rejects malformed sequences and the Punycode paths check
round trips and label limits.  However, mapping never observes the public
AllowUnassigned setting; all code-point validation omits Unicode assignment
data.

## Finding references

- SR-AUD-282 — medium — `AllowUnassigned` has no effect on GetAscii/GetUnicode.

## Other missing assertions and diagnostics

- Test Unicode assignment-version behavior, separator variants, bidi/context
  rules, invalid byte-range slicing, and settings interaction with STD3.

## Final assessment

SR-AUD-282 applies through this implementation.
