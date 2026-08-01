# Audit: `modules/runtime/include/System/Runtime/InteropServices/ExternalException.hpp`

## Metadata

- AUDITED: 27-line inline exception declaration, fully read.
- Validation: `ExternalExceptionTests.*` passed 4/4 on 2026-07-27; the
  complete shared fixture passed 82/82.
- Reference/probe: local current-.NET `InteropServices/ExternalException.cs`
  and `HResults.cs`; linked C++ probe prints `80131501`, while the matching
  managed probe prints the normal `80004005` and custom `81234567` ErrorCode.

## Related SR-AUD-157 — medium — ExternalException retains the SystemException HResult

All three C++ constructors delegate to `SystemException` without replacing
its `0x80131501` code.  Current .NET assigns `E_FAIL` (`0x80004005`) for the
default, message, and inner routes.  The linked C++/managed probes demonstrate
the mismatch; [SR-AUD-157](../AmbiguousImplementationException.hpp.audit.md#sr-aud-157--medium--ambiguousimplementationexception-and-externalexception-retain-systemexception-hresult)
owns the shared finding.

## SR-AUD-159 — medium — ExternalException omits its public error-code constructor and ErrorCode identity API

Current .NET exposes `ExternalException(string?, int errorCode)`, a virtual
`ErrorCode` property returning HResult, and a specialized `ToString()` that
includes the type and hexadecimal code.  C++ exposes only default, message,
and `exception_ptr` constructors; it has no error-code overload or named
ErrorCode/ToString API.  A C++ API probe attempting
`ExternalException("message", 7)` fails with no matching constructor, whereas
the managed probe constructs `0x81234567` and reads the same value through
`ErrorCode`.

Users can mutate the generic inherited HResult manually, but that neither
provides source-compatible error-code construction nor the public semantic
name and formatted diagnostic of this specialized exception.

## Other missing assertions and diagnostics

- The shared fixture covers default/custom text, a generic inner pointer, and
  throwability only; it omits normal/custom HResult, missing error-code API,
  `ErrorCode`, and formatted type/code output.
- It does not rethrow/check inner-cause identity, empty/UTF-8 text, nullable
  managed-message adaptation, or downstream COM/SEH producer translation.

## Final assessment

The represented constructors preserve their provided text and native inner
pointer, but ExternalException loses both its default diagnostic code and its
public native-error identity route.  No source or test was modified.

## Post-audit remediation — ticket #1875 (2026-08-01)

The represented-constructor part of SR-AUD-157 is remediated: default, message,
and message-plus-inner constructors now assign `E_FAIL` (`0x80004005`) exactly,
with one permanent assertion per constructor. The historical evidence above is
retained. SR-AUD-159 remains confirmed because no error-code constructor,
`ErrorCode` accessor, or specialized formatting API was added.
