# Audit: `modules/threading/include/System/Threading/ReaderWriterLockSlim.hpp`

## Metadata

- AUDITED: 338-line state-machine reader/writer lock implementation, fully
  read, including its thread-local ownership records and condition-variable
  waits.
- Validation: `ReaderWriterLockSlimTests.*` passed 27/27 on 2026-07-27;
  complete Threading validation was previously green at 359/359. Direct C++
  and .NET 10 probes exercised a queued writer/read contender, disposal while
  held, invalid recursion-policy input, and concurrent disposal. The concurrent
  probe was built with `-fsanitize=thread`.
- Reference basis: current .NET 10 `ReaderWriterLockSlim` behavior and
  Microsoft API remarks (writer preference, managed thread affinity, and
  thread-safe public surface).

## SR-AUD-203 — high — `Dispose` races with entry and permits disposal while the caller owns a lock

`disposed_` is an ordinary `bool`: `Dispose()` writes it with no lock while all
`TryEnter*` routes read it through `throwIfDisposed()`. A two-thread TSan probe
that loops `TryEnterReadLock(0)` while the main thread calls `Dispose()` reports
the direct read/write data race at `ReaderWriterLockSlim.hpp:98` and `:316`.
This is C++ undefined behavior on a type whose managed counterpart is
thread-safe.

The same `Dispose()` implementation also merely sets the flag and does not
reject disposing while the current thread owns a mode. The C++ probe prints
`dispose=normal` after `EnterReadLock`; the identical .NET 10 probe prints
`dispose=exception:System.Threading.SynchronizationLockException`. It creates
an invalid partially-disposed ownership state in addition to the race.

## SR-AUD-204 — high — queued writers do not block new readers and can be starved indefinitely

The read predicate is only `!writerActive_`; the implementation stores neither
waiting-writer state nor an admission rule for it. Hold a read lock, start a
writer that blocks on `EnterWriteLock`, then issue `TryEnterReadLock(0)` from a
different thread. C++ prints `readerEntered=1`, whereas the identical .NET 10
probe prints `readerEntered=False`. Current .NET blocks new readers once a
writer is queued, both to honor its documented writer-preference policy and to
ensure that an ongoing stream of readers cannot indefinitely starve the writer.

## SR-AUD-205 — medium — invalid `LockRecursionPolicy` values are retained rather than normalized to `NoRecursion`

The policy-taking constructor stores its enum argument verbatim and the public
property returns it verbatim. Constructing with `static_cast<LockRecursionPolicy>(2)`
prints `constructed policy=2` in C++. The matching current-.NET 10 program
constructs successfully but prints `constructed policy=0`: only
`SupportsRecursion` enables recursion and every other underlying value is
reported as `NoRecursion`. The direct ordinal fixture covers only the two valid
values, so it cannot detect the observable invalid-cast divergence.

## Assessment

The ordinary single-thread ownership, timeout, recursive-mode, and upgrade
regressions are valuable and green. They do not exercise public disposal
concurrency, disposal ownership validation, writer admission, or invalid
recursion-policy reflection. The three findings are independent from the
previously repaired upgrade-to-write and recursive-count paths.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-203 concurrent `Dispose`/entry diagnostics and every
  disposal-while-held mode; a TSan target should cover all three `TryEnter*`
  paths.
- Tests omit SR-AUD-204 queued-writer admission for read and upgradeable-read
  contenders, sustained-reader starvation, waiter ordering, and timing-free
  coordination of a queued writer.
- No fixture constructs an invalid underlying `LockRecursionPolicy` value or
  checks its property normalization (SR-AUD-205).
- They also omit `TimeSpan` overload/conversion boundaries, maximum recursion
  counts, move/destruction while other threads own/wait, and the public waiting
  and current-reader count properties absent from this native surface.

## Final assessment

SR-AUD-203 through SR-AUD-205 are confirmed by direct current-.NET comparison
and native sanitizer evidence. No production or test source was changed.


---

## Remediation record — ticket #1954 (2026-08-03), SR-AUD-205 → `remediated`

