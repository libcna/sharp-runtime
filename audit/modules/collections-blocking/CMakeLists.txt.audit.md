# Audit: `modules/collections-blocking/CMakeLists.txt`

## Metadata

- Audit status: AUDITED (9 lines, full read).
- Subsystem: `Collections.Blocking` component declaration.
- Evidence: declaration, direct selective fixture, and boundary validator.

## Purpose

Registers the header-only component that owns `BlockingCollection<T>` and
declares its public `Collections.Core`, `Core.Base`, and `Threading`
dependencies.

## Assessment

The declaration is intentionally narrow and matches every public include in
`BlockingCollection.hpp`.  The audit ran
`scripts/check_selective_components.sh Collections.Blocking
blocking_collection.cpp`: its eight component tests and the direct consumer
both passed.  The missing GitHub Actions coverage is external to this module
declaration (SR-AUD-001).

## Findings

None in this file.

## Final assessment

Correct physical ownership and dependency visibility.
