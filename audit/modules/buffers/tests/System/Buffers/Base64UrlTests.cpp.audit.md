# Audit: `modules/buffers/tests/System/Buffers/Base64UrlTests.cpp`

## Metadata

- Audit status: AUDITED (293 lines, 31 tests, fully read).
- Validation: `build/SharpRuntimeTests_Buffers --gtest_filter='Base64UrlTest.*'`
  passed 31/31 on 2026-07-26.
- Companion implementation report:
  `modules/buffers/include/System/Buffers/Text/Base64Url.hpp.audit.md`.

## Assessment

The suite covers ordinary unpadded URL output, a URL-alphabet smoke check,
length values, straightforward invalid/odd input, wrappers, and one streaming
remainder.  Comments cite an earlier audit conclusion that the core was
verified correct, but the test vectors leave both the duplicated in-place
overwrite and key current-.NET grammar paths absent.  Every direct test still
passes despite the confirmed implementation divergences.

## Finding references

- **SR-AUD-078 (extended):** `TryEncodeToUtf8InPlace_RoundTrip` uses exactly
  one triple.  It does not exercise a triple plus a one/two-byte remainder,
  the combination that overwrites unread input.
- **SR-AUD-079 (extended):** no final two-/three-symbol case sets nonzero
  unused low bits or compares the `Decode*` and `IsValid` results to canonical
  Base64Url requirements.
- **SR-AUD-082:** no test accepts `AQ==` or `AQ%%` on a final call, even though
  current .NET supports these optional padding forms.  The suite only tests
  an illegal `!`, an odd length, and no-padding normal output.

## Other missing assertions and diagnostics

- Tests do not cover padding placement/count errors, padding interleaved with
  whitespace, `%` versus `=`, standard Base64 `+`/`/` rejection, high-bit
  bytes, or narrow-`char` signedness.
- None of the short-destination paths assert `consumed`, `written`, sentinels,
  or the distinction between a complete group and final two-/three-symbol
  remainder.
- The streaming test has a preceding full group but omits empty source,
  two-symbol remainder, invalid one-symbol remainder on final call, and the
  documented padded input behavior of a non-final call.
- Length helpers omit zero and all residue values around the maximum; their
  results are not cross-checked against actual encoder output lengths.
- The full local .NET Base64Url validation corpus is not represented, and no
  test asserts error details from the generic Base64-oriented exception text.

## Final assessment

All 31 tests pass, but their normal-path emphasis fails to protect the three
confirmed Base64Url failures.  No test source was modified during this audit.
