# Audit: `modules/core/include/System/Convert.hpp`

## Metadata

- Audit status: AUDITED (660 lines, public conversion surface, full read).
- Validation: `ConvertTests.*` passed 204/204 in `SharpRuntimeTests_Core_Base`
  on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-convert-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

The overload family documents important C++ overload-resolution safeguards and
has solid checked paths for many signed/narrowing conversions. A small inline
set instead applies unchecked `static_cast` where adjacent overloads and the
declared .NET counterpart require range rejection.

## SR-AUD-026 — high — checked `Convert` integral overloads silently wrap instead of rejecting loss of range/sign

`ToByte(longcs)`, `ToUInt32(intcs/longcs)`, and `ToUInt64(intcs/longcs)` are
direct casts. `ToChar(intcs)` also directly casts, while the neighboring long
overload rejects values outside this port's byte-backed character range. The
direct probe observed:

```
byte_long_neg=255
byte_long_256=0
uint32_int_neg=4294967295
uint32_long_over=0
uint64_int_neg=18446744073709551615
uint64_long_neg=18446744073709551615
char_int_neg=255
```

These are silent loss-of-data results, not `System::OverflowException`. The
official Convert source checks signed inputs for UInt64 and routes
`ToByte(long)` through a checked unsigned conversion:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Convert.cs.html>.
The public Convert contract says narrowing conversions that cannot be
represented throw `OverflowException`:
<https://learn.microsoft.com/en-us/dotnet/api/system.convert?view=net-10.0>.
Even under the port's documented one-byte `char` adaptation, the int overload
must agree with its checked long sibling.

### Required post-audit verification

Check every source range before converting. Add exact `OverflowException`
tests for the shown invalid inputs and positive/max-boundary controls; retain
raw string-literal overload coverage.

## Final assessment

The broad declared surface contains multiple carefully repaired paths, but the
unchecked inline set is a high-impact public data-conversion defect.

### Remediated — ticket #1853 (2026-07-30)

**SR-AUD-026** (six integral overloads): `ToChar(intcs)`, `ToByte(longcs)`,
`ToUInt32(intcs)`, `ToUInt32(longcs)`, `ToUInt64(intcs)`, `ToUInt64(longcs)` now
range-check and throw `OverflowException` before the cast, joining the sibling
overloads that already guarded. `ToChar(intcs)` uses `[0, 255]` — this port backs
`char` with a 1-byte type, so it matches the `ToChar(longcs)` sibling, not .NET's
`[0, 65535]`. Value-only change (integer narrowing is well-defined in C++20, no
UB); every in-range value is unchanged.

**SR-AUD-027** (the two inline `double` converters here): `ToUInt32(double)` and
`ToUInt64(double)` now reject NaN and ±Inf with `OverflowException` via a
`!std::isfinite` guard before `Math::Round`/the cast. (The other two,
`ToInt32/ToInt64(double)`, are in `Convert.cpp` — see its report.) **Premise
correction:** the CCF-005 plan called the NaN→int cast "implementation-defined,
not UB"; it is in fact genuine undefined behavior per `[conv.fpint]` (a
float→integer conversion whose value is not representable in the destination is
UB, and NaN never is). UBSan's `float-cast-overflow` reproduced
`nan is outside the range of representable values of type 'unsigned int'` at
`Convert.hpp:390` pre-fix (`build-probe/1853_convert_nan_prefix.log`) and was
clean post-fix (`…_postfix.log`). +41 tests total across 026/027. No
`noexcept`/signature/layout change. `docs/ConversionBoundaryFamilyPlan.md` §19.4.
