# Audit: `modules/core/src/System/Delegate.cpp`

## Metadata

- Audit status: AUDITED (166-line implementation, fully read).
- Validation: direct delegate suites passed 70/70 and the standalone probe
  completed on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-delegate-audit-probe` is compiled
  against `build/libsharp_runtime_core.a` and prints the four values recorded
  in the companion declaration audit.
- Reference basis: local .NET `Delegate.cs:18-51,158-183` and CoreCLR
  `MulticastDelegate.CoreCLR.cs:212-292,315-390`.

## SR-AUD-118 — medium — Delegate composition accepts different concrete delegate types and converts every combined/removal result into base Delegate

`Combine` neither compares dynamic types nor asks a concrete delegate to create
the result; it always constructs `new Delegate(MulticastTag{}, ...)`
(`Delegate.cpp:106-122`).  Multicast `Remove` uses the same base construction
(`:143-150`).  Current .NET rejects mismatched delegate types before combining
or removing, and `MulticastDelegate.CombineImpl` creates the same runtime
delegate type.  The probe combines `MulticastDelegate` objects and prints
`combined_dynamic_type_preserved=0`; it also combines a `MulticastDelegate`
with a distinct C++ subclass and prints `mixed_types_accepted=1` rather than
an argument error.

The 70 green direct tests verify only target invocation and list lengths.  The
MulticastDelegate fixture even labels Combine as producing a shared base
Delegate, so it does not assert the required dynamic type or mismatch failure.

## SR-AUD-119 — medium — Delegate equality compares multicast entries by shared-pointer identity instead of delegate equality

Single plain function pointers receive a special value-equality path
(`Delegate.cpp:34-48`), but multicast lists compare only `shared_ptr::get()`
values (`:50-53`) and hash those addresses (`:60-67`).  CoreCLR's
`EqualInvocationLists` calls each entry's `Equals`, so independently-created
but equal delegates form equal invocation lists.  The probe combines two
separate `[First, Second]` lists and prints `equal_function_lists_equal=0`.

This also leaves the eventual equality/hash repair coupled: pointer-folded
hashes will not represent the required value-equal lists.  Existing tests only
compare two lists built from the exact same entry pointers.

## SR-AUD-120 — medium — Delegate.Remove cannot remove a multicast value's last matching invocation-list subsequence

For a multicast source, `Remove` scans individual entries and compares each to
the whole `value` object (`Delegate.cpp:143-151`).  It never searches a
multi-entry value as a trailing sequence.  Current CoreCLR scans backward over
the candidate invocation-list range and removes the last equal subsequence
(`MulticastDelegate.CoreCLR.cs:369-390`).  The probe removes `[First, Second]`
from `[First, Second, First, Second]` and prints
`remove_multicast_subsequence_size=4` instead of the expected remaining size
two; `RemoveAll` inherits the fault.

## Other missing assertions and diagnostics

- No test covers same-type enforcement, dynamic result preservation, equal
  independently allocated multicast entries, multi-entry Remove/RemoveAll,
  or mismatch diagnostics.
- `GetInvocationList` has the documented `shared_from_this` requirement but
  lacks a checked/diagnosed stack-object route.
- The function-pointer-to-`void*` hash conversion is implementation-dependent
  C++ territory; no portability build asserts it on the supported compilers.

## Final assessment

Basic invocation works, but the composition/equality/removal contract diverges
on reachable ordinary delegate values.  No source or test was modified during
this audit.
