# Audit: `cmake/SharpRuntimeComponents.cmake`

## Metadata

- Audit status: AUDITED (694 lines, full read).
- Subsystem: component registration, dependency closure, and test-target
  construction.
- Evidence: the CMake implementation, root configuration, boundary validator,
  generated catalogue check, and successful configure/build portion of the
  initial local gate.

## Purpose

Validates component metadata, records it in global properties, constructs the
requested target closure with public/private visibility, provides compatibility
aliases/umbrellas, and creates component-specific GoogleTest binaries.

## Assessment

The implementation rejects duplicate registrations, invalid/static-empty and
interface-with-source definitions, mixed legacy/explicit dependency syntax,
and recursive enablement.  It resolves dependencies before creating targets,
so public usage requirements are populated consistently.  Test-only
dependencies are enabled and linked only for their owning test executable.
The module-boundary Python validator independently checks the declarations
against real include usage, reducing the risk that this metadata-only layer
drifts silently.

The generic registration API has no present misuse in the registered module
set.  In particular, the architecture's narrow component targets are created
before compatibility umbrellas rather than making internal code depend on an
umbrella.

## Findings

None confirmed in this file.

## Final assessment

The current component graph is internally coherent.  The separate CI-matrix
coverage gap is recorded against `.github/workflows/components.yml`, not this
target-construction implementation.
