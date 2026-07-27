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

## Post-audit design note for SR-AUD-358 (ticket #1770, 2026-07-27)

**Status: DESIGN-COMPLETE — NOT REMEDIATED.** The original audit evidence above
is retained unchanged. SR-AUD-358 stays `confirmed`; only implementation closure
may change its status. This section records the design-only ticket that
preceded any production change.

Design ticket #1770 traced the boundary rather than trusting the summary and
recorded the result in `docs/ICollectionCopyToDesign.md`. Two facts beyond the original evidence were established by direct probe against
the current, unmodified headers:

- The six implementations do **not** agree on the destination element type
  (`std::any*`, `void**`, and `DictionaryEntry*`, sizes 16/8/32), so a caller
  holding an `ICollection*` cannot allocate a correct destination even in
  principle. A per-collection bounds patch therefore cannot close the finding.
- An element-type mismatch reached through `ICollection*` produces **no crash
  at all**: `Hashtable::CopyTo` into `std::vector<void*>` storage assigns a
  `DictionaryEntry` over storage that was never a `DictionaryEntry` and is
  caught only by LeakSanitizer (`Direct leak of 32 byte(s)` in
  `std::any::_Manager_external<std::string>::_S_create` ← `Hashtable::CopyTo`).
  The other three scenarios reproduce the recorded ASan/UBSan aborts
  (null destination SEGV, undersized heap-buffer-overflow read, negative-index
  heap-buffer-overflow write).

Selected architecture: a length-aware, statically typed destination
(`System::Span<std::any>`) behind a non-virtual interface, so validation runs
exactly once in `ICollection` before any implementation writes, and each
implementation supplies only a `copyToCore` hook. `CopyTo(void*, intcs)` leaves
the virtual interface and is retained briefly as a deprecated, never-writing
shim. .NET's rank, non-zero-lower-bound, and element-type-mismatch diagnostics
are declared intentionally unsupported because they require a runtime `Array`
object and a working `System::Type`, both permanently out of scope.

Implementation is proposed as ticket #1771 (`REMED-COLL-COPYTO`) and left
inactive: removing a pure virtual member from a public interface is a
compatibility-breaking public-header change and needs explicit user approval
first.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
