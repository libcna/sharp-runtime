# Audit: `modules/core/include/System/Func.hpp`

## Metadata

- AUDITED: 166-line alias-only public header, fully read.
- Validation: `ConverterTests.*:PredicateTest.*:FuncTests.*` passed 17/17 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-functional-alias-audit-probe` compiles
  with `-Wall -Wextra -Wpedantic` and prints `func_void=1`,
  `converter_void_result=1`, `func_void_is_action=1`, and
  `converter_void_is_action_t=1`; the counterpart C# probe fails with two
  `CS1547` diagnostics.
- Reference basis: local .NET `System/Function.cs:6-190` and
  `System/Action.cs:184-192`.

## SR-AUD-126 — medium — Func and Converter permit void result specializations that collapse the separate .NET Action category

Every C++ alias accepts an unconstrained `R`.  Consequently `Func<void>` and
`Converter<int, void>` compile, invoke normally, and are respectively the
same types as `Action` and `ActionT<int>`.  The C++ probe confirms both type
equalities.  Current .NET declares value-returning `Func<TResult>` and
`Converter<TInput,TOutput>` separately from `Action`; `void` cannot be a C#
generic argument, as the local `mcs` probe shows with `CS1547` for both forms.

This exposes action-like delegate forms under incompatible public names and
prevents consumers from preserving the category distinction that the .NET API
uses for overload selection and documentation.  Constrain the C++ result
types to non-`void`, or explicitly document and test this as a deliberate C++
extension while preventing APIs that require .NET parity from accepting the
substitute aliases.

## Other missing assertions and diagnostics

- Tests exercise arities 0–4, 8, and 16 only; aliases 5–7 and 9–15 have no
  direct compile/use fixture.
- There is no default empty Func invocation test, so the public native
  `std::bad_function_call` diagnostic is not documented.
- The `FuncT`, `FuncT2`, … names are a necessary C++ arity adaptation rather
  than .NET's single overloaded `Func` name; no API note tells consumers that
  source spelling changes with arity.
- No test covers exceptions, captured lifetime, move-only callable rejection,
  or result/argument reference preservation.

## Final assessment

All ordinary aliases are direct stateless wrappers, but their unconstrained
result category creates the confirmed SR-AUD-126 public-contract gap. No
source or test was modified during this audit.
