# Audit: `modules/threading/include/System/Threading/ThreadLocal.hpp`

## Metadata

- AUDITED: 137-line `ThreadLocal<T>` adapter, including per-thread ID-keyed
  storage, value factories, disposed state, recursive-factory guard, and the
  advertised `trackAllValues` constructor options.
- Validation: `ThreadLocalTests.*` passed 11/11 on 2026-07-27.  A direct
  C++20/current-.NET 10 probe checked empty factories and
  `IsValueCreated` after disposal; a focused native probe was run with TSan.
- Reference basis: current .NET 10 `ThreadLocal<T>` constructor,
  `IsValueCreated`, `Values`, and disposal contracts.

## SR-AUD-218 — high — Dispose races with Value access through an ordinary shared disposed flag

`disposed_` is an ordinary `bool`: `Dispose()` writes it while
`getValueProperty` and `setValueProperty` read it through `ThrowIfDisposed()`.
A reader running concurrently with `Dispose()` produces a direct TSan report
between `ThreadLocal.hpp:133` and `:65`.  This creates undefined behavior on a
public synchronization utility before it can produce its intended
`ObjectDisposedException` result.

## SR-AUD-219 — medium — empty factories and IsValueCreated-after-Dispose bypass the managed validation contract

The C++ factory constructors store an empty `std::function` without checking
it; first value access fails later with `bad_function_call`, whereas current
.NET construction rejects the null factory immediately with
`ArgumentNullException`.  Separately, the native
`getIsValueCreatedProperty()` bypasses `ThrowIfDisposed()` and prints `0`
after `Dispose`; current .NET throws `ObjectDisposedException`.  The direct
fixture covers only `Value` get/set after disposal and so misses both paths.

## SR-AUD-220 — medium — trackAllValues is accepted but inert and the public Values API is absent

Both tracking constructors retain `trackAllValues_`, but no method reads it
and the class exposes no `Values` property at all.  Current .NET documents
that `trackAllValues=true` preserves every thread's values and makes them
available through `Values`, while `Values` with false tracking throws.  The
native signature therefore advertises a capability that cannot affect behavior
or be observed by a caller.

## Assessment

Per-instance IDs prevent stale-address data corruption and the recursive
factory guard matches the managed error path.  The documented remote-thread
slot retention remains a resource-lifetime trade-off to revisit during
remediation; it is not counted separately here because the three confirmed
findings already cover reachable synchronization, validation, and advertised
tracking behavior.

## Other missing assertions and diagnostics

- Add TSan coverage for `Dispose` racing every public getter/setter/property.
- Assert empty factory construction, `IsValueCreated` and `Values` after
Dispose, and factory exception/retry behavior.
- Add multi-thread tracking checks with worker exit, false-tracking `Values`
diagnostics, and bounded evidence that remote slots are released on disposal.
- Tests should retain the cross-thread stale-address regression but add a
resource-growth diagnostic for the intentional ID-based cleanup trade-off.

## Final assessment

SR-AUD-218 is TSan-confirmed; SR-AUD-219 is confirmed by direct
C++/current-.NET comparison; SR-AUD-220 follows from the public source/API
surface and the installed current-.NET contract.  No production or test source
was changed.


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-219 **factory half only**

SR-AUD-219 is **split by cause** and stays `confirmed` until both halves land.

**Factory half — done.** Cause **T-B** (CCF-011 in `modules/threading`). Both factory
constructors now reject an empty `std::function` with
`System::ArgumentNullException("valueFactory")`, matching .NET's
`ArgumentNullException.ThrowIfNull(valueFactory)` in `ThreadLocal(Func<T>)` and
`ThreadLocal(Func<T>, bool)`. The check is a `requireFactory()` call in the constructor
*body*, the same shape `Lazy<T>` uses (`modules/core/include/System/Lazy.hpp`, ticket #1867):
a check written against the *parameter* would inspect a moved-from `std::function` and report
every factory as empty.

**Measured correction to this finding's stated consequence.** The report says the stored empty
factory "fails later with `bad_function_call`" at first value access. **It did not.**
`getValueProperty()` wrote `std::make_unique<T>(factory_ ? factory_() : T{})`, so an empty
factory silently produced a **default-constructed value** on every thread and raised nothing
at all — probe row `threadlocal.empty_factory_value=normal` before the change. The divergence
from .NET is therefore *worse* than recorded in one respect (a silent wrong value rather than
a loud native exception) and *narrower* in another (no `bad_function_call` was ever reachable
from this constructor). The finding's direction — .NET rejects at construction, this port did
not — is confirmed. The ternary is deliberately left in place: the default and
`bool`-only constructors legitimately leave `factory_` empty and must keep defaulting, which
`ThreadLocal_NonFactoryConstructors_StillDefault` pins.

**IsValueCreated-after-Dispose half — still open, ticket #1956** (cause T-G, approval-gated):
`getIsValueCreatedProperty()` still bypasses `ThrowIfDisposed()` and returns `false` after
`Dispose`, where .NET throws `ObjectDisposedException`.

Evidence: `threadlocal.empty_factory_ctor` and `threadlocal.empty_factory2_ctor` moved from
`normal` to `ArgumentNullException|Value cannot be null. (Parameter 'valueFactory')`; the two
non-factory controls and the real-factory control are unchanged. Tests:
`ThreadingEmptyCallableTests.ThreadLocal_*`.

**SR-AUD-218 and SR-AUD-220 are untouched and remain `confirmed`** — the ordinary `disposed_`
race is cause T-A (ticket #1955) and the inert `trackAllValues`/absent `Values` surface is
cause T-H (ticket #1958).


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-218 → `remediated`

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

`disposed_` is now `std::atomic<bool>`: `Dispose()` performs a release store,
`ThrowIfDisposed()` an acquire load. `sizeof(ThreadLocal<int>)` 56 → 56, `alignof` 8 → 8.
Scenario `threadlocal.disposed` reported one race before and none after.

**SR-AUD-219's IsValueCreated-after-Dispose half is still open** (cause T-G, approval-gated
ticket #1956). The flag is now race-free, but `getIsValueCreatedProperty()` still does not
consult it and still returns `false` after `Dispose` where .NET throws
`ObjectDisposedException`. #1955 made the guard sound; #1956 is what applies it. SR-AUD-219's
factory half landed with #1951, recorded above.

**SR-AUD-220 is untouched and remains `confirmed`** — the inert `trackAllValues` flag and the
absent `Values` surface are cause T-H, design ticket #1958.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
