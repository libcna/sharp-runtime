# Audit: `modules/io/include/System/IO/PathTooLongException.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

Path normalization, root handling, extension rules, temporary-file creation, and platform separator handling were reviewed.  No additional evidence-backed defect is assigned to this individual file beyond any cross-file findings indexed from their owning implementation reports.

## Missing assertions and diagnostics

- Add embedded-NUL, permission, concurrent temp creation, drive/UNC, symlink, and invalid-byte assertions.
- Preserve exact System exception types/messages and observable state, rather than only asserting no-throw or a final happy-path value.

## Final assessment

AUDITED. No standalone confirmed defect was added from this file in the current audit pass.

## Post-audit remediation — ticket #1875 (2026-08-01)

Current .NET assigns `COR_E_PATHTOOLONG` (`0x800700CE`) in every constructor;
the port previously inherited `COR_E_IO` (`0x80131620`). All three represented
constructors now assign the exact reference value and are permanently pinned.
No public declaration, message, layout, vtable or audit identifier changed.
