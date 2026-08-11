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

### Status: REMEDIATED (#2265 review, #2266 implementation, 2026-08-11)

Reproduced exactly: `CreateInstance<std::vector<int>>(3, 7)` built `{3, 7}`
where `CreateInstancePtr` built `{7, 7, 7}`. Perfect forwarding was never at
fault — `std::forward` was and remains correct, and value category, move-only
arguments, explicit constructors and constructor exception propagation are
measured identical throughout. The defect is entirely the choice of `{}` over
`()`, because a braced-init-list considers `initializer_list` constructors
before every other candidate.

**Premise correction to this report: brace initialization is NOT the complete
cause, and the repair this report implies is a public source break.** Twelve
type categories were measured in one translation unit. Switching
unconditionally to parentheses stops four categories that compile today from
compiling — `std::array<int, 3>(1, 2, 3)`, nested-aggregate brace elision,
`std::vector<int>(1, 2, 3)`, and any type reachable only through an
`initializer_list` constructor — and additionally starts silently **accepting**
narrowing conversions that braces reject (`Agg(1, 2.5)`, `int(2.5)`). Under the
SR-AUD-063 precedent that is an approval boundary, so it was **not**
implemented; it remains available to a future ticket carrying an approval, and
no ticket is opened for it.

Ticket #2266 instead forwards to a constructor only where the type has one to
forward to: `is_class_v<T> && !is_aggregate_v<T> && is_constructible_v<T,
Args...>` selects `T(...)`, and everything else keeps `T{...}`. An aggregate has
no constructor to forward to, and braces are the only spelling that carries
brace elision and still refuses to narrow. The repaired contract is exact:
**wherever both creation forms are well-formed they now construct identically**,
and where only the value form is well-formed it keeps today's behaviour. That
domain asymmetry is pre-existing — `make_unique` uses parentheses and already
rejected all four of those categories — and option A would have "resolved" it by
deleting capability from the value form. Zero compile-domain losses; one
widening, `std::string(3, 'x')`, whose braced form is ill-formed once the `3` is
a forwarded parameter rather than a constant, and which the pointer form already
accepted — so the widening removes a value/pointer disagreement.

This report's closing claim that no direct test instantiates either template
with an initializer-list-capable type was accurate: the seven pre-existing
`ActivatorTests` all use ordinary constructors, none changes behaviour, and all
seven pass unmodified. There is **no first-party production call site** of either
overload, so the internal blast radius is zero.

+14 tests in `ActivatorConstructionPathTests.cpp`, plus
`test/consumer/core_activator_construction_negative.cpp` (2 sites) pinning that
narrowing stays rejected for aggregates and scalars. Four mutations, all caught,
none equivalent: reverting to braces; unconditional parentheses (which fails on
`std::array` **and** makes both negative sites compile — the repository's own
tooling measuring the source break); and dropping either conjunct, each of which
flips exactly one negative site. A SFINAE probe on `CreateInstance` was written
and discarded as vacuous — both overloads are unconstrained, so an ill-formed
body is a hard error, not a substitution failure; the same error had made an
earlier `make_unique` domain measurement wrong, and every such claim was
re-measured by real instantiation. Header-only templates: no layout, vtable,
`noexcept` or symbol change. `docs/CoreActivatorConstructionPathPlan.md`.

The third bullet below — that the header relies on transitive availability of
forwarding utilities through `<memory>` — is addressed in passing: `<type_traits>`
and `<utility>` are now included explicitly. It was never a live fault; the
`modules/core` self-sufficiency sweep in
`docs/CoreModuleHandleHeaderSelfSufficiencyPlan.md` §3.1 measured this header
compiling standalone.

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
