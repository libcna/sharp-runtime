<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a negative unsigned value now overflows instead of failing to parse (ticket #2362)

*2026-08-18.* `Byte`, `UInt16`, `UInt32` and `UInt64`'s `Parse`/`TryParse` now accept a
negative-indicating token grammatically and reject it as a **range** failure, matching .NET.
`UInt32::Parse("-1")` raises `OverflowException` where it used to raise `FormatException`, and
`UInt32::Parse("-0")` returns **0** where it used to throw at all.

Landed under `docs/StandingApprovals.md` SA-5. No signature change, no layout change, no
`noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `UInt32::Parse("-1")` | `FormatException` | `OverflowException` |
| `UInt32::Parse("-0")` | `FormatException` | **`0`** |
| `UInt32::Parse("-000")`, `"-0E30"` | `FormatException` | `0` |
| `UInt32::Parse("-0.0")` (`NumberStyles::Number`) | `FormatException` | `OverflowException` |
| `UInt32::Parse("(1)")` (`AllowParentheses`) | `FormatException` | `OverflowException` |
| `UInt32::Parse("(0)")` | `FormatException` | `0` |
| `UInt32::Parse("1-")` (`AllowTrailingSign`) | `FormatException` | `OverflowException` |
| `UInt32::Parse("(1")` — unclosed | `FormatException` | `FormatException` (unchanged) |
| `UInt32::Parse("--1")`, `"-1-"` | `FormatException` | `FormatException` (unchanged) |
| `UInt32::Parse("+42")` | `42` | `42` (unchanged) |
| every input with no negative token | — | **unchanged** |

`TryParse` still returns `false` for every row that throws, so a caller using `TryParse` sees no
difference at all. Only the exception **type** moved, plus the one row that stopped failing.

## 2. Why the family had to move together

This was a documented, deliberate deviation, recorded on `TryParseUnsignedCore` for about a year.
The argument for it was that the only practical effect was *which exception type a clearly
invalid input throws*.

**That argument was wrong on its own terms, and the reference is why.** `"-0"` is not a clearly
invalid input. It is a **valid** one that returns `0`:

```csharp
// Number.Parsing.Common.cs:259-268
if ((state & StateNonZero) == 0)
{
    if (number.Kind != NumberBufferKind.Decimal)                          number.Scale = 0;
    if ((number.Kind == NumberBufferKind.Integer) && (state & StateDecimal) == 0)
        number.IsNegative = false;
}
```

So the old rule did not merely substitute one exception for another — it **rejected an input .NET
accepts**. And the two rows could not be separated: repairing `"-0"` alone would have aligned one
spelling while leaving `"-1"` diverging, which is worse than the old consistent rule. The grammar,
the all-zero normalisation and the negative-is-overflow rejection therefore landed in one change.

The rejection itself is `Number.Parsing.cs:157`:

```csharp
if ((i > TInteger.MaxDigitCount) || (i < number.DigitsCount)
    || (!TInteger.IsSigned && number.IsNegative) || number.HasNonZeroTail)
```

Note it sits in the **same disjunction** as the digit-count overflow, so a caller cannot tell the
two apart — both are `OverflowException` — which is why the order between them in this port is
free.

## 3. The asymmetry is .NET's, not a convenience

`"-0"` is `0` and `"-0.0"` overflows, because a decimal separator sets `StateDecimal` and the sign
is then **not** cleared. That single `!sawDecimal` guard is the only thing separating the two rows.

Ticket #2356 transcribed the same guard into the **signed** core and recorded honestly that its
mutation was *not caught* — negating zero is zero either way, so it was unobservable there. It is
observable here, and `Fix2362_ANegativeUnsignedValueOverflowsAndMinusZeroIsZero` is the test that
makes it so.

## 4. To migrate

Catch `OverflowException` as well as `FormatException`, or catch `SystemException`:

```cpp
// before
try { value = UInt32::Parse(text); }
catch (const System::FormatException&) { value = 0; }

// after
try { value = UInt32::Parse(text); }
catch (const System::FormatException&) { value = 0; }
catch (const System::OverflowException&) { value = 0; }
```

Code that already used `TryParse` needs no change.

## 5. One mutation that is not a mutation

Reinstating the old grammar-level rejection **immediately before the digit scan** changes nothing
and is not evidence of a gap: the leading-token loop has already consumed the `'-'` by then, so
the check never fires. This is the same shape #2138 recorded — an allow-list and a deny-list are
equivalent once the token is gone. The meaningful mutation is removing the acceptance from the
loop itself, and that is caught by four tests.

| Mutation | Caught |
|---|---|
| Drop the negative-is-overflow rejection | ✅ (4 tests) |
| Drop the sign half of the all-zero normalisation | ✅ (2 tests) |
| Clear the sign unconditionally, ignoring `sawDecimal` | ✅ |
| Leading loop stops accepting `'-'` | ✅ (4 tests) |
| Leading loop stops accepting `'('` | ✅ (2 tests) |
| Drop the unclosed-paren check | ✅ |
| Reinstate the grammar rejection after the loop | **no-op, see above** |

## 6. Downstream, measured

Per SA-2 condition 5: neither `cna` nor `mobile-eggbert` calls `Byte`, `UInt16`, `UInt32` or
`UInt64` `Parse`/`TryParse` — **zero sites in both**. Neither repository was modified.
