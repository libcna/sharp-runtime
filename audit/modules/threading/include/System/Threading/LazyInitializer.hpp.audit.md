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
