# Audit: `modules/core/include/System/LocalDataStoreSlot.hpp`

## Metadata

- AUDITED: 54-line public storage wrapper, fully read.
- Validation: `LocalDataStoreSlotTests.*` passed 6/6 in the combined 14-test
  `ContextBoundObjectTests.*:LocalDataStoreSlotTests.*:MarshalByRefObjectNewTests.*`
  Core.Base filter on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-remoting-audit-probe` sets `parent`,
  writes `child` from another thread, joins, and prints
  `slot_value_after_child=child`.
- Reference basis: local .NET `System/LocalDataStoreSlot.cs:7-21` and
  `System/Threading/Thread.cs:502-507,660-727`.

## SR-AUD-129 — medium — LocalDataStoreSlot stores one shared std::any and has no Thread-local slot API

Current .NET gives `LocalDataStoreSlot` an internal constructor containing a
`ThreadLocal<object?>`; public callers obtain it only through
`Thread.AllocateDataSlot`/named-slot APIs and access data through
`Thread.GetData`/`SetData`. The C++ header instead makes a default-constructible
object that owns one `std::any` value. The probe shows a child write replacing
the parent's stored value after join. Repository search finds no C++ Thread
allocate/get/set/named-slot API at all.

The header documents this limitation, but the public type still claims to be
the .NET counterpart and its per-object `getData`/`setData` surface cannot
provide the advertised local-data semantics. It also supplies no
synchronization policy for concurrent access. Implement thread-indexed slot
storage behind the Thread APIs, or demote/rename the wrapper as an explicit
non-thread-local `std::any` holder.

## Other missing assertions and diagnostics

- No test uses two threads, named slots, missing slot validation, null value,
  disposal/finalization, or named-slot lifecycle behavior.
- No first-party Thread surface includes this header, so no integration test
  validates the promised Thread.GetData/SetData correspondence.
- The `noexcept` setters can terminate if `std::any` assignment throws; tests
  use only small ordinary copyable values and do not state the failure policy.

## Final assessment

The wrapper behaves consistently as a single `std::any`, but that is the
confirmed SR-AUD-129 semantic mismatch, not a LocalDataStoreSlot equivalent.
No source or test was modified during this audit.
