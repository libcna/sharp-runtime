# Audit: `modules/core/tests/System/ExceptionNewTests.cpp`

## Metadata

- Audit status: AUDITED (48 lines, ten tests across three suites, fully read).
- Validation: all its suites passed 10/10 within the complete 124/124 shared
  exception test filter in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

This compact companion adds basic Exception state, ArgumentNull inheritance,
and ObjectDisposed smoke coverage.  Its assertions are intentionally shallow;
in particular, it reproduces the empty default Exception expectation instead
of testing the current .NET diagnostic contract.

## Finding references

- **SR-AUD-089 / SR-AUD-090:** the ArgumentNull tests omit null C-string
  parameter construction and exact message/suffix checks.
- **SR-AUD-092:** `DefaultCtor_MessageEmpty` directly locks in the confirmed
  incompatible empty default Exception message.

## Other missing assertions and diagnostics

- InnerException is checked only for non-null presence; no test rethrows it,
  checks type/message/identity, or tests a null pointer.
- Data uses exactly one string key/value and omits the documented native
  adaptation's type, ownership, overwrite, erase, copy/move, and concurrency
  behavior.
- StackTrace checks the explicitly unsupported empty result but does not make
  the limitation visible in diagnostics or test throw/catch behavior.
- ObjectDisposed `ThrowIf` does not validate object name/message, false-path
  side effects, null/empty names, HResult, inner exception, or inheritance.

## Final assessment

All ten companion tests pass but contribute little negative-path evidence and
preserve the base default-message mismatch.  No source or test was modified
during this audit.
