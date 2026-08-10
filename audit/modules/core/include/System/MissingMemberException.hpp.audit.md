# Audit: `modules/core/include/System/MissingMemberException.hpp`

## Metadata

- Audit status: AUDITED (68-line inline implementation, fully read).
- Validation: the complete plural/singular member-access filter passed 61/61 on 2026-07-26.
- Reference basis: local .NET runtime exception source and `COR_E_MISSINGMEMBER` (`0x80131512`).

## Assessment

The default, message, class/member, and message-plus-inner overloads set the
derived HResult after `MemberAccessException` construction.  The class/member
overload produces the documented `Member 'Class.Member' not found.` format;
its exact form and all overload HResults are directly asserted.  No standalone
implementation defect was confirmed.

## Other missing assertions and diagnostics

- Current tests cover ordinary ASCII names only. Empty, quoted, dotted, and
  UTF-8 class/member names are not asserted, so message escaping and diagnostic
  legibility at those boundaries are unknown.
- The inner-exception test only checks outer text; it does not retrieve, compare,
  or rethrow the stored `std::exception_ptr`.
- Dynamic member lookup is a reflection-oriented .NET behavior with no general
  native-C++ dispatcher in this module. The missing integration path is a
  documented adaptation limitation, not evidence of a constructor defect.

## Final assessment

The examined constructor and HResult paths are coherent and pass focused
evidence. No source or test was modified.
