# Audit: `modules/io-hashing/CMakeLists.txt`

## Metadata

- AUDITED: physical module registration and declared dependencies.
- Evidence: the static `IO.Hashing` target declares only `Core.Base` and `IO`,
  matching its public includes and module-boundary validation.

## Assessment

The component registration is coherent; the hashing implementations do not add
undeclared production dependencies.

## Other missing assertions and diagnostics

- Retain a target-level focused test invocation in CI, including sanitizer
  coverage for raw pointer contracts.

## Final assessment

No separate finding. No source or test changed.
