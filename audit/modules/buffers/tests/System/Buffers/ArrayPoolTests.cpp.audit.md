# Audit: `modules/buffers/tests/System/Buffers/ArrayPoolTests.cpp`

## Metadata

- Audit status: AUDITED (51 lines, seven tests, fully read).
- Validation: `ArrayPoolTest.*` passed 7/7 in `SharpRuntimeTests_Buffers` on
  2026-07-26.

## Assessment

The compact suite covers shared availability, ordinary Rent length, optional
clear, default/custom factory construction, zero Rent, and negative Rent. It
is useful smoke coverage, but treats the configurable factory as a non-null
constructor rather than verifying its mandatory validation or semantics.

## Finding references

- **SR-AUD-076:** `CreateWithCapacity` supplies only a valid pair and asserts
  non-null. It leaves zero/negative configuration and all configured capacity
  behavior unasserted, so the implementation can discard both public values.
- **SR-AUD-070 (extended):** all fixtures use `int`/`double`; no test exposes
  vector/default-construction requirements in Rent or clear.

## Other missing assertions and diagnostics

- No test returns then rents a buffer, detects clear versus non-clear content,
  or records the deliberate no-reuse performance adaptation.
- No test covers post-Return ownership, double/foreign return, concurrent
  callers, huge sizes/allocation failure, or exception type for configuration
  input.
- `SharedNotNull` does not assert singleton identity; no consumer receives the
  pool polymorphically or validates virtual destruction.
- Zero Rent tests only empty size. They omit a nonempty pool's configured
  bucket selection and the source's allowed larger-than-minimum return rule.

## Final assessment

All smoke tests pass, but they do not exercise the public configuration
contract that is currently a silent stub. No test source was modified during
this audit.
