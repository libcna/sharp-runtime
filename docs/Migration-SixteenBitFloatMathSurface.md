<!-- SPDX-License-Identifier: MIT -->
# Migration — the 16-bit floats gain .NET's math surface (#2384), unit 1

Ticket **#2384**, decided 2026-08-19: **add the surface**, per .NET. Its acceptance criterion
requires it to land **as bounded units that move both types in step**, and this is unit 1.

**Purely additive.** No existing member changed, no layout moved (`sizeof(BFloat16)` is still 2),
and nothing that compiled before fails now.

## Why both types, always

#2340 established that this surface must move for `System::Half` and
`System::Numerics::BFloat16` **together or not at all**: adding it to one leaves the port's two
16-bit floats inconsistent with each other, which is **worse than either policy applied
consistently**.

That rule is enforced by the build, and it worked as designed: adding these members to `BFloat16`
alone **broke the compile**, because #2382's pin asserted their absence with the message *"it must
move System::Half too"*. The pin is now partially inverted — it asserts presence on **both** types,
and still asserts unit 2's absence on both, so the next unit gets the same signal this one did.

## Unit 1 — nine members, and four of them are not what you would guess

| member | .NET | shape |
|---|---|---|
| `Abs` | `Half.cs:1756`, `BFloat16.cs:1396` | **bit mask** `value & ~SignMask` |
| `CopySign` | `Half.cs:1654-1664`, `BFloat16.cs:1300-1310` | **bitwise** |
| `BitIncrement` | `Half.cs:1478-1508`, `BFloat16.cs:1134-1164` | **bit arithmetic** |
| `BitDecrement` | `Half.cs:1445-1475`, `BFloat16.cs:1101-1131` | **bit arithmetic** |
| `Clamp` | `Half.cs:1641`, `BFloat16.cs:1297` | float round-trip |
| `Max`, `Min` | `Half.cs:1667…` | float round-trip |
| `MaxMagnitude`, `MinMagnitude` | `Half.cs:1851,1879`, `BFloat16.cs:1491,1519` | float round-trip |

**A blanket "forward to `float`" would be wrong for the first four**, and .NET says so itself:
`CopySign` carries the comment *"This method is required to work for all inputs, including NaN, so
we operate on the raw bits."*

**Three edges of `BitIncrement`/`BitDecrement` are not arithmetic** and are transcribed rather than
derived: a NaN returns itself, `-Infinity` **increments to `MinValue`** and `+Infinity`
**decrements to `MaxValue`**, and — the one most easily lost — **`-0.0` increments to `Epsilon`**
while **`+0.0` decrements to `-Epsilon`**.

## Every body was derived per type, not copied

This is #2382's lesson applied rather than quoted. There, copying `Half::GetHashCode` into
`BFloat16` would have **compiled, satisfied the hash contract and returned the wrong number**,
because .NET's `BFloat16` delegates its identity trio to `float` while `Half` masks to 16 bits.

For these nine the two references **do** agree — and that was *checked*, not assumed. The only
textual difference is that `BFloat16.Clamp` calls `Math.Clamp` where `Half.Clamp` calls
`float.Clamp`; same semantics.

## Mutation testing

Six mutations, all caught — **but M6 only after a measurement corrected the test**:

| # | Mutation | Caught by |
|---|---|---|
| M1 | `Half::Abs` via a float round-trip | the bit-operations case |
| M2 | `Half::CopySign` via `std::copysign` | the same |
| M3 | `BitIncrement(-0.0)` returns `+0.0` | the edges case |
| M4 | `BitIncrement` drops the negative branch | the same |
| M5 | `MaxMagnitude` is really `Max` | the round-trip case |
| M6 | `BFloat16::Abs` via a float round-trip | the bit-operations case, **after repair** |

**M6 was first NOT CAUGHT while its `Half` twin M1 was**, and the reason was in the test, not the
code: the case used `0x7FC1`, a NaN that is **already quiet**, so a round-trip returned it
unchanged. Probed exhaustively over all 65,536 patterns, the two forms differ on **exactly 126** —
every one a **signalling** NaN, which `fromFloat` quiets by OR-ing in `0x0040`. The case now uses
`0x7F81` and asserts the exact bits.

---

# Unit 2a — rounding, `Sign`, and the IEEE 754:2019 `*Number` family

Landed 2026-08-19, same day. Purely additive again.

## The `*Number` family is not `Max`/`Min`, and two rules separate them

`MaxNumber`/`MinNumber`/`MaxMagnitudeNumber`/`MinMagnitudeNumber` are IEEE 754:2019
`maximumNumber` and friends (`Half.cs:1673-1720, 1854-1904`). Two differences from `Max`/`Min`,
both real and both pinned, because a forward to `Max` satisfies every ordinary row and fails
exactly these:

