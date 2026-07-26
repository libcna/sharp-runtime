# Audit: `modules/runtime/include/System/Runtime/CompilerServices/StateMachineAttribute.hpp`

## Metadata

- AUDITED: 32-line inline value-metadata declaration, fully read.
- Validation: `StateMachineAttributeTests.*` passed 2/2 on 2026-07-27; the
  compiler-services shared filter passed 9/9.
- Reference basis: local current-.NET `StateMachineAttribute.cs`.

## Assessment

The constructor and read-only `System::Type` property match the directly
representable managed value contract.  The header expressly documents that C++
has no CLR compiler-generated state-machine metadata; retaining a supplied
type is therefore an intentional explicit adaptation rather than an implied
claim that `async` or iterator compilation is annotated automatically.

## Other missing assertions and diagnostics

- Tests verify a normal retained type and the two derived attribute types, but
  omit default/null-like `Type` representation, type lifetime/copy behavior,
  and compile-time method attachment.
- No diagnostic tells callers that applying/constructing this object cannot
  make an ordinary C++ coroutine or iterator discoverable as managed
  state-machine metadata.

## Final assessment

The value property is correct within its stated metadata-only boundary.  No
confirmed source defect and no source or test modification resulted from this
review.
