# Audit: `modules/core/include/System/IO/IOException.hpp`

## Metadata

- Audit status: AUDITED (27-line declaration, fully read with implementation).
- Validation: `IOExceptionTests.*:DirectoryNotFoundExceptionTests.*:CryptographicExceptionTests.*` selected 0 tests on 2026-07-26; the shared HResult probe prints `IOException=80131620`.
- Reference basis: local .NET `System/IO/IOException.cs`.

## SR-AUD-101 — medium — I/O and crypto exception ports omit public error-context overloads and have no direct test coverage

The .NET `IOException(string?, int hresult)` constructor is absent from this
published C++ declaration, so callers cannot preserve an OS/native error code
while providing a diagnostic message. The related `DirectoryNotFoundException`
port omits its public `(message, directoryPath, innerException)` overload, and
`CryptographicException` omits its public composite-format/insertion overload.
All three headers represent their types as implemented ports, yet their focused
filter selects zero tests; missing overloads are therefore neither compiled nor
behaviorally diagnosed.

## Other missing assertions and diagnostics

- No direct test checks default, C-string, string, inner, inheritance, or the verified `COR_E_IO` (`0x80131620`) HResult.
- The C-string null path, exact default resource text, stored-inner identity/rethrow, and native OS I/O error translation are untested.

## Final assessment

Existing constructors use the right code, but the public compatibility surface is incomplete and untested. No source or test was modified.
