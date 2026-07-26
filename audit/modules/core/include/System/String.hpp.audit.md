# Audit: `modules/core/include/System/String.hpp`

## Metadata

- AUDITED: 712-line public static utility declaration, documented native
  string model, overload/range contracts, and formatter declarations.
- Validation: `SharpRuntimeTests_Core_Base` passed 4,946/4,946; implementation
  and direct-probe evidence were reviewed in the paired `String.cpp` report.

## Assessment

The header supplies a broad `std::string`-based adapter and documents many
range and formatting diagnostics precisely.  It is necessarily a byte-string
subset of the managed UTF-16/culture-sensitive API.  Its declarations expose
the existing implementation defects rather than cause independent behavior:
SR-AUD-015 covers composite-brace handling and SR-AUD-016 covers bounded
substring `LastIndexOf` range escape.

## Other missing assertions and diagnostics

- Keep exact public-header self-containment coverage and compile representative
  overloads from an external consumer TU.
- Add tests for Unicode/culture-sensitive comparison, trim, case conversion,
  format precision diagnostic translation, allocation-size overflow, escaped
  braces, and range-constrained substring search.
- Clearly distinguish the supported UTF-8 byte adapter from current .NET
  UTF-16 indexing and null-string overload semantics.

## Final assessment

The public contract remains broadly documented, with SR-AUD-015/016 as its
known implementation contradictions. No source or test was changed.
