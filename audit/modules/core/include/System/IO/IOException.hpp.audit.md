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

### Status: REMEDIATED (#2277 review, #2278 implementation, 2026-08-11)

All three omissions are implemented. **The "pure additions" ranking this unit
inherited was wrong for two of the three members**, and both were measured rather
than discovered afterwards.

`build-probe/2277_probe1_before.cpp` prints a `std::is_constructible_v` row over
thirteen argument packs for each type plus the observable state of every
construction they support, and the identical source was compiled before and after
the repair. **No cell anywhere flips `yes` → `no`; nine flip `no` → `yes`.** But
one already-compiling spelling changes meaning and one would have stopped
compiling:

- `IOException(message, 0)` used to select the inner-exception overload, because
  the literal `0` is a null pointer constant; it now selects
  `IOException(string, int)` and yields HResult `0` instead of `COR_E_IO`. The
  message and the absent inner exception are unchanged. This is **irreducible** —
  `0` has type `int` and is indistinguishable from any other `int` — and no
  first-party call site spells it: every 2-argument `IOException` construction in
  this repository passes a real `std::exception_ptr`. `nullptr` is unaffected and
  is pinned by a test, as is the re-targeting itself.
- adding `CryptographicException(format, insert)` **removed**
  `CryptographicException(message, nullptr)`, which compiles today, because C++23
  declares `std::basic_string(std::nullptr_t) = delete` and a deleted function is
  still a candidate, so `nullptr` reached `std::string` and `std::exception_ptr`
  through equal-rank user-defined conversions. That public source break was
  designed out with an explicit `(const std::string&, std::nullptr_t)` overload
  delegating to the empty-`exception_ptr` meaning the spelling already had.

**Premise correction — the zero-test claim is executable-scoped.** Measured
against all 38 executables, the focused filter selects **15** tests, not zero: 11
in `SharpRuntimeTests_IO` and 4 in `SharpRuntimeTests_Security_Cryptography`, the
binaries of two *consumer* components. What is true is that
`SharpRuntimeTests_Core_Base`, which owns all three headers, asserted nothing, and
that every route listed below really was unasserted anywhere. 33 tests were added
to the owning binary and none of the 15 was retired, so the filter now selects 48.

**Premise correction — `DirectoryNotFoundException`'s path family has no
verifiable .NET counterpart.** `/rv` is absent, so the reference source named
above could not be re-read, and the port's existing `(message, directoryPath)`
constructor and `getDirectoryPathProperty()` are not a .NET shape this repository
can point at. The `(message, directoryPath, inner)` addition is justified without
that premise: it closes the user-visible gap the finding states and completes the
`(message, fileName, inner)` shape `FileNotFoundException` and `FileLoadException`
already carry.

`sizeof` (168/200/168), `alignof` (8/8/8) and polymorphism are unchanged; no
signature, `noexcept`, vtable or component-dependency declaration changed; the
three out-of-line constructors are new symbols and none was removed. Sanitizers
were deliberately not run — the defect class is a missing overload, so they are
not discriminating. Five mutations, all caught; the fifth is a compile-domain
mutation caught at compile time with both ambiguous candidates named.
`docs/CoreExceptionErrorContextOverloadPlan.md`.

## Other missing assertions and diagnostics

- No direct test checks default, C-string, string, inner, inheritance, or the verified `COR_E_IO` (`0x80131620`) HResult.
- The C-string null path, exact default resource text, stored-inner identity/rethrow, and native OS I/O error translation are untested.

## Final assessment

Existing constructors use the right code, but the public compatibility surface is incomplete and untested. No source or test was modified.
