# Audit: `modules/core/include/System/UInt128.hpp`

## Metadata

- Audit status: AUDITED (258 lines, header-only implementation, full read).
- Runtime validation: `UInt128Test.*` and `UInt128NewTests.*` passed 50/50 in
  `Core.Base`; integration `UInt128Tests.*` passed 7/7 on 2026-07-25.
- Direct UB/formating probe: `/tmp/sharp_runtimervc_int128_audit_probe.cpp`,
  compiled with UBSan as recorded in the paired Int128 report.

## Assessment

Basic arithmetic, exact decimal parsing, zeroing `TryParse`, and common
formatting are straightforward and broadly tested.  The class is nonetheless
less hardened than `Int128`: its shift operators pass the caller's count
directly to the native operator, even though the public C++ counterpart should
retain .NET's modulo-128 shift semantics.  It also duplicates Int128's weak
format parser.

## SR-AUD-020 — high — `UInt128` shift operators pass out-of-range counts to undefined native shifts

`operator<<` and `operator>>` apply `value_ << n` / `value_ >> n` directly.
For a 128-bit operand, a count of 128 or more (and a negative count) is outside
the C++ shift precondition.  .NET integral shifts mask the count to the low
seven bits, and the sibling `Int128` implementation already explicitly does
so.  The sanitizer probe reached the public operator with an ordinary count:

```
UInt128.hpp:95:73: runtime error: shift exponent 128 is too large for 128-bit type '__int128 unsigned'
unsigned_shift=1
```

The observed `1` coincides with the expected result of `1 << (128 & 127)`, but
it follows undefined behavior and can vary with compiler/version/optimization.
The focused tests exercise shifts by only 4 and therefore cannot expose the
unsupported range.

### Required post-audit verification

Mask the shift count with `127` before either native shift, as Int128 does.
Add left/right vectors for 128, 129, 255, and a negative count; run the
boundary vectors under UBSan.  Preserve unsigned right-shift behavior.

## Finding reference

**SR-AUD-021:** `ToString(format)` has the same invalid-format behavior as
`Int128`: `"Q"` returns decimal, while `"Xz"` and an oversized suffix expose
`std::stoi` exceptions rather than `System::FormatException`.

**SR-AUD-022:** `Clamp` has no `min > max` validation and returns a bound for
an invalid interval rather than .NET's `ArgumentException` behavior.

**SR-AUD-023:** the unsigned integral binary `B`/`b` format is missing; the
same decimal fallback observed for UInt64 applies here.

## Other missing assertions and diagnostics

- No test crosses the 64-bit word boundary for add/subtract/multiply/divide,
  compare, decimal conversion, or `IsPow2` at bit 127.
- The public conversion to signed `long long` truncates and may change sign;
  callers need an explicit checked-conversion alternative before using it for
  untrusted values.

## Final assessment

Usable common-path wrapper with a sanitizer-confirmed undefined shift path and
shared formatting-diagnostic defect.  The shift fix and boundary tests should
precede further numeric API expansion.
