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

### Status: REMEDIATED (#2277 review, #2278 implementation, 2026-08-11)

`CryptographicException(const std::string& format, const std::string& insert)`
added, defined out of line in the new
`modules/core/src/System/Security/Cryptography/CryptographicException.cpp` so
that `System/String.hpp` stays out of this public header's include set. The
substitution runs through `System::String::Format`, i.e. the single shared
`System::detail::runCompositeFormat` scanner, rather than a bespoke `{0}`
replacement — a fifth composite-format grammar is the defect class CCF-012 and
#2020 exist to stop. A malformed `format` therefore throws
`System::FormatException` from the constructor, matching .NET's
`string.Format`-based composition and confined to the new overload.

**This member was not a pure addition as first written.** Measured on isolated
mock shapes before touching the tree, adding `(string, string)` **removes**
`CryptographicException(message, nullptr)` and
`CryptographicException("literal", nullptr)`, both of which compile today: C++23
declares `std::basic_string(std::nullptr_t) = delete`, a deleted function is
still a candidate, so `nullptr` reaches `std::string` and `std::exception_ptr`
through equal-rank user-defined conversions and the call becomes ambiguous. That
public source break was designed out with an explicit
`CryptographicException(const std::string&, std::nullptr_t)` overload that
delegates to `(message, std::exception_ptr{})` — the object `nullptr` produced
before, pinned by a test that compares the two spellings field by field. It is
deliberately **not** added to `DirectoryNotFoundException`, `FileNotFoundException`
or `FileLoadException`: there the spelling never compiled, so adding it would be a
new widening rather than a preservation.

**Premise correction:** "no direct crypto exception test exists" is
executable-scoped. `CryptographicExceptionTests` already had 4 tests in
`SharpRuntimeTests_Security_Cryptography`; what had none was
`SharpRuntimeTests_Core_Base`, which owns this header. 13 tests were added there,
including the previously unasserted inherited `COR_E_SYSTEM` default, the
rethrown identity of a stored inner exception, and UTF-8 insert text. Two
mutations, both caught — one of them at compile time, naming both ambiguous
candidates. `docs/CoreExceptionErrorContextOverloadPlan.md`.

## Other missing assertions and diagnostics

- Tests omit the default HResult, custom HResult, all messages, inner identity/rethrow, UTF-8 text, and any cryptographic producer integration.
- The default code intentionally remains `SystemException`'s value in .NET; no unsupported derived-code claim was found.

## Final assessment

Existing constructors behave coherently, but the declared public compatibility surface and validation are incomplete. No source or test was modified.
