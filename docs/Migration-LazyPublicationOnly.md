<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Lazy<T>` `PublicationOnly` matches .NET (ticket #2238)

*2026-08-18.* `PublicationOnly` serialized its factory behind a mutex and rejected recursion.
.NET does neither. Both deviations are gone.

Landed under SA-5 on the user's decision of the same date (`docs/StandingApprovals.md`, SA-11).
**This reverses a guarantee the class advertised for its whole life** — read §2. No signature,
layout, vtable or `noexcept` change.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| the factory | serialized behind `publicationOnlyMutex_`, ran **at most once** | runs **with no lock held**; may run **concurrently and more than once** |
| a recursive `Value()` | `InvalidOperationException` | the factory simply runs again |
| which value survives | the only one | the **first** published; every loser discards its own |
| a throwing factory | not cached, retryable | **unchanged** |
| `None`, `ExecutionAndPublication` | — | **unchanged**, including their recursion guard |

Transcribed from `Lazy.cs:351-378`: .NET calls `factory()` and only then attempts an
`Interlocked.CompareExchange`; a thread that loses the exchange throws its result away.

## 2. The two consequences, stated before the decision and accepted with it

**A `PublicationOnly` factory may now run concurrently, and more than once.** That is what
`PublicationOnly` *means* in .NET — the mode exists to let several threads race and publish
whichever finishes first. If your factory has side effects, or is expensive, it was never the
right mode; use `ExecutionAndPublication`, which is still the default and still runs the factory
exactly once.

**A recursive `PublicationOnly` factory now recurses.** It no longer receives a clean, catchable
`InvalidOperationException`. An *unconditionally* recursive factory will exhaust the stack,
precisely as it does in .NET. The guard remains in force for `None` and
`ExecutionAndPublication`, where .NET also raises it and where the alternative is undefined
behaviour — a recursive `std::call_once` on one flag from one thread.

The class doc-comment describes both in the past tense on purpose: a reader who remembers the old
contract has to be told it changed, not left to discover it.

## 3. To migrate

```cpp
// If you relied on "the factory runs exactly once":
Lazy<Heavy> h(makeHeavy, LazyThreadSafetyMode::PublicationOnly);      // no longer guaranteed
Lazy<Heavy> h(makeHeavy, LazyThreadSafetyMode::ExecutionAndPublication);  // still guaranteed

// If you caught InvalidOperationException to detect a recursive factory:
// that exception no longer arrives in PublicationOnly. Break the recursion in the factory.
```

## 4. Evidence

Four mutations. **Two caught outright**: making the loser overwrite the winner, and removing the
published-value short circuit. **One caught as a hang rather than a failure** — re-serializing the
factory deadlocks the recursion test, which is inherent to the mutation (it reintroduces the very
UB the repair removes) rather than a gap in the tests.

**One is not caught, and the code says so at the site.** Restoring the reentrancy guard to
`PublicationOnly` changes nothing observable, because `creatingThreadId_` is written only by
`initValue`, which this path no longer calls — so the guard could not fire even if it ran. The
condition is kept because it states the contract where the contract is decided, and the comment
records that it is redundant today instead of looking load-bearing.

A concurrency test asserts the reversal directly: four threads enter the factory at once,
`started > 1` proves the serialization is gone, and every thread observes the same surviving
value.
