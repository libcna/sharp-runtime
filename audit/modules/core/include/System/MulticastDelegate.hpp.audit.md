# Audit: `modules/core/include/System/MulticastDelegate.hpp`

## Metadata

- Audit status: AUDITED (60-line derived declaration, fully read).
- Validation: `MulticastDelegateTests.*` passed 25/25 in the 70-test direct
  delegate filter on 2026-07-26.
- Reference basis: local .NET `MulticastDelegate.cs` and CoreCLR
  `MulticastDelegate.CoreCLR.cs` composition behavior.

## Findings

The direct single-target `Clone()` retains the dynamic `MulticastDelegate`
type, as the tests confirm.  In contrast, static Combine and multicast Remove
are implemented in the base class and discard it, producing SR-AUD-118.
The header's claim that multicast machinery lives in `Delegate` accurately
identifies that ownership but hides this concrete-type loss from callers.

## Other missing assertions and diagnostics

- Tests do not clone or remove a combined delegate, assert its dynamic type,
  use a distinct derived delegate class, or validate mismatch errors.
- Same-method list equality and multi-value removal are omitted (SR-AUD-119
  and SR-AUD-120).
- The empty stack-constructed no-op object is a C++ adaptation, not an
  instantiable .NET `MulticastDelegate` value.

## Final assessment

The named hierarchy node and direct clone path are sound, but static
composition fails to preserve its advertised role.  No source or test was
modified during this audit.
