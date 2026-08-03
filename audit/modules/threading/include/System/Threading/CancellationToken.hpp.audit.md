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


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-198 → `remediated`

Cause **T-B** of `docs/ThreadingNamespaceReviewPlan.md` (CCF-011 in `modules/threading`).

`CancellationToken::Register` now throws `System::ArgumentNullException("callback")` as its
first statement, before `state_->mutex` is taken. .NET's `Register(Action callback)` forwards
`callback ?? throw new ArgumentNullException(nameof(callback))`, so the rejection precedes any
consultation of the token's state — including the already-cancelled fast path, where the
callback would otherwise have been invoked immediately. Both orderings are pinned by tests
(`CancellationToken_RegisterEmpty_*`), as is the no-partial-state guarantee: after a rejected
registration a subsequent real registration runs exactly once on `Cancel()`, so no empty slot
was recorded and no id consumed.

Evidence: `cancellationtoken.register_empty` moved from `normal` to
`ArgumentNullException|Value cannot be null. (Parameter 'callback')` in
`build-probe/1951_probe1_before.log` / `1951_probe1_after.log`. ASan/UBSan/LSan clean.

**SR-AUD-199 is untouched and remains `confirmed`** — it is cause T-C (a null dereference
through the public shared-state constructor) and belongs to ticket #1953.
