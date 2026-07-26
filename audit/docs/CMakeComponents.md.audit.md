# Audit: `docs/CMakeComponents.md`

## Metadata

- Audit status: AUDITED (236 lines, full read).
- Subsystem: component architecture and consumer guidance.
- Evidence: component CMake files, generated catalogue, validator, local
  selective script, and GitHub Actions workflow.

## Purpose

Documents physical ownership, dependency visibility, selective configurations,
compatibility umbrellas, and the architecture-maintainer validation flow.

## Assessment

The explanation of public/private/test-only dependencies, Core/Collections
umbrellas, Text.Json closure, and vendor isolation agrees with the inspected
CMake metadata.  The generated catalogue and validator passed at the audit
snapshot.

## Findings

The statement that `.github/workflows/components.yml` “runs the ten selective
configurations” (lines 151–153) is false: the workflow lists nine and omits
`Collections.Blocking`.  This is the documentation side of SR-AUD-001; the
local script does have ten entries.

## Final assessment

Strong architecture documentation with one CI-coverage claim that must be
corrected together with the workflow remediation for SR-AUD-001.
