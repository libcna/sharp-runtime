# Audit: `modules/core/include/System/IO/DirectoryNotFoundException.hpp`

## Metadata

- Audit status: AUDITED (24-line declaration, fully read with implementation).
- Validation: the focused I/O/crypto filter selected 0 tests; the shared probe prints `DirectoryNotFoundException=80070003`.
- Reference basis: local .NET `System/IO/DirectoryNotFoundException.cs` and `COR_E_DIRECTORYNOTFOUND` (`0x80070003`).

## SR-AUD-101 — medium — I/O and crypto exception ports omit public error-context overloads and have no direct test coverage

The port exposes a message/path constructor but omits current .NET's public
`(message, directoryPath, innerException)` overload. Consequently callers
cannot retain both the failed path and causal exception in one portable C++
object. See the owning `IOException.hpp.audit.md` report for the related
IOException and CryptographicException omissions and zero-test evidence.

## Other missing assertions and diagnostics

- No test asserts any HResult, path property, path-derived default diagnostic, inner identity/rethrow, C-string null, or UTF-8 path/message boundary.
- No reviewed filesystem operation constructs the exception from a real missing directory, so the mapping and retained path have no integration coverage.

## Final assessment

Existing HResult assignment is correct, but public context retention is incomplete and untested. No source or test was modified.
