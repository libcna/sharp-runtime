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

The counter became `System::Collections::detail::NarrowMutationCounter` — still 32 bits,
deliberately. `BitArray` was **missing from ticket #1786's inventory** altogether, and
it is also the one collection whose counter was already `std::uint32_t` rather than
`intcs`, so it never had the signed-overflow undefined behaviour the other fourteen
did. It did have the assignment defect, which is fixed. Closing the remaining 2^32
snapshot-reuse horizon needs the **public** `Enumerator`'s snapshot widened too, and
that grows `sizeof(BitArray::Enumerator)` from 32 to 40 bytes — nine bytes are needed
after an 8-byte snapshot where eight are available, in any member order. Widening only
the container would be *wrong* rather than partial, since the snapshot would become a
truncation of the counter. **Blocked ticket #1789** holds both, pending approval.

## Post-remediation follow-up: ticket #1789 (2026-07-29)

Ticket #1789 (`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, size XS) received that
approval and closed the residual the previous paragraph describes. It carries **no
`SR-AUD-*` identifier** — the numbering stays frozen at 364 — and it **reopens no
finding**: SR-AUD-364 stays `remediated`, ticket #1767's enumerator lifecycle guard
and ticket #1793's owning `Current` are untouched, and every one of the 21
`BitArrayTests.cpp` cases plus the `Batch18`/`Batch18b` gap-fills passes unmodified.

`BitArray::version_` moved from `NarrowMutationCounter` to the 64-bit
`detail::MutationCounter`, and `BitArray::Enumerator::version_` from
`NarrowMutationVersion` to `MutationVersion`, **in one change** — the truncation
hazard above is exactly why they could not be separated. The nine increment sites
(`Set`, `SetAll`, `Not`, `And`, `Or`, `Xor`, `LeftShift`, `RightShift`,
`setLengthProperty`) and the three read/compare sites are unchanged in spelling; the
production diff is two field declarations plus documentation.

Reproduced before any production change (`build-probe/1789_prefix_defects.log`):
positioning the counter 2^32 forward truncated it back onto an outstanding
enumerator's snapshot and the guard stopped firing — `guard-fired=0` for `MoveNext`,
for `Reset`, and at seven laps — `defects-observed=3`. The identical source post-fix
reads `defects-observed=0`, and the whole diff of the two logs is the counter width,
those three outcomes, and one sentinel probe reaching a larger maximum; **every
mutation-delta line is byte-identical**. UBSan reported **0** runtime errors on both
sides, confirming the pre-existing unsigned representation had already ruled out the
undefined behaviour.

Measured: `sizeof(BitArray::Enumerator)` **32 → 40** (`arr_` keeps offset 8; the
snapshot at 16 widens and `index_`/`current_`/`state_` each move by 8);
`sizeof(BitArray)` **unchanged at 48**, because the wider counter landed in the four
bytes of tail padding the container already had; `alignof` 8 on both; **0 `BitArray`
symbols added, removed or renamed** (64 on each side, byte-identical name lists). No
public signature, return type, or `const` qualification changed, and every
in-repository caller compiles unmodified.

The break is therefore **binary-only and silent**. `Enumerator` is a public nested
class, so a consumer may store one by value; an object file compiled against the old
header links against a new one with **no diagnostic in any of eight tested
configurations**, and then either corrupts the member following an embedded
enumerator with **no AddressSanitizer report at all** (the bytes are inside the same
allocation), or — at `-O2` — reports **zero elements for a non-empty array**. A
complete consumer rebuild is mandatory. Full record:
`docs/CollectionVersionCounterSweep.md` §20.

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
