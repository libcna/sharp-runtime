# Audit: `modules/threading/include/System/Threading/CancellationToken.hpp`

## Metadata

- AUDITED: 90-line token/state declaration, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; native
  ASan/UBSan and managed callback-validation probes were run.

## SR-AUD-198 — medium — CancellationToken Register accepts an empty callback and turns invalid input into a silent no-op

`Register({})` stores an empty `std::function`, and Cancel later skips it. The
C++ probe prints `empty_callback=normal`; the managed probe prints
`empty_callback=argument_null`. This hides invalid registration rather than
reporting it at entry.

## SR-AUD-199 — high — Public null-state CancellationToken construction reaches a null dereference

The public shared-state constructor accepts an empty `shared_ptr`; the next
`getIsCancellationRequestedProperty` dereferences it. ASan/UBSan reports
member access through null and a zero-page SEGV. Managed callers have no
equivalent internal-state constructor.

## Final assessment

SR-AUD-198/199 are confirmed. Normal cancellation state and callback ordering
are otherwise covered by existing fixtures. No source or test was changed.
