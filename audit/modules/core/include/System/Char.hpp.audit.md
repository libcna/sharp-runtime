# Audit: `modules/core/include/System/Char.hpp`

## Metadata

- Audit status: AUDITED (710 lines, header-only implementation, full read).
- Runtime evidence: `./build/SharpRuntimeTests_Core_Base --gtest_filter='CharTests.*:CharTests2.*' --gtest_color=no` passed 120 tests on 2026-07-25.
- Direct UTF-8 probe: `Char::Parse("\\xC0\\x80")` returned code unit `0`
  rather than throwing, as recorded below.

## Assessment

The header has broad ASCII/BMP helpers, UTF-8 conversion, categories, and
string-indexed overloads.  It unusually documents an important permanent/partial
adaptation: string-indexed APIs inspect `std::string` bytes, not decoded UTF-16
code units, and therefore cannot faithfully process non-ASCII input or surrogate
pairs.  The existing `CharTests2` suite makes this limitation explicit for a
supplementary UTF-8 sequence; it should remain documented as an adaptation,
not silently treated as full `System.Char` parity.

## SR-AUD-017 — medium — `Char::Parse` accepts invalid overlong UTF-8 as a valid BMP character

`Parse` derives its byte count solely from the first byte and verifies only
continuation-bit shape.  It does not reject invalid leading bytes, shortest-form
violations, or decoded scalars below the minimum for a multibyte sequence.  The
two-byte overlong encoding `C0 80` is therefore decoded as U+0000.  The direct
probe printed:

```
overlong_parse=0
```

although the API documentation in this same header promises
`FormatException` for invalid UTF-8.  The 120 passing focused tests cover valid
two/three-byte sequences and cardinality only; none supplies malformed or
overlong input.

### Required post-audit verification

Validate RFC 3629 lead-byte ranges, minimum scalar values, continuation bytes,
surrogate UTF-8 encodings, and upper scalar limits before returning a BMP code
unit.  Add rejected tests for `C0 80`, a lone continuation byte, truncated
two/three-byte sequences, `E0 80 80`, and a UTF-8-encoded surrogate, plus
valid boundary vectors.  `TryParse` should return false and clear the output
for the same invalid input.

## Other missing assertions and diagnostics

- The string-indexed byte/UTF-16 adaptation is documented but tests only an
  ASCII result and one supplementary case.  It needs a clearly maintained
  compatibility matrix for multibyte letters, numeric/category queries, and
  index semantics so future callers do not mistake byte offsets for UTF-16
  indexes.
- `ToUpper`/`ToLower` depend on the process locale and invariant case conversion
  is ASCII-only.  Existing tests do not establish behavior for non-ASCII BMP
  letters or locale-sensitive cases.
- `Parse`/`ToString` do not test UTF-8 encoded lone surrogates, invalid lead
  bytes, truncated sequences, or supplementary input in `TryParse`.
- The checked source probe ran UBSan over `GetHashCode(0x8000)` without a
  diagnostic on this toolchain.  Retain an unsigned bit-pattern implementation
  if this code is changed, but do not claim a sanitizer-confirmed issue from
  that particular expression without a reproducer on a supported compiler.

## Final assessment

Well-documented partial string-indexed behavior and broad ASCII tests, but the
public UTF-8 parser violates its own invalid-input contract.  Fix its decoder
and add malformed-sequence diagnostics before expanding surface area.

---

## Remediation record — ticket #2225 (SR-AUD-017), 2026-08-10

**`remediated`.** Original evidence retained unchanged.

`Parse` now decodes through `System::detail::DecodeUtf8`
(`modules/core/include/System/detail/Utf8Text.hpp`), the module's single validating UTF-8 decoder,
which rejects stray continuations and `0xC0`/`0xC1` leads, truncation, malformed continuations,
**overlong** forms (checked against the minimum scalar for the length actually used), UTF-8-encoded
**surrogates**, and scalars beyond `U+10FFFF`.

The old body's `b0 < 0xE0` two-byte test was the root cause: it admitted `0x80..0xBF` and
`0xC0`/`0xC1` as two-byte leads, after which only continuation-bit *shape* was checked. Measured
before the repair: `C0 80` → U+0000, `C1 BF` → U+007F, `E0 80 80` → U+0000, `ED A0 80` → U+D800,
`F8 80 80 80` → U+0000. Two cases the finding lists as defects — a lone `0x80` and `0xFF` — were
already rejected, but **by the cardinality check rather than by validation**; that is recorded here
so the finding's extent is neither overstated nor understated.

Validity and length are checked **separately**, so a well-formed sequence followed by more text
still reports the long-standing *"String must be exactly one character long."* message and a
well-formed non-BMP scalar still reports its own *"outside BMP"* diagnostic.

**Evidence.** 5 wrong → 0 in the SR-AUD-017 group, with all nine valid vectors (U+0000, U+0080,
U+00E9, U+07FF, U+0800, U+20AC, U+FFFF, ASCII, and the outside-BMP diagnostic) unchanged.
`TryParse` mirrors `Parse` and clears its output. **+5 permanent regressions.**

No signature, `noexcept`, layout, vtable or ABI change.
