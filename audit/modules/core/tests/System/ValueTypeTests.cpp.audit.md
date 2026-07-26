# Audit: `modules/core/tests/System/ValueTypeTests.cpp`

## Metadata

- Audit status: AUDITED (57 lines, five direct tests, fully read).
- Supporting evidence: the five `ValueTypeTests.*` smoke cases in
  `Batch15TypesTests.cpp` were included only in the focused filter; that
  aggregate source remains pending its own file-wide audit.
- Validation: `ValueTypeTest.*:ValueTypeTests.*` passed 10/10 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The compact direct fixture makes the difference between explicit overrides and
the base identity fallback visible. It correctly checks polymorphic dispatch
through a base pointer and equal/different values for the manually overridden
type. However, the expected identity result for a fieldless derived wrapper
preserves a fallback that differs from the .NET `ValueType` default contract.

## Finding references

- **SR-AUD-068:** `DefaultEqualsIdentity` asserts that the base implementation
  is identity-based, even though current .NET `ValueType` is abstract and
  provides fieldwise default equality/hash semantics. The fixture neither
  signals this as an intentional unsupported C++ adaptation nor prevents the
  public default from reaching consumers.

## Other missing assertions and diagnostics

- No direct test compiles or rejects direct `ValueType` construction, although
  that public state is unavailable for abstract .NET `ValueType`.
- No test proves field-equality behavior for a derived value type that omits
  `Equals`/`GetHashCode`, nor records a diagnostic that such a wrapper must
  override them.
- The base hash is never observed; the suite therefore misses address-hash
  truncation/implementation-defined narrowing on a platform wider than
  `intcs`.
- The manual `ConcreteValueType` checks only equal and unequal integers. It
  omits type mismatch, self equality via a base reference, reference fields,
  floating-point NaN, and a base `ToString` / runtime-name comparison.

## Final assessment

All focused checks pass, but they lock in an observable public divergence
rather than exercise a compatible default value-semantic path. No test source
was modified during this audit.
