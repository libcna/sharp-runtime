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

---

## SR-AUD-065 — REMEDIATED (ticket #1867, 2026-07-30, CCF-011)

The original evidence above is retained unchanged.

`Lazy<T>`'s three factory constructors — `Lazy(F&&)`, `Lazy(F&&, bool)` and
`Lazy(F&&, LazyThreadSafetyMode)` — now call a new private `requireFactory()`
from their constructor bodies, which throws
`System::ArgumentNullException("valueFactory")` when the callable stored in
`factory_` is an empty `std::function`. That is .NET's rule, read from
`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Lazy.cs:301`
(`ArgumentNullException.ThrowIfNull(valueFactory)`), and the message is
byte-identical to .NET's: `Value cannot be null. (Parameter 'valueFactory')`.

The finding's premise is confirmed exactly as written. Measured before the fix
(`build-probe/1866_prefix.log`): `lazy.ctor.factory=no-throw`,
`lazy.value.factory=bad_function_call`, and the same for the `bool` and
`LazyThreadSafetyMode` overloads. Measured after
(`build-probe/1867_postfix_asan.log`): all four report
`ArgumentNullException:Value cannot be null. (Parameter 'valueFactory')`.

The four **non**-factory constructors — `Lazy()`, `Lazy(T)`, `Lazy(bool)`,
`Lazy(LazyThreadSafetyMode)` — synthesise their own `[]{ return T{}; }` and are
deliberately unaffected; a permanent test pins that.

Closure evidence: 9 new permanent regressions in `LazyTests.cpp` (each of the
three factory forms, every `LazyThreadSafetyMode` value, the exact `paramName`
in the message, catchability as `System::Exception` — which `std::bad_function_call`
never had — non-emptiness of a real `std::function` factory, the unaffected
non-factory constructors, and a 1,000-iteration throwing-construction loop that
would expose a leaked factory target). `LazyTests` + `AggregateExceptionTests`
75/75. The direct probe `build-probe/1866_empty_callable_probe.cpp`, compiled
**with** `-fsanitize=address,undefined` so the header-only change is itself
instrumented, exits 0 with zero AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer reports, including a 2,000-iteration stress loop over heap-owning
`std::function` targets.

Source, ABI and layout consequences: none. No signature, template parameter,
`noexcept` specification, virtual function or data member changed; the check is a
constructor-body statement. `Lazy<T>` remains non-copyable and non-movable. The
one observable change is that a call that used to construct successfully and then
fail at the first `Value()` with an uncatchable native exception now fails at the
constructor with the documented .NET one.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).
The report's remaining open items — PublicationOnly recursion, `getModeProperty`'s
debug-view adaptation, `ToString`'s narrow fallback and the move-only/
non-default-constructible `T` coverage gaps — are **not** closed by this ticket.

---

## SR-AUD-064 — REMEDIATED (ticket #2236, family #2235, 2026-08-10)

The original evidence above is retained unchanged, and the probe confirms it
exactly as written — no premise correction was needed.

Measured before the fix over the shipped library
(`build-probe/2235_probe1_before.cpp`, log `…_before.log`, 17 cases, 10 OK /
5 BAD / 1 DEVIATION):

```
[064] Lazy<int>(mode=99)                          BAD  (got no-throw, want ArgumentOutOfRangeException)
[064] Lazy<int>(mode=-1)                          BAD  (got no-throw, ...)
[064] Lazy<int>(factory, mode=99)                 BAD  (got no-throw, ...)
[064] getModeProperty() after mode=99             BAD  (got 99)
[064] invalid mode dispatches as PublicationOnly  BAD  (got 2 factory calls)
```

The fifth row is worth more than the exception type and the report's own
`99,0` reproducer: two `getValueProperty()` calls over a **throwing** factory
produced **two** factory invocations. No fault caching is `PublicationOnly`'s
contract and neither `None`'s nor `ExecutionAndPublication`'s, so the invalid
instance was not merely tolerated — the `default:` label of the switch in
`getValueProperty()` was deciding a public fault-caching contract by accident.

The repair is a private `static void requireValidMode(LazyThreadSafetyMode)`
holding .NET's three cases and .NET's `default:`
(`ArgumentOutOfRangeException(nameof(mode), SR.Lazy_ctor_ModeInvalid)` in
`LazyHelper.Create`), called from the bodies of the two constructors that take a
`LazyThreadSafetyMode`. It runs **after** `requireFactory()`, matching
`Lazy(Func<T>, LazyThreadSafetyMode)`, which calls
`ArgumentNullException.ThrowIfNull(valueFactory)` before `LazyHelper.Create`; a
permanent test pins that ordering. The other five constructors hard-code or
select between defined values and are deliberately untouched.

