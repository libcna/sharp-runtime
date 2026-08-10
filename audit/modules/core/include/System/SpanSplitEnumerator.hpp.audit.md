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
