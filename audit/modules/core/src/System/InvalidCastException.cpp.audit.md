# Audit: `modules/core/src/System/InvalidCastException.cpp`

## Metadata

- Audit status: AUDITED (23-line implementation, fully read with declaration).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference basis: local .NET `InvalidCastException.cs` and `COR_E_INVALIDCAST` (`0x80004002`).

## Assessment

All ordinary constructors initialize their message/inner state through
`SystemException` then set `COR_E_INVALIDCAST`. The final constructor correctly
retains a caller-provided HResult rather than overwriting it. The focused tests
assert all ordinary `std::string` overload codes and the custom-code route. No
standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- The C-string overload is not checked for null, empty, or non-ASCII input; the base currently makes null an empty message, whose .NET diagnostic parity is unasserted.
- Tests observe only `what()` text for message construction and never retrieve/rethrow the stored inner exception.
- The source relies on headers to make `std::move` available; it builds in the current configuration, but no include-self-sufficiency build protects that dependency.

## Final assessment

HResult and custom-code behavior are consistent with the intended API. No source or test was modified.
