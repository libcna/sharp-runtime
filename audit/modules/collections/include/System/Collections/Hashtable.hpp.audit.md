# Audit: `modules/collections/include/System/Collections/Hashtable.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-363 — medium — Hashtable accepts null keys and returns null instead of the public Keys/Values collections

The raw-key path stringifies a null pointer as `"0"`, so `Add(nullptr, …)` succeeds rather than reporting the required null-key argument failure.  The same type advertises `IDictionary.Keys` and `Values` but returns `nullptr` for both.  The direct probe prints `null-key=accepted count=1` and `keys-null=1 values-null=1`; callers cannot safely consume the promised views.

## Missing assertions and diagnostics

- Hashtable tests omit null-key rejection and never dereference/use Keys or Values through the IDictionary contract.
- Add boundary diagnostics for null keys and a lifetime-safe view implementation or an explicit unavailable-feature result.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
