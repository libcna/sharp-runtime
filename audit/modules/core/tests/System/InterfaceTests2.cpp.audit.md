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
