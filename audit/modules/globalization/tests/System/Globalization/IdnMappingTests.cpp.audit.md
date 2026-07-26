# Audit: `modules/globalization/tests/System/Globalization/IdnMappingTests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The 40 tests use useful RFC/Punycode, UTF-8 rejection, STD3, and limit vectors.
`AllowUnassigned` is tested only as a stored property, not as an input-policy
control on an unassigned Unicode character.

## Finding references

- SR-AUD-282 — medium — setting has no observable mapping effect.

## Other missing assertions and diagnostics

- Test an unassigned code point under both setting values, IDNA Unicode-version
  behavior, bidi/context rules, and malformed UTF-8 in index/count overloads.

## Final assessment

The test gap leaves SR-AUD-282 unobserved.
