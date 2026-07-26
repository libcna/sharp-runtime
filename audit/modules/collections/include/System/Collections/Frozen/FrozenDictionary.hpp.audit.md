# Audit: `modules/collections/include/System/Collections/Frozen/FrozenDictionary.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-362 — medium — FrozenDictionary Create silently overwrites duplicate keys

`Create` assigns each pair through `unordered_map::operator[]` and documents last-value-wins behavior.  The direct probe with `{7,10}, {7,20}` prints `duplicate-create=accepted count=1 value=20`.  The corresponding .NET FrozenDictionary factory/extension rejects duplicate keys rather than choosing an input-order-dependent value.

## Missing assertions and diagnostics

- Frozen tests do not require duplicate-key rejection for Create, CreateFromMap conversion routes, or ToFrozenDictionary.
- Include the duplicate key and source position in the thrown diagnostic.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
