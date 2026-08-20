<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Multi-format `ParseExact` across all five exact-parsing types — #1944

**Purely additive**, under SA-5. No existing signature, layout, vtable, `noexcept` specification or
accepted input changed. This is the **last open post-audit implementation ticket**.

## One loop for five types

`DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly` and `TimeSpan` all gain
`ParseExact(input, formats, …)` / `TryParseExact(…)`. The ordered first-success loop is
`detail::MatchFirstOfManyFormats`, written **once** — five copies of one taxonomy is how five doors
come to disagree about what an empty element means. .NET writes it twice
(`TryParseExactMultiple`, `TryParseExactMultipleTimeSpan`) and the two agree, so sharing is faithful
rather than a shortcut.

## The rules, and the one a plausible implementation gets wrong

* **Ordered, first success wins.** Pinned with an input two formats both accept:
  `01/02/2024` is 2 January under `MM/dd` and 1 February under `dd/MM`, so the two orders give
  **different dates** rather than merely a different code path.
* **An empty element ABORTS the whole loop rather than being skipped.** .NET returns
  `SetBadFormatSpecifierFailure` immediately. *"Skip it and carry on"* is the plausible
  implementation and it is wrong; the pin puts the empty element **before** a format that would
  have matched, so the wrong rule succeeds where the right one fails.
* **An empty collection is a FORMAT failure, not an argument one** — .NET's
  `Format_NoFormatSpecifier`, easy to get wrong in the direction of `ArgumentException`.
* **.NET's null-array arm has no C++ counterpart** and is deliberately not reproduced: the
  parameter is a `const std::vector<std::string>&`, which cannot be null.
* **The style is validated once, before the loop**, so an illegal style raises whatever the formats
  are — including an empty collection, where no single-format call would ever run. Validating
  inside the loop would make the exception depend on the format list.

## Two failure kinds, because .NET gives them two messages

Measured while writing the tests: with the empty-collection guard simply **removed**, the loop body
never runs and the fall-through gives the same answer — so the guard would be a **proven
equivalence** and the mutation uncaught. But **.NET's two failures carry different messages**:
*"No format specifiers were provided."* against *"String was not recognized as a valid …"*.

Carrying both makes the guard load-bearing **and** gives the right diagnosis: telling a caller who
supplied no formats that their *input* was unrecognised is wrong. An **empty element** gets the
format-specifier message too, being the same kind — the caller's formats are wrong, not their input.

## The overload-resolution hazard the ticket anticipated

#1944's acceptance criteria asked for *"compile ambiguity fixtures"*, and there was a real one.
Measured with a five-way probe:

| spelling | before |
|---|---|
| `ParseExact(s, {"a", "b"})` | **ambiguous** |
| `ParseExact(s, {"one"})` | **ambiguous** |
| `ParseExact(s, {"a", "b", "c"})` | ok |
| `ParseExact(s, std::vector<std::string>{…})` | ok |
| `ParseExact(s, {std::string("a"), std::string("b")})` | ok |

Two `const char*` in braces match `std::basic_string(InputIt first, InputIt last)` over two
**unrelated** pointers, so the single-format overload was a candidate — **and if it had ever won,
the result would be undefined behaviour rather than a wrong answer.** That is what decided the fix
rather than documenting the papercut.

`std::initializer_list<std::string>` overloads resolve it: a braced list binds to an
`initializer_list` parameter by a **list-initialization sequence**, which outranks any user-defined
conversion, so the dangerous candidate can no longer win. All five spellings now compile, and a
case asserts the **unbraced** single-format overload is still reachable — the half a fix aimed only
at the braced spelling could have broken.

## Evidence

Four mutations, **three caught**. **M3 is a proven equivalence, recorded at the site rather than
counted**: with the empty-input guard removed the loop still runs, every format fails to match an
empty input, and the fall-through returns the same outcome with the same message. **.NET's is an
equivalence too** — its `s.Length == 0` arm and its all-formats-failed arm are both
`SetBadDateTimeFailure`, one kind and one text — so the line is a statement of intent, kept because
it is .NET's and because it says *why* an empty input cannot succeed without relying on every
format's scanner to refuse it. One mutation was **invalid as first written** and reformulated
rather than counted (`-Werror=unused-parameter`).

Gate: **17,732 / 38, 0 failed, 0 skipped** (+7; `SharpRuntimeTests_Core_Base` 6,141 → 6,148).
Module graph **41 / 95**. Downstream: **zero sites** — the members did not exist.

## What #1944 asked for and did not get

The **span-like** shapes. `System::ReadOnlySpan<char>` exists in this port, and `std::string_view`
is the idiomatic C++ counterpart, but neither is added: **every exact-parsing door here takes
`const std::string&`**, and adding a second text representation beside it repeats exactly the
overload hazard this ticket just closed, in a place where the wrong branch is silent rather than
ambiguous. It is recorded here rather than taken, because it is a public-shape decision and the
collection half — which is what every multi-format caller actually needs — is complete without it.
