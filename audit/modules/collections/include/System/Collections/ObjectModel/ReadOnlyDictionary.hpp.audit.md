# Audit: `modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-359 — medium — ReadOnlyDictionary::Empty returns a mutable singleton reference

`Empty()` returns a non-const reference to a process-static wrapper, so default copy assignment can rebind its private shared backing map.  The direct probe prints `empty-before=0`, assigns a one-entry read-only wrapper into `Empty()`, then prints `empty-after-assignment=1`.  The globally published empty instance is therefore mutable and process-contaminable despite exposing no collection mutator.

## Missing assertions and diagnostics

- Tests do not verify Empty remains empty after copies, assignments, or access from another consumer.
- Return an immutable value/const reference or explicitly suppress assignment; log accidental mutation attempts in debug builds.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
