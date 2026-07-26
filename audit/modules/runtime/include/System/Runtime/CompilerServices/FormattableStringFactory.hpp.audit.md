# Audit: `modules/runtime/include/System/Runtime/CompilerServices/FormattableStringFactory.hpp`

## Metadata

- Audit status: AUDITED (39-line public header, fully read).
- Supporting validation: integration
  `FormattableStringFactoryTests.*` passed 3/3 on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-formattable-string-audit-probe.cpp` creates
  an empty-format instance and prints `0,0` (zero arguments, zero output length).

## Assessment

The factory is intentionally a thin by-value adapter over the local
string-only FormattableString representation.  Its implementation correctly
permits an empty C++ string, consistent with current .NET factory source which
rejects `null` format/argument references, not an empty valid format.  C++
`std::string` has no null string state, so the code's behavior is the sensible
local translation.

## SR-AUD-059 — low — factory documentation promises an exception for a valid empty format

The `Create` documentation says it throws `std::invalid_argument` when
`format` is empty (`FormattableStringFactory.hpp:32`), but the implementation
unconditionally forwards it (`:34-36`).  The local current .NET
`FormattableStringFactory.cs` checks only nullness, so empty format is valid.
The standalone probe confirms the implementation returns an empty, zero-
argument formattable string.  This is a public documentation/diagnostic
contradiction, not a reason to add an empty-string rejection.

## Other missing assertions and diagnostics

- No test constructs an empty format, so both the implementation's valid
  behavior and the contradictory `@throws` claim escaped review.
- The factory cannot distinguish null from empty `std::string`; this deliberate
  C++ representation limit should be stated alongside the null-only .NET
  requirement.

## Final assessment

The factory behavior is compatible with empty .NET composite format text, but
its exception documentation is false.  No source or test was modified during
this audit.
