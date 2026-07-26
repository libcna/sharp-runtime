# Audit: `modules/core/include/System/ArgumentNullException.hpp`

## Metadata

- Audit status: AUDITED (77-line public declaration, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- ASan/UBSan reproducer: `/tmp/sharp-runtimervc-argumentexception-audit-probe.cpp`
  calls the public `const char*` constructor with a null pointer.  UBSan reports
  a null argument to `char_traits::length` and ASan reports a null-read SEGV in
  `makeMsg`.
- Reference: local .NET `ArgumentNullException.cs` exposes
  `ArgumentNullException(string? paramName)` and therefore permits a null
  parameter name.

## Assessment

The declared inheritance and pointer guard are appropriate C++ counterparts,
but the overload accepting `const char*` presents a nullable native pointer
without a null contract.  Its implementation also duplicates a formatted
parameter suffix before forwarding to the base constructor.

## SR-AUD-089 — high — ArgumentNullException null C-string parameter crashes during message construction

`ArgumentNullException(const char* paramName)` accepts a raw C-string but
forwards it to `makeMsg`, which concatenates it before the base constructor can
apply its null fallback.  The explicit public call
`ArgumentNullException(static_cast<const char*>(nullptr))` reaches
`std::char_traits<char>::length(nullptr)`; UBSan diagnoses the invalid null
argument and ASan confirms a read from address zero.

The local .NET constructor accepts a nullable parameter name and produces a
valid exception with no parameter suffix.  A C++ raw-pointer overload must
either handle null deterministically or reject it with a system diagnostic
before string construction.

## SR-AUD-090 — low — ArgumentNullException duplicates its parameter marker in `what()`

The parameter-name constructors compose `"Value cannot be null. (Parameter
'item')"` and then call the two-argument `ArgumentException` constructor,
which appends the same suffix again.  The direct probe prints `Value cannot be
null. (Parameter 'item') (Parameter 'item')`, while the local .NET base call
formats the marker once.  The parameter property and exception type remain
correct, but logs and user diagnostics are observably malformed.

## Other missing assertions and diagnostics

- The pointer guard tests only `int*`; they omit const/volatile pointers,
  `void*`, derived/base conversion, null parameter-name inputs, and the
  message/property pair after a null-name construction.
- No declaration states C-string ownership/encoding or whether a null
  `paramName` is valid.  The `std::string` overload cannot represent that state.
- Tests do not count the parameter marker, assert exact default diagnostics,
  or exercise copy/move and inner-exception lifetime.

## Final assessment

The normal non-null path passes, but one public raw-pointer constructor is
ASan-confirmed unsafe and its ordinary message has a duplicate marker.  No
source or test was modified during this audit.
