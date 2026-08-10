# Audit: `modules/runtime/include/System/Runtime/CompilerServices/CallerAttributes.hpp`

## Metadata

- AUDITED: 42-line inline four-attribute declaration, fully read.
- Validation: `CallerAttributesTests.*` passed 5/5 on 2026-07-27; the
  compiler-services shared filter passed 9/9.
- Reference basis: local current-.NET CallerMemberName, CallerFilePath,
  CallerLineNumber, and CallerArgumentExpression attribute sources; all four
  are parameter-only compiler-consumed attributes in the managed model.

## Assessment

The header is candid that ordinary C++ attributes cannot make the compiler
inject caller information: it instructs consumers to pass `__FUNCTION__`/
`__func__`, `__FILE__`, and `__LINE__` explicitly.  Searches find no runtime
consumer beyond the shared construction tests.  The three marker classes and
CallerArgumentExpression's retained parameter name therefore form a documented
metadata/source-fidelity adaptation, not a silently partial implementation.

## Other missing assertions and diagnostics

- All three marker tests stop at construction with `SUCCEED()`.  They do not
  demonstrate a portable macro wrapper forwarding member, path, or line at a
  call boundary.
- CallerArgumentExpression tests retain only ordinary/empty parameter names;
  they omit a real source-expression capture, UTF-8/embedded-NUL text, and the
  managed nullable parameter-name state that `std::string` cannot represent.
- No compile-failure test makes the C++ macro-only requirement explicit when a
  caller instead tries to attach one of these normal objects to a parameter.

## Final assessment

Caller context is intentionally delegated to standard C++ macros and this
limitation is visible in the public header.  No confirmed source defect and no
source or test modification resulted from this review.
