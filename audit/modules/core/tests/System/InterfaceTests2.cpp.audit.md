# Audit: `modules/core/tests/System/InterfaceTests2.cpp`

## Metadata

- Audit status: AUDITED (187 lines, 11 tests, fully read).
- Validation: `IFormatProviderTests2.*:IFormattableTests2.*:IObservableTests2.*:
  IObserverTests2.*:IParsableTests2.*:IProgressTests2.*:IServiceProviderTests2.*:
  ISpanFormattableTests2.*` passed 11/11 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The source provides compact representatives for eight interface adapters.
Provider forwarding is correctly tested through base references for
`IFormattable` and `ISpanFormattable`; the simple format, parsing, progress,
and service/null paths compile and execute.  The observable fixture, however,
is used as behavioral evidence while it contradicts the documented
subscription and terminal-callback contract.

## SR-AUD-056 — medium — observable fixture discards the subscription handle and permits post-completion delivery

`IntObservable2::Subscribe` stores an observer then returns `nullptr`
(`InterfaceTests2.cpp:68-72`), although `IObservable<T>::Subscribe` documents
that it returns an `IDisposable` by which the observer stops notifications.
`IntObservable2::Complete` notifies its stored observers but neither records a
terminal state nor clears them (`:75`), while `Emit` remains callable and sends
`OnNext` to the same observers (`:73`).  This conflicts with the local
`IObserver<T>` contract that no `OnNext` or `OnCompleted` follows a terminal
`OnCompleted` call.

Both tests pass because the returned handle is discarded and no event is sent
after completion.  A future implementation can therefore copy this fixture's
wrong behavior without a failing regression.  No production `IObservable<T>`
implementation exists in the first-party tree, so this is a confirmed
test-contract and missing-assertion defect rather than a shipped provider bug.

Suggested future assertions: require a non-null subscription, dispose it and
verify suppression of later values, reject/diagnose a null observer, send a
post-completion value and prove it is suppressed, and cover `OnError` as the
alternative terminal path.

### Status: REMEDIATED (#2301 review, #2302 implementation, 2026-08-11)

Repaired by **all five of the assertions suggested above**, plus the fixture
behaviour they require — the finding is a test-contract defect, so the fixture
and its assertions are the whole of it, and no production file was touched.
Re-measured first: the production interfaces are **correct** and were the right
oracle all along. `IObservable<T>::Subscribe` already returns
`std::shared_ptr<IDisposable>` and documents what it is for, and `IObserver<T>`
already documents that no `OnNext` or `OnCompleted` follows a terminal call. Only
the fixture contradicted them, and the finding's own observation that no
first-party `IObservable<T>` implementation exists still holds, so nothing
shipped was wrong.

`IntObservable2::Subscribe` now returns a real `Subscription` that unsubscribes
exactly its own observer, rejects a null observer with
`System::ArgumentNullException`, and is a no-op on a second `Dispose()`, as
`IDisposable` requires. The provider records a terminal state, so `Emit`,
`Complete` and `Fail` all deliver nothing once a terminal notification has been
sent, and `Fail` supplies the `OnError` path the report notes was never covered.

Two deliberate design choices, both documented in the source. The observer list
lives in a `State` that the provider and every subscription **co-own** — the
pattern this repository already uses to isolate a view's storage from its
owner's lifetime (`SortedSet<T>`, ticket #1786) — so a subscription is safe to
dispose in any destruction order and there is **no ownership cycle**, which
answers the last "other missing assertions" bullet. And `~Subscription()`
deliberately does **not** dispose: unsubscription is tied to `Dispose()`, as the
interface documents, not to the handle's lifetime, so dropping the returned
`shared_ptr` leaves the observer subscribed. That was measured rather than
assumed — an RAII destructor made the two pre-existing cases fail, because both
discard the handle.

**Seven cases added, none retired** (`IObservableTests2` and `IObserverTests2`
go 2 → 9); the two pre-existing cases keep their names and assertions, with a
non-null subscription assertion added to the first. Both defects the finding
names were re-introduced as mutations, rebuilt and relinked:

- returning `nullptr` from `Subscribe` again fails
  `Subscribe_ReturnsADisposableSubscription` **and** the pre-existing
  `Subscribe_AndReceiveValues`;
- removing the terminal guard from `Emit` fails
  `NoValueIsDeliveredAfterCompletion` **and** `OnError_IsTerminalToo`.

So the "a future implementation can copy this fixture's wrong behavior without a
failing regression" risk is closed in both directions.

**Still open, and outside this finding** — the remaining "other missing
assertions" bullets: `ParseableInt` leaking `std::stoi` exceptions and not
inspecting a pre-populated `TryParse` output, and the format/service provider
cases exercising only `nullptr`. Subscription **ordering** was likewise not
pinned; the fixture notifies in subscription order but no interface documents
that, so asserting it would invent a contract.

`docs/CoreObservableFixtureContractPlan.md`.

## Other missing assertions and diagnostics

- `ParseableInt` leaks `std::stoi` exceptions and invalid `TryParse` does not
  inspect a pre-populated output result.
- The format/service provider tests exercise only `nullptr`; type/lifetime
  behavior of a supplied object remains untested.
- No test documents observer error payloads, duplicate terminal calls,
  subscription ordering, or shared-pointer cycle behavior.

## Final assessment

The source supplies useful interface smoke coverage but its observable
representative is not a valid behavioral oracle.  No test was modified during
this audit.
