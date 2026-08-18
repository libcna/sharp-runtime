<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — an all-zero magnitude never overflows (ticket #2356)

*2026-08-18.* `Int32::Parse("0E30", NumberStyles::Any)` threw `OverflowException`. .NET returns
`0`. So does this port now.

Landed under `docs/StandingApprovals.md` SA-5 (derivation from the reference). A **widening**: no
input that parsed before stops parsing. No signature, layout, vtable or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Parse("0E30")` | `OverflowException` | `0` |
| `Parse("0E100000000")` | `OverflowException` | `0` |
| `Parse("0E-1")`, `"0E-2"`, `"00E-2"`, `"0.0"` | `0` | `0` — unchanged |
| `Parse("65E-1")`, `"1E100000000"` | `OverflowException` | unchanged |
| every input with a nonzero digit | — | **unchanged** |

## 2. Why — and why the recorded answer was wrong twice

The deferred ticket asked whether an all-zero magnitude with a **non-positive scale** overflows
*"as .NET's source says it does"*. It does not, at any scale, and the pin that guarded the
question asserted the opposite for the positive case.

The pin's own comment named the reason it was unreliable — *"the reading is an unexecutable source
trace"* — and the line the trace had missed is `Number.Parsing.Common.cs:259-268`, which runs
after the trailing-token loop:

```csharp
if ((state & StateNonZero) == 0)
{
    if (number.Kind != NumberBufferKind.Decimal)                  number.Scale = 0;
    if (number.Kind == NumberBufferKind.Integer && !StateDecimal) number.IsNegative = false;
}
```

`StateNonZero` is set only inside `if (ch != '0' || (state & StateNonZero) != 0)`
(`Number.Parsing.Common.cs:103`), so an all-zero magnitude never sets it — and **the scale it
would have overflowed on is discarded before `TryNumberBufferToBinaryInteger` ever sees it.**

**A middle answer was reached and is also wrong**, so it is recorded here rather than left as a
trap for the next reader. Because line 103 makes a *leading* zero skip the whole block, that zero
does not advance the scale either. So the count of zeros written cannot matter: `"0E-2"` and
`"00E-2"` must agree, and they do. An implementation that distinguishes them is wrong in one
direction or the other.

## 3. The second half of the same normalisation

.NET also drops the **sign** — but only for an integer buffer that saw no decimal separator. So
`UInt32.Parse("-0")` is `0` in .NET while `UInt32.Parse("-0.0")` overflows.

This port does not reproduce that, and the reason is a pre-existing, deliberate deviation rather
than an oversight: the unsigned grammar rejects `-` outright, so `UInt32::Parse("-0")` and
`UInt32::Parse("-1")` are both `FormatException` where .NET answers `0` and `OverflowException`.
Repairing the `"-0"` row alone would align one spelling and leave the other diverging, which is
worse than the present consistent rule. **Ticket #2362** holds the family.

One honest consequence, measured: the `!sawDecimal` guard in the signed core is therefore
**unobservable today** — a mutation that drops it is not caught by any test, and was measured not
to be. It is kept as a faithful transcription that becomes live when #2362 lands, and the test
says so rather than implying a defence it cannot provide.

## 4. To migrate

Nothing to do. Code that caught `OverflowException` around a zero-valued literal with a large
exponent will simply stop seeing it.
