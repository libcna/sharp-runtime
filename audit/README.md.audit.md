# Audit: `README.md`

## Metadata

- Audit status: AUDITED (286 lines, full read).
- Subsystem: public project overview and build guidance.
- Evidence: current CMake/scripts/CI implementation and audit validation runs.

## Purpose

Describes project scope, prerequisites, standalone and selective builds,
component architecture, platform evidence, planning data, and documentation
validation.

## Assessment

The build commands, component closures, local-network prerequisite, platform
limitations, and Doxygen baseline agree with inspected implementation.  The
README correctly distinguishes Linux full testing from narrower cross-build
evidence.

## Findings

The statement that the “complete ten-job selective matrix” passes and the
nearby CI description together overstate tracked CI coverage.  The local
script has ten entries, but the workflow has nine and excludes
`Collections.Blocking`.  This is another documentation surface for
SR-AUD-001.

## Final assessment

Strong consumer documentation with one selective-CI claim requiring alignment
with the eventual SR-AUD-001 remediation.
