# Audit: `modules/threading/include/System/Threading/LazyInitializer.hpp`

## Metadata

- AUDITED: 58-line `LazyInitializer` pointer publication adapter, including
  default/factory construction, candidate disposal, and `atomic_ref` use.
- Validation: `LazyInitializerTests.*` passed 5/5 on 2026-07-27.  A
  200-round synchronized native probe was built with `-fsanitize=thread`;
  direct C++20/current-.NET 10 probes also exercised an empty value factory.
- Reference basis: current .NET 10 `LazyInitializer.EnsureInitialized`
  behavior and the local header's intended compare-exchange publication model.

## SR-AUD-216 — high — lock-free initialization reads the shared target non-atomically while another caller atomically publishes it

`EnsureInitialized` first evaluates `if (!target)` as an ordinary pointer
read, then writes the same caller-owned pointer through `std::atomic_ref` at
the compare-exchange.  Two synchronized callers initializing one target make
those accesses concurrent.  TSan reports the direct race between the ordinary
read at `LazyInitializer.hpp:37` and the atomic compare-exchange write at
`:41`.  Mixing atomic and non-atomic accesses to the same object is undefined
behavior, so the claimed lock-free replacement for .NET's thread-safe
publication is itself unsafe.

## SR-AUD-217 — medium — an empty native factory is deferred to `bad_function_call` instead of preserving managed null-delegate behavior

`EnsureInitialized(target, std::function<T*()>{})` calls the empty function
only after deciding target is null.  The direct native probe prints
`lazy_emptyFactory=exception:bad_function_call`; the equivalent current-.NET
10 call prints `lazy_emptyFactory=exception:System.NullReferenceException`.
The delayed native-library exception is both a different observable result and
data-dependent: a preinitialized target suppresses it altogether.

## Assessment

The reviewed fixture correctly protects the earlier same-type reentrancy
deadlock regression and checks deletion of a losing candidate only indirectly.
It never races two callers on the same target or supplies an empty native
factory, leaving both findings undetected.

## Other missing assertions and diagnostics

- Add a TSan-targeted two-caller shared-target test for default and factory
  overloads, including one factory that records every constructed candidate.
- Assert factory exception propagation, an empty factory before/after an
  already initialized target, allocator failure, and candidate lifetime.
- The C++ raw-pointer ownership/deletion adaptation needs explicit behavior
  for a loser with externally observable destruction; current .NET documents
  that it does not dispose a losing managed object.

## Final assessment

SR-AUD-216 is TSan-confirmed and SR-AUD-217 is confirmed by direct
C++/current-.NET comparison.  No production or test source was changed.


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-217 → `remediated`

Cause **T-B** of `docs/ThreadingNamespaceReviewPlan.md` — **and the one member of that cause
whose .NET answer is not `ArgumentNullException`.**

Verified against the reference contract this finding itself cites:
`LazyInitializer.EnsureInitialized<T>(ref T? target, Func<T> valueFactory)` performs **no**
null check on the delegate. It is
`Volatile.Read(ref target) ?? EnsureInitializedCore(ref target, valueFactory)`, and the core
simply calls `valueFactory()`. A null delegate therefore raises `NullReferenceException`, and
an already-initialized target short-circuits the call so the fault does not occur at all —
which is exactly the data-dependence this report records as part of the divergence.

The repair reproduces **both** properties rather than overriding them: the port now throws
`System::NullReferenceException` on the path that would have invoked the factory, and leaves
the already-initialized path returning the existing value. Applying the family's usual
`ArgumentNullException`-at-entry spelling here would have left the observable *still*
different from .NET and would additionally have made a call .NET accepts (initialized target,
null factory) start throwing.

What the change closes is the **hierarchy**: `std::bad_function_call` derives from
`std::exception`, not `System::Exception`, so a ported `catch (const System::Exception&)`
could not see it and the process terminated. That is the substance of CCF-011 and it is now
fixed at this site.

Evidence: `lazyinit.empty_factory_null_target` moved from `bad_function_call` to
`NullReferenceException|Object reference not set to an instance of an object.`;
`lazyinit.empty_factory_set_target` stayed `normal`, proving the .NET short-circuit is
preserved; `lazyinit.control_factory` unchanged. Tests:
`ThreadingEmptyCallableTests.LazyInitializer_*`, which also assert the target is still null
after the throw.

**SR-AUD-216 is untouched and remains `confirmed`** — the ordinary `if (!target)` read racing
the `atomic_ref` publication is cause T-A and belongs to ticket #1955.


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-216 → `remediated`

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

Both `EnsureInitialized` overloads now open with `std::atomic_ref<T*> ref(target);` and read
through it — `ref.load(std::memory_order_acquire)` for the guard **and** for the returned
reference — instead of reading `target` directly. This is the C++ spelling of the
`Volatile.Read(ref target)` that .NET's own `EnsureInitialized` opens with, and it pairs with
the `compare_exchange_strong` that publishes the value.

### Correction: two racing reads, not one

The finding names the `if (!target)` guard. TSan reported the **return statement** as well:
`return *target;` was a second ordinary read of the same object another caller publishes
atomically. Six of the pre-fix run's thirteen reports came from this one scenario, which is
what made the second site visible. Both are repaired.

`LazyInitializer` declares no data members and has a deleted constructor, so no layout question
arises here at all.

`ThreadingSharedStateTests.LazyInitializer_ConcurrentCallersAgreeOnOneInstance` runs 200 rounds
of two threads racing on a fresh target and asserts that both observe the *same* published
instance and that it is the one left in `target` — .NET's documented contract that the loser's
candidate is discarded rather than returned.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
