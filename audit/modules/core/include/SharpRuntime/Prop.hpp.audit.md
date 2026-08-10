# Audit: `modules/core/include/SharpRuntime/Prop.hpp`

## Metadata

- AUDITED: 193-line macro property declaration/implementation helper, fully
  read.
- Validation: `PropMacroTests.*` passed 4/4 on 2026-07-27.
- Reference basis: the documented macro examples and direct integration
  fixture.

## Assessment

The tested `DDATA`/`IDATA` and `DGETTER`/`IGETTER` pairs expand to a
member-backed const-reference getter plus lvalue/rvalue setters as documented.
The direct fixture declares and defines a real `PropBox`, then verifies read,
write, and read-only access.  This is an intentionally native macro facility,
not a .NET surface.

## Other missing assertions and diagnostics

- No test compiles `DGETTERSTATIC`/`IGETTERSTATIC`, nontrivial move-only
values, const objects, references after move, or multiple classes in one
translation unit.
- Macro expansion diagnostics are fragile by nature; a preprocessor/compile
  fixture should cover all public macro pairs, not only the two nominal forms.
- The implementation source supplies an empty `namespace SharpRuntime` solely
  as a syntactic anchor; no namespaced C++ API is exposed for tooling to
  inspect.

## Final assessment

The exercised member-backed macro paths are coherent.  No new finding and no
source or test change.
