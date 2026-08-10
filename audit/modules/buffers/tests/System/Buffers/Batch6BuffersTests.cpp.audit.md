# Audit: `modules/buffers/tests/System/Buffers/Batch6BuffersTests.cpp`

## Metadata

- Audit status: AUDITED (514 lines, 63 tests in six suites, fully read).
- Validation: `SequencePositionTests.*:ArrayBufferWriterTests.*:MemoryPoolTests.*:ReadOnlySequenceTests.*:SequenceReaderTests.*:BinaryPrimitivesTests.*`
  passed 63/63 in `SharpRuntimeTests_Buffers` on 2026-07-26.
- Scope note: this is a mixed batch fixture. Its complete file-level review is
  recorded here; production implementation reports for the individual buffer
  types remain separate pending work.

## Assessment

The fixture has useful positive-path coverage across the six small buffer
surfaces. It includes meaningful regressions for `MemoryPool` default sizing,
`SequenceReader::Advance`/`Rewind` exception taxonomy, and binary primitive
short spans. Most tests remain one-value smoke checks, so they cannot yet
establish edge behavior of the broader public APIs they introduce.

## Finding references

- **SR-AUD-069:** all six `SequencePosition` tests use getters and comparison
  only; none asserts that its components are opaque/readonly. The associated
  public header permits callers to mutate both components after construction,
  contrary to the source value type.
- **SR-AUD-053 (context):** `MemoryPoolTests.MaxBufferSize_MatchesArrayMaxLength`
  locks in the current .NET `Array.MaxLength` literal, while core
  `Array::MaxLengthProperty` is separately confirmed to expose `INT32_MAX`.
  This is useful cross-surface evidence, not a duplicate buffer finding.
- **SR-AUD-071:** `MemoryPoolTests.Dispose_ClearsBuffer` expects an empty
  memory view after disposal. Current .NET requires the owner `Memory` getter
  to throw `ObjectDisposedException`; retained C++ views can then reach an
  ASan-confirmed native fault.
- **SR-AUD-072/SR-AUD-073/SR-AUD-074:** the nine ReadOnlySequence tests cover
  basic constructors, slicing, and `GetPosition` only. They omit raw-pointer
  validation and every `TryGet`/enumerator state path that now demonstrates
  null dereference, forged-position out-of-bounds access, and default-sequence
  enumeration divergence.
- **SR-AUD-075:** the 13 SequenceReader cases assert false at end but never
  inspect the supplied output afterward. Failed `TryRead` and `TryPeek` retain
  a caller's stale value where the .NET `out` contract writes default.

## Other missing assertions and diagnostics

- `SequencePosition`: no non-null unequal-pointer, default-equality,
  mutation/encapsulation, hash, or foreign-sequence-location assertion.
- `ArrayBufferWriter`: no zero-size `GetSpan`/`GetMemory`, advance-past-free
  capacity, overflowing growth, written-span/memory alias lifetime, clear-data
  semantics, or custom allocator/exception path coverage.
- `MemoryPool`: no post-dispose exception, retained-view-after-dispose,
  repeated-dispose terminal-state, independent-rent, concurrent `Shared`,
  allocation-failure, or nontrivial element-lifetime coverage. The tests do
  not establish whether returned storage is cleared before its release.
- `ReadOnlySequence`: no negative pointer length, null/nonzero pointer,
  multi-segment position validation, foreign/reversed positions, value
  ownership, `TryGet`, default versus explicit-empty enumeration, or Slice
  overflow coverage.
- `SequenceReader`: no negative `Advance`, multi-segment traversal,
  `TryReadTo`, delimiter escaping, failed-read default output, or state
  preservation after every exceptional boundary.
- `BinaryPrimitives`: vectors cover only selected signed/unsigned widths and
  basic byte orders. They omit all `TryRead`/`TryWrite` variants, float/double,
  native-width, reverse-endianness, exact-size versus oversized spans,
  destination nonmutation after failure, and every malformed view boundary
  already relevant to Span construction.

## Final assessment

The file is valuable focused evidence for later buffer implementation audits,
but its intentionally shallow batch format leaves most state, ownership,
multi-segment, and failure contracts unasserted. No test source was modified
during this audit.
