# Audit: `modules/core/CMakeLists.txt`

## Metadata

- Audit status: AUDITED (full read).
- Component: `Core.Base` static foundation and `Core` compatibility umbrella.
- Evidence: module registration, dependency-validator result, generated
  component catalogue, and targeted Core.Base test executable.

## Assessment

`Core.Base` declares no production dependency, which is consistent with its
role as the acyclic runtime foundation.  Its nine test-only dependencies are
explicit and the compatibility umbrella deliberately owns the broader legacy
surface (`Core.Base`, `Console`, `Uri`, and `TimeZone`).  No CMake ownership or
visibility defect was found in this declaration.

## Testability

The module compiles into `SharpRuntimeTests_Core_Base`; the DateTime-focused
filter ran 127 tests across the two reviewed unit suites.  Component-level
tests alone do not validate all integration tests that also consume Core.Base.

## Final assessment

The declaration is coherent.  Runtime findings for this component are tracked
in the mirrored DateTime and DateTimeOffset reports rather than attributed to
the CMake boundary.
