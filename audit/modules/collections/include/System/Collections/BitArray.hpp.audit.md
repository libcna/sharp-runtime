# Audit: `modules/collections/include/System/Collections/BitArray.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-364 — medium — BitArray enumerator exposes Current outside its valid lifecycle and ignores mutation

At audit time, the enumerator stored only an integer cursor and a cached bool.  Before the first successful `MoveNext()` or after exhaustion, `getCurrentProperty()` returned a pointer to the cache instead of throwing; no version state detected Set, SetAll, length, or bitwise mutation during iteration.  This violated the standard enumerator state and fail-fast contract even when it did not immediately cross a native buffer boundary.

## Missing assertions and diagnostics

- BitArray tests do not check Current before start/after end or mutation after enumerator creation.
- Record a version and expose an InvalidOperationException diagnostic for invalid state or mutation.

## Remediation

**REMEDIATED by ticket #1767 on 2026-07-27.** `BitArray` now uses the shared
lifecycle state for `Current`, captures a defined-width version, and increments
it after Set, SetAll, Length, all bitwise operations, Not, and both shifts.
`MoveNext` and `Reset` reject mutation with `InvalidOperationException`.
Permanent tests cover every mutator plus normal/before/after/Reset behavior;
the focused 13/13 suite, 1,435/1,435 Collections.Core target, direct
ASan/UBSan probe, and network-permitted 12,694-test gate pass.

## Final assessment

AUDITED. SR-AUD-364 was confirmed with reproducible evidence and is now
REMEDIATED; the original evidence is retained above.

## Post-remediation follow-up: ticket #1787 (2026-07-28)

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) repaired
the mutation counter this file's fail-fast enumerator snapshots. It carries **no
`SR-AUD-*` identifier** — the numbering is frozen at 364 and the pattern was
found during remediation, by ticket #1786's own inventory — and it reopens no
finding here. The original evidence above is retained unchanged.

The counter is now `System::Collections::detail::NarrowMutationCounter` — still 32 bits,
deliberately. `BitArray` was **missing from ticket #1786's inventory** altogether, and
it is also the one collection whose counter was already `std::uint32_t` rather than
`intcs`, so it never had the signed-overflow undefined behaviour the other fourteen
did. It did have the assignment defect, which is fixed. Closing the remaining 2^32
snapshot-reuse horizon needs the **public** `Enumerator`'s snapshot widened too, and
that grows `sizeof(BitArray::Enumerator)` from 32 to 40 bytes — nine bytes are needed
after an 8-byte snapshot where eight are available, in any member order. Widening only
the container would be *wrong* rather than partial, since the snapshot would become a
truncation of the counter. **Blocked ticket #1789** holds both, pending approval.

Repository-wide, three defect classes were reproduced against the committed
pre-fix headers before anything changed
(gitignored `build-probe-collversion/probe2_defects.cpp`): fourteen UBSan
signed-integer-overflow reports at `++version_`, fifteen 2^32
snapshot-reuse (ABA) reproductions, and — recorded in neither #1786's nor
#1787's description — an assignment defect that transplanted the *source's*
counter into the destination and needed no overflow at all, with six
AddressSanitizer `heap-use-after-free`/`heap-buffer-overflow` reproductions. The
full record, including the .NET comparison and the per-type layout measurements,
is `docs/CollectionVersionCounterSweep.md`.
