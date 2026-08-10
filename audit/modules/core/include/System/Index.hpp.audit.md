# Audit: `modules/core/include/System/Index.hpp`

## Metadata

- Audit status: AUDITED (109-line public header, fully read).
- Supporting validation: dedicated `IndexTests2.*` passed 13/13 and the
  complementary `IndexTests.*` smoke cases in the pending
  `SystemTypesRemainingTests.cpp` passed 7/7 on 2026-07-26.
- Reproducer:
  `/tmp/sharp-runtimervc-index-range-audit-probe.cpp`, compiled with
  `-fsanitize=undefined`, reported the arithmetic at `Index.hpp:61`.

## SR-AUD-057 — high — unchecked .NET index-offset semantics use signed C++ overflow

`Index::GetOffset` directly evaluates `length - value_` for an end-based
index (`Index.hpp:61`).  The public constructor permits every nonnegative
`intcs` value and `GetOffset` deliberately does not validate its public
`length`, matching .NET's performance-oriented API shape.  Thus
`Index::FromEnd(INT_MAX).GetOffset(INT_MIN)` reaches signed C++ overflow.

The standalone UBSan probe reports:

```
modules/core/include/System/Index.hpp:61:29: runtime error: signed integer
overflow: -2147483648 - 2147483647 cannot be represented in type 'int'
```

The locally checked current .NET `Index.cs` source also deliberately skips
range validation, but C# default integer arithmetic has defined unchecked
two's-complement behavior.  A C++ port cannot preserve that decision by
executing signed overflow.  `Range::GetOffsetAndLength` consumes this operation
and independently overflows during its resolved-length subtraction; see the
paired `Range.hpp` report.

## Other missing assertions and diagnostics

- Dedicated tests cover ordinary out-of-range offsets but no extrema or
  negative-length call under UBSan, so the overflow remains invisible in the
  green 13/13 filter.
- The tests do not verify the C++-defined representation expected for the
  .NET unchecked result; remediation needs an unsigned calculation then an
  explicit well-defined conversion policy, not added validation that changes
  the documented no-check behavior.

## Final assessment

The normal Index shape and simple behavior are correct, but its documented
unvalidated arithmetic needs a C++-defined implementation.  No source or test
was modified during this audit.
