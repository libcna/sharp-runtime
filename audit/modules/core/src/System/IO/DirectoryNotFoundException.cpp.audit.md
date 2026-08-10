# Audit: `modules/core/src/System/IO/DirectoryNotFoundException.cpp`

## Metadata

- Audit status: AUDITED (26-line implementation, fully read with declaration).
- Validation: the focused I/O/crypto filter selected 0 tests; the shared probe prints `DirectoryNotFoundException=80070003`.
- Reference basis: local .NET `System/IO/DirectoryNotFoundException.cs` and `COR_E_DIRECTORYNOTFOUND` (`0x80070003`).

## Assessment

All five implemented constructors replace the inherited IOException code with
`COR_E_DIRECTORYNOTFOUND`; the probe confirms it. The message/path constructor
stores its path but lacks the .NET public inner-exception companion overload,
as recorded in SR-AUD-101. No separate defect was confirmed in implemented
routes.

## Other missing assertions and diagnostics

- No direct tests exercise default text, C-string null behavior, every HResult, path property, inner identity/rethrow, or path-specific message formatting.
- No missing-directory operation reaches this code, leaving platform error translation unverified.

## Final assessment

Implemented HResult behavior is correct, but path-plus-cause compatibility is incomplete and untested. No source or test was modified.
