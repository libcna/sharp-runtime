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

## Correction (recorded 2026-07-27, discovered while selecting ticket #1778; reconciled under ticket #1779)

This finding's premise does not hold against the current .NET reference. Ticket
#1778 checked `FrozenDictionary::Create` against
`/rv/tmp/runtime/src/libraries/System.Collections.Immutable/src/System/Collections/Frozen/FrozenDictionary.cs`
while choosing between this finding and SR-AUD-360 as the next candidate:
.NET's own doc-comment on `Create`/`ToFrozenDictionary` states that
last-value-wins is the *intended* behavior for duplicate keys, explicitly
contrasted with `Enumerable.ToDictionary`'s throw-on-duplicate behavior ("If
the same key appears multiple times in the input, the latter one in the
sequence takes precedence. This differs from `Enumerable.ToDictionary`, with
which multiple duplicate keys will result in an exception."), and
`GetExistingFrozenOrNewDictionary`/`CreateFromDictionary` deliberately use the
indexer rather than `Add` "to avoid throwing and to overwrite existing entries
such that last one wins." sharp-runtime's current `FrozenDictionary::Create`
already implements exactly this — it is not a defect, it is parity.

This correction is recorded here, alongside the original evidence above rather
than in place of it, per this repository's practice of preserving historical
audit narrative (see `NEXT.md`'s and `plan.md`'s equivalent corrections for
tickets #1776 and #1778). The repository's findings-index status vocabulary
currently supports only `confirmed`/`remediated` (see
`audit/AUDIT_FINDINGS_INDEX.md`); no `not-a-defect`/`false-positive` status
exists to reclassify this row into, so SR-AUD-362 is deliberately left
`confirmed` in the index rather than invented a new status ad hoc. It must not
be treated as an active, un-investigated defect, and must not be counted as
`remediated` — no code changed. Full detail:
`audit/AUDIT_FINAL_REPORT.md`'s "Planning-accuracy note" under ticket #1778,
and ticket #1778's own `plan.sqlite3` row.

## Final assessment

AUDITED. The confirmed finding above (§ "FrozenDictionary Create silently
overwrites duplicate keys") does not survive comparison against the current
.NET reference; see the Correction above for why it is left `confirmed`
rather than acted on further, and why it is not reopened as a fix target.
