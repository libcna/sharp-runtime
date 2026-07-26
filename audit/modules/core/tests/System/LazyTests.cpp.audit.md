# Audit: `modules/core/tests/System/LazyTests.cpp`

## Metadata

- Audit status: AUDITED (159 lines, 22 direct tests, fully read).
- Validation: `LazyTests.*` passed 38/38 in `SharpRuntimeTests_Core_Base` on
  2026-07-26, including 16 related smoke cases from a pending larger file.

## Assessment

The direct suite has useful ordinary coverage for constructors, initialization,
fault caching/retry, default mode, normal recursive rejection, and value text.
It tests all three modes only on regular values or factory exceptions, not their
invalid-config, callback-validity, recursion, or inter-thread distinctions.

## Finding references

- **SR-AUD-064:** no test supplies an invalid cast `LazyThreadSafetyMode` or
  asserts `ArgumentOutOfRangeException` from either mode-taking constructor.
- **SR-AUD-065:** no test supplies an empty `std::function<T()>` and asserts a
  constructor-boundary error rather than delayed `std::bad_function_call`.
- **SR-AUD-066:** the sole recursive test uses the default
  ExecutionAndPublication mode. It does not test that PublicationOnly must not
  throw the same reentrancy exception.

## Other missing assertions and diagnostics

- No concurrent first-access test checks execution count, publication identity,
  `IsValueCreated` observation, or exception visibility for any thread-safe
  mode.
- No mode test verifies the bool constructor maps `false` to None under a
  factory failure/reentry case, or checks mode persistence after creation.
- Tests do not verify output state after a PublicationOnly failure, precomputed
  value with a nontrivial `ToString`, move-only/default-constructor constraints,
  or native exception translation.

## Final assessment

The 38 passing filtered tests give good normal-state confidence but miss every
confirmed public validation and mode-specific reentrancy defect. No test was
modified during this audit.
