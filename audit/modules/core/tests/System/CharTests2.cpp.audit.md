# Audit: `modules/core/tests/System/CharTests2.cpp`

## Metadata

- AUDITED: 362-line direct Char continuation fixture, fully read.
- Validation: `CharTests2.*` passed 63/63 on 2026-07-27.
- Reference basis: local current-.NET Char/CharUnicodeInfo behavior and the
  mirrored `CharUnicodeInfo.hpp` source/probes.

## Assessment

The fixture gives valuable nominal coverage for ASCII casing/comparison/hash,
control and separator distinctions, parsing, UTF-8 conversion, string-index
bounds, and the documented native byte-string surrogate limitation.  Its
ThreadSanitizer-style precision is not relevant here, but its comments clearly
distinguish intentional UTF-8 byte indexing from a managed UTF-16 string.

Every direct numeric/category assertion is ASCII.  The green 63/63 result
therefore cannot detect the public Unicode table gaps confirmed as SR-AUD-173
and SR-AUD-174.

## Missing assertions and diagnostics

- Add Arabic-Indic decimal, superscript digit, Roman numeral, vulgar fraction,
  and a no-value character cases for decimal/digit/numeric distinctions.
- Assert every relevant UnicodeCategory family: combining mark, dash,
  private-use, surrogate, line/paragraph separator, symbol, and unassigned.
- Exercise `CharUnicodeInfo` UTF-16 string overloads directly with valid
  supplementary pairs, lone surrogates, invalid indices, and locale changes;
  Char's UTF-8 `std::string` adaptation cannot stand in for those methods.
- The tests deliberately lock raw-byte behavior for UTF-8 string overloads but
  do not show callers how that differs from the separate UTF-16
  `CharUnicodeInfo` public overloads.

## Final assessment

The fixture is useful for implemented ASCII/UTF-8 adaptation paths but leaves
both confirmed Unicode information defects unasserted.  No source or test was
modified.
