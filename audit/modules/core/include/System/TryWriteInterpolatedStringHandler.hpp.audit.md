# Audit: `modules/core/include/System/TryWriteInterpolatedStringHandler.hpp`

## Metadata

- AUDITED: 123-line inline manual interpolation handler, fully read.
- Validation: `TryWriteInterpolatedStringHandlerTests.*` passed 13/13 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproduction: the format probe prints `bool=1`, `hex=255`, and
  `double=3.140000`; the ASan null-destination probe reaches a null write in
  `appendRaw` and exits 134.
- Reference basis: local .NET `System/MemoryExtensions.cs:5684-5710,6055-6250`.

## SR-AUD-132 — high — public raw destination pointer can be null and reaches an ASan-confirmed write crash

The two constructors accept arbitrary `char*` plus a positive length without
validation. `TryWriteInterpolatedStringHandler(nullptr, 1).AppendLiteral("x")`
passes the capacity check and reaches `std::memcpy(dest_ + pos_, ...)`; ASan
reports a zero-page write and the isolated probe aborts with exit 134. The
counterpart accepts a `Span<char>`, which cannot represent this public
nonempty-null destination state.

The C-string literal overload independently forwards a null `value` to
`std::strlen`, so both raw-pointer entry boundaries rely on native undefined
behavior rather than a documented failure result. Represent destination as the
project Span abstraction or reject invalid pointer/length combinations before
any pointer arithmetic, and define the null-literal policy.

## SR-AUD-133 — medium — AppendFormatted ignores format and replaces .NET formatting with hardcoded C++ spellings

The explicit format overload discards its format string, while the unformatted
route routes all arithmetic through `std::to_string` and every unsupported
type to `"[?]"`; it never performs the documented IFormattable fallback.
The standalone probe therefore emits `1` for `true`, `255` for `255` with
`"X2"`, and `3.140000` for `3.14`. Current .NET formats those normal cases as
`True`, `FF`, and the general `3.14` respectively, and its handler honors
IFormattable/ISpanFormattable, provider, alignment, and custom formatter
paths.

This makes interpolated text observably wrong even when capacity succeeds.
Implement format/provider-aware established project formatting and retain a
failure result for a short destination, or reduce/rename the surface as a
strictly documented primitive C++ formatter rather than the .NET handler.

## Other missing assertions and diagnostics

- Missing null destination/literal, zero-length, overlapping source,
  embedded-NUL, exact-boundary, and failure-prefix preservation vectors.
- Missing format, alignment, provider, enum, custom type/IFormattable,
  ISpanFormattable, string-view, and Unicode formatting coverage.
- The class is a manually used ordinary C++ object, not a compiler-generated
  `ref struct`; no diagnostic prevents copying, escaping, or calling it without
  the .NET `MemoryExtensions.TryWrite` completion boundary.
- `pos_ + len` is unchecked `size_t` arithmetic and no test exercises hostile
  length arithmetic or exception propagation from string/format creation.

## Final assessment

Ordinary literal accumulation works, but raw-pointer safety and formatting
semantics have the confirmed SR-AUD-132/133 gaps. No source or test was
modified during this audit.
