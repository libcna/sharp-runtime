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
