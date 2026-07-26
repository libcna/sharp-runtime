# Audit: `modules/runtime/include/System/Runtime/CompilerServices/AsyncStateMachineAttribute.hpp`

## Metadata

- AUDITED: 22-line inline derived-marker declaration, fully read.
- Validation: `StateMachineAttributeTests.*` passed 2/2 on 2026-07-27.
- Reference basis: local current-.NET `AsyncStateMachineAttribute.cs`.

## Assessment

The final derived class and forwarding `System::Type` constructor match the
representable managed class shape.  C++ has no CLR async-state-machine lowering
or method metadata attachment; the already audited base header records that
intentional boundary.  No production consumer was found beyond the shared
fixture.

## Other missing assertions and diagnostics

- Tests retain one ordinary Type through the base property but omit native
  declaration attachment, null-like Type state, and compiler-generated async
  metadata discovery.
- There is no diagnostic when users construct this marker expecting a C++
  coroutine to acquire managed async metadata.

## Final assessment

The narrow value wrapper is coherent with the documented metadata-only
adaptation.  No confirmed source defect and no source or test modification
resulted from this review.
