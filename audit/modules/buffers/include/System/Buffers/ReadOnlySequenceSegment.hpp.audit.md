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
