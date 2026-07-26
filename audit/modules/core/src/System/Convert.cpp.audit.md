# Audit: `modules/core/src/System/Convert.cpp`

## Metadata

- Audit status: AUDITED (327 lines, implementation, full read).
- Validation: `ConvertTests.*` passed 204/204 on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-convert-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

The implementation centralizes many repaired parse, rounding, base, casing,
and standard-exception paths. Two input families remain under-validated:
non-finite floating values fall through comparisons before integer casts, and
the Base64 decoder validates only length and alphabet membership rather than
padding grammar or permitted whitespace.

## SR-AUD-027 — high — non-finite floating `Convert` inputs bypass checks and return spurious integers

`ToInt32`, `ToInt64`, `ToUInt32`, and `ToUInt64` call `Math::Round`, compare
the result with ordinary inequalities, then cast. Every comparison with NaN is
false, so it reaches the native conversion. The reproducible results are:

```
int32_nan=-2147483648
int64_nan=-9223372036854775808
uint32_nan=0
uint64_nan=9223372036854775808
```

The same input should raise `System::OverflowException`. The official Convert
implementation takes the out-of-range route for `ToInt32(double)` and uses
checked conversion for `ToInt64`/`ToUInt64`:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Convert.cs.html>.
The varying C++ results make this a correctness and portability defect.

### Required post-audit verification

Check `std::isfinite` before rounding/casting in all four direct conversions.
Verify NaN and positive/negative infinity throw `OverflowException` for double
and float overloads, while preserving midpoint-to-even vectors.

## SR-AUD-028 — medium — Base64 decoding accepts malformed padding and rejects valid whitespace

`FromBase64String` treats `=` as value `-1` without constraining it to one or
two trailing positions. It accepts malformed `"=AAA"` (three output bytes) and
`"AA=A"` (two output bytes). Conversely, it rejects `"T Q=="` at the raw
length check, although it is valid after ignored whitespace and decodes to
`"M"` in .NET. The official contract permits arbitrary space, tab, CR, and LF
and permits padding only at the end:
<https://learn.microsoft.com/en-us/dotnet/api/system.convert.frombase64string?view=netframework-4.8.1>.

### Required post-audit verification

Normalize permitted ASCII whitespace before validating length, reject any
non-trailing `=`, enforce zero/one/two trailing padding and unused-bit rules,
and add valid-whitespace plus leading/middle/excess-padding failure vectors.

## Final assessment

The repaired normal paths are well explained, but two reachable validation gaps
still produce silent wrong values or malformed decoded data.
