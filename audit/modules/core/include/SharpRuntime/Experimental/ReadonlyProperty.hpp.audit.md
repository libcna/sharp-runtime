# Audit: `modules/core/include/SharpRuntime/Experimental/ReadonlyProperty.hpp`

## Metadata

- AUDITED: 64-line experimental derived property wrapper, fully read.
- Validation: the related `ExperimentalPropertyTests.*` passed 4/4 on
  2026-07-27; no first-party instantiation of this derived type was found.
- Reference basis: its base `Property.hpp` contract and source-consumer search.

## Assessment

The constructor correctly supplies a null setter and deletes the derived
`operator=(const T&)`, so ordinary derived assignment is rejected at compile
time.  The public inherited `set` operation still exists and predictably
throws the base's `NotSupportedException`; this is a clear runtime read-only
policy rather than a mutation hole.

The class deliberately inherits the base's callback and stale-assignment
implementation.  SR-AUD-179 therefore remains relevant if callers cast to the
base or use inherited forms, but its root cause belongs to `Property.hpp` and
is not duplicated here.

## Other missing assertions and diagnostics

- No compile-time fixture verifies deleted assignment, nor runtime fixture
  verifies `set` throws through a `ReadOnlyProperty` instance.
- Tests omit copy/move/lifetime of a getter capturing an owner, implicit
  conversion, and behavior through a base-class reference.

## Final assessment

The derived read-only wrapper is internally coherent within the explicitly
experimental helper design.  No new finding and no source or test change.
