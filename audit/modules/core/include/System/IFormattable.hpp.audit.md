# Audit: `modules/core/include/System/IFormattable.hpp`

## Metadata

- Audit status: AUDITED (49-line public interface, fully read).
- Supporting validation: `IFormattableTests.*` and `IFormattableTests2.*`
  passed 3/3 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The required one-argument const virtual formatter and the optional
provider-aware forwarding overload are coherent.  Calling the latter through
an `IFormattable` reference directly verifies that the default does not retain
the provider and dispatches to the required operation.  The intentional
`std::string` substitution for .NET formatting text is a documented C++
adapter choice.

## Other missing assertions and diagnostics

- Tests use only a null provider, so no fixture establishes a concrete
  provider-aware override or documented provider-ignorance with a non-null
  sentinel.
- A derived one-argument override hides the two-argument base overload for
  concrete-object calls unless it introduces a `using` declaration; the tests
  correctly call through the base but do not retain a compile-only regression.

## Final assessment

Provider forwarding and normal formatting are directly covered.  No
evidence-backed declaration defect was found and no source or test was modified
during this audit.