`/rv` is absent in this container, so the message sentence — `"The mode argument
specifies an invalid value."`, mirroring `SR.Lazy_ctor_ModeInvalid` — is not
independently verifiable here. The exception **type**, the **`paramName`** and
the **door** are what the finding states and what the tests pin; the sentence
follows this repository's established practice of not chasing verbatim message
text (`ArgumentOutOfRangeException.hpp`'s own "KNOWN MINOR GAP" note).

After the fix all five BAD rows are OK and every control is unchanged
(`…_after.log`, 15 OK / 0 BAD / 1 DEVIATION). +8 permanent regressions in
`LazyTests.cpp`; `LazyTests` 60/60. Mutation-checked three ways, each rebuilt
and re-run: dropping the check from the factory+mode constructor fails 3 tests;
swapping it ahead of `requireFactory()` fails the ordering test; weakening the
pinned recursion message fails the message test.

Source, ABI and layout consequences: none. No signature, template parameter,
`noexcept` specification, virtual function or data member changed — the check is
a constructor-body statement, and `Lazy<T>` is a header-only class template with
no out-of-line definition to move. The `case PublicationOnly: default:` labels
in `getValueProperty()` stay fused, now unreachable by construction, with a
comment saying so; splitting them would mean inventing a behaviour for a state
construction forbids. No production consumer and no test pinned the old
behaviour.

Plan: `docs/CoreLazyThreadSafetyModeFamilyPlan.md` §4.1.

## SR-AUD-066 — REMEDIATED by option (b) (ticket #2237, family #2235, 2026-08-10)

**The behavioural divergence is retained by design.** This finding states its own
two acceptable repairs — "implement the PublicationOnly rule without deadlock
**or** document and deliberately expose the incompatible restriction; silently
throwing the .NET-specific exception is not the stated behavior" — and the
second one is what landed. A reader must not take `remediated` here to mean that
`PublicationOnly` recursion now matches .NET. It does not.

Why the first option was not taken autonomously: `checkNotReentrant()` is
load-bearing for **memory safety**, not merely for contract. This port serialises
`PublicationOnly` behind `publicationOnlyMutex_` — itself a deviation the class
doc-comment has always carried, because real `PublicationOnly` runs the factory
concurrently on several threads, which for an arbitrary `T` is a C++ data race.
A recursive `getValueProperty()` on the owning thread would therefore re-`lock()`
a **non-recursive** `std::mutex`, which `[thread.mutex.requirements.mutex]` makes
undefined behaviour outright. Reaching .NET's rule needs one of two structural
changes, and both are user-visible design decisions:

1. *publish-only locking* — factory outside the mutex, mutex held only across the
   publication, first writer wins. Closest to .NET, but it **reverses** the
   documented serialisation deviation, so factories would again run concurrently
   on several threads;
2. *same-thread reentrancy* — mutex kept for other threads, factory re-invoked
   without re-locking on the owning thread, with a new discard rule in
   `initValue()` so the outer invocation does not overwrite the nested
   publication.

Both also let an unconditionally recursive factory recurse without bound: in
.NET that is `StackOverflowException` and process death, in C++ it is stack
exhaustion, i.e. undefined behaviour replacing a clean, catchable
`System::InvalidOperationException`. That trade contradicts `CLAUDE.md`'s stated
preference for throwing clearly, and is ticket **#2238 (`needs_user`)**.

What #2237 changed, with no executable statement touched:

- the class doc-comment's deviation block is now numbered (1/2) and (2/2), and
  (2/2) states the `PublicationOnly` reentrancy restriction, the .NET rule it
  does not implement, the `std::mutex` reason, the two routes and their cost, and
  the reopening ticket;
- a stale comment above `requireFactory()` that asserted ".NET turns this into a
  clean InvalidOperationException" — true only for two of the three modes, which
  is precisely this finding — was removed and replaced by an accurate one on
  `checkNotReentrant()` itself;
- `getValueProperty()`'s `@throws` clause records that .NET raises this for
  `None` and `ExecutionAndPublication` only;
- +5 permanent regressions pin recursion in **all three** modes, the exception
  message, the non-recursive `PublicationOnly` control (one factory call, two
  identical reads), fault-retry after a rejected recursion, and legal recursion
  into a *different* instance — the controls a future option-(a) implementation
  must not break.

The DEVIATION row of the family probe is unchanged before and after, by design.
Plan: `docs/CoreLazyThreadSafetyModeFamilyPlan.md` §4.2.

The report's remaining open items — the `getModeProperty()` debug-view
adaptation, `ToString`'s narrow fallback, the move-only/non-default-constructible
`T` coverage gap and the None-mode single-thread requirement — carry no
`SR-AUD-*` identifier and are **not** closed by this family.
