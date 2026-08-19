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

## What is still to come

Unit 1 is the sign/magnitude family. **Unit 2** is the transcendental families (`Sqrt`, `Exp`,
`Log`, the trigonometric and hyperbolic sets, `Pow`, `FusedMultiplyAdd`, `ReciprocalEstimate`) and
**unit 3** is the conversion operators. Both remain, both must move the two types in step, and
unit 2's absence is pinned on both types today.

Out of scope permanently, and unchanged by this ticket: **generic-math conformance**
(`INumber<T>`, `IFloatingPointIeee754<T>`, `IMinMaxValue<T>`). .NET's `BFloat16` implements 36
interfaces and none of them is expressible here.
