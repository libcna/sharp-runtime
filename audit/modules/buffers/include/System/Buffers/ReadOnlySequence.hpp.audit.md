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

## Post-audit remediation for SR-AUD-072 and SR-AUD-073, and design closure for SR-AUD-074 (tickets #2049, #2050, #2057, 2026-08-04)

The audit evidence above is retained unchanged and is **not** rewritten. This is an
appended record. The owning review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048); **no `SR-AUD-*` identifier was issued and numbering stays frozen at 364.**

**SR-AUD-072 — REMEDIATED (ticket #2049).** `ReadOnlySequence(const T*, intcs)` used to run
`data_(ptr, ptr + length)` from its member-initialiser list, so validation had nowhere to
live. It now initialises `data_` from a static `validatedCopy` helper that throws
`ArgumentOutOfRangeException("length")` for a negative length and
`ArgumentNullException("ptr")` for a null pointer with a positive length. `(nullptr, 0)`
stays valid — two pre-existing tests rely on it, and adding 0 to a null pointer is well
defined.

**One premise corrected.** The finding says a negative length *"performs pointer arithmetic
before any range error can be represented"*. Measured, the **observable** outcome was a
native `std::length_error: cannot create std::vector larger than max_size()` escaping a
public door, and UBSan reported nothing for that mode. The defect is real; its class is an
untranslated native exception, not an out-of-bounds access, so the closure criterion is the
declared `System::` exception rather than merely a clean ASan run.

**SR-AUD-073 — REMEDIATED (ticket #2050).** `TryGet` tested only `pos >= end_`. It now
requires `start_ <= pos <= end_` before forming any view. `pos == end_` keeps its pinned
end-of-sequence result.

**One premise corrected, and it changed the repair.** The finding frames the defect around a
*forged* position and cites SR-AUD-069's mutable `SequencePosition` as the enabler. **Forgery
is not required.** `seq.getStartProperty()` is a legitimately obtained position; held across
`seq.Slice(...)` and passed to the slice's `TryGet`, it returned a view covering elements the
slice does not contain. That is an ordinary caller mistake, so the finding *understates*
reachability, and validating the **range** — rather than the segment marker the audit
suggested — is what closes both that path and the negative-integer path together.

**Measured before and after**, from one probe source compiled twice, the `before` column's
include path shadowed by `build-probe/2048_before_include/` materialised from `git show`:

| Probe mode | Before | After |
|---|---|---|
| `trygetneg` | **ASan `heap-buffer-overflow` READ**, 4 bytes before a 12-byte region | `ArgumentOutOfRangeException` |
| `trygetslice` | **no exception** — a 3-element view for a 2-element slice, first element `10` | `ArgumentOutOfRangeException` |
| `ctornull` | **UBSan `load of null pointer of type 'const int'`** then **ASan `SEGV` on 0x0** | `ArgumentNullException` |
| `ctorneg` | native `std::length_error` | `ArgumentOutOfRangeException` |

**SR-AUD-074 — `confirmed (design-complete)`, NOT remediated (ticket #2057, blocked).**
Reproduced: a default-constructed `ReadOnlySequence<int>` enumerates **1** segment and
`getEmpty()` also enumerates 1, where .NET yields 0 and 1. Distinguishing them needs state
the type does not have: measured `sizeof(ReadOnlySequence<int>)` is **32**
(`std::vector` 24 + two `intcs` 8), fully packed with no padding, so a discriminator is a
**public object-layout change** and the enumerator's `MoveNext` result changes for one input.
`getEmpty`'s doc-comment now says so (#2061) and the behaviour is pinned by
`ReadOnlySequenceDefaultPinTests`, mutation-checked.

Closure evidence for #2049/#2050: **18 permanent regressions**;
`SharpRuntimeTests_Buffers` **554/554** at that commit, and the whole suite clean under
AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer. Source and ABI consequences:
none — no signature, virtual, `noexcept`, member or layout change;
`sizeof(ReadOnlySequence<int>)` stays 32 and its `Enumerator` 16, both now `static_assert`ed.
**The accepted input set narrows** in the direction of memory safety: calls that used to be
undefined, or to return data outside the sequence, now throw.
