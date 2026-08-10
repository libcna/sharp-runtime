# Audit: `modules/buffers/tests/System/Buffers/Base64Tests.cpp`

## Metadata

- Audit status: AUDITED (360 lines, 40 tests, fully read).
- Validation: `build/SharpRuntimeTests_Buffers --gtest_filter='Base64Test.*'`
  passed 40/40 on 2026-07-26.
- Companion implementation report:
  `modules/buffers/include/System/Buffers/Text/Base64.hpp.audit.md`.

## Assessment

The suite gives useful ordinary coverage of bytes/chars, whitespace, final and
non-final incomplete groups, short output, two-argument throwing wrappers,
allocation conveniences, in-place success, and both documented length bounds.
It is nevertheless a happy-path regression suite: all in-place successes use
input lengths two or three, and the one non-final decode path has no padding.
Consequently all four confirmed Base64 findings remain green under the full
direct filter.

## Finding references

- **SR-AUD-078:** `EncodeToUtf8InPlace_RoundTrip` covers three bytes and
  `TryEncodeToUtf8InPlace_Success` two bytes.  Neither has both a full triple
  and a trailing remainder, so the unread remainder overwrite is unobserved.
- **SR-AUD-079:** invalid-input coverage uses an illegal `!` and trailing data
  after padding, but never nonzero unused bits in an otherwise grammatical
  final quantum (`AB==` / `AAB=`).  It also never relates `IsValid` to decode
  acceptance for those forms.
- **SR-AUD-080:** the sole `isFinalBlock == false` decode assertion is `QQQ`;
  a padded `QQ==` is not tested even though current .NET must reject it for a
  non-final streaming call.
- **SR-AUD-081:** embedded whitespace success does not assert `consumed`, and
  no padded source with trailing whitespace checks that cursor semantics leave
  the trailing whitespace unconsumed.

## Other missing assertions and diagnostics

- `DestinationTooSmall` does not assert the reset/partial `consumed` and
  `written` outputs.  It needs first-, middle-, and final-quantum destinations
  with sentinels verifying that the unreported region was not changed.
- Throwing and `Try*` convenience overloads lack tests for a short destination
  after prior successful output and for exact error types/messages on invalid
  data.
- There are no all-whitespace, leading/trailing whitespace, padding split by
  whitespace, double padding, invalid early padding, high-bit byte, or
  narrow-`char` signedness cases.
- Length helper tests omit zero and all rounding residues around multiples of
  three/four.  The maximum test intentionally avoids allocation but does not
  cross-check `GetEncodedLength` and both decoded helpers at every boundary.
- There is no generated/vector corpus or differential case set against the
  local .NET Base64 test vectors, so RFC grammar regressions can remain hidden
  behind a passing 40-case suite.

## Final assessment

All direct tests pass, but their narrow vector selection does not protect the
four demonstrated Base64 contract failures.  No test source was modified during
this audit.
