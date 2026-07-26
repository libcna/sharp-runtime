# Audit: `modules/runtime/include/System/Runtime/CompilerServices/ConditionalWeakTable.hpp`

## Metadata

- AUDITED: 312-line inline weak-key table implementation, fully read.
- Validation: `ConditionalWeakTableTests.*` passed 7/7 on 2026-07-27.
- Reference/probes: local current-.NET `ConditionalWeakTable.cs`; a C++20
  standalone probe linked against Runtime/Core prints `after_reset=1`, retains
  a non-current snapshot value until enumerator destruction, and successfully
  uses `ConditionalWeakTable<int, int>`.  The matching managed Reset probe
  prints `after_reset=False`; compiling the scalar managed table fails CS0452.

## SR-AUD-161 — medium — snapshot enumerators rewind and retain non-current values unlike ConditionalWeakTable enumeration

C++ copies every `Entry` (including its strong `ValuePtr`) into the enumerator
snapshot and `Reset()` resets its index to zero.  The C++ probe advances once,
calls Reset, then prints `after_reset=1`; it also creates an enumerator before
the first advance, drops the only external key/value references, calls Clear,
and prints `retained_by_enumerator=1` until the enumerator is destroyed.

Current .NET's enumerator implements `Reset()` as an empty method.  Its public
enumeration remarks explicitly promise that an enumerator will not extend the
lifetime of any pair other than `Current`.  The native snapshot therefore both
replays entries when Reset is used and retains all captured value graphs even
when no entry has become Current and the table has released the pair.

## SR-AUD-162 — medium — ConditionalWeakTable admits scalar generic arguments that the managed API forbids

Current .NET constrains both `TKey` and `TValue` to reference types.  The C++
template has no equivalent constraint: the probe instantiates
`ConditionalWeakTable<int, int>`, inserts `shared_ptr<int>` key/value objects,
and prints `scalar_table=1:2`.  The matching managed declaration fails CS0452
because `int` is not a reference type.

This changes the public generic domain and permits weak association semantics
for native scalar allocations that callers cannot express in the managed API.

## Assessment

For supported shared-object arguments, identity lookup, null-key checks,
expired-key cleanup, factory-outside-lock behavior, duplicate handling, and
the current public AddOrUpdate/TryAdd surface are coherent.  Null values are
intentionally permitted by both this header and current .NET; they are not a
finding.  The explicit `shared_ptr`/`weak_ptr` lifetime model is a necessary
native adaptation, but it does not justify the two enumerator/constraint
contract changes above.

## Other missing assertions and diagnostics

- Direct tests omit Reset, Current before/after traversal, enumerator value
  retention after key expiry/Clear, and disposal/ownership of the raw returned
  enumerator.
- They omit scalar/non-class template instantiation, null values, null factory
  callbacks, factory exceptions, TArg factory overloads, GetValue and
  GetOrCreateValue, Clear, and competing-factory cardinality under race.
- The eight-thread test checks returned-pointer convergence only; it is not a
  stress/race diagnostic for cleanup, enumeration, or factory side effects.

## Final assessment

The normal weak-key table operations are useful and green, but its snapshot
enumerator and unconstrained generic domain diverge observably from current
.NET.  No source or test was modified.
