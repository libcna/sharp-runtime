# Audit: `modules/core/include/System/MemberAccessException.hpp`

## Metadata

- Audit status: AUDITED (48-line inline implementation, fully read).
- Validation: the complete plural/singular member-access filter passed 61/61 on 2026-07-26.
- Reference basis: local .NET runtime exception source and the documented `COR_E_MEMBERACCESS` value (`0x8013151A`).

## Assessment

All three constructors delegate to `SystemException`, preserve an explicitly
supplied `std::exception_ptr`, and set `COR_E_MEMBERACCESS` after base
construction.  The default and derived exception paths expose non-empty
messages and the focused test filter verifies the HResult for every overload.
No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests do not assert the exact default message, `what()` after an empty or
  UTF-8 message, or the identity/rethrow behavior of the stored inner pointer.
- There is no test showing an externally observable member-access failure.  In
  native C++, access control is ordinarily diagnosed at compile time; reflection
  and partial-trust activation are documented adaptation boundaries rather than
  evidence that this constructor itself is faulty.
- No null C-string boundary exists in this public API: its message overloads
  require `std::string`.

## Final assessment

Constructor, inheritance, and diagnostic-code behavior are consistent with the
implemented C++ contract. No source or test was modified.
