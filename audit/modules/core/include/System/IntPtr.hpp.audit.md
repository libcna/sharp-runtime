# Audit: `modules/core/include/System/IntPtr.hpp`

## Metadata

- Audit status: AUDITED (126 lines, header-only implementation, full read).
- Validation: `IntPtrTests2.*:UIntPtrTest.*` passed 20/20 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-intptr-audit-probe.cpp`, compiled with
  `-fsanitize=undefined` against `build/libsharp_runtime_core.a`.

## Assessment

The wrapper has clear conversions, range-checked `ToInt32`, pointer-size
constants, and ordinary comparison/hash/string behavior. Its byte-offset
arithmetic, however, uses signed C++ addition/subtraction before conversion,
which makes legitimate unchecked native-integer wrapping undefined behavior.

## SR-AUD-025 — high — `IntPtr::Add` and `Subtract` execute signed-overflow UB at native boundaries

`IntPtr::Add` evaluates `pointer.value + offset` in `intptr_t`, and
`Subtract` likewise evaluates signed subtraction. UBSan reports both reachable
boundary cases:

```
IntPtr.hpp:105:63: runtime error: signed integer overflow:
9223372036854775807 + 1 cannot be represented in type 'long int'
IntPtr.hpp:114:63: runtime error: signed integer overflow:
-9223372036854775808 - 1 cannot be represented in type 'long int'
add_max=-9223372036854775808
subtract_min=9223372036854775807
```

The observed values are the intended native-width wraps, but C++ cannot rely
on them after UB. The .NET source exposes the corresponding operations as
`pointer + offset` and `pointer - offset` for `nint`
(<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/IntPtr.cs.html>), which execute under ordinary
unchecked C# arithmetic. The port must express that modulo-width behavior with
unsigned arithmetic before converting back, rather than rely on signed C++
overflow.

### Required post-audit verification

Implement Add/Subtract through `uintptr_t` modulo arithmetic and add exact
`MaxValue + 1 -> MinValue` and `MinValue - 1 -> MaxValue` assertions. Run that
filter under UBSan and retain ordinary positive/negative offset vectors.

## Final assessment

A concise pointer wrapper with one sanitizer-confirmed boundary UB in a public
operation. No source or test was altered during this audit.
