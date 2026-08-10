# Audit: `modules/core/tests/System/DelegateTests.cpp`

## Metadata

- Audit status: AUDITED (303-line direct fixture, fully read).
- Validation: `DelegateTests.*` passed 33/33 in the 70-test direct delegate
  filter on 2026-07-26.
- Reference basis: `Delegate.hpp`, `Delegate.cpp`, and local .NET delegate
  composition semantics.

## Findings

The suite covers normal invocation, null identities, single-entry removal,
enumeration, and the explicit DynamicInvoke/Target limitations.  It omits all
three nontrivial list contracts now demonstrated by the probe: concrete type
preservation/mismatch rejection (SR-AUD-118), equal independently allocated
list entries (SR-AUD-119), and multi-entry sequence removal (SR-AUD-120).

## Other missing assertions and diagnostics

- Default no-op behavior is tested as valid and `HasSingleTarget` lacks an
  empty-delegate vector, despite .NET's abstract base having no such object.
- `GetInvocationList` is tested only for shared ownership; stack objects and
  their documented `bad_weak_ptr` failure remain unasserted.
- Hash tests check only consistency/nonzero, not equal values, mismatched
  types, portability, or collisions.

## Final assessment

The fixture detects simple callback regressions but misses the core delegate
value/composition rules.  No source or test was modified during this audit.
