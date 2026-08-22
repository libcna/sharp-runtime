<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DateOnly`/`TimeOnly` `ParseExact` take a provider and styles (ticket #2412)

*2026-08-20.* **Purely additive**: both new parameters are defaulted, so every existing call site
compiles and behaves exactly as before. **Downstream, measured: zero sites** in `cna` and in
`mobile-eggbert`.

---

## 1. Why this ticket exists: a cycle in the records

**#1942** ("style validation and effects") was blocked on *"the relevant exact overload"* — an
overload that takes a `DateTimeStyles`. Measured: **nothing in `modules/` accepted one at all**; the
only mentions outside the enum header were doc-comments and `HttpDateParser` prose.

**#1943** ("single-format provider exact APIs") was blocked on *"#1940–#1942"* — which includes
**#1942**.

Each waited for the other. Neither could go first as written.

**What separates cleanly needs no approval.** #1942's remaining blocker beyond #1940 was *"a
reliable timezone contract"*, and that is true only of the **kind-affecting** styles —
`AdjustToUniversal`, `AssumeLocal`, `AssumeUniversal`, `RoundtripKind` — which need #1941 **phase 2**,
still unapproved. **`DateOnly` and `TimeOnly` have no `DateTimeKind` at all**, so .NET rejects every
one of those styles outright, and the whole contract is one line (`DateOnly.cs:317-320`,
`TimeOnly.cs:458,486,520`):

```csharp
if ((style & ~DateTimeStyles.AllowWhiteSpaces) != 0)
    throw new ArgumentException(SR.Argument_InvalidDateStyles, nameof(style));
```

**The styles that would need a timezone contract are exactly the styles that are illegal here.**

## 2. The new surface

```cpp
static DateOnly ParseExact(const std::string& input, const std::string& format,
                           const System::IFormatProvider* provider,
                           System::Globalization::DateTimeStyles style = DateTimeStyles::None);
static bool TryParseExact(const std::string& input, const std::string& format,
                          const System::IFormatProvider* provider,
                          System::Globalization::DateTimeStyles style, DateOnly& result);
```
…and the same pair on `TimeOnly`.

### 2.1 A `Try*` method that throws

`TryParseExact` **raises `ArgumentException` for an illegal style** rather than returning false, and
that is .NET's behaviour (`DateOnly.cs:519-522`), not an oversight here. A *parse* failure still
returns false. Validation happens **before** the result is written, so a rejected style leaves the
caller's variable untouched — asserted, because "throws" and "leaves the out-parameter alone" are
two claims and only one of them is obvious.

### 2.2 The whitespace styles

`AllowLeadingWhite` and `AllowTrailingWhite` trim the input; `AllowInnerWhite` skips whitespace **at
a token boundary** — before each literal and before each field.

**It is deliberately not "skip whitespace anywhere".** A field reader consumes its own digits
without consulting the style, so `"20 24-06-15"` against `"yyyy-MM-dd"` still **fails** on the
four-digit year. That row is what separates .NET's rule from a blanket skip, and it has its own
assertion. Each bit is separate: leading does not buy trailing.

### 2.3 The provider is honoured

Resolved by `DateTimeFormatInfo::GetInstance` (#1940). Its month and day names are what
`MMM`/`MMMM`/`ddd`/`dddd` match, so a provider with different names parses different input — and a
provider's names do **not** leak into the invariant parse, which is asserted separately.

## 3. A gap #1939 recorded honestly, now closed

#1939 wrote that the scanner's longest-first name matching was *"defensive rather than load-bearing"*
for the four invariant tables — no invariant month or day name is a prefix of another, and a
first-match mutation went **uncaught**.

**A provider's names carry no such guarantee.** `"Ma"` and `"March"` are a perfectly legal pair, so
the rule now decides the answer, and #2412 pins it with exactly that shape. Mutation M6 takes the
first match instead and **is** caught.

## 4. Component cost

`DateTimeStyles.hpp` moved into `Core.Base` — the same shape-C move as #1940's, for the same reason
and with the same result: it is a bare enum with two inline operators, its only includers were two
Globalization *tests*, and ownership by logical path uniqueness means **no include line changed**.
Graph stays **41 / 94**.

## 5. Historical boundary at this checkpoint

- At this checkpoint the `DateTime` half of #1942 still needed #1941 phase 2's timezone contract.
  Both later landed; #2418 then completed the Kind propagation audit across consumers.
- #1943's remaining exact shapes subsequently landed, and #1944 supplied the multi-format forms.
- Seven mutations after reformulation, five caught. **Two are proven equivalences**: the two
  empty-name guards are defence in depth, because `len <= bestLen` with `bestLen` starting at zero
  already excludes a zero-length name arithmetically. **Three mutations were invalid as first
  written** — `-Werror` rejected them for an unused variable rather than the tests rejecting them —
  and were reformulated rather than counted.
