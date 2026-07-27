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

## Post-audit remediation note for SR-AUD-358 (ticket #1771, 2026-07-27)

**Status: REMEDIATED.** The original audit evidence and the design note above
are retained unchanged; this section records what closed the finding.

The user approved the public source- and ABI-breaking change on 2026-07-27, and
ticket #1771 landed it. `virtual void CopyTo(void* array, intcs index) = 0;` is
removed from `ICollection`. The boundary is now

```cpp
using ObjectSpan = System::Span<std::any>;

class ICollection : public IEnumerable {
public:
    void CopyTo(ObjectSpan destination, intcs index);             // validating, non-virtual
    void CopyTo(std::vector<std::any>& destination, intcs index); // convenience, non-virtual
protected:
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;
};
```

`detail::requireValidCopyDestination` is the single validation site for all six
implementations and for the typed concrete overloads, so the "five independent
bounds omissions" reading of the finding is structurally impossible now: an
implementation cannot be reached without it. Null destination →
`ArgumentNullException`; negative index → `ArgumentOutOfRangeException`
("Non-negative number required."); index past the end or insufficient remaining
capacity → `ArgumentException`, tested as `length - index < count` so a large
index cannot overflow past the check. Validation precedes the copy, so a rejected
call performs no partial write.

The approved decision differs from the design record in one respect, recorded in
section 21 of `docs/ICollectionCopyToDesign.md`: the deprecated, never-writing
`CopyTo(void*, intcs)` shim was **not** retained. A shim would have let a stale
call site compile and fail at run time, whereas removal makes every one a compile
error naming the replacement.

Evidence that the finding is closed:

- The four scenarios of the original probe no longer compile at all — the same
  source now yields four `error: no matching function for call to
  '...CopyTo(<raw pointer>, int)'` diagnostics, each followed by `note:
  candidate:` lines for the two surviving overloads
  (`build-probe-copyto/probe5_removed_api.log`).
- The replacement probe (`build-probe-copyto/probe8_new_boundary.cpp`, 13
  assertions) runs the same scenarios against the new API under
  `-fsanitize=address,undefined` with LeakSanitizer active: every one throws the
  documented exception, `failures=0`, exit 0, **no sanitizer diagnostic and no
  leak** — including the element-type-mismatch case that previously leaked 32
  bytes silently, whose unsafe form is no longer expressible.
- The permanent suite
  `modules/collections/tests/System/Collections/CopyToBoundaryTests.cpp` adds the
  128 tests the "Missing assertions and diagnostics" section asked for: null,
  negative, past-end, undersized, overflow-index, zero-length, and no-partial-write
  cases run through *every* `ICollection` implementation, including both private
  `MemberCollection` views. It also passes 128/128 under ASan + UBSan + LSan.
- `SharpRuntimeTests_Collections_Core` 1,612/1,612; repository total 12,871 across
  37 executables (floor was 12,743).

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
