# Audit: `modules/core/include/System/StringNormalizationExtensions.hpp`

## Metadata

- AUDITED: 64-line inline string-normalization adapter, fully read.
- Validation: `StringNormalizationExtensionsTests.*` passed 5/5 on
  2026-07-27.  The relevant five cases live in `StringTests.cpp`, whose
  complete source-level audit remains pending; this focused result is runtime
  evidence only.
- Reference/probe: local current-.NET `StringNormalizationExtensions.cs` and
  `String.cs`; standalone C++20 and managed probes compare the UTF-8
  decomposed string `e` + U+0301 under `NormalizationForm::FormC`.

## SR-AUD-182 — medium — public Unicode normalization is an always-successful no-op outside ASCII

The two-form overload of `IsNormalized` unconditionally returns `true`, and
the two-form overload of `Normalize` returns its input unchanged.  This makes
every non-ASCII string appear normalized and prevents all NFC/NFD/NFKC/NFKD
transformations.  The default overloads delegate to those same stubs.

For the decomposed UTF-8 sequence `65CC81`, the C++ probe prints
`formc_is_normalized=1 normalized_hex=65CC81`.  The equivalent managed probe
prints `formc_is_normalized=False normalized_hex=C3A9`: current .NET recognizes
that the sequence is not Form C and composes it to U+00E9.  The local current
.NET source delegates to the Unicode `Normalization` implementation after the
normalization-form validation path.

The header does disclose that full Unicode tables are out of scope and that
the behavior is correct for ASCII.  That disclosure is useful context, but it
does not mark the public API unsupported or change the advertised
`StringNormalizationExtensions` counterpart contract.  Returning a positive
answer for unnormalized Unicode is observably misleading for callers that use
`IsNormalized` as a guard, so this remains a confirmed parity defect rather
than merely an undocumented feature omission.

## Other missing assertions and diagnostics

- All five direct cases use ASCII or an empty string.  They omit decomposed
  and compatibility text, all four forms, negative `IsNormalized` results,
  actual changed output, and idempotence after normalization.
- The fixture has no invalid-form diagnostic coverage.  Current .NET reports
  an argument error for an invalid normalization form; the C++ stub accepts it
  as a normal success.
- The test names assert `ReturnsInput`, which encodes the stub implementation
  rather than the API contract and makes the missing Unicode behavior appear
  intentional.

## Final assessment

The documented ASCII-only adaptation still exposes an always-successful
Unicode API under the managed counterpart name.  SR-AUD-182 is confirmed.  No
source or test was modified.