Cause **T-C** of `docs/ThreadingNamespaceReviewPlan.md`. **SR-AUD-203 and SR-AUD-204 are
untouched and remain `confirmed`** — the `disposed_` race is cause T-A (ticket #1955), the
dispose-while-held half is cause T-G (ticket #1956, approval-gated), and the missing
queued-writer state is cause T-E/2 (ticket #1957, approval-gated).

The constructor now normalises:

```cpp
explicit ReaderWriterLockSlim(LockRecursionPolicy recursionPolicy)
    : recursionPolicy_(recursionPolicy == LockRecursionPolicy::SupportsRecursion
                           ? LockRecursionPolicy::SupportsRecursion
                           : LockRecursionPolicy::NoRecursion) {}
```

which is .NET's construction reduced to one expression: `ReaderWriterLockSlim` stores only
`_fIsReentrant = (recursionPolicy == LockRecursionPolicy.SupportsRecursion)` and exposes
`RecursionPolicy` derived from that bool, so an undeclared value can never be read back.
Normalisation rather than rejection is therefore parity, not leniency — and it is the
**opposite** of what `EventWaitHandle` does in the same ticket, because .NET treats the two
enums differently.

### Correction to the finding's extent

The finding says the invalid policy is "stored and reflected verbatim". Measured
(`build-probe/1954_probe1_argument_domain.cpp`): only the **property** diverged.
`rwls.policy2.property` printed `2` before the change and `0` after, but the row
`rwls.policy2.recursion_rejected` printed `(recursion rejected)` **both** before and after —
because `isReentrant()` already tested for `SupportsRecursion`, so a lock constructed with
policy 2 already *behaved* as `NoRecursion` and already threw `LockRecursionException` on
recursive entry. The defect was that the reported policy contradicted the lock's own
behaviour, not that the lock behaved unpredictably. The repair makes the stored value, the
reported value and the behaviour agree, and
`ThreadingArgumentDomainTests.ReaderWriterLockSlim_UndeclaredPolicy_AlsoBehavesAsNoRecursion`
pins the behavioural half so the two cannot drift apart again in either direction.

Coverage: `ThreadingArgumentDomainTests.ReaderWriterLockSlim_*` — raw policies 2, 7 and -1
all report `NoRecursion`; the default, `NoRecursion` and `SupportsRecursion` constructors are
unchanged, including `SupportsRecursion` still permitting matched recursive read acquisition.
No signature, layout, vtable or exception-specification change.


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-203 **race half only**

SR-AUD-203 is **split by cause** and stays `confirmed` until both halves land.

Cause **T-A** of `docs/ThreadingNamespaceReviewPlan.md`, "shared mutable state is observed
outside its own mutex". Evidence: `build-probe/1955_probe1_shared_state_races.cpp` under
`-fsanitize=thread`, logs `1955_probe1_tsan_before.log` (**13** data-race reports across
seven scenarios, exit 66) and `1955_probe1_tsan_after.log` (**zero** reports, exit 0, every
control value unchanged). Instrumentation was proved rather than assumed: 132 `__tsan_*`
symbols in the sanitized binary. The layout gate passed — `sizeof` and `alignof` are
byte-identical for all six affected types before and after
(`1955_probe1_layout_before.log` / `1955_probe1_layout_after.log`), so no user approval was
required, and the numbers are pinned by
`ThreadingSharedStateTests.RepairedTypes_LayoutUnchanged` in
`modules/threading/tests/System/Threading/ThreadingSharedStateTests.cpp`.

**Race half — done.** `disposed_` is now `std::atomic<bool>`: `Dispose()` performs a release
store, `throwIfDisposed()` an acquire load, so the flag every `TryEnter*` route consults is no
longer an ordinary `bool` racing an ordinary write. Scenario `rwls.disposed_vs_entry` reported
one race before and none after. `sizeof(ReaderWriterLockSlim)` 120 → 120, `alignof` 8 → 8.

**Dispose-while-held half — still open, approval-gated ticket #1956** (cause T-G): `Dispose()`
still succeeds while the calling thread owns a read, write or upgradeable mode, where .NET
throws `SynchronizationLockException`. Making the flag race-free is a prerequisite for
enforcing it, which is why the plan orders T-A before T-G, but it is not that enforcement.

**SR-AUD-204 is untouched and remains `confirmed`** — the missing queued-writer state is cause
T-E/2 and belongs to approval-gated ticket #1957.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
