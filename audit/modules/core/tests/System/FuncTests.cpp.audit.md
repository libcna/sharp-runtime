# Audit: `modules/core/tests/System/FuncTests.cpp`

## Metadata

- AUDITED: 49-line dedicated fixture, fully read.
- Validation: `ConverterTests.*:PredicateTest.*:FuncTests.*` passed 17/17 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26; its own seven tests passed.

## Findings

The fixture checks normal invocation at representative arities 0–4, 8, and
16.  It never tests the result-type boundary, so `Func<void>` remains accepted
and type-identical to `Action`, as confirmed by the standalone probe for
SR-AUD-126.

## Missing assertions and diagnostics

- Missing direct use tests for arities 5–7 and 9–15.
- Missing empty invocation, throwing callable, reference result, and
  captured-lifetime vectors.
- No compile-time assertion rejects `Func<void>` or verifies that it is a
  deliberate, documented extension.
- Test names encode project-specific `FuncT*` spellings but provide no mapping
  back to the corresponding .NET `Func<...>` arities.

## Final assessment

Representative happy paths pass but do not protect the confirmed
SR-AUD-126 type-category boundary. No source or test was modified during this
audit.
