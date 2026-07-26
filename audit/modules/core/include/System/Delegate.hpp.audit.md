# Audit: `modules/core/include/System/Delegate.hpp`

## Metadata

- Audit status: AUDITED (245-line public delegate base and enumerator, fully
  read with its implementation and four direct fixtures).
- Validation: `DelegateTests.*:MulticastDelegateTests.*:MulticastActionTests.*`
  passed 70/70 on 2026-07-26; the related Batch14 filter passed 25/25.
- Reproduction: `/tmp/sharp-runtimervc-delegate-audit-probe` prints
  `combined_dynamic_type_preserved=0`, `equal_function_lists_equal=0`,
  `remove_multicast_subsequence_size=4`, and `mixed_types_accepted=1`.
- Reference basis: local .NET `System/Delegate.cs:12-24,158-183` and CoreCLR
  `MulticastDelegate.CoreCLR.cs:212-292,315-390`.

## Findings

The public Combine/Remove surface has no same-concrete-delegate-type guarantee
and returns `shared_ptr<Delegate>`, while the implementation creates a base
`Delegate` for every nontrivial result.  This underlies SR-AUD-118.  Its
enumerator is otherwise a reasonable allocation-owning C++ adaptation of the
current .NET iterator.

## Other missing assertions and diagnostics

- `GetInvocationList()` requires `shared_from_this` for a single target and
  throws `std::bad_weak_ptr` for a stack object.  The header documents this
  constraint, but no test records the native exception or offers a safer error.
- A default-constructed no-op Delegate has no direct CLR analogue because the
  .NET base is abstract.  Tests do not distinguish it from a valid single
  target in `HasSingleTarget` or invocation-list behavior.
- `DynamicInvoke`, `Method`, target-object tracking, and reflection creation
  are explicit permanent limitations; no test covers diagnostic text for the
  unsupported route.

## Final assessment

The API provides useful C++ invocation mechanics, but composition does not
preserve delegate type or the .NET value contract.  No source or test was
modified during this audit.
