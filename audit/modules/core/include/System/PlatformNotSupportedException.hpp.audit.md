# Audit: `modules/core/include/System/PlatformNotSupportedException.hpp`

## Metadata

- Audit status: AUDITED (47-line inline implementation, fully read).
- Validation: the focused five-type exception filter passed 29/29 on 2026-07-26.
- Reference basis: local .NET `PlatformNotSupportedException.cs` and `COR_E_PLATFORMNOTSUPPORTED` (`0x80131539`).

## Assessment

The class correctly supersedes `NotSupportedException`'s base code with
`COR_E_PLATFORMNOTSUPPORTED` in every constructor. Shared tests validate normal
message and broad exception inheritance. No standalone implementation defect
was confirmed.

## Other missing assertions and diagnostics

- Tests omit derived HResult, NotSupportedException inheritance, exact default text, stored-inner identity/rethrow, and empty/UTF-8 messages.
- No reviewed platform-gated API uses this type, so the operational policy for unsupported native/CLR features is untested.

## Final assessment

Constructor-level diagnostic behavior is coherent. No source or test was modified.
