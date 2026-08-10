# Audit: `modules/buffers/tests/System/Buffers/Utf8FormatterTests.cpp`

## Metadata

- Audit status: AUDITED (229 lines, 25 tests, fully read).
- Validation: `build/SharpRuntimeTests_Buffers --gtest_filter='Utf8FormatterTest.*'`
  passed 25/25 on 2026-07-26.
- Companion implementation report:
  `modules/buffers/include/System/Buffers/Text/Utf8Formatter.hpp.audit.md`.

## Assessment

The fixture provides valuable regression coverage for the prior precision-99
stack-buffer overflows: all four D/signed-D/X/N high-precision cases now
format into sufficiently large destinations.  It also verifies representative
bool, decimal, hex, grouping, error, and ordinary short-buffer behavior.  It
is still a narrow smoke suite and cannot establish parity across every exposed
integer overload or status-output combination.

## Other missing assertions and diagnostics

- No test instantiates `int8_t`, `uint16_t`, or `int16_t`, and no signed-minimum
  value validates the carefully written non-overflow magnitude conversion.
- Every false short-buffer path starts `written` at zero and uses a zeroed
  buffer.  It should prefill output and use exact-one-byte-short destinations
  for ordinary and precision-99 D/X/N output to prove reset and nonmutation.
- Cases omit lowercase format symbols, G/R aliases, bool false/lowercase false,
  explicit NoPrecision, zero symbol, and precision attached to a bool or R.
- There is no high-precision negative N case, zero with every specifier,
  grouping boundary series (999/1,000/1,000,000), or unsigned/signed extrema.
- The test names document a historical ticket but no sanitizer execution or
  generated .NET vector comparison is part of the current fixture; future
  repair validation must retain the wide-output boundary coverage.

## Final assessment

All 25 direct tests pass.  They protect the known stack-buffer repair but leave
significant overload, extrema, and failure-diagnostic coverage gaps.  No test
source was modified during this audit.
