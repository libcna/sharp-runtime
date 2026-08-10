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


---

## Remediation record — ticket #1953 (2026-08-03), SR-AUD-199 → `remediated`

Cause **T-C** of `docs/ThreadingNamespaceReviewPlan.md`.

**Selected repair: tolerance, not rejection.** Ticket #1953's acceptance criteria offered
three routes — reject the empty pointer, treat an absent state as the default non-cancellable
token, or remove the constructor from the public surface. The second was selected because it
is *.NET's own model*, not a compromise:

- `CancellationToken.IsCancellationRequested` is
  `_source != null && _source.IsCancellationRequested`;
- `ThrowIfCancellationRequested` therefore cannot fire for a token with no source;
- `Register` returns a default registration, with .NET's own comment at that line: *"Nothing
  to do for tokens than can never reach the canceled state. Give back a dummy registration."*

The three members here now read exactly that way. The constructor keeps its signature,
accessibility and semantics, so **nothing is narrowed or removed** and the source-break clause
in #1953's criteria was never reached.

### A second route to the same crash, not named by the finding

While reproducing SR-AUD-199 the probe also exercised a **moved-from** token.
`CancellationToken` holds a `shared_ptr` and has an implicitly declared move constructor, so
moving one leaves the *source* object with an empty `state_` — reachable through ordinary
well-formed C++, with no mention of the internal-state constructor at all. Both
`cancellationtoken.moved_from.isrequested` and `cancellationtoken.moved_from.register` printed
`child-signal:11(SEGV)` before the change.

This is why rejection would have been the wrong repair: it would have closed the route the
finding names while leaving the unnamed one open, and it would have made the same absent state
legal via move and illegal via constructor. One tolerant definition covers both. Writing a
user-declared move constructor that installs a fresh state was rejected as the alternative: it
would cost a heap allocation on every move of a type designed to be passed by value.

The sibling `CancellationTokenRegistration` already used this idiom (`if (!state_) return;`,
`getIsActiveProperty()` returning `false`), so the two types are now consistent rather than
divergent.

### Evidence

`build-probe/1953_probe1_null_argument_crashes.cpp`, logs `1953_probe1_before.log`,
`1953_probe1_after.log`, `1953_probe1_asan.log`.

| Row | Before | After |
|---|---|---|
| `cancellationtoken.empty_state.ctor` | `normal` | `normal` |
| `cancellationtoken.empty_state.isrequested` | `child-signal:11(SEGV)` | `normal`, value 0 |
| `cancellationtoken.empty_state.throwif` | `child-signal:11(SEGV)` | `normal` |
| `cancellationtoken.empty_state.register` | `child-signal:11(SEGV)` | `normal`, inactive registration |
| `cancellationtoken.moved_from.isrequested` | `child-signal:11(SEGV)` | `normal`, value 0 |
| `cancellationtoken.moved_from.register` | `child-signal:11(SEGV)` | `normal` |
| `cancellationtoken.default.isrequested` | `normal` | `normal` (control) |
| `cancellationtoken.shared_state.valid` | `normal` | `normal` (control: cancellation still observed) |

ASan/UBSan/LSan clean after the change, instrumentation verified by symbol inspection.

Coverage: `ThreadingNullArgumentTests.CancellationToken_*` — absent state, absent-state
`Register`, moved-from token, and a control proving a live state still observes cancellation,
still runs its callbacks and still throws `OperationCanceledException`. The #1951
empty-callable rejection is asserted to still win over the absent state.
