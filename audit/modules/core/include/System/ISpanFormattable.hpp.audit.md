# Audit: `modules/core/include/System/ISpanFormattable.hpp`

## Metadata

- Audit status: AUDITED (61-line public interface, fully read).
- Supporting validation: `ISpanFormattableTests.*` and
  `ISpanFormattableTests2.*` passed 5/5 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.
- The supporting fixture is the `IFormattable` / `ISpanFormattable` section of
  `modules/core/tests/System/SystemTypesRemainingTests.cpp`; that larger test
  source remains pending its own complete, file-wide audit.

## Assessment

This is a documented C++ adaptation of .NET's span-formatting interface.  It
preserves virtual polymorphism, a const formatting operation, buffer-capacity
failure reporting, and the `IFormattable` relationship.  The overload with an
`IFormatProvider` has a deliberately safe default: it ignores the optional
provider and dispatches to the required four-argument operation.  The
implementation owns no buffer arithmetic or output state, so a formatter is
responsible for validating and writing its supplied buffer.

The C++ API deliberately substitutes a `char*` plus `size_t` capacity and a
`std::string` format for .NET's `Span<char>`, `int`, and
`ReadOnlySpan<char>`.  The header explicitly calls this an approximation; it
is a documented adaptation boundary, not an independently confirmed defect.

## Positive findings

- The direct fixture confirms successful output, short-buffer failure with a
  zero write count, and polymorphic conversion to `IFormattable`.
- The base provider overload is virtual and forwards without allocating or
  retaining the provider pointer.

## Other missing assertions and diagnostics

- The provider overload is now invoked through an `ISpanFormattable` reference
  by `InterfaceTests2.cpp`, but only with `nullptr`; no sentinel provider tests
  the documented intentional provider-ignorance.
- No fixture documents the C++ name-hiding rule: a derived four-argument
  override hides the five-argument base overload for calls made on the
  concrete derived type unless it adds a `using ISpanFormattable::TryFormat`
  declaration.  Calls through the interface remain valid.
- No diagnostic explains how a null destination with nonzero capacity should
  be handled; the adapted interface leaves that validation to implementations.

## Final assessment

The small interface is internally coherent and its intentional representation
differences are documented.  No evidence-backed defect was found and no source
or test was modified during this audit.
