# Audit: `modules/buffers/include/System/Buffers/ReadOnlySequence.hpp`

## Metadata

- Audit status: AUDITED (311-line public header-only implementation, fully
  read).
- Validation: `ReadOnlySequenceTests.*` passed 9/9 within the complete 63/63
  `Batch6BuffersTests.cpp` focused filter.  The dedicated
  `ReadOnlySequenceEnumeratorTest.*` filter passed 4/4; together with the
  direct ArrayBufferWriter and BinaryPrimitives fixtures, their combined
  Buffers filter passed 54/54 in `SharpRuntimeTests_Buffers` on 2026-07-26.
- ASan/UBSan reproducer: `/tmp/sharp-runtimervc-readonlysequence-audit-probe.cpp`
  was built with `-fsanitize=address,undefined` and
  `build/libsharp_runtime_core.a`. Its modes produce the below evidence.
- Reference: local .NET `ReadOnlySequence.cs`, `ReadOnlySequence.Helpers.cs`,
  and default/empty/TryGet tests were reviewed.

## Assessment

The current single-vector adaptation supplies useful basic slicing, length,
and position operations, including a previously repaired overflow-safe
`GetPosition` path. But it exposes raw-memory construction and position APIs
without validating the boundaries that make a sequence safe, and it collapses
the source distinction between a default sequence and `ReadOnlySequence.Empty`.

## SR-AUD-072 — high — raw pointer ReadOnlySequence construction dereferences invalid pointer/length metadata

`ReadOnlySequence(const T* ptr, intcs length)` immediately forms
`data_(ptr, ptr + length)` with no null or signed-length validation. A public
`ReadOnlySequence<int>(nullptr, 1)` reaches the vector range copy; UBSan first
reports a null load and ASan then reports a null-read segmentation fault. A
negative length likewise performs pointer arithmetic before any range error
can be represented.

.NET does not expose an unchecked pointer/length constructor for this type;
its array and memory constructors validate their managed source/range. This
C++ adaptation needs a deterministic contract for null/zero, null/nonzero,
negative length, and caller-owned source lifetime before creating vector
iterators.

## SR-AUD-073 — high — TryGet accepts positions before the sequence start and forms out-of-bounds views

`TryGet` treats only `pos >= end_` as invalid. It never checks `pos < start_`
or whether the `SequencePosition` object component belongs to the sequence.
For a `[20, 30]` slice of `{10, 20, 30}`, a forged position `(nullptr, 0)`
returns success, a three-element memory, and value `10`: data outside the
logical slice is exposed. With `(nullptr, -1)`, it constructs a pointer one
element before the vector; accessing the returned Memory produces an
ASan-confirmed heap-buffer-overflow.

The local .NET helper rejects a position whose object is not the sequence's
current segment and uses its encoded validated indices. A single-segment C++
adaptation can still require the expected null segment marker and enforce
`start_ <= integer <= end_` before creating the memory view.

## SR-AUD-074 — medium — default ReadOnlySequence enumerates one empty segment instead of none

Current .NET intentionally distinguishes `default(ReadOnlySequence<T>)` from
`ReadOnlySequence<T>.Empty`: the former enumerator immediately returns false,
whereas `Empty` yields one empty array segment. C++ `getEmpty()` returns the
default value and the enumerator's unconditional first transition returns true
for both. The probe prints `1,0` for the default C++ value. This changes
enumeration count and prevents callers from preserving the source's default
versus explicit-empty state.

## Finding references

- **SR-AUD-069 (context):** mutable public SequencePosition components make
  forged position construction trivial. Even an immutable C++ replacement
  would still require `TryGet` to validate its logical range and provenance.

## Other missing assertions and diagnostics

- Direct tests omit null/nonzero and negative raw-pointer construction,
  source lifetime, zero-length null policy, and nontrivial element copying.
- No test calls `TryGet`, tests `advance=false`/`true`, preserves a position at
  end, or rejects a before-start/foreign/reversed position.  The dedicated
  enumerator fixture calls `MoveNext` on the default state but discards its
  return value, so it still does not establish default-versus-explicit-empty
  enumeration behavior.
- Multi-segment constructors, `FirstSpan`, `TryCopyTo`, equality/hash,
  memory-manager/string backing, and position/segment provenance are absent
  from this single-vector subset and need an explicit unsupported-surface
  decision rather than an implicit API-shaped stub.
- Slices copy the full backing vector and retain raw views into their own copy;
  no test establishes allocation cost, aliasing, or view lifetime after a
  sequence is copied/moved/destroyed.

## Final assessment

Happy-path single-segment operations pass, but raw construction and position
validation expose caller-controlled metadata to ASan-confirmed invalid memory
access. The default/empty enumeration state also differs observably from .NET.
No source or test was modified during this audit.
