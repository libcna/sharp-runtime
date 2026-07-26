# Audit: `tests/integration/System/Text/TextRemainingTests.cpp`

## Metadata

- Audit status: AUDITED (581 lines, 82 tests in 12 suites, full read).
- Runtime evidence: the focused normalization, composite-format, rune,
  encoding, encoder/decoder, regex, match, and collection filter passed all 82
  cases on 2026-07-25.

## Coverage observed

The aggregate suite has strong targeted regression evidence for UTF-8 rune
decoding (including overlong and invalid continuation rejection), ASCII’s
UTF-16-code-unit fallback width, composite-format syntax failures, regex match
index preservation, `NextMatch` anchor behavior, named group names, and bounds
checking.  It also establishes basic public metadata and happy paths for the
remaining encodings and wrapper types.

## Missing assertions and diagnostics

- `UTF32EncodingTests.GetBytes_WithBOM_SizeIs8ForSingleChar` asserts only the
  byte count.  It does not verify BOM bytes, endianness, decoding, a
  supplementary scalar, malformed UTF-8 input, or invalid scalar handling.
- UTF-7 coverage checks one non-ASCII encoding and a fallback byte but not
  shifted-sequence state across chunks, optional-direct-character mode,
  malformed shift sequences, or exact exception/fallback diagnostics.
- `Encoder`/`Decoder` cover ASCII only; they do not prove incremental state
  across split multibyte UTF-8 input, reset semantics after pending state, or
  invalid-byte behavior.  A `Reset_NoThrow` assertion alone cannot establish a
  reset contract.
- Regex coverage is useful but lacks options, timeout/culture adaptations,
  zero-length `NextMatch`, captures, replacement escaping, split edge cases,
  and error offset diagnostics.  The `RegexParseException` default offset of
  zero is an explicit adaptation, not a parsed position.
- `Rune` category/casing checks are ASCII-only; Unicode category and culture
  behavior must not be inferred from them.

## Required post-audit verification

Add exact byte-vector and round-trip tests for UTF-32 BOM/endian modes, split
multibyte state tests for encoder/decoder and UTF-7, and bounded regex edge
cases.  Keep the existing malformed UTF-8 rune tests: they are clear regression
coverage rather than redundant happy paths.

## Final assessment

This is a well-targeted integration file with several high-value regressions.
Its remaining gaps are broad-format/state coverage, not a newly demonstrated
source defect.
