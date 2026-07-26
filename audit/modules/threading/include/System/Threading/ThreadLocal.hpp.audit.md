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
