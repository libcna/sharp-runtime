<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `BFloat16` float-conversion rounding — plan

Ticket #2261. One frozen audit finding in
`modules/core/include/System/Numerics/BFloat16.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-175 | medium | float conversion truncates BFloat16 payloads instead of .NET's round-to-nearest-even |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **singleton on one private static conversion**,
not a `BFloat16` review.

---

## 1. Scope, and why SR-AUD-176 is not in it

In scope: `BFloat16::fromFloat`, the single private static function through
which the `BFloat16(float)` constructor **and all four arithmetic operators**
produce a payload.

**SR-AUD-176 is deliberately rejected from this unit.** It is the sibling
finding in the same header, and sharing a header is not sharing a root cause:
176 says the public type omits `Parse`/`TryParse`, standard format overloads,
comparison/hash APIs, classification (`IsFinite`/`IsNormal`/`IsSubnormal`/
`IsNegative`/`IsZero`), `CopySign`, `BitIncrement`/`BitDecrement`, the numeric
conversions and the generic-math interface contract. That is not a defect in an
existing behaviour — it is porting most of a 2,152-line .NET type, a large
additive API unit that needs its own review and its own decision about how much
of the generic-math surface this port takes on. Ranking it here would have made
this unit unbounded. It stays `confirmed` and unclaimed.

Also out of scope: `toFloat` (exact by construction — BFloat16 is a strict
subset of float32), the raw-bits constructor, the comparison operators, and
`ToString`.

## 2. Before evidence, measured 2026-08-11

`build-probe/2261_probe1_before.cpp`, 26 cases, every one stated as an **exact
32-bit float bit pattern** so nothing depends on decimal-literal parsing or on
the host's rounding mode. Full output in `build-probe/2261_probe1_before.log`.

**Result: 14 OK / 12 BAD.** Both of the audit report's own probes reproduce
exactly: `0x3F818000` → `0x3F81` where .NET gives `0x3F82`, and `0x3F808001` →
`0x3F80` where .NET gives `0x3F81`.

The probe establishes four things the finding does not state:

1. **Truncation turns a NaN into an infinity.** `0x7F800001` is a signalling
   NaN whose upper 16 bits are *exactly* the `+Infinity` pattern `0x7F80`, so
   `u >> 16` returned an infinity for a NaN input. `0xFF800001` likewise. This
   is the sharpest consequence of the defect and the report describes only a
   downward bias.
2. **A rounding bias must not be applied to a NaN either** — adding the bias to
   a NaN payload can carry into the exponent and produce the same silent
   infinity. That is why .NET rounds only *non-NaN* bits, and why the repair
   needs a NaN branch rather than only a different bias.
3. **Overflow-to-infinity is part of correct rounding, not a defect.** Float
   `MaxValue` lies above BFloat16 `MaxValue` + half ulp, and the exact tie at
   that midpoint rounds to the even neighbour — which *is* the infinity
   pattern. A finite input therefore rounds to infinity, correctly.
4. **Underflow has a tie too.** `0x00008000` is the exact midpoint between `+0`
   and `Epsilon`; ties-to-even gives `+0`, and `0x00008001` gives `Epsilon`.

One probe expectation was wrong in its first draft and is recorded rather than
quietly corrected: `0x7F7F8000` was written as expecting the finite `0x7F7F`.
The tie at (max + half ulp) rounds to the **even** neighbour, and above odd
payload `0x7F7F` the even neighbour is `0x7F80` — infinity. The corrected
expectation moved that case from OK to BAD, which is why the before-state reads
14/12 and not 15/11.

## 3. Premise correction to the audit report

The report states that the direct tests "never construct from float, perform
arithmetic, format/parse, compare" and that BFloat16 coverage is "only four raw
bit conversions and no float construction". **That is true of
`BitConverterTests.cpp` and false of the repository as a whole**:
`tests/integration/Task40Tests.cpp` carries a 14-test `BFloat16Tests` suite that
constructs from float (`BFloat16 v(2.0f)`), performs arithmetic (`a + b`),
negates, and compares.

The finding's *conclusion* survives the correction: none of those 14 tests pins
truncation, because every value they use (`0.0f`, `1.0f`, `2.0f`, `3.0f`) has
zero discarded bits, and the loose ones use `EXPECT_NEAR`. **No test was
retired**, and all 14 pass unchanged after the repair — they are this ticket's
compatibility control.

## 4. The repair (#2262)

```cpp
static uint16_t fromFloat(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0u) {
        return static_cast<uint16_t>((u >> 16) | 0x0040u);   // quiet NaN, sign kept
    }
    const uint32_t rounded = u + 0x7FFFu + ((u >> 16) & 1u); // round half to even
    return static_cast<uint16_t>(rounded >> 16);
}
```

`0x7FFF` is half an ulp of the discarded field; adding one more when the
retained payload is odd is what makes an exact midpoint land on the even
neighbour. A carry out of the mantissa flows into the exponent by itself, which
is also how overflow to infinity falls out without a special case.

## 5. Compatibility, ABI, layout and `noexcept`

| Property | Before | After |
|---|---|---|
| `sizeof` / `alignof` | 2 / 2 (measured) | unchanged |
| Data members | one `uint16_t` | unchanged |
| Public signatures | — | unchanged |
| `noexcept` | none declared | unchanged |
| Virtuals / vtable | none | unchanged |
| Exported symbols | header-only, private static | unchanged |

**This is an observable numeric change and it is deliberate.** Any float whose
low 16 bits are non-zero may now convert to the next payload up, and a NaN with
its payload only in the low bits no longer becomes an infinity. Every exactly
representable value — every value with zero discarded bits, which includes every
value produced by the raw-bits constructor and round-tripped through
`BitConverter` — is unchanged. Blast radius is four translation units:
`BitConverter.hpp`, `BinaryPrimitives.hpp`, `BitConverterTests.cpp` and
`Task40Tests.cpp`; `BitConverter`'s BFloat16 entry points are all raw-bit
reinterpretation and never call `fromFloat`.

## 6. CCF relationships — none minted, none extended

No CCF applies. This is not CCF-019 (no ownership or lifetime), not CCF-011 (no
callable), and not the CCF-006/CCF-007 float *formatting* families — those
concern `Single`/`Double` `ToString` text, whereas this is a bit-level
conversion with no formatting involved. CCF-021 and CCF-022 stay unminted.

## 7. Test matrix (#2262)

`modules/core/tests/System/BFloat16RoundingTests.cpp`, 17 tests, every input an
exact bit pattern: the finding's two reproductions; both directions of
ties-to-even; just-below and just-above midpoint; sign symmetry; carry out of
the mantissa into the exponent; exactly representable values unchanged (the
compatibility control); signed zero; overflow to infinity including the tie at
max + half ulp; infinity passthrough; signalling NaN staying NaN in both signs;
quiet NaN keeping its sign; a **sweep of every single-bit NaN mantissa in both
signs**, none of which may leave the conversion as an infinity or a finite
value; the four underflow cases; arithmetic rounding; exact arithmetic staying
exact; and the `sizeof`/`alignof` pin.

One test's first draft was wrong and is recorded rather than quietly fixed:
`ArithmeticResultsRoundRatherThanTruncate` originally doubled a single payload.
**Doubling a BFloat16 is always exact** — it only increments the exponent — so
that case could never discriminate. It now sums two *different* payloads
(`0x3F81 + 0x3F82`, whose float result `0x40018000` is an exact midpoint above
odd payload `0x4001`), and is checked in both operand orders.

## 8. Sanitizers — not applicable, deliberately

The before-state computes a *wrong `uint16_t`* on a fully defined path: the
`memcpy` type pun, the shift and the addition are all well-defined, and the
addition is on `uint32_t`, where overflow is defined to wrap. ASan/UBSan cannot
discriminate before from after. Not run, for the reason #2254 and #2258 record.

## 9. Outcome, measured 2026-08-11

**After-probe: 26 OK / 0 BAD**, from the same probe source as the before-run so
the expectations cannot drift. `sizeof`/`alignof` measured 2/2 before and after.

**Tests: +17.** The 14 pre-existing integration `BFloat16Tests` and the 113
`BitConverterTests` all pass unchanged.

**Mutations: 4 planned, 4 run, 4 caught** (`build-probe/2262_mutations.log`).

| Mutation | Result |
|---|---|
| P1 restore the pre-repair `u >> 16` | CAUGHT — 9 tests fail |
| P2 drop the NaN guard, letting the bias reach a NaN | CAUGHT — 2 tests fail |
| P3 fixed `0x8000` bias (round half up, not ties-to-even) | CAUGHT — 2 tests fail |
| P4 fixed `0x7FFF` bias (ties always round down) | CAUGHT — 6 tests fail |

P2 is the one that matters most: it is the *plausible* wrong repair — fix the
rounding, forget that NaN cannot be rounded — and it is caught only by the two
NaN tests, including the single-bit mantissa sweep.
