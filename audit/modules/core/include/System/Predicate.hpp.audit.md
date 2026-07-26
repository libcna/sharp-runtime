# Audit: `modules/core/include/System/Predicate.hpp`

## Metadata

- AUDITED: 21-line alias-only public header, fully read.
- Validation: `ConverterTests.*:PredicateTest.*:FuncTests.*` passed 17/17 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: local .NET `System/Action.cs:188-190`.

## Findings

`Predicate<T>` correctly maps an ordinary one-input boolean callable to
`std::function<bool(T)>`.  C++ itself rejects `Predicate<void>` because `void`
cannot be a function parameter, so the header does not introduce the
value-returning `void` extension found in Func/Converter.

The default-constructed alias deliberately has the native empty
`std::function` state.  Its direct invocation throws `std::bad_function_call`,
which the existing test explicitly records.  This is an adapter boundary, not
an independently reachable library dispatch defect: no first-party method
accepts a Predicate in this header.

## Other missing assertions and diagnostics

- No fixture exercises a predicate that throws, captures shared state, or
  accepts a non-copyable input by move.
- The test suite does not distinguish a null .NET delegate reference from the
  C++ empty-callable state, nor does the header state the intended diagnostic
  mapping at an invocation boundary.
- No compile-only vector covers all language-valid/invalid generic argument
  categories or C++ include composition with `Action.hpp`.

## Final assessment

Correct thin callable adaptation for ordinary inputs; no standalone
evidence-backed defect was found. No source or test was modified during this
audit.
