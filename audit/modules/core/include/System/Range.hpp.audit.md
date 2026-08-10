# Audit: `modules/core/include/System/Range.hpp`

## Metadata

- Audit status: AUDITED (138-line public header, fully read).
- Supporting validation: dedicated `RangeTest.*` passed 16/16 and the
  complementary `RangeTests.*` smoke cases in pending
  `SystemTypesRemainingTests.cpp` passed 4/4 on 2026-07-26.
- Reproducer:
  `/tmp/sharp-runtimervc-index-range-audit-probe.cpp`, compiled with
  `-fsanitize=undefined`, reported the arithmetic at `Range.hpp:99`.

## Assessment

The normal range construction, half-open resolution, equality, and ordinary
out-of-range diagnostics closely follow the local .NET source.  `GetOffsetAndLength`
intentionally relies on `Index::GetOffset` and then uses unsigned comparisons,
matching .NET's decision not to prevalidate a negative `length`.  This makes
the port particularly sensitive to the difference between C# unchecked integer
arithmetic and undefined signed C++ arithmetic.

## Finding references

- **SR-AUD-057:** with `Range(Index::FromEnd(INT_MAX), Index::End())` and
  `length == INT_MIN`, the inherited index calculation is already undefined;
  after checks, `end - start` at `Range.hpp:99` independently overflows.  UBSan
  reports both sites and the probe prints the wrapped-looking result `1,2147483647`.

## Other missing assertions and diagnostics

- Tests cover ordinary from-end ranges, order, and bounds but no negative
  length or extrema arithmetic under a sanitizer.
- The tests should distinguish an intended .NET unchecked compatibility result
  from a C++ overflow diagnostic; simply adding an argument error would alter
  the documented .NET no-prevalidation behavior.
- Hash checks establish equality consistency only; they correctly avoid
  assuming different ranges must have different hashes.

## Final assessment

Normal range resolution is well covered, but its inherited unchecked arithmetic
is a confirmed high-severity C++ safety/parity defect.  No source or test was
modified during this audit.
