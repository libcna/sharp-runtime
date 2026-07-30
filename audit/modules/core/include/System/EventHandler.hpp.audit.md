# Audit: `modules/core/include/System/EventHandler.hpp`

## Metadata

- Audit status: AUDITED (266-line template event-field implementation, fully
  read with its implementation and direct fixture).
- Validation: `EventHandlerTests.*` passed 24/24 in the 32-test event-core
  filter on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-event-handler-audit-probe` prints
  `empty_handler_raises=1`; the mutable-handler compile probe is rejected with
  `HandlerType = std::function<void(System::Object*, const MutableArgs&)>`.
- Reference basis: local .NET `System/EventHandler.cs:6-20`.

## SR-AUD-121 — medium — EventHandler stores an empty handler and defers failure to std::bad_function_call during Raise

`Add` unconditionally invokes its optional replay hook and then stores every
`HandlerType`, including an empty `std::function` (`EventHandler.hpp:171-179`).
`Raise` later calls it unconditionally (`:244-251`).  The runtime probe adds
an empty handler and prints `empty_handler_raises=1` after catching
`std::bad_function_call`.  Adding a null delegate to a .NET event is a no-op;
it cannot create a delayed native invocation failure.  If a replay hook calls
the empty handler, failure occurs even earlier.

All green tests use nonempty callbacks; the empty test covers only an empty
collection, not an empty stored subscriber.  This extends CCF-011.

## SR-AUD-122 — medium — EventHandler exposes event arguments as const and cannot represent .NET handlers that mutate their event-data object

The public callback alias is `void(Object*, const TEventArgs&)`
(`EventHandler.hpp:88`).  Current .NET's `EventHandler<TEventArgs>` passes
`TEventArgs e` without a readonly restriction; for the normal reference-type
event-args pattern, subscribers can modify its public/mutable state.  The
compile probe attempts a handler taking `MutableArgs&` and is rejected because
the port requires `const MutableArgs&`.  This is a public source and behavior
restriction, not merely a different storage implementation.

The direct custom-args test reads a field only, so no test reveals whether a
subscriber can implement a stateful acknowledgement/cancellation-style event.

## Other missing assertions and diagnostics

- Snapshot reentrancy is covered for token removal, but not Clear, assignment,
  replay-hook mutation, empty callback/replay combinations, exception
  propagation, recursive raise, copied collections, token wrap, or concurrency.
- The extra replay hook is a documented project-specific adaptation; no test
  checks whether it throws before storage or recursively subscribes.
- The class intentionally combines a .NET delegate type with event storage;
  its Object-pointer sender and absent two-generic-parameter EventHandler are
  explicit API adaptations.

## Final assessment

The normal tokenized event collection works, but empty callbacks and mutable
event-data handlers have incompatible public behavior.  No source or test was
modified during this audit.

---

## SR-AUD-121 — REMEDIATED (ticket #1868, 2026-07-30, CCF-011)

The original evidence above is retained unchanged.

`Add` now consumes and returns a token but stores nothing when the supplied
`HandlerType` is empty, and it returns **before** the replay hook rather than
after it. `operator+=` delegates to `Add` and inherits the rule. `Raise` invokes
only truthy handlers. Adding a null delegate to a .NET event is
`Delegate.Combine(d, null) == d` — a no-op that cannot create a later invocation
failure — so this surface does not throw.

**Correction to the finding's premise (measured 2026-07-30).** The finding's
title says the failure is deferred "during Raise"; its body already notes that a
replay hook makes it happen earlier. Measured, that second path is the binding
one for the repair: with a hook set, `eventhandler.add.replayhook` reported
`bad_function_call` raised **inside `Add` itself**, before any storage happened
(`build-probe/1866_prefix.log`). The empty check therefore has to precede the
hook, not merely precede the `emplace_back`. Both now report `no-throw`
(`build-probe/1868_postfix_asan.log`). The historical text above is left as
written, per this repository's practice.

**One observable behaviour change, deliberate.** `Size()` no longer counts an
empty subscription: `eventhandler.size` went from `1` to `0` and `Empty()` from
`false` to `true` for a lone empty `Add`. A consumer counting *subscriptions*
rather than *subscribers* would see a different number — but only for a
subscriber that could never have been invoked. This is recorded as B2 in
`docs/EmptyCallableBoundaryPlan.md` §9 and is not an approval-gated change: no
signature, `noexcept`, vtable or layout is affected.

The token is still consumed for an ignored handler. Returning the *unchanged*
`nextToken_` would have made a later `Remove()` of the ignored subscription
unsubscribe the next real handler instead; a permanent test pins that the two
tokens differ and that removing the ignored one leaves the real subscriber
attached.

Closure evidence: 8 new permanent regressions in `EventHandlerTests.cpp` (empty
`Add` and empty `operator+=` each storing nothing and leaving `Size()`/`Empty()`
alone; `Raise`/`Invoke` silent afterwards; the replay hook not invoked; ordering
preserved across an ignored empty subscription between two real ones; token
uniqueness and safe `Remove`; 100 ignored subscriptions followed by a real one;
and an empty subscription made from *inside* `Raise`, which must not disturb the
snapshot iteration). `ProgressTest` + `EventHandlerTests` 46/46,
`SharpRuntimeTests_Core_Base` 5,280/5,280. The direct probe compiled **with**
`-fsanitize=address,undefined` exits 0 with zero AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer reports, including a handler that
adds, removes and `Clear()`s the list from inside `Raise` — the case where
iterating the live list rather than the snapshot would be a use-after-free.

Source, ABI and layout consequences: none. `Add`, `operator+=`, `Raise` and
`Invoke` keep their signatures and their (absent) `noexcept` specifications; no
data member was added, removed, reordered or retyped. `SetReplayHook`'s empty
value remains the documented "clear the hook" spelling and is explicitly out of
this family's scope.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).
