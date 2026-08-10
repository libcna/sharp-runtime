# Audit: `modules/core/include/System/AttributeUsageAttribute.hpp`

## Metadata

- Audit status: AUDITED (90-line declaration, read with its 10-line static
  definition and shared fixture).
- Validation: `AttributeUsageAttributeTests.*` passed 9/9 in the 77-test
  focused Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/AttributeUsageAttribute.cs:10-45`.

## Findings

The constructors, `ValidOn`, `AllowMultiple`, `Inherited`, and the All/false/
true `Default` instance agree with current .NET.  The C++ port additionally
exposes `Default` publicly and leaves the type derivable, while .NET keeps the
field internal and seals the class; this is an additive C++ surface difference
rather than a demonstrated runtime fault.

## Other missing assertions and diagnostics

- Tests do not verify the static object from a separately linked consumer;
  that link route is supplied by `AttributeUsageAttribute.cpp`.
- `validOn_`, `allowMultiple_`, and `inherited_` are ordinary object state.
  There is no syntax, registry, or compiler/reflection consumer that attaches
  the object to an attribute class or enforces target, multiplicity, and
  inheritance restrictions.  That follows the documented global reflection
  exclusion, but the API comment should not be read as runtime enforcement.

## Final assessment

Stored values have parity, whereas metadata application is deliberately not a
feature of this C++ runtime.  No source or test was modified during this audit.
