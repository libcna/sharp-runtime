# Audit: `modules/core/include/System/IParsable.hpp`

## Metadata

- Audit status: AUDITED (49-line public template interface, fully read).
- Supporting validation: `IParsableTests2.*` passed 2/2 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The header explicitly and accurately records the necessary virtual-instance
adaptation of .NET static-abstract members.  It has no parser implementation,
string conversion, or error construction; concrete types own those semantics.
`ISpanParsable<TSelf>` correctly builds on this interface and was audited
separately.

## Other missing assertions and diagnostics

- The representative parser uses `std::stoi`; Parse's invalid-path exception
  type is not tested against the documented `System::FormatException`.
- Failed `TryParse` is checked only for `false` with a default-initialized
  result.  The test does not document whether a stale pre-populated result must
  be preserved or defaulted by this local adapter.
- No non-null provider, overflow, whitespace, or polymorphic base-pointer
  parser call is tested for this string-only interface.

## Final assessment

The static-to-virtual adaptation is documented and structurally complete.  No
new declaration defect is confirmed; no source or test was modified during
this audit.
