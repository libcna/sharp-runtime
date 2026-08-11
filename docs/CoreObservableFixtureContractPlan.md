<!-- SPDX-License-Identifier: MIT -->

# The `IObservable`/`IObserver` fixture — SR-AUD-056 review and repair

Tickets: **#2301** (review), **#2302** (implementation). Date: 2026-08-11. The
audit numbering stays frozen at 364, **no new `SR-AUD-*` identifier was created**
and **no CCF was minted**.

---

## 1. Why this one was selected

It is the only remaining `modules/core` finding that met every criterion for an
additional singleton in this batch: `confirmed`, unclaimed, measurable, an
ordinary compatibility fix, a small blast radius, no missing external reference
semantics, no public representation decision, no signature or source break, no
ownership or lifetime *policy* question, and no dependency-cycle design. It is
also the only one that can actually **close**, because the defect is entirely
inside a test file.

## 2. What was measured before anything was written

The finding blames the fixture, and the fixture is where the fault is — the
production interfaces are correct and were the right oracle all along:

- `IObservable<T>::Subscribe` already returns `std::shared_ptr<IDisposable>` and
  already documents it as the handle "that allows observers to stop receiving
  notifications before the provider has finished sending them".
- `IObserver<T>` already documents that after a terminal `OnError` or
  `OnCompleted` the provider makes no further `OnNext` or `OnCompleted` call.

Only `IntObservable2` contradicted them: `Subscribe` stored the observer and
returned `nullptr`, and `Complete()` neither recorded a terminal state nor
cleared the observers, leaving `Emit` fully functional afterwards. The finding's
own scoping also re-measured true — **no first-party `IObservable<T>`
implementation exists** in the production tree, so nothing shipped was wrong and
this is a test-contract defect, not a provider bug.

## 3. The repair

`IntObservable2` now honours both contracts.

- **`Subscribe` returns a real `Subscription`.** It unsubscribes exactly its own
  observer, and a second `Dispose()` is a no-op, as `IDisposable` requires of
  every implementation.
- **A null observer is rejected** with `System::ArgumentNullException`, which is
  what .NET's own implementations do.
- **A terminal state is recorded.** `Emit`, `Complete` and `Fail` all deliver
  nothing once a terminal notification has been sent, so `OnCompleted` is
  delivered at most once.
- **`Fail` supplies the `OnError` path** the report notes was never covered.

### 3.1 Two design choices, both measured rather than assumed

**The observer list lives in a `State` that the provider and every subscription
co-own.** That is the pattern this repository already uses to isolate a view's
storage from its owner's lifetime (`SortedSet<T>`, ticket #1786). A subscription
is therefore safe to dispose whatever order things are destroyed in, and there is
**no ownership cycle** — the subscription owns `State`, `State` owns the
observers, an observer owns nothing. This answers the report's last "other
missing assertions" bullet without a raw back-pointer into the provider.

**`~Subscription()` deliberately does not dispose.** Unsubscription is tied to
`Dispose()`, as the interface documents, not to the handle's lifetime: dropping
the returned `shared_ptr` must leave the observer subscribed, which is what .NET
does. This was measured, not reasoned: the first attempt gave `Subscription` an
RAII destructor, and **both pre-existing cases failed immediately**, because both
discard the handle returned by `Subscribe`. An RAII subscription would have been
a different, unannounced contract.

## 4. Tests — seven added, none retired

`IObservableTests2` and `IObserverTests2` go 2 → 9. The two pre-existing cases
keep their names and assertions; `Subscribe_AndReceiveValues` additionally
asserts the subscription is non-null. The five assertions the report itself
suggests are all present: a non-null subscription
(`Subscribe_ReturnsADisposableSubscription`), disposal suppressing later values
(`DisposedSubscription_StopsReceivingValues`), a rejected null observer
(`SubscribeRejectsANullObserver`), a suppressed post-completion value
(`NoValueIsDeliveredAfterCompletion`), and `OnError` as the alternative terminal
path (`OnError_IsTerminalToo`). `DisposingASubscriptionTwiceIsANoOp` and
`CompletionIsNotDeliveredTwice` cover the two duplicate-call paths.

### 4.1 Mutations — both original defects, re-introduced

Rebuilt and relinked for each:

| Mutation | Caught by |
|---|---|
| `Subscribe` returns `nullptr` again | `Subscribe_ReturnsADisposableSubscription` **and** the pre-existing `Subscribe_AndReceiveValues` |
| `Emit` ignores the terminal state | `NoValueIsDeliveredAfterCompletion` **and** `OnError_IsTerminalToo` |

Both defects the finding names are now caught, each by more than one case. The
report's stated risk — "a future implementation can therefore copy this fixture's
wrong behavior without a failing regression" — is closed in both directions.

**Deliberately not pinned:** subscription **ordering**. The fixture notifies in
subscription order, but no interface in this repository documents that, and
asserting it would invent a contract rather than pin one.

## 5. Compatibility

No production file was touched. No public source, ABI, symbol, layout, vtable or
`noexcept` change; no include or component-graph change; no behaviour change
outside the test binary.

## 6. Validation

`build/` only, `cmake --build build --parallel 2`, maximum two jobs.
`SharpRuntimeTests_Core_Base` 5,930 → 5,937, rebuilt and relinked for each
mutation; the focused `IObservableTests2.*:IObserverTests2.*` filter reads 9/9.
**No sanitizer run**: the one lifetime question in the unit — whether a
subscription can outlive its provider — is answered by construction, since the
shared `State` is co-owned and no raw back-pointer exists; there is nothing for
ASan or TSan to discriminate in a single-threaded fixture with no raw ownership.
**Selective components not rerun**: nothing outside one test translation unit
changed.

## 7. Disposition

| Finding | Before | After | Owner |
|---|---|---|---|
| SR-AUD-056 | confirmed | **remediated** | #2301 review, #2302 implementation |

Still open and **outside** this finding, from the report's "other missing
assertions": `ParseableInt` leaks `std::stoi` exceptions and does not inspect a
pre-populated `TryParse` output, and the format/service provider cases exercise
only `nullptr`. Neither is part of SR-AUD-056 and neither was absorbed into it.
