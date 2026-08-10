# Audit: `modules/core/include/System/ReadOnlyMemory.hpp`

## Metadata

- Audit status: AUDITED (223-line header-only implementation, fully read).
- Validation: `ReadOnlyMemoryTest.*` passed 23/23 in `SharpRuntimeTests_Buffers`
  and the redundant `ReadOnlyMemoryTests.*` passed 7/7 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-memory-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-25.

## Assessment

The two-argument slice has the corrected unsigned range validation and normal
read-only views work.  The raw pointer/length constructor remains an unchecked
entry point for invalid span metadata, while the one-argument `Slice` performs
signed subtraction before validating user input.  Its copy paths forward to
the already-confirmed unsafe span copy implementation.

Reference: [.NET `ReadOnlyMemory<T>.Slice` contract](https://learn.microsoft.com/en-us/dotnet/api/system.readonlymemory-1.slice?view=net-10.0).

## Finding references

- **SR-AUD-043 (extended):** the public `ReadOnlyMemory(const T*, intcs)`
  constructor stores a negative length unchanged, and the vector constructor
  narrows an oversized `size_t` length to the same invalid signed state.
  `getSpanProperty`, `ToArray`, `Slice`, and `CopyTo` then consume it.  This is
  an additional public producer of the malformed length that reaches the
  ASan-confirmed `HashCode::AddBytes` over-read.
- **SR-AUD-044 (extended):** `CopyTo` and `TryCopyTo` forward to
  `ReadOnlySpan::CopyTo`/`TryCopyTo`, so the observed forward-copy corruption
  applies to read-only-memory source views as well.

### SR-AUD-049 — high — ReadOnlyMemory.Slice(start) performs signed overflow before range validation

`Slice(intcs start)` at line 140 evaluates `length_ - start` before it calls
the checked two-argument overload.  A normal three-element memory with
`start == INT_MIN` reaches signed 32-bit overflow first; UBSan reports
`runtime error: signed integer overflow: 3 - -2147483648 cannot be represented
in type 'int'`.  It subsequently happens to throw
`ArgumentOutOfRangeException`, but C++ undefined behavior has already occurred.

.NET specifies that an out-of-range `start` must simply throw
`ArgumentOutOfRangeException`.  This public input must be validated before
any signed arithmetic.

## Required post-audit verification

Validate `start` in the one-argument overload before subtraction (or use an
unsigned/subtraction-safe expression only after the bound is known).  Add
`INT_MIN`, `-1`, `Length + 1`, zero, and `Length` vectors under UBSan.  Coordinate
raw constructor negative-length and oversized-vector validation with
SR-AUD-043, then run `ToArray`, `getSpanProperty`, `CopyTo`, and `Pin` malformed
input tests under ASan/UBSan.

## Other missing assertions and diagnostics

- The tests never pass `INT_MIN` to the one-argument slice; their overflow test
  covers only `Slice(start, length)`.
- Raw pointer construction is tested only with a valid positive length; no
  negative length/null nonzero pointer boundary is asserted or documented.
- No copy test has overlapping views or a nontrivial element type, so the
  forwarding of SR-AUD-044 is invisible.
- Default/empty `Slice(0)`, `ToArray`, and `Pin` rely on null-pointer
  range/pointer operations without an explicit empty fast path or C++
  precondition.

## Final assessment

The normal read-only view is functional, but public malformed lengths propagate
unchecked and `Slice(INT_MIN)` is UBSan-confirmed undefined behavior before it
returns its intended exception.  No implementation was modified during this
audit.
