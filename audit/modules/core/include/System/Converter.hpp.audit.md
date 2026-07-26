# Audit: `modules/core/include/System/Converter.hpp`

## Metadata

- AUDITED: 21-line alias-only public header, fully read.
- Validation: `ConverterTests.*:PredicateTest.*:FuncTests.*` passed 17/17 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: local .NET `System/Action.cs:184-192` and the corresponding
  C++ `Action.hpp` alias adaptation.

## Findings

The ordinary `Converter<TInput, TOutput>` callable signature is a direct C++
adaptation of the .NET delegate.  Its duplicate declaration in `Action.hpp`
is include-compatible: the standalone probe included both headers under
`-Wall -Wextra -Wpedantic` without a diagnostic.

`TOutput` is nevertheless unconstrained, so `Converter<int, void>` is a valid
alias and is exactly `ActionT<int>`; this participates in SR-AUD-126.  A
current .NET `Converter<TInput, TOutput>` cannot use `void` as `TOutput` and
keeps value-returning delegates distinct from `Action`.

## Other missing assertions and diagnostics

- The fixture has no default/empty Converter invocation vector, so it does
  not document the native `std::bad_function_call` behavior selected by this
  C++ adapter.
- No fixture proves that reference, move-only, or exception-throwing callable
  targets retain the documented C++ ownership and propagation behavior.
- No compile-time regression rejects the unsupported `void` result category or
  documents it as an intentional extension.

## Final assessment

The regular value-returning alias is stateless and mechanically correct; the
missing output-type boundary is recorded by SR-AUD-126. No source or test was
modified during this audit.