1. **They do not propagate NaN.** `Max(NaN, 2)` is NaN; `MaxNumber(NaN, 2)` is `2`, from either
   side. .NET says so in its own comment.
2. **`+0` is treated as larger than `-0`** — and **no comparison can see that**, since `+0.0 ==
   -0.0`, so the pin asserts the **bits**. A naive `(x > y) ? x : y` returns the wrong zero and
   passes everything else in the file.

## `Sign` has two transcribed edges

It **throws** `ArithmeticException` on NaN rather than returning a sentinel, and it tests `IsZero`
**before** `IsNegative` — so **`Sign(-0.0)` is `0`, not `-1`**.

## `Round` is ties-to-even

Forwarded to `MathF::Round`, not `std::round`: `Round(2.5)` is `2`, not `3`. The mutation that
swaps them is caught.

## Four members exist on `Half` only, and that is transcription rather than asymmetry

Measured by diffing the two reference surfaces: **`MaxNative`, `MinNative`, `ClampNative` and
`MultiplyAddEstimate` are declared on `Half` only.** So **#2340's in-step rule means *each type
gets what .NET gives it*, not that the two surfaces are identical** — a distinction that only
becomes visible once the surface is large enough to differ. Their absence on `BFloat16` is pinned,
so a later unit that "completes the symmetry" has to justify inventing them.

## Mutation testing

Six mutations, all caught: `MaxNumber` forwarding to `Max`; dropping its signed-zero tie; `Sign`
returning `-1` for `-0.0`; `Sign` returning `0` instead of throwing; `Round` ties away from zero;
and `BFloat16::MinNumber` propagating NaN.

---

# Unit 2b — the transcendental, root, power and angular families

Landed 2026-08-19, same day. Purely additive. **39 members on each type.**

## Every one is a float round-trip, and that is .NET's own shape

Verified member by member against `Half.cs` and `BFloat16.cs`: they are all
`(Half)MathF.Sqrt((float)x)` and friends. So unlike units 1 and 2a there is **no bit-level body to
get wrong here** — which changes what is worth testing. The risk in a forwarding table is not
arithmetic, it is **mis-wiring**: `Sin` calling `Cos`, or `Atan2`'s arguments swapped. Every
compile-only test passes against both.

So the tests assert that each name reaches **its own** float function, comparing against
`FromSingle(expected(in))` rather than a literal — which makes them tests of the *forwarding*
rather than of `MathF`'s accuracy — plus explicit cross-checks that three members **disagree** with
each other on the same input, and operands chosen so that a swapped argument order gives a
different answer.

## The count in the previous unit's note was wrong, and the measurement corrects it

Unit 2a's note estimated unit 2b at "~45 members per type, mostly one-line forwards". **Only 39 of
them could be forwarded**, and the reason is a real boundary rather than an oversight:

| forward target | members |
|---|---|
| `System::MathF` | 25 |
| `System::Single` | 13 |
| **neither — absent from this port's `float` surface entirely** | **10** |

**Ten members .NET declares have no counterpart in this port's `System::MathF` *or*
`System::Single`**: `Compound`, `ExpM1`, `Exp2M1`, `Exp10M1`, `LogP1`, `Log2P1`, `Log10P1`, `Lerp`,
`MultiplyAddEstimate` and `ClampNative`. Adding them to the 16-bit floats would mean widening
**`float`'s own public surface first** — a different type's API, and a different question from
*"should the 16-bit floats carry the MathF-forwarding surface"*. They are pinned absent on both
types, so whichever ticket widens `System::Single` trips that pin and can complete the 16-bit types
in the same change.

`MultiplyAddEstimate` and `ClampNative` are **doubly** absent: `float` lacks them here *and* .NET
declares them on `Half` only.

## One naming difference worth keeping

`Ieee754Remainder` is the only member whose .NET name and this port's `MathF` name differ — .NET
calls it `Ieee754Remainder` on the float types and `IEEERemainder` on `MathF`, and this port's
`MathF` follows the latter. The forward is spelled out and pinned, because a mis-wiring there would
be invisible to a name-based reading.

## Mutation testing

Seven mutations, all caught: `Sin` wired to `Cos`; `Atan2` arguments swapped; two-argument `Log`
swapped; `ScaleB`'s integer argument routed through `float`; `Exp2` wired to `Exp10`;
`BFloat16::Sqrt` wired to `Cbrt`; and `Ieee754Remainder` wired to `Pow`.

## What is still to come

**Unit 3** — the conversion operators, measured at **43 on `Half` and 47 on `BFloat16`**. It must
move the two types in step like every unit before it.

Out of scope permanently, and unchanged by this ticket: **generic-math conformance**
(`INumber<T>`, `IFloatingPointIeee754<T>`, `IMinMaxValue<T>`). .NET's `BFloat16` implements 36
interfaces and none of them is expressible here.
