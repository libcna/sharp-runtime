# Audit: `scripts/check_selective_components.sh`

## Metadata

- Audit status: AUDITED (129 lines, full read).
- Subsystem: selective component/consumer isolation validation.
- Evidence: shell implementation, consumer fixtures, and the workflow matrix.

## Purpose

Configures each selected component in a temporary directory, builds its tests
and a direct consumer, validates target absence for Text.Json, and compiles
negative include-leakage fixtures expecting failure.

## Assessment

The local matrix contains ten components, including the direct
`Collections.Blocking` fixture.  Temporary paths come from `mktemp -d` and
are removed only through quoted, scoped paths.  The negative fixtures validate
the intended failure mode rather than accepting every compilation failure.
The Text.Json closure check covers both accidental target configuration and
private/sibling header leakage.

## Cross-file observation

This script's matrix is stronger than the tracked workflow: the workflow
omits its `Collections.Blocking` entry.  See SR-AUD-001 in
`../.github/workflows/components.yml.audit.md`.

## Findings

None in this script itself.

## Final assessment

The local selective gate implements the documented ten-fixture check; CI
integration, not the script, is incomplete.
