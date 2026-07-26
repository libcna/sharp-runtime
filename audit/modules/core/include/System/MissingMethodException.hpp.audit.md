# Audit: `modules/core/include/System/MissingMethodException.hpp`

## Metadata

- Audit status: AUDITED (61-line inline implementation, fully read).
- Validation: the complete plural/singular member-access filter passed 61/61 on 2026-07-26.
- Reference basis: local .NET runtime exception source and `COR_E_MISSINGMETHOD` (`0x80131513`).

## Assessment

The default, message, message-plus-inner, and class/method overloads replace
the `MissingMemberException` code with `COR_E_MISSINGMETHOD`.  The class/method
constructor forms the documented `Method 'Class.Method' not found.` string,
and direct tests assert it and every overload HResult. No standalone
implementation defect was confirmed.

## Other missing assertions and diagnostics

- Existing cases use simple ASCII names only; they omit empty, quote-containing,
  dotted, and UTF-8 names that could make formatted diagnostics ambiguous.
- The inner-exception assertion observes only the outer `what()` string, not
  pointer identity or rethrow behavior.
- Native member lookup normally fails during compilation, so the absence of a
  reflection-based runtime missing-method integration test is an adaptation
  boundary rather than a proven fault in these constructors.

## Final assessment

The reviewed inline behavior is internally consistent and its normal public
paths pass focused validation. No source or test was modified.
