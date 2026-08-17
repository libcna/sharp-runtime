<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the integer parsers honour `NumberStyles::AllowExponent` (ticket #2268)

*2026-08-17.* All eight integer wrappers — `SByte`, `Byte`, `Int16`, `UInt16`, `Int32`,
`UInt32`, `Int64`, `UInt64` — now parse an exponent when the style allows one. They previously
rejected `"1E2"` with a `FormatException`, including under `NumberStyles::Any`.

Landed under `docs/StandingApprovals.md` SA-5. **This is a widening**: nothing that parsed
before parses differently, and no signature, layout, vtable or `noexcept` changed.

---

## 1. What changed

| Input | Style | Was | Is |
|---|---|---|---|
| `"1E2"`, `"1E+2"`, `"1e2"` | `AllowExponent` | `FormatException` | `100` |
| `"1E0"` | `AllowExponent` | `FormatException` | `1` |
| `"-1E2"` | `+ AllowLeadingSign` | `FormatException` | `-100` |
| `"(1E2)"` | `+ AllowParentheses` | `FormatException` | `-100` |
| `"100E-2"` | `AllowExponent` | `FormatException` | `1` |
| `"1.5E1"` | `Any` | `FormatException` | `15` |
| `"1,000E-3"` | `Any` | `FormatException` | `1` |
| `"65E10"`, `"2E10"`, `"65E-1"`, `"1E-2"` | `AllowExponent` | `FormatException` | `OverflowException` |
| `"1E"`, `"1E+"`, `"E2"` | `AllowExponent` | `FormatException` | `FormatException` |
| `"1E23"` | `Integer` (no flag) | `FormatException` | `FormatException` |
| anything without an `E` | any | — | **unchanged** |

Note the middle block: an exponent that *is* well-formed but denotes something the type cannot
hold moves from `FormatException` to `OverflowException`. That is the correct .NET exception and
is pinned by .NET's own suite.

## 2. Why this ticket was blocked, and what unblocked it

The gap was never disputed — `NumberStyles::Any` is `0x1FF` and includes `AllowExponent`. What
was missing was .NET's exact rule, because an exponent grammar has to agree on how the exponent
folds into `number.Scale` and how that interacts with the "significant digits exceed scale"
overflow rule. Guessing it would have replaced a documented, uniform absence with an
undocumented partial divergence.

The model, from `Number.Parsing.Common.cs:98-232` and `Number.Parsing.cs:150-208`:

* the buffer holds only **significant** digits — leading zeros are never stored, and for an
  integer buffer a **trailing** zero does not advance `DigitsCount` either, so `"100"` is
  digits `"1"` at scale 3;
* `Scale` counts digits before the separator and decrements for each leading zero after it;
* the exponent is added to `Scale` wholesale;
* the value is `digits × 10^(Scale − DigitsCount)`, and the conversion fails — always as
  `OverflowException` — when `Scale > MaxDigitCount` or `Scale < DigitsCount`.

.NET's own tests pin the decisive rows, so the answer is executable evidence rather than a
reading: `Int32Tests.cs:353-358` fixes `1E2 → 100` and `(1E2) → -100`; `:546-548` fixes
`65E10`, `65E+10` **and `65E-1`** as `OverflowException`; `:473` fixes `1E23` under
`NumberStyles.Integer` as `FormatException`; `:665` fixes `2E10`.

`65E-1` is the row the ticket was waiting for. It denotes 6.5, and .NET reports it as an
**overflow** — scale 1 cannot carry two significant digits. That single rule then decides
everything the ticket named: `1.5E1` is digits `"15"` at scale 2, so 15; `100E-2` is digits
`"1"` at scale 1, so 1; `1E-2` is digits `"1"` at scale −1, so `OverflowException`.

## 3. One deliberate deviation

Read literally, the same rule makes an **all-zero magnitude with a non-positive scale** overflow:
`"0.0"` leaves `Scale` at −1 against `DigitsCount` 0. This port keeps returning `0` for `"0.0"`,
`"000.000"` and `"0E-2"`.

That is not disagreement with the reference. It is that .NET's own suite pins no such row, the
reading is a source trace that cannot be executed here, and turning `Parse("0.0",
NumberStyles::Number)` — an input that looks valid, is valid for every other numeric type, and
works today — into an `OverflowException` is a narrowing SR-AUD-177 never asked for. The
question is ticket **#2356**, and the current answer is pinned by a test so it cannot change by
accident.

The deviation is confined to a **non-positive** scale. `"0E100000000"` still overflows, matching
.NET.

## 4. To migrate

Nothing. Text that parsed before parses to the same value. If you were relying on `"1E2"`
throwing `FormatException` — for instance to reject scientific notation in a config file — pass
a style without `AllowExponent`; `NumberStyles::Integer` and `NumberStyles::Number` both exclude
it, and only `NumberStyles::Any` and an explicit `AllowExponent` are affected.

## 5. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched for `NumberStyles` and for the
`Parse`/`TryParse` overloads that take one: **zero sites in `cna` and `mobile-eggbert`**. Neither
repository was modified.
