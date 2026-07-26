# Audit: `modules/core/include/System/Buffer.hpp`

## Metadata

- Audit status: AUDITED (253-line public header-only implementation, fully
  read).
- Validation: `BufferTests.*` passed 38/38 in `SharpRuntimeTests_Core_Base` on
  2026-07-26; the direct generic-vector/unsigned-MemoryCopy filter passed
  10/10 on 2026-07-27 and is fully reviewed in
  `Batch13BufferTests.cpp.audit.md`.
- ASan reproducer: `/tmp/sharp-runtimervc-buffer-audit-probe.cpp`, compiled
  with `-fsanitize=address`, reports the raw negative-size and nontrivial
  vector failures below.

## Assessment

The checked vector overloads use correct unsigned range arithmetic, use
`memmove` for overlap, and have meaningful regression coverage. `GetByte` /
`SetByte` correctly reject signed indexes through unsigned comparison, and both
MemoryCopy overloads check destination capacity. The raw BlockCopy overload and
the nominally primitive typed vector templates bypass key public safety
boundaries.

## SR-AUD-067 — high — raw Buffer::BlockCopy converts negative metadata into an unbounded memmove

The raw-pointer `BlockCopy` performs pointer arithmetic on `srcOffset` /
`dstOffset` and casts signed `count` directly to `size_t`, without even the
negative checks that are independent of unavailable pointer lengths. The ASan
reproducer calls `BlockCopy(src, 0, dst, 0, -1)` and reports
`negative-size-param` at `memmove`. Negative offsets similarly form invalid
pointers before the raw operation.

The header's raw-pointer note correctly says capacity cannot be discovered,
but that does not justify accepting invalid signed metadata. .NET's Array API
checks all three inputs before its internal memmove. This C++ adaptation must
at minimum reject negative offsets/count deterministically; it should also
make the unchecked raw-pointer capacity contract impossible to mistake for the
checked vector overload.

## Finding references

- **SR-AUD-051 (extended):** `BlockCopy(const std::vector<T>&, ...)`,
  `ByteLength`, `GetByte`, and `SetByte` state that T must be primitive or
  trivially copyable but impose no constraint. The ASan reproducer passes
  `std::vector<std::string>` to typed BlockCopy and reports a double-free at
  vector destruction after raw object-representation copying. This is the same
  unsafe byte-copy/nontrivial-lifetime pattern confirmed for `Array::Copy`.

## Other missing assertions and diagnostics

- No raw-pointer test supplies negative offset/count, null pointer with
  nonzero count, insufficient storage, or invalid aliasing alignment.
- No test ensures nontrivial typed vectors are rejected at compile/public API
  boundary, nor covers enum/bool/custom trivially-copyable element semantics.
- `array.size() * sizeof(T)` is narrowed to `intcs` without an allocation-size
  diagnostic; practical vectors are smaller today, but huge-vector behavior is
  not established.
- Byte order is host-native by design; the only test's compound reconstruction
  assumes little-endian layout despite its comment saying it avoids such a
  dependency.

## Final assessment

Checked primitive-vector paths are materially improved, but the raw public
overload remains ASan-unsafe for negative metadata and generic vector templates
permit nontrivial lifetime corruption. No source or test was modified during
this audit.
