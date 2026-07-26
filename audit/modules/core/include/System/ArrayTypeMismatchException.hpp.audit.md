# Audit: `modules/core/include/System/ArrayTypeMismatchException.hpp`

## Metadata

- Audit status: AUDITED (55-line inline implementation, fully read).
- Validation: its six shared fixture cases passed within 124/124 on 2026-07-26.
- Reference/probe: local .NET `ArrayTypeMismatchException.cs` assigns
  `COR_E_ARRAYTYPEMISMATCH`; `/tmp/sharp-runtimervc-arraytypemismatch-audit-probe`
  prints the C++ default `80131501`.

## SR-AUD-093 — medium — ArrayTypeMismatchException inherits SystemException HResult instead of its own error code

None of the inline constructors calls `setHResultProperty`, so every instance
keeps `COR_E_SYSTEM` (`0x80131501`).  Current .NET assigns
`COR_E_ARRAYTYPEMISMATCH` (`0x80131503`) in each constructor.  The direct probe
confirms the incorrect C++ value; existing message/inheritance tests omit it.

## Other missing assertions and diagnostics

- Tests omit all constructor HResults, null C-string, inner identity, exact
  resource text, and actual array-store integration.
- C++ arrays are statically typed, so no public store path documents when this
  exception is meaningful versus a compile error or native cast failure.

## Final assessment

Message/inheritance paths pass, but the public diagnostic code is wrong. No source or test was modified.
