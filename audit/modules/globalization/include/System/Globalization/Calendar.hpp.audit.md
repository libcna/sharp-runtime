# Audit: `modules/globalization/include/System/Globalization/Calendar.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: current .NET [`Calendar`](https://learn.microsoft.com/en-us/dotnet/api/system.globalization.calendar?view=net-10.0)
  is abstract.

## Assessment

Although its own comment calls it an abstract base class, the C++ type has no
pure virtual member and is publicly constructible.  It exposes a concrete
Gregorian fallback as an ordinary `Calendar`, a managed construction that is
not possible.  `CalendarTests.cpp` repeatedly constructs this invalid public
shape and therefore locks it in.

### SR-AUD-281 — medium — Calendar is publicly constructible with a fabricated Gregorian fallback

The direct probe constructs `Calendar` and reports `IsLeapYear(2024) == true`.
Current .NET declares `Calendar` abstract; callers must select a concrete
calendar.  The fallback can silently bypass a caller's intended calendar
system instead of producing a compile-time/API-shape failure.

## Finding references

- SR-AUD-281 — medium — public base-class shape and behavior diverge from .NET.

## Other missing assertions and diagnostics

- Compile-check that the base type cannot be directly instantiated after the
  API shape is corrected.
- Exercise concrete calendar polymorphism rather than using `Calendar` as a
  Gregorian substitute.

## Final assessment

SR-AUD-281 applies.
