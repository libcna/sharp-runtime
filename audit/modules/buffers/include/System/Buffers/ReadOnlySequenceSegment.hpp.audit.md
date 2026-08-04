# Audit: `modules/buffers/include/System/Buffers/ReadOnlySequenceSegment.hpp`

## Metadata

- Audit status: AUDITED (62-line public header-only declaration, fully read).
- Validation: `ReadOnlySequenceSegmentTests.*` passed 4/4 within the focused
  11/11 Batch17 subset on 2026-07-26.
- Reference: local .NET `System/Buffers/ReadOnlySequenceSegment.cs` and
  `ReadOnlySequence<T>` constructors were reviewed.

## Assessment

The protected memory/next/running-index fields and read-only public accessors
mirror the basic .NET segment node shape.  However, the only C++
`ReadOnlySequence<T>` constructors accept a vector or raw pointer/length;
there is no segment-chain constructor or any other use of this type in the
repository.  The header's principal public claim is therefore not implementable
by a consumer.

## SR-AUD-087 — medium — ReadOnlySequenceSegment cannot construct the advertised multi-segment sequence

The class documentation says a `ReadOnlySequence<T>` can be constructed from a
chain of these segments.  Repository search finds `ReadOnlySequenceSegment<T>`
only in this header and its four node-local tests.  `ReadOnlySequence.hpp`
provides only default, vector, and raw pointer/length construction; it neither
includes nor accepts a segment node.  Its previously audited implementation is
also vector-backed and has no multi-segment state.

Current .NET uses this protected-mutable node specifically with public
`ReadOnlySequence<T>` segment constructors to represent noncontiguous buffers.
The C++ node can be linked but cannot form the sequence described in its public
documentation.  The focused fixture asserts fields after linking but never
attempts the missing consumer operation, so all four tests pass.

## Other missing assertions and diagnostics

- No test constructs a two-/three-node sequence, verifies segment boundaries,
  sliced positions, enumeration, `TryGet`, running-index monotonicity, or
  lifetime/ownership of linked nodes.
- Accessors expose mutable raw next pointers through a const segment.  Current
  .NET's protected setter model has the same builder pattern, but native
  dangling/cycle/null invalidation needs a documented ownership decision and
  sanitizer-backed consumer tests.
- RunningIndex uses `long long` rather than project `longcs`; it is compatible
  on the present target but has no width assertion or ABI policy.

## Final assessment

The standalone node shape exists, but its advertised reason for existing — a
multi-segment ReadOnlySequence — is unavailable.  No production or test source
was modified during this audit.

## Design closure for SR-AUD-087 (ticket #2058, 2026-08-04): DESIGN-COMPLETE — NOT REMEDIATED

The audit evidence above is retained unchanged. SR-AUD-087 stays **open**, now marked
`confirmed (design-complete)`. The owning review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048) §4.8; **no `SR-AUD-*` identifier was issued.**

Re-verified: `ReadOnlySequenceSegment<T>` appears **only** in this header and its four
node-local tests. `ReadOnlySequence<T>` has exactly three constructors — default,
`std::vector` and raw pointer/length — none of which accepts a segment, and its
`getIsSingleSegmentProperty()` is a hard-coded `true`.

Implementing the segment-chain constructor is **blocked ticket #2058**: it needs new public
members on `ReadOnlySequence<T>` (an object-layout change, measured 32 bytes today) plus
multi-segment rewrites of `First`, all six `Slice` overloads, both `GetPosition` overloads,
`TryGet`, `ToArray`, `CopyTo` and the enumerator, and of `SequenceReader<T>`'s single-segment
`First()` snapshot. It **supersedes #2057** (SR-AUD-074): a real multi-segment representation
carries the default-versus-`Empty` discriminator for free, so the two must not be implemented
in the other order.

**What did land (#2061, doc-only, zero executable change):** the class comment no longer
claims a sequence can be built from a chain of these segments; it says the opposite, names
#2058, and explains why the node shape is retained (ported C# declaring segment types still
compiles). The limitation is pinned two ways by
`ReadOnlySequenceSegmentPinTests` — a runtime test that builds and inspects a two-node chain,
and two `static_assert`s over `std::is_constructible_v` that fire at **compile time** the
moment such a constructor appears. Mutation-checked: adding a four-argument segment
constructor fired the `static_assert` with the message *"a segment-chain constructor now
exists -- #2058 has landed"*.
