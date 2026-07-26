# Audit: `modules/core/include/System/Security/Cryptography/CryptographicException.hpp`

## Metadata

- Audit status: AUDITED (35-line inline implementation, fully read).
- Validation: the focused I/O/crypto filter selected 0 tests; the shared probe prints default inherited `80131501`, matching the current .NET default SystemException code.
- Reference basis: local .NET `CryptographicException.cs`.

## SR-AUD-101 — medium — I/O and crypto exception ports omit public error-context overloads and have no direct test coverage

Current .NET exposes `CryptographicException(format, insert)` to produce a
formatted diagnostic. This port exposes only default, HResult, message, and
message-plus-inner overloads, so valid public callers cannot express that
construction path. The same audit confirms missing custom-HResult IOException
and directory-path-plus-inner constructors; see the owning IOException report.
No direct crypto exception test exists.

## Other missing assertions and diagnostics

- Tests omit the default HResult, custom HResult, all messages, inner identity/rethrow, UTF-8 text, and any cryptographic producer integration.
- The default code intentionally remains `SystemException`'s value in .NET; no unsupported derived-code claim was found.

## Final assessment

Existing constructors behave coherently, but the declared public compatibility surface and validation are incomplete. No source or test was modified.
