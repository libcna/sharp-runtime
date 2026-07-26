# Audit: `modules/buffers/include/System/Buffers/StandardFormat.hpp`

## Metadata

- Audit status: AUDITED (130-line public header-only implementation, fully
  read).
- Validation: the complete owning `BuffersTests.cpp` filter (`ArrayPoolTests.*`,
  `OperationStatusTests.*`, and `StandardFormatTests.*`) passed 38/38 in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-standardformat-audit-probe.cpp` was
  compiled with `g++ -std=c++20 -I modules/buffers/include -I
  modules/core/include /tmp/sharp-runtimervc-standardformat-audit-probe.cpp
  build/libsharp_runtime_core.a -o /tmp/sharp-runtimervc-standardformat-audit-probe`
  and executed on 2026-07-26.
- Reference: local .NET `System/Buffers/StandardFormat.cs` and
  `System.Memory` `ParsersAndFormatters/StandardFormatTests.cs` were reviewed.

## Assessment

The explicit byte precision bound, zero-initialized representation,
NoPrecision sentinel, equality, xor hash, ordinary parsing, and ordinary
format strings agree with the current .NET struct.  `ToString`, however,
always begins a C++ string with `format_` and appends precision whenever it is
not `NoPrecision`; it omits .NET's special empty representation for a zero
symbol.  This makes the public default value serialise to embedded-NUL text.

## SR-AUD-083 — medium — StandardFormat::ToString serializes the default symbol as embedded NUL text

Current .NET returns an empty string whenever `Symbol` is zero: its internal
`Format` first checks `symbol != default`, and returns an empty span otherwise.
That applies to `default(StandardFormat)`, to `Parse("")`, and to an explicitly
constructed zero symbol with either precision state.

The C++ implementation instead starts `std::string s(1, format_)` and then
appends decimal precision at lines 83–86.  The standalone probe prints string
sizes and byte values for `default`, `StandardFormat('\0')`, and
`StandardFormat('\0', 0)`:

```text
2,0,48
1,0
2,0,48
```

They should all be a zero-length string.  The default C++ object therefore
renders as `"\\0" + "0"`, not `""`; loggers, format forwarding, equality of
textual forms, and consumers that pass `ToString()` to a formatter observe a
different value despite the header claiming the default representation matches
.NET exactly.  The local .NET suite explicitly includes the expected empty
`ToString` form; the C++ suite checks default state but never its rendering.

## Other missing assertions and diagnostics

- The direct fixture omits `ToString()` for default, `Parse("")`, zero symbol
  with NoPrecision, and zero symbol with explicit zero precision; it also never
  checks `std::string::size()`/embedded bytes.
- No test attempts a non-byte Unicode format symbol.  Current .NET rejects
  `(char)256`; the C++ narrow-`char` API cannot express that call and needs an
  explicit representation/diagnostic decision.
- The .NET implicit char-to-StandardFormat conversion has no C++ counterpart
  because the constructor is `explicit`.  This is a possible source-level
  adaptation, but it is undocumented and has no compile-consumer evidence.
- `Parse(string? null)` and span overload parity are not representable by the
  C++ `std::string`-only entry points.  No documentation differentiates this
  adaptation from a missing overload.
- Failed `TryParse` tests only inspect the boolean.  They should assert that
  result returns to the true default (`symbol == 0`, `precision == 0`) after a
  prior non-default value, matching the .NET `out` behavior.
- No tests cover all precision residues 0–99, a leading NUL symbol, bad UTF-8,
  hash collision acceptability, or malformed multi-byte format input.

## Final assessment

Normal format values work, but default/zero-symbol `ToString` is a confirmed
observable parity defect.  No production or test source was modified during
this audit.
