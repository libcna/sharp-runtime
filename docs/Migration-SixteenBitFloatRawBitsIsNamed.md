<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: a bit pattern and a number now have different spellings (#2395)

**Landed:** 2026-08-19, branch `next`. **Ticket:** #2395 (found by #2384 unit 3).

## What changed

```cpp
// before
Half     h(0x3C00);                 // the BIT PATTERN -- 1.0
BFloat16 b(static_cast<uint16_t>(0x3F80));

// after
Half     h = Half::FromBits(0x3C00);        // the bit pattern -- 1.0
BFloat16 b = BFloat16::FromBits(0x3F80);

Half     n(static_cast<SharpRuntime::ushortcs>(0x3C00));  // the NUMBER -- 15360.0
```

The raw-bits constructor became the named static `FromBits`, which freed the constructor
signature for .NET's value conversions. Both 16-bit floats moved in step (#2340).

## Why the signature had to be freed

`Half(uint16_t)` was the raw bit pattern. .NET spends exactly that signature on
`explicit operator Half(ushort)`, whose body is `(Half)(float)value` — a **numeric** conversion.
`ushortcs` and `uint16_t` are the same C++ type, so the two could not coexist.

It was worse than one overload. An exact `int` match beats an `int -> uint16_t` conversion, so
adding **any** value-taking integer constructor made `Half(0x7BFF)` silently stop meaning "these
bits". Measured by #2384 unit 3: the constructors were written and built, and **44 shipped tests
turned red**, `Half::MaxValue` and `Half::NegativeInfinity` among them, because this type's own
constants are spelled that way.

## Read this before migrating: the compiler cannot help you

**`Half(0x3C00)` is valid under both readings** — 1.0 as bits, 15360.0 as a number — so a site
that is not migrated **keeps compiling and silently changes meaning**. This is the dangerous class
of source break, and it is why the work was sequenced as it was:

1. the raw-bits constructor was removed and `FromBits` added, **with no value-taking constructor
   present**, so the compiler *had* to name every site;
2. all 66 first-party sites were migrated;
3. only then were the value conversions added.

Doing it in the other order would have left the meaning changes invisible. Two lessons came out of
step 1 and both are worth carrying: truncating the compiler's error list (`| head`) hid sites that
then compiled silently, and a `-k`/keep-going build is needed because the build stops at the first
failing translation unit. Three sites were found only by an exhaustive re-grep after the fact:
`BitConverter::ToBFloat16` and `UInt16BitsToBFloat16`, both of which are **bit** functions that
would have started returning a number. **A fourth was found only by the full gate**, in
`tests/integration/Task40Tests.cpp` — a tree that `modules/` and `test/` do not cover, which is
the same gap #1958/SR-AUD-196 recorded when its own grep missed it. The migration count is
therefore **67 sites, not the ticket's 66**.

## What landed with it

.NET's conversions **into** both types, which #2384 unit 3 had to leave out:

| | Half | BFloat16 |
|---|---|---|
| char, byte, sbyte, short, ushort | via `float` | via `float` |
| int, uint, long, ulong | via `float` | **`RoundFromSigned` / `RoundFromUnsigned`** |
| float, double | `FromSingle` / `FromDouble` | via `float` |

**The bodies are not all the same, and that is .NET's doing** (`BFloat16.cs:560-646` against
`Half.cs:562-807`). BFloat16 keeps only 8 significand bits, so a 32- or 64-bit integer routed
through a 24-bit float **rounds twice**. Probed over 400,000 random values of each width, the
direct path differs from the float path on **4 int32 and 2 int64 inputs**. The first is
`1119879149`: direct `0x4E85`, float route `0x4E86`. Checked by hand — the value is
1.0429… × 2³⁰, so the significand field is 5 and the biased exponent 157, giving `0x4E85`. The
float route is one ulp out. Half needs none of this and .NET gives it none.

`double` on both types narrows via an intermediate float, which is the deviation
`System::Half::FromDouble` **already declared for itself**: it can double-round differently right
at an exact tie.

## `byte` and `sbyte` are explicit here

.NET makes them **implicit** (`Half.cs:980,986`; `BFloat16.cs:824,830`) and this port cannot: C++
permits a standard conversion *before* a user-defined one where C# does not, so an implicit
converting constructor from `bytecs` makes **every** integer argument ambiguous. Measured. What is
lost is the implicitness, never the conversion.

## Absent in both directions

`nint`, `nuint`, `decimal`, `Int128` and `UInt128`. This port declares no `nint` type at all, and
#2384's `from` set omits the others — adding one direction alone would make the pair asymmetric.
Pinned, so a later unit "completing" one direction has to justify it.

## Migration scope

66 first-party sites (20 `Half`, 46 `BFloat16`), **zero** consumer sites in `cna` and
`mobile-eggbert`. Negative fixture:
`test/consumer/core_sixteen_bit_float_conversions_negative.cpp`, five sites — and its own header
says plainly what it cannot do, which is reject the old spelling.
