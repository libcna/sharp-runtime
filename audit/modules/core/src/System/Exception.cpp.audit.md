# Audit: `modules/core/src/System/Exception.cpp`

## Metadata

- Audit status: AUDITED (76-line implementation, fully read).
- Validation: the complete `ExceptionTests.cpp` and `ExceptionNewTests.cpp`
  suite filter passed 124/124 in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET `Exception.cs`, especially its nonempty `Message`
  fallback and lazily created Data dictionary, was reviewed.

## Assessment

The implementation safely accepts a null C-string as an empty explicit message,
keeps `what()` tied to owned string storage, initializes the expected base
HResult, and holds source/help/data/inner state without raw ownership hazards.
It returns the stored default empty message directly instead of implementing
the source fallback diagnostic.

## Finding references

- **SR-AUD-092:** `Exception::Exception()` initializes `message_` to empty and
  `getMessageProperty()` returns it unchanged, making the default public
  message/`what()` blank contrary to current .NET.

## Other missing assertions and diagnostics

- No selected test covers a null C-string constructor argument, embedded NUL,
  non-ASCII message/source/help text, copy/move after a captured `what()`
  pointer, or concurrent use of mutable Data.
- Data's eagerly allocated string map differs from .NET's object dictionary;
  no test establishes allowed key/value conversion, exception behavior, or
  source compatibility.
- `innerException_` is opaque `exception_ptr`; no helper diagnoses foreign
  native exception types or supports base-chain inspection.

## Final assessment

State storage is straightforward and safe for its native types, but the empty
default message is an observable compatibility regression.  No source or test
was modified during this audit.
