# Audit: `modules/core/include/System/Lazy.hpp`

## Metadata

- Audit status: AUDITED (282-line public template header, fully read).
- Supporting validation: `LazyTests.*` passed 38/38 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26; 22 tests are in the audited
  direct file and 16 are supporting smoke cases in pending
  `SystemTypesRemainingTests.cpp`.
- Reproducer: `/tmp/sharp-runtimervc-lazy-audit-probe.cpp` prints `99,0`,
  `bad_function_call`, and `invalid_operation` for the three cases below.

## Assessment

The default/precomputed/factory construction paths, fault caching for None and
ExecutionAndPublication, retry after PublicationOnly faults, state visibility,
and ordinary recursive guard are thoughtfully implemented. The header also
explicitly documents its safe serialized adaptation of PublicationOnly's
concurrent-racing behavior. Three public input/state boundaries are not given
an explicit policy, however, and the switch's default path silently changes
invalid configuration into PublicationOnly behavior.

## SR-AUD-064 — medium — Lazy constructors silently accept an invalid thread-safety mode

All mode-taking constructors store any `LazyThreadSafetyMode` value without
validation. `getValueProperty()` then directs an unrecognized value through its
`default` branch to PublicationOnly. The reproducer constructs
`Lazy<int>(static_cast<LazyThreadSafetyMode>(99))` and prints `99,0`: no
constructor error occurs, the invalid public mode remains observable, and
access silently uses another mode.

Current local `Lazy.cs` routes mode construction through `LazyHelper.Create`,
whose default case throws `ArgumentOutOfRangeException`. The C++ constructors
need a shared three-value validation path and a deterministic project argument
exception before any lazy state is created.

## SR-AUD-065 — medium — Lazy accepts an empty factory and defers failure to std::bad_function_call

The `F` constraints only verify that a callable *type* has `T()` invocation;
they do not reject an empty `std::function<T()>`. Such a factory is copied into
`factory_`, and the first `Value()` call invokes it through `initValue`,
producing native `std::bad_function_call` (`empty-factory` probe output) rather
than failing at the public constructor boundary.

Local .NET explicitly rejects a null factory in `Lazy(Func<T>, mode)` with
`ArgumentNullException`. The C++ equivalent must choose and document a
deterministic empty-function error at construction, instead of caching or
propagating a delayed native invocation error. This extends CCF-011's shared
empty-`std::function` policy issue.

## SR-AUD-066 — medium — PublicationOnly wrongly rejects recursive Value access

`checkNotReentrant()` runs before dispatching on `mode_`, so it throws
`InvalidOperationException` for recursion in all three modes. The reproducer's
PublicationOnly factory recursively calls `Value()` and prints
`invalid_operation`. Current local `LazyThreadSafetyMode.cs` and `Lazy.cs`
specify this exception for None and ExecutionAndPublication only; PublicationOnly
must not throw it (its repeated factory invocation/publication behavior has a
different contract).

The serialized C++ adaptation avoids publication races, but it does not
document this changed reentrancy behavior. A repair must either implement the
PublicationOnly rule without deadlock or document and deliberately expose the
incompatible restriction; silently throwing the .NET-specific exception is not
the stated behavior.

## Other missing assertions and diagnostics

- None mode is intentionally non-thread-safe, but tests do not establish its
  single-thread requirement or show a deterministic warning for concurrent use.
- No test covers a default constructor for a non-default-constructible T, a
  move-only T, empty factory with each constructor form, or a custom exception
  identity after fault caching.
- The `getModeProperty()` debug-style value remains the original enum after
  initialization/fault, unlike .NET's internal nullable debug view; the public
  adaptation is undocumented.
- `ToString` supports only member `ToString` and `std::to_string`, omitting
  stream insertion and null-like .NET behavior; its fallback policy is narrow
  but explicitly present in comments.

## Final assessment

The ordinary state machine is well tested, but public configuration/factory
validation and PublicationOnly recursion diverge from the local .NET contract.
No source or test was modified during this audit.
