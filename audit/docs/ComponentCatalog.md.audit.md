# Audit: `docs/ComponentCatalog.md`

## Metadata

- Audit status: AUDITED (83 lines, generated file).
- Subsystem: derived component catalogue.
- Evidence: generator source and successful `--check` in the local gate.

## Purpose

Publishes physical-component owners, dependency visibility, external
dependencies, representative headers, and compatibility targets generated
from CMake declarations.

## Assessment

The recorded 41 modules and 90 production edges match the validator's direct
audit result.  The `Collections.Blocking` public closure and the compatibility
umbrella are represented correctly.  The generated-file banner, source
generator, and CI/local freshness check make manual drift unlikely.

## Findings

None.

## Final assessment

Current, authoritative generated documentation at this snapshot.
