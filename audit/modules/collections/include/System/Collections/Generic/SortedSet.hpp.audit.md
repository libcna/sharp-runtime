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

---

## Remediation note (ticket #1783, 2026-07-28)

*The original evidence above, and the design-complete note of ticket #1782, are
preserved verbatim and unaltered. SR-AUD-361 moves from `confirmed
(design-complete)` to **`remediated`**: the production header changed under this
ticket.*

Implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L)
landed the architecture #1782 selected, on local branch
`feature/remediation-coll-sortedset-live-view`, after the user granted the exact
approval design section 28 required — removing the `const` qualifier from
`GetViewBetween`, the snapshot-to-live-view semantic change, and the
`SortedSet<T>` object-layout change.

`SortedSet<T>` now holds `std::shared_ptr<State>` (the `State` owning the
`std::set<T>` and the single version counter) plus `std::optional<T>` lower and
upper bounds, so one public type is either an owning full set or a bounded live
view. `GetViewBetween` still returns `SortedSet<T>` by value, but the returned
object is a handle onto the same tree, not a copy of its elements.

The finding's own symptom is inverted where it was measured: the post-fix probe
reports `source-add-visible-in-view=1`, `source-remove-visible-in-view=1`,
`view-add-visible-in-source=1`, and `view-remove-visible-in-source=1`, where the
original evidence recorded 0 in both directions. The two "missing assertions and
diagnostics" items are closed as well: bidirectional write-through, live
updates, and out-of-range view mutation are covered by 47 permanent regressions
in `modules/collections/tests/System/Collections/Generic/SortedSetLiveViewTests.cpp`,
and `getIsViewProperty()` plus `IsWithinRange()` are the view-bound diagnostics
the note asked for.

The four adjacent defects recorded in the #1782 note are fixed as a consequence
of the design rather than as separate work:

1. ordering is taken from `std::set::key_comp()` by value, so an element type
   with `operator<` alone now instantiates `GetViewBetween` — probe 3 with
   `-DSORTEDSET_PROBE_INSTANTIATE_VIEW`, which previously failed with two
   `no match for 'operator>'` errors, now compiles `-Werror` and runs;
2. bounds are enforced for the whole life of the view;
3. a nested `GetViewBetween` may only narrow, throwing
   `ArgumentOutOfRangeException("lowerValue"/"upperValue")` on a widening bound;
4. assignment rebinds the handle instead of overwriting the version counter, and
   iterators hold their own strong reference to the state they enumerate — the
   pre-fix `copy-assign` silently wrong dereference now yields the correct
   pre-assignment element, and the `move-assign` **ASan-confirmed
   `heap-use-after-free`** and the `outlive` **ASan `stack-use-after-scope`** are
   both gone, exit 0 with no diagnostic.

The invalid-range message now matches .NET exactly: `Must be less than or equal
to upperValue. (Parameter 'lowerValue')`.

Two things this remediation deliberately did **not** do, both recorded in
`docs/SortedSetLiveViewDesign.md` section 30: it follows the design's
exception-*ordering* rule for a nested call that is simultaneously inverted and
widening, which differs from .NET's incidental order; and it does not add thread
safety — a ThreadSanitizer probe found that concurrent `getCountProperty()` on
*one* view object races on the lazy Count cache that mirrors .NET's
`TreeSubSet._countVersion`, which is documented in the header rather than
synchronized, since the type claims no thread-safety guarantee.

Closure gates: `SharpRuntimeTests_Collections_Core` 1,783/1,783 (was 1,736, all
41 pre-existing SortedSet cases passing with no assertion edited);
`scripts/local_ci_check.sh build` 13,069 tests across 37 executables with zero
warnings and zero errors; 41 modules / 90 edges with no new dependency edge;
validator tests, catalogue check, database consistency, and `git diff --check`
all clean; Doxygen 1,937 warnings against the 1,942 ceiling; all ten selective
components pass with a repository-local `TMPDIR`; the positive standalone
consumer fixture compiles `-Werror` against only `Collections.Core` and exits 0;
and the negative fixture proving a `const` caller no longer compiles is
correctly rejected.
