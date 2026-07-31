<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Decimal::Parse`/`TryParse` reads `,` as a group separator

**Ticket #1858**, second half. Approved on 2026-07-31 in the exact words of
`docs/RemainingApprovalDecisions.md` §B.8 item (3), which required this change to
land **as its own commit, with this note**, so it can be reverted alone.

This note exists for one reason: **`System::Decimal::Parse` and
`System::Decimal::TryParse` now return a different value for input they already
accepted.** There is **no compiler diagnostic** for this. It is the only change
in the approved Groups A–D batch with that property, and the decision packet
called it "the only genuinely dangerous row in groups A–D" (§B.4).

Nothing else about `Decimal` changed: no signature, no object layout, no ABI, no
`noexcept`, and no other member's behaviour.

---

## 1. What changed, in one sentence

`,` used to be a second accepted spelling of the decimal point; it is now the
invariant-culture **group (thousands) separator**, which is what .NET's
`decimal.Parse(string)` does, because its default is `NumberStyles.Number` and
that includes `AllowThousands`.

---

## 2. The exact before/after table

Measured, `build-probe/1865_{prefix,postfix}_plain.log`, cases B90–B99.

| Input | Before | After | .NET |
|---|---|---|---|
| `"1,5"` | **`1.5m`** | **`15m`** | `15m` |
| `"1,234.5"` | `FormatException` | `1234.5m` | `1234.5m` |
| `" 1,234.5 "` | `FormatException` | `1234.5m` | `1234.5m` |
| `"1,234,567"` | `FormatException` | `1234567m` | `1234567m` |
| `",5"` | **`0.5m`** | **`FormatException`** | `FormatException` |
| `"1,"` | `1m` | `1m` | `1m` |
| `"1.5,"` | `FormatException` | `FormatException` | `FormatException` |
| `"1.5"` | `1.5m` | `1.5m` | `1.5m` |
| `"-0"` | negative zero | negative zero | negative zero |

**Two rows change the answer for input that already succeeded**: `"1,5"`
(`1.5m` → `15m`, a factor of ten) and `",5"` (`0.5m` → rejected). The packet's
§B.3 named only the first; the second follows from the same rule and matches
.NET, which requires a digit before a group separator.

---

## 3. Which callers are affected

A caller is affected if, and only if, it feeds `Decimal::Parse` or
`Decimal::TryParse` a string that contains a `,` **and** meant it as a decimal
point. In practice that means text originating from a European-style decimal
convention (`"3,14"`) reaching an **invariant-culture** parser.

**Nothing in this repository does that.** The risk is entirely downstream and
entirely silent.

Note this port has no culture-aware parse overload, so there is no
`Decimal.Parse(s, CultureInfo.GetCultureInfo("de-DE"))` to switch to — the same
gap that existed before this change.

## 4. What to do

1. **If you produce the text**, emit `.` as the decimal point. Every
   `Decimal::ToString()` in this port already does.
2. **If you receive the text from elsewhere** and it may use `,` as a decimal
   point, normalise it before parsing:
   ```cpp
   std::string normalised = raw;
   std::replace(normalised.begin(), normalised.end(), ',', '.');
   System::Decimal d = System::Decimal::Parse(normalised);
   ```
   Do this only when you *know* the source convention; applying it to grouped
   text (`"1,234.5"`) would corrupt it.
3. **If you cannot tell**, reject ambiguous input explicitly rather than letting
   either interpretation win silently.

There is no automated migration and no diagnostic, so step 3 is the honest
default for untrusted input.

---

## 5. Rollback

This change is one commit and reverting it restores the previous behaviour
exactly. That isolation was a condition of the approval (§B.5: "Approving B(i)
as a whole is reasonable; approving it *as one commit* is not"). Reverting it
does **not** affect the rest of ticket #1858 (the
`FormatException` → `OverflowException` taxonomy), which is a separate commit,
nor ticket #1865 (`Single`/`Double`), which is another.

---

## 6. Consistency this buys

Before, `,` meant "decimal point" to `Decimal` and was rejected outright by
`Single`/`Double`. After #1858 and #1865 it means **group separator** to all
three, which is the single answer §B of the decision packet existed to obtain.
