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

### Status: CONFIRMED (DESIGN-COMPLETE) — approval-bound, #2299 `needs_user` (#2296 review, 2026-08-11)

**Reproduces as filed:** `Func<void>` *is* `Action` and `Converter<T, void>` *is*
`ActionT<T>` — identical types, not convertible ones, because an alias template
introduces no new type.

**The second prescription above is structurally impossible.** "Preventing APIs
that require .NET parity from accepting the substitute aliases" cannot be
satisfied by any alias-based design: there is only one type, so no declaration
written in terms of these names can accept `Action` while rejecting `Func<void>`,
or the reverse. Preserving the category would mean replacing every alias with a
distinct class type — a whole-API break. The realistic choice is therefore
narrower than this report implies, and it is recorded as such rather than
attempted.

**The first prescription is mechanically available, and measured:** a constrained
alias-template parameter (`template<NonVoid R> using Func = std::function<R()>;`)
is legal C++23 here and makes `Func<void>` fail with `error: template constraint
failure … constraints not satisfied`. It is a **compile-domain public source
break** at every `Func<void>` / `Converter<T, void>` downstream, across all 17
`Func` aliases and both `Converter` declarations. Consumers measured: **zero**
production sites name the `Func`/`FuncT*` aliases and **zero** sites anywhere
spell a `void` result — which licenses nothing, the headers being public in
`Core.Base`. Also measured and **not** a defect: `Converter` is declared
identically in `System/Converter.hpp` and `System/Action.hpp`, and including both
in one translation unit compiles clean under `-Wall -Wextra -Wpedantic -Werror`.

**Taken anyway, true under every outcome (#2300):** `Func.hpp` now carries the
arity-spelling note this report asks for, the `std::bad_function_call` note, and
a `void` warning that includes the structural reason above; `Converter.hpp`
carries the matching warning and `Action.hpp` a cross-reference at its duplicate
declaration. **This does not close the finding** — `Func<void>` still compiles.

**Five tests added, none retired**, closing three of the four "Other missing
assertions" bullets: `Arities5Through7_CompileAndInvoke` and
`Arities9Through15_CompileAndInvoke` (the report's 5–7 and 9–15 gap),
`EmptyFunc_ThrowsBadFunctionCall`, `TargetExceptionPropagatesUnchanged` and
`ReferenceResultIsNotCopied`. Mutation: dropping `T11` from `FuncT11`'s signature
is caught at compile time by the arity case and by nothing else. **No test
asserts `Func<void>` is `Action`**, deliberately — that is the surface #2299 may
remove.

**Not a family with SR-AUD-128 or SR-AUD-129.**
`docs/CoreMarshalSlotAndFuncShapePlan.md` §4.

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
