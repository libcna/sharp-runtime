# Audit: `modules/collections/include/System/Collections/Generic/SortedSet.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-361 — medium — GetViewBetween returns a detached snapshot rather than the required live bounded view

The implementation returns a separate `SortedSet` copy and explicitly documents the divergence.  The direct probe reports `view-add-visible-in-source=0` and `source-add-visible-in-view=0`: mutations do not flow in either direction.  .NET returns a range-enforced, write-through live view, so callers can silently mutate the wrong object.

## Missing assertions and diagnostics

- Tests exercise range membership but not bidirectional write-through, live updates, or out-of-range view mutation.
- A future implementation needs view-bound diagnostics for source/view updates and violations.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
