<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Complex::Abs` returns a `double` (ticket #2172)

*2026-08-18.* `System::Numerics::Complex::Abs` returned a `Complex` with a zero imaginary part —
a value .NET never produces — which is why the port had grown an invented `AbsD` beside it.

Landed under `docs/StandingApprovals.md` **SA-10** with SA-2's five conditions.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `Complex::Abs(z)` | `Complex` | **`double`** |
| `Complex::AbsD(z)` | `double` | **removed** |
| `Sqrt`, `Exp` and the other complex→complex factories | — | **unchanged** |

.NET's is `public static double Abs(Complex value)` (`Complex.cs:292`).

`AbsD` had **no .NET counterpart**: its only purpose was to work around the wrong return type, so
keeping it would have left invented surface behind a repair. It is removed with the same change.

## 2. The break is loud, and that is measured

There is **no implicit conversion in either direction** — `Complex(double, double)` has no
defaulted second parameter — so an affected caller gets a **hard compile error**, not a silent
change of meaning. That matters here more than usual: both spellings computed the *same
magnitude*, so no value comparison could ever have caught the difference. A `static_assert` on the
return type is the only test that can.

## 3. To migrate

```cpp
// before
Complex m = Complex::Abs(z);      // now a compile error
double  d = Complex::AbsD(z);     // AbsD is gone

// after
double d = Complex::Abs(z);
```

## 4. A checker change this ticket had to make first

This is the repository's **first `Numerics` negative consumer fixture**, and it could not exist
until now for a reason unrelated to the ticket: `Complex.hpp` reaches `Math.hpp` through
`Double.hpp`, `-Wpedantic` rejects `__int128`, and `-Werror` turned that into a **broken fixture
baseline** — which means no site verdict can be attributed to its own source at all. SA-2's
condition 2 was therefore unsatisfiable for this whole area.

`scripts/check_negative_consumer_fixtures.py` now accepts a named relaxation from a **closed set**:

```cpp
// NEGATIVE-FIXTURE: component=Numerics allow=int128-extension
```

It is deliberately **not** a general escape hatch — an unknown `allow=` value is an error, and each
entry documents a specific, permanent property of this codebase (here, the
`SHARP_RUNTIME_HAS_NATIVE_INT128` platform boundary `CLAUDE.md` already records).

**`-isystem` on the module roots was tried first and rejected.** It suppresses header warnings
wholesale, and measured, it made **two live sites** of
`core_activator_construction_negative.cpp` stop being rejected, because their narrowing
diagnostics originate inside a header. Losing two real assertions to gain one fixture is not a
trade worth making, and the failure is recorded here so the idea is not retried blind.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` calls `Complex::Abs` or `Complex::AbsD` — **zero sites in
both**. Zero in-repository callers used `Abs`; all three used `AbsD`.
