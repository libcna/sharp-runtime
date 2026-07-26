# Audit: `modules/core/include/System/AggregateException.hpp`

## Metadata

- Audit status: AUDITED (145-line inline implementation, fully read).
- Validation: `AggregateExceptionTests.*` passed 13/13 on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-aggregateexception-audit-probe` reports lost custom message/inner state and, in isolated modes, a null-inner segfault (exit 139) and empty-handler `std::bad_function_call` termination (exit 134).
- Reference basis: local .NET `AggregateException.cs` constructor validation, `InnerException`, `Handle`, and `Flatten` implementations.

## SR-AUD-097 — high — null inner `exception_ptr` is accepted and later crashes in aggregate message processing

Unlike .NET, which rejects null inner exceptions at construction, `std::vector<std::exception_ptr>{nullptr}` is accepted. The collection constructor immediately sends that null pointer to `std::rethrow_exception` in `buildMessage`; the isolated probe exits 139 with a segmentation fault. The message-plus-vector and message-plus-single constructors also retain null entries without validation, leaving later `GetBaseException` / `Flatten` paths unsafe. Existing tests cover only non-null runtime errors.

## SR-AUD-098 — medium — aggregate constructors and transformations lose .NET causal diagnostics and flatten ordering

The message-plus-inner/vector constructors neither set the base `Exception::innerException_` to the first inner exception nor append contained messages to `what()`. The probe prints `custom_message=outer` and `custom_inner=null`; .NET exposes the first inner through `InnerException` and includes each inner diagnostic in `Message`. `Handle` rebuilds unhandled errors with the generic default text, and `Flatten` likewise discards the caller's message. Its recursive depth-first `collectLeaves` emits nested leaves before a later direct leaf (`a,b,c`), where current .NET's queue algorithm returns direct leaves before queued nested leaves (`c,a,b`). The green test `MessageAndSingle_Ctor_StoresInner` locks the incomplete C++ text rather than the .NET-shaped causal message.

## SR-AUD-099 — medium — `Handle` accepts an empty predicate and defers validation to `std::bad_function_call`

`Handle` invokes its `std::function` without validating it. An empty predicate therefore raises native `std::bad_function_call` only after aggregate construction and first traversal; the isolated probe terminates when this exception is left uncaught. .NET rejects a null predicate deterministically at the public boundary with `ArgumentNullException`. This extends the empty-callable pattern in CCF-011.

## Other missing assertions and diagnostics

- Tests omit empty/null inner entries, first-inner identity, custom-message aggregation, `Handle` message/order preservation, predicate exceptions, empty predicates, and nested/direct leaf order.
- `GetBaseException` returns copied `exception_ptr` state for a zero/multiple aggregate because this C++ value API cannot return an owning pointer to `*this`; tests do not document this adaptation or compare nested identity.
- No HResult assertion checks the inherited `Exception` code.

## Final assessment

Normal non-null happy paths pass, but public invalid-input handling and key causal diagnostics are incompatible. No source or test was modified.
