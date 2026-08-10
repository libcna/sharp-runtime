# Audit: `modules/core/include/System/SpanSplitEnumerator.hpp`

## Metadata

- Audit status: AUDITED (134-line header-only implementation, fully read).
- Validation: `SpanSplitEnumeratorTests.*` passed 11/11 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-spansplit-audit-probe.cpp`, built
  with `-fsanitize=address,undefined -fno-omit-frame-pointer` and run with
  LeakSanitizer disabled only because the sandbox tracer cannot support it.

## Assessment

Single-element, any-of, and nonempty exact-sequence splitting produce the
expected normal segments, including leading/trailing/consecutive separators.
The exact-sequence constructor mishandles an empty sequence: the zero-length
match never advances `startNext_`, so a range-for loop does not terminate.

Current .NET represents this case as `EmptySequence`; `MoveNext` then treats it
as no separator, returns the complete source once, and changes its mode to
finished.  The local `findSequence` instead treats an empty `std::vector<T>` as
a match at every current position.  The probe reports three successive
successful moves with `length=0` for source `{1,2,3}` and empty sequence.

Reference: [current .NET SpanSplitEnumerator source](https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/MemoryExtensions.cs.html).

## Finding references

### SR-AUD-045 — high — an empty sequence separator produces an infinite stream of empty spans

Constructing `SpanSplitEnumerator<int>(source, std::vector<int>{}, false)`
selects `Mode::Sequence`; `findSequence` immediately succeeds with index zero
and `sepLen` is zero.  `MoveNext` consequently leaves `startNext_` unchanged
and returns `true` forever.  An ordinary range-for loop hangs/consumes
unbounded work instead of returning the complete source as one segment.  This
is a user-controlled denial-of-service/liveness failure in a public splitter.

## Required post-audit verification

Give empty sequence its own no-split mode, or special-case it before calling
`findSequence`, so the first result is the full source and the next
`MoveNext()` is false.  Add explicit and range-for bounded tests for empty
sequence with empty and nonempty sources, plus nonempty sequence at the start,
end, and adjacent locations.  Preserve the separately chosen semantics for an
empty any-of separator list and document any divergence from .NET extensions.

## Other missing assertions and diagnostics

- Tests never pass an empty exact sequence, the case that yields nontermination.
- No test inspects `Range` from `getCurrentProperty`, checks all range-for
  iterator state transitions, or invokes `GetEnumerator()` after completion.
- The class accepts a `ReadOnlySpan<T>` whose invalid negative length is now
  covered by SR-AUD-043; it has no boundary diagnostic before using that length
  in its search loops.
- The `std::vector<T>` separator API is a project adaptation, but the header
  does not define the empty-any-of semantics or distinguish it from the .NET
  char-whitespace special case.

## Final assessment

Normal splitting is functional, but the empty exact-sequence path has a
reproducible infinite-enumeration defect.  No implementation was modified
during this audit.

---

## SR-AUD-045 — REMEDIATED (ticket #2211, 2026-08-10, family CMS-D)

The original evidence above is retained unchanged. This is the only finding in this
report, and it is now closed. **No `SR-AUD-*` identifier was created**; numbering stays
frozen at 364. Family record: `docs/CoreMemorySafetyFamilyPlan.md`.

`MoveNext()` now has current .NET's `SpanSplitEnumeratorMode.EmptySequence` arm: when the
exact-sequence separator is empty it takes the "no separator found" path directly
(`sepIdx = -1`), so the whole source is yielded once and `done_` latches. `findSequence`
is deliberately **not** hardened as well — a second guard would make the first one
unobservable to a mutation test, and it is a private helper with exactly one caller.

**Measured before and after** (`build-probe/2210_probe_before.cpp`, logs
`build-probe/2210_before.log` and `build-probe/2211_after.log`), on all three reachable
shapes: explicit `MoveNext()` loop, range-`for`, and an empty source.

| Shape | Before | After |
|---|---|---|
| `MoveNext()` over `{1,2,3}` | **1000/1000 moves, every one `length=0`** | 1 move, `length=3` |
| range-`for` over `{1,2,3}` | **1000/1000** | 1 |
| empty source | **1000/1000** | 1 (one empty segment) |
| control: sequence `{2}` | 3 | 3 |
| control: **empty any-of list** | 1 | 1 |

**Premise corrections.**

1. **This finding is not sanitizer-decidable, and the batch says so rather than implying
   otherwise.** It was run under AddressSanitizer *and* UndefinedBehaviorSanitizer with
   **zero reports** before the fix. Non-termination is a liveness defect; no sanitizer in
   this repository can observe it. The evidence is the bounded iteration counter above,
   and a deliberate heap-buffer-overflow control fires in the same binary to prove the
   instrumentation was live (`san.control_heap_overflow`).
2. **The empty any-of separator list already terminated** (one move, whole source). The
   audit asked that its "separately chosen semantics" be preserved; measurement shows
   there was nothing to preserve it *from* — only the exact-sequence mode was broken — and
   the repair is scoped so it cannot touch the any-of path. A test pins that.
3. **`SpanSplitEnumerator` has no in-repo production consumer.** The only non-header
   reference in the whole repository is its own test file, so this repair cannot regress
   anything outside its own suite.

**Closure evidence.** 7 permanent regressions in
`modules/core/tests/System/CoreMemorySafetyTests.cpp` (whole source once then `false`;
`false` again after completion; the `Range` from `getCurrentProperty()`; a bounded
range-`for`; an empty source; the empty any-of control; a five-shape nonempty-sequence
regression pass covering start, end, adjacent, middle and over-long separators; and a
nontrivial element type). `SharpRuntimeTests_Core_Base` **5,593/5,593** (1 pre-existing
skip), whole repository builds with zero errors and zero warnings.

**Three mutations, three killed.** M1 delete the new branch → 5 of 7 fail, and the two
that pass are exactly the two controls. M2 `sepIdx = 0` instead of `-1` (terminates, but
splits wrongly) → the same 5 fail. M3 widen the guard to `|| separators_.empty()` → kills
the *nonempty*-sequence path, caught by the new control **and** by the pre-existing
`SpanSplitEnumeratorTests.Sequence_TwoSegments`.

**Source, ABI and layout consequences: none.** One `else if` in one inline body plus
doc-comments. No member added (the mode is derived from `sequence_.empty()`, not stored),
so `sizeof`/`alignof` are unchanged; no signature, no `noexcept` specification, no virtual
function, no default argument and no mangled symbol changed.

The three "other missing assertions" bullets above are now covered except the last: the
`std::vector<T>` separator API's empty-any-of semantics are now **documented** in the
constructor's doc-comment and pinned by a test, rather than left undefined.
