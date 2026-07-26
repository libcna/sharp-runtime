# Audit: `modules/core/include/System/Threading/LazyThreadSafetyMode.hpp`

## Metadata

- Audit status: AUDITED (18-line public enum header, fully read).
- Supporting validation: `LazyTests.*` passed 38/38 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The three enum values and numeric ordering match local .NET:
`None = 0`, `PublicationOnly = 1`, and `ExecutionAndPublication = 2`.
Its brief documentation accurately describes the broad synchronization modes,
but it omits the exception/reentrancy differences that make those values
behaviorally significant.

## Finding references

- **SR-AUD-064:** an arbitrary cast enum value is accepted by `Lazy<T>` rather
  than rejected as invalid mode.
- **SR-AUD-066:** PublicationOnly's contract differs materially from the other
  values for recursive `Value` access, but the implementation currently treats
  all modes alike and throws.

## Other missing assertions and diagnostics

- There is no dedicated enum-value test, invalid-cast constructor test, or
  mode-specific recursion/concurrency test.
- Documentation does not state that None offers no concurrent-use guarantee,
  that PublicationOnly normally permits multiple factories, or that exception
  caching/reentrancy differ by mode.

## Final assessment

The values are correct, while their behavioral validation is delegated to
`Lazy<T>` and currently exposes two confirmed mode-handling defects. No source
or test was modified during this audit.
