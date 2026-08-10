# Audit: `modules/collections/include/System/Collections/Generic/KeyNotFoundException.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## Assessment

The public declaration, its immediate implementation path, and focused call sites were reviewed.  No separate evidence-backed finding is assigned to this file beyond the related findings recorded in their owning reports.

## Missing assertions and diagnostics

- Keep invalid-input, lifecycle, ownership, and native-boundary diagnostics covered by focused tests as this surface evolves.

## Final assessment

AUDITED. No separate evidence-backed finding is assigned to this file.

## Post-audit remediation — ticket #1875 (2026-08-01)

Current .NET assigns `COR_E_KEYNOTFOUND` (`0x80131577`) in every constructor.
The port previously inherited `COR_E_SYSTEM`; all three represented
constructors now assign the reference value and are pinned by the permanent
integration matrix. No new audit identifier was issued; declarations, layout,
vtables and messages are unchanged.
