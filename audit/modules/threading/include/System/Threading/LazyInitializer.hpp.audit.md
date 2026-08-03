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
