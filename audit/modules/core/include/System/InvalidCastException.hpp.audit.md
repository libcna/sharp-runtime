# Audit: `modules/core/include/System/InvalidCastException.hpp`

## Metadata

- Audit status: AUDITED (33-line public declaration, fully read with implementation).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference basis: local .NET `InvalidCastException.cs` and `COR_E_INVALIDCAST` (`0x80004002`).

## Assessment

The declaration exposes the expected default, C-string, string, inner, and
custom-error-code constructors. Its paired implementation assigns
`COR_E_INVALIDCAST` to the ordinary overloads and preserves the explicit code
where the local API intentionally exposes one. Direct tests exercise both
paths. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit the C-string null boundary, message/inner combinations with empty or UTF-8 text, stored-inner identity/rethrow, and the default code for the C-string constructor.
- The C++ custom-error-code overload is carried from .NET's public API but lacks boundary diagnostics for unusual signed HRESULT values.
- Consumer coverage exists through `DBNull`, but no broader conversion dispatch test verifies the exception's error code at the throw site.

## Final assessment

The public declaration matches the reviewed implementation's normal diagnostic contract. No source or test was modified.
