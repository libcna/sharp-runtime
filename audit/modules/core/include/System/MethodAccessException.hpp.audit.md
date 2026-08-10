# Audit: `modules/core/include/System/MethodAccessException.hpp`

## Metadata

- Audit status: AUDITED (55-line inline implementation, fully read).
- Validation: the complete plural/singular member-access filter passed 61/61 on 2026-07-26.
- Reference basis: local .NET runtime exception source and `COR_E_METHODACCESS` (`0x80131510`).

## Assessment

Each overload correctly constructs `MemberAccessException` then replaces its
base HResult with `COR_E_METHODACCESS`.  The default text identifies a method,
and the message-plus-inner path retains the caller's text.  Dedicated tests
cover its base relationship and the replacement HResult for all public
overloads.  No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit an exact assertion for the default diagnostic, the stored inner
  `std::exception_ptr` identity, and empty/UTF-8 message boundaries.
- The suite checks inheritance through both direct and shared fixtures but has
  no runtime access-control scenario; native compile-time access failures are
  not equivalent to .NET reflection/partial-trust `MethodAccessException`.
- No null C-string parameter is accepted by this C++ API.

## Final assessment

The inline implementation consistently assigns the derived HResult and
preserves the declared constructor behavior. No source or test was modified.
