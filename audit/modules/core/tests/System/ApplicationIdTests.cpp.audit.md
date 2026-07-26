# Audit: `modules/core/tests/System/ApplicationIdTests.cpp`

## Metadata

- Audit status: AUDITED (69-line dedicated fixture, fully read).
- Validation: `ApplicationIdTests.*` passed 16/16 in the 22-test identity
  filter on 2026-07-26.

## Findings

The fixture checks normal printable values, equality, copy, and non-exact
ToString fragments.  It leaves SR-AUD-124 constructor/binary/null modeling and
SR-AUD-125 exact identity text entirely unobserved.

## Other missing assertions and diagnostics

- Missing empty name, byte values including NUL/high bytes, optional field
  absence versus empty, exact quoted/hex output, unequal-token output, escaping,
  hash contract, and copy isolation.

## Final assessment

Green happy-path values do not demonstrate current .NET identity behavior.  No
source or test was modified during this audit.
