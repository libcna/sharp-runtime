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

---

## Design-complete note (ticket #1782, 2026-07-27)

*The original evidence above is preserved verbatim and unaltered. SR-AUD-361
remains `confirmed`; it is **not** `remediated` — no production code changed.*

Design ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M,
design-only) completed the full contract analysis for this finding on local
branch `feature/remediation-coll-sortedset-view-design`. The selected
architecture is recorded in `docs/SortedSetLiveViewDesign.md`:
`SortedSet<T>` holds `std::shared_ptr<State>` (the `State` owning the
`std::set<T>` and the single version counter) plus `std::optional<T>` lower and
upper bounds, so one public type is either an owning full set or a bounded live
view. `GetViewBetween` keeps returning `SortedSet<T>` by value; the returned
object becomes a write-through handle onto the parent's state.

A repository-local, gitignored `build-probe-sortedset/` probe tree independently
re-reproduced this finding's own symptom (`source-add-visible-in-view=0`,
`view-add-visible-in-source=0`) and established the complete pre-fix contract
under ASan+UBSan+LeakSanitizer with no diagnostic — the current implementation
is memory-safe and semantically wrong. A working prototype of the selected
architecture passes the identical scenario matrix with `failures=0`, including
owner destruction with surviving views and a 100,000-element scale case.

Four adjacent defects inside this member's surface were measured during the
design and are folded into the implementation ticket's scope rather than
receiving new `SR-AUD-*` identifiers (the audit numbering is frozen at 364):

1. `GetViewBetween` is the only member of the class that spells its comparisons
   with `operator>`, so an element type providing `operator<` alone — exactly
   the contract this header's own class doc-comment states — fails to compile
   at `SortedSet.hpp:297` and `:300`.
2. The returned object does not enforce its bounds after construction:
   `view.Add(99)` on a `[3,7]` range succeeds and moves the object's `Max` to
   99.
3. A nested `GetViewBetween` may silently **widen** either bound, where .NET
   throws `ArgumentOutOfRangeException`.
4. Whole-object assignment defeats this class's advertised fail-fast version
   guard, because `version_` is a plain member that assignment overwrites
   instead of bumping: copy-assignment yields a silently wrong dereference with
   no diagnostic, and move-assignment is an ASan-confirmed
   `heap-use-after-free`.

The invalid-range exception **message** also diverges (`lowerValue is greater
than upperValue.` versus .NET's `Must be less than or equal to upperValue.`);
the exception type and parameter name already match.

**Correction to an earlier planning claim.** `NEXT.md`, `plan.md`, and this
header's own `@warning KNOWN DIVERGENCE` block each state that a live view is
"not achievable on top of `std::set` … without replacing this type's entire
internal representation with a hand-rolled tree structure matching .NET's own".
That premise does not hold: `std::set` already provides `lower_bound`,
`upper_bound`, and stable iterators, so a bounded view needs only a shared owner
for the container plus a pair of bounds. The prototype demonstrates it. The
real cost is the ownership model, the copy/move semantics, and one required
`const` removal — which is why this stayed a design-first item.

Implementation is proposed as separate ticket **#1783**
(`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L), created **`blocked`** pending
explicit user approval of the `const` removal on `GetViewBetween`, the semantic
snapshot-to-live-view change, and the `SortedSet<T>` object-layout change — the
same approval category tickets #1770/#1771 and #1779/#1780 required.
