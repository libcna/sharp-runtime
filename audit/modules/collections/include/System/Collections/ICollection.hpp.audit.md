# Audit: `modules/collections/include/System/Collections/ICollection.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-358 — high — raw ICollection CopyTo contract cannot validate destination capacity and crashes on null storage

The polymorphic `CopyTo(void*, int)` interface carries neither an element type nor a destination length.  Implementations in ArrayList, Queue, Stack, Hashtable, and ListDictionaryInternal trust the pointer/index and write directly.  The direct ASan/UBSan probe performs `ArrayList::CopyTo(nullptr, 0)`; it reaches `std::any::operator=` through a null destination and crashes.  Negative indexes and undersized buffers have the same unchecked native-write character, whereas the .NET contract diagnoses null, rank/type, index, and capacity.

## Missing assertions and diagnostics

- No interface test passes null, negative indexes, or an explicitly undersized destination through every ICollection implementation.
- The public boundary needs a representable typed/length-aware destination or a checked adapter with a single failure diagnostic before the first write.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
