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

### Status: CONFIRMED (DESIGN-COMPLETE) — approval-bound, #2298 `needs_user` (#2296 review, 2026-08-11)

**Reproduces as filed**, and the "no C++ Thread allocate/get/set/named-slot API
at all" observation was re-measured and holds: the public default constructor and
the `getData`/`setData` pair are a project-owned surface under a .NET name, since
.NET's own constructor is internal and its doors — `AllocateDataSlot`,
`AllocateNamedDataSlot`, `GetData`, `SetData`, `FreeNamedDataSlot` — do not exist
here. Consumers measured: **zero** production, one test file.
`sizeof(LocalDataStoreSlot)` is 16.

**Premise correction — the third "Other missing assertions" bullet is wrong.** It
states the `noexcept` setters can terminate if `std::any` assignment throws.
Measured: `std::any`'s move assignment and `reset()` are **`noexcept` by the
standard**, and `setData`'s parameter is taken **by value**, so any allocation
for it happens at the call site, outside this function's exception specification.
`noexcept(slot.setData(std::move(v)))` is 1 and sound. **No defect ticket was
filed**, unlike the genuinely unsound `noexcept` at #2292. What the bullet
understates is the rest of it: there is no synchronization policy at all, so two
threads touching one slot with at least one write is a data race with undefined
behaviour.

**Routes priced (plan §3.2):** thread-indexed storage behind a new `Thread` slot
API is a substantial new public surface **and a behaviour change**, since
`getData`/`setData` stop being shared; making the constructor non-public on top
of that is the most faithful and adds a public source break; demoting or renaming
is a source break that removes the .NET name. All three are user decisions and
none was taken.

**Taken anyway, true under every outcome (#2300):** the header now states that
.NET's constructor is internal and this port has no `Thread` slot API, that the
one value is shared by every thread, that concurrent access is an unsynchronized
data race, and that the `noexcept` specifications are sound and why. It also
corrects the pre-existing "Use std::thread_local", which named no C++ facility.
**This does not close the finding** — the slot is still one shared value. No
two-thread test was added, deliberately: it would pin the sharing that #2298 may
remove.

**Not a family with SR-AUD-128 or SR-AUD-126.**
`docs/CoreMarshalSlotAndFuncShapePlan.md` §3.

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
