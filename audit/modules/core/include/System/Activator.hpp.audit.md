# Audit: `modules/core/include/System/Activator.hpp`

## Metadata

- Audit status: AUDITED (72-line template implementation, fully read).
- Validation: no dedicated first-party Activator fixture exists.  The isolated
  `/tmp/sharp-runtimervc-activator-audit-probe` compiles and prints
  `value_size=2`, `value_items=3,7`, `pointer_size=3`, and
  `pointer_item0=7` for identical `std::vector<int>(3, 7)` inputs.
- Reference basis: local .NET `Activator` generic construction contract and
  standard C++ direct-initialization/initializer-list selection rules.

## SR-AUD-109 — medium — value CreateInstance changes forwarded constructor arguments through braced initialization

`CreateInstance<T, Args...>` returns
`T{std::forward<Args>(args)...}` (`Activator.hpp:39`) even though its contract
says that it forwards constructor arguments.  Braces prefer an
`initializer_list` constructor: the probe's value call
`CreateInstance<std::vector<int>>(3, 7)` prints `value_size=2` and
`value_items=3,7`, rather than the requested three copies of seven.  The
otherwise equivalent `CreateInstancePtr` calls `std::make_unique<T>(args...)`
and prints `pointer_size=3`, `pointer_item0=7`.

This makes the two published Activator creation forms observably disagree and
silently chooses a different object construction than the caller specified.
No direct test instantiates either template with an initializer-list-capable
type.

## Other missing assertions and diagnostics

- No first-party test compiles default construction, forwarding, move-only
  arguments, explicit constructors, initializer-list overloads, constructor
  exceptions, or the value/pointer equivalence boundary.
- The generic `CreateInstance<T>()` constraint and behavior differ from .NET's
  managed generic/new constraints and nullable/reference-type activation;
  reflection overloads are explicitly absent.
- The header relies on transitive availability of forwarding utilities through
  `<memory>`; no standalone public-header include test guards that dependency.

## Final assessment

The reflection omission is documented, but the implemented value template does
not honor its own forwarding contract.  No source or test was modified during
this audit.
