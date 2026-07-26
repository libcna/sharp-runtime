# Audit: `modules/core/include/System/StringComparer.hpp`

## Metadata

- Audit status: AUDITED (287-line header-only implementation, fully read).
- Validation: `OrdinalComparerTests.*:CultureAwareComparerTests.*:StringComparerTests.*`
  passed 42/42 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The factory mapping, case-sensitive byte ordering, ASCII case-folded equality,
and equal-input hash consistency are coherent. `Ordinal` and the invariant
instances are cached while current-culture factories create a fresh comparer,
matching the intended ownership distinction. Invalid `StringComparison` values
are rejected.

The implementation deliberately follows the repository-wide UTF-8-byte/C
locale adaptation: `OrdinalIgnoreCase` folds ASCII bytes, and all culture-aware
variants are documented to fall back to ordinal semantics because no
ICU/CultureInfo layer exists. This matches the limitation already recorded for
`String` and `Char`; it is a documented adaptation boundary, not a separate
confirmed defect in this report.

## Finding references

- **SR-AUD-018 (extended):** the paired direct suite asserts that the hashes
  of distinct case-sensitive strings `"ABC"` and `"abc"` must differ.  A hash
  contract only requires equal inputs (under this comparer) to have equal
  hashes; unequal inputs may collide.

## Other missing assertions and diagnostics

- No test covers an invalid `StringComparison` value and its required
  `ArgumentException` diagnostic.
- Tests exercise only ASCII. They omit multibyte UTF-8, a Unicode case pair,
  embedded NUL, signed-byte ordering, and the documented culture-fallback
  difference from real .NET culture-aware comparers.
- No test verifies that `Compare == 0`, `Equals == true`, and `GetHashCode`
  agree for each factory or that repeated factory calls have the documented
  cached/fresh identity behavior.
- `std::hash<std::string>` and C `std::tolower` behavior remain implementation
  and process-locale dependent. The header does not expose an explicit stable
  hash or locale policy beyond its narrow documented adaptation.

## Final assessment

The small, documented C++ adaptation is internally coherent for covered ASCII
inputs. Its direct suite extends the known invalid hash-test assertion pattern;
no source or test was modified during this audit.
