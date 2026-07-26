# Audit: `modules/runtime/include/System/Runtime/CompilerServices/RequiredMemberAttribute.hpp`

## Metadata

- AUDITED: 22-line inline marker declaration, fully read.
- Validation: the compiler-metadata marker fixture passed 1/1 on 2026-07-27.
- Reference basis: local current-.NET `RequiredMemberAttribute.cs`.

## Assessment

The final empty marker matches the directly representable managed type.  Its
documentation correctly limits it to metadata because required-member
validation is a C# compiler responsibility; no C++ declaration attachment,
construction check, or production consumer exists.  This is an explicit
capability omission, not an undisclosed runtime failure.

## Other missing assertions and diagnostics

- The shared fixture creates the marker but does not model a required field or
  property, incomplete construction, or a compiler failure.
- No native alternative policy is exercised for enforcing required values in
  C++ constructors/builders.

## Final assessment

The inert marker is faithful to its stated native scope.  No confirmed source
defect and no source or test modification resulted from this review.
