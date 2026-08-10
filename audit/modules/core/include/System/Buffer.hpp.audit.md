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

---

## SR-AUD-067 — REMEDIATED (ticket #2212, 2026-08-10, family CMS-A)

The original evidence above is retained unchanged. **Only SR-AUD-067 is closed by this
ticket**; the SR-AUD-051 extension recorded in this same report stays `confirmed` and is
closed separately by ticket #2213. **No `SR-AUD-*` identifier was created**; numbering
stays frozen at 364. Family record: `docs/CoreMemorySafetyFamilyPlan.md`.

The raw-pointer `BlockCopy` now runs
`ArgumentOutOfRangeException::ThrowIfNegative` on `srcOffset`, then `dstOffset`, then
`count`, **before any pointer arithmetic** — the same helper, the same order and the same
exception as the sibling `std::vector` overload's `requireValidBlockCopyRange`, and the
same order .NET's `Buffer.BlockCopy` uses before its internal `Memmove`. The upper-bound
limitation the header documents is unchanged: a raw pointer still carries no capacity, and
that is still the caller's responsibility.

**Premise correction — the finding is three doors, not one, and the defect class is not
the predicted one.** The audit demonstrated `count = -1` and asserted that negative offsets
"similarly form invalid pointers". Measured under AddressSanitizer on 2026-08-10
(`build-probe/2210_before.log`), all three are separately reproducible and they are **not
the same class**:

| Argument | Before | After |
|---|---|---|
| `count = -1` | **stack-buffer-overflow** in `memmove` (`Buffer.hpp:59`) | `ArgumentOutOfRangeException` `(Parameter 'count')` |
| `srcOffset = -4` | **stack-buffer-underflow** in `memmove` | `ArgumentOutOfRangeException` `(Parameter 'srcOffset')` |
| `dstOffset = -4` | **stack-buffer-overflow** in `memmove` | `ArgumentOutOfRangeException` `(Parameter 'dstOffset')` |

The audit predicted `negative-size-param`. It is not what this toolchain reports: GCC 13.3
at `-O1` with `_FORTIFY_SOURCE` inlines `__memmove_chk`, so libsanitizer's `memmove`
interceptor — the code that emits `negative-size-param` — is bypassed and ASan's ordinary
shadow check fires instead. The compiler additionally emits `-Wstringop-overflow=` and
`-Wstringop-overread` naming `Buffer.hpp:59` for all three. After-log:
`build-probe/2212_after.log`; the deliberate `heap-buffer-overflow` control still reports
in the same binary, so the absence is an absence and not a dead instrumentation.

**Closure evidence.** 8 permanent regressions in
`modules/core/tests/System/CoreMemorySafetyTests.cpp`: each of the three arguments rejected;
the exact `paramName` **and** the srcOffset → dstOffset → count order; a negative offset
rejected **even when `count == 0`** (the case the old code silently got away with, because
`memmove` of zero bytes never touched the bad pointer); `INTCS_MIN`; no destination byte
written by a rejected call; every valid copy, `count == 0`, and the overlapping
`memmove` unchanged; and catchability as `System::Exception`.
`SharpRuntimeTests_Core_Base` **5,600/5,600** (1 pre-existing skip); `BufferTests` and
`Batch13BufferTests` unchanged and green. Whole repository builds with zero errors and zero
warnings.

**Five mutations, five killed — four of them by assertion.** M1 delete the `srcOffset`
guard → 3 fail. M2 delete the `dstOffset` guard → 3 fail. M3 reorder the three guards →
the order test fails. M4 `ThrowIfNegative(count - 1)` (over-rejects a legal `count == 0`)
→ the valid-call test fails. **M0, deleting all three guards at once, is reported
separately and honestly: it kills by process abort, not by assertion**, because a negative
`count` reaches `memmove` and crashes the runner — that abort *is* the pre-fix defect, so
it is real but it is a weaker signal, which is exactly why the zero-count rejection tests
were added to give M1 and M2 an assertion-level kill.

**Source, ABI and layout consequences: none.** Three statements added to one inline static
member of a stateless class (`Buffer() = delete`, no data members). No signature, no
`noexcept` specification, no virtual function, no default argument and no mangled symbol
changed.
