# Audit: `modules/buffers/tests/System/Buffers/Utf8ParserTests.cpp`

## Metadata

- Audit status: AUDITED (180 lines, 25 tests, fully read).
- Validation: `build/SharpRuntimeTests_Buffers --gtest_filter='Utf8ParserTest.*'`
  passed 25/25 on 2026-07-26.
- Companion implementation report:
  `modules/buffers/include/System/Buffers/Text/Utf8Parser.hpp.audit.md`.

## Assessment

The fixture usefully retains regression coverage for the previous multi-wrap
unsigned D/N overflow bugs, normal prefix parsing, a hex prefix stop, grouped
integer text, and nonzero fractional rejection.  It lacks the signed extrema,
failure-output, and leading-sign cases that expose all three current findings.

## Finding references

- **SR-AUD-084:** no test parses decimal or grouped `INT64_MIN`, so both
  signed-minimum negation paths can execute UB under a green filter.
- **SR-AUD-085:** invalid integer test initializes `v = 99` but checks only
  `bytesConsumed == 0`; no false case asserts default output for bool or any
  integer overload.
- **SR-AUD-086:** no default/D case uses an allowed leading `+`; N's positive
  sign grammar is not tested either, so the inconsistent parser branches are
  invisible.

## Other missing assertions and diagnostics

- Missing type coverage includes int8/int16/uint16, signed/unsigned extrema,
  leading zeros/signs, empty source, trailing suffixes, and malformed byte
  sequences for all supported formats.
- N tests omit comma/fraction edge grammar, `+`, minimum, overflow, and exact
  cursor behavior on trailing nonnumeric text versus nonzero fractional data.
- Hex tests omit empty/invalid-first input, width overflow, lower/upper mixed
  cases, signed width bit patterns beyond int8, and pre-populated result state.
- No test checks G/g/R aliases, lowercase D/N, exceptions with prepopulated
  outputs, or formatter-parser differential round trips.

## Final assessment

All 25 tests pass, but their normal value focus misses the three reproduced
parser contract failures.  No test source was modified during this audit.
