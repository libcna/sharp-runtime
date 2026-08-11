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

### Status: REMEDIATED (#2277 review, #2278 implementation, 2026-08-11)

`DirectoryNotFoundException(const std::string&, const std::string&, std::exception_ptr)`
added. This member **is** the pure addition the unit was ranked as: measured
before and after, three construction cells flip `no` → `yes` and none flips
`yes` → `no`, and no existing construction changes its observable state.

**Premise correction:** the ".NET public inner-exception companion overload"
could not be verified — `/rv` is absent, so the reference source named above was
unavailable, and the port's existing `(message, directoryPath)` constructor and
`getDirectoryPathProperty()` are not a .NET shape this repository can point at
either. The addition is justified without that premise: it closes the stated gap
(a caller had to discard either the failed path or its cause) and completes the
`(message, fileName, inner)` shape `FileNotFoundException` and
`FileLoadException` already provide. Note that `(message, nullptr)` was **already**
ill-formed on this type before the change and still is — it carries both a
`(string, string)` and a `(string, exception_ptr)` overload — which is exactly the
ambiguity the sibling `CryptographicException` repair had to design around.

8 tests added to `SharpRuntimeTests_Core_Base`, covering the new constructor's
retained path, rethrown inner identity, `COR_E_DIRECTORYNOTFOUND`, `nullptr`
cause, null C-string message and UTF-8 path/message. Two mutations, both caught.
`docs/CoreExceptionErrorContextOverloadPlan.md`.

## Other missing assertions and diagnostics

- No test asserts any HResult, path property, path-derived default diagnostic, inner identity/rethrow, C-string null, or UTF-8 path/message boundary.
- No reviewed filesystem operation constructs the exception from a real missing directory, so the mapping and retained path have no integration coverage.

## Final assessment

Existing HResult assignment is correct, but public context retention is incomplete and untested. No source or test was modified.
