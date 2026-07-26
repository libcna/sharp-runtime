# Audit: `modules/core/tests/System/ArgumentNullExceptionTests.cpp`

## Metadata

- Audit status: AUDITED (57 lines, eleven tests, fully read).
- Validation: `ArgumentNullExceptionTests.*` passed 11/11 within the 64/64
  argument-exception filter in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The fixture confirms inheritance, E_POINTER, normal parameter preservation,
custom-message presence, and simple pointer guard behavior.  It checks only
substring presence, so both the duplicate suffix and null C-string crash pass
unobserved.

## Finding references

- **SR-AUD-089:** no test constructs the public C-string overload with an
  explicit null pointer; the ASan-confirmed fault is outside the suite.
- **SR-AUD-090:** `ParamNameCtor_WhatContainsParamName` accepts one or more
  occurrences, so the doubled parameter marker passes.

## Other missing assertions and diagnostics

- Add explicit null/empty C-string parameter-name cases, assert successful
  construction and a zero/one suffix rule, and run the null case under ASan.
- Test `ThrowIfNull` for cv-qualified/void/derived pointers and validate the
  message, HResult, and parameter property together rather than only type.
- No test verifies an inner exception's identity, default message exactness,
  custom empty/null message policy, or copy/move stability.

## Final assessment

Eleven smoke tests pass while missing both demonstrated public constructor
failures.  No source or test was modified during this audit.
