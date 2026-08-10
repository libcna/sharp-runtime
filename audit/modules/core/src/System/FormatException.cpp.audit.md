# Audit: `modules/core/src/System/FormatException.cpp`

## Metadata

- Audit status: AUDITED (35-line implementation, fully read with declaration).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference basis: local .NET `FormatException.cs` and `COR_E_FORMAT` (`0x80131537`).

## Assessment

The local default diagnostic and all constructor paths set `COR_E_FORMAT`; the
inner overload retains its supplied `std::exception_ptr`. The focused suite
checks default HResult and normal message behavior. No standalone implementation
defect was confirmed.

## Other missing assertions and diagnostics

- C-string null normalization, non-default HResults, exact resource text, stored-inner identity/rethrow, and UTF-8 text remain untested.
- The implementation relies on transitive availability of `std::move`; current builds succeed, but no include-self-sufficiency test documents that dependency.

## Final assessment

Normal constructor and diagnostic-code behavior are coherent. No source or test was modified.
