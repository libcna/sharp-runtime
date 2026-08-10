# Audit: `modules/core/include/System/ThreadStaticAttribute.hpp`

## Metadata

- Audit status: AUDITED (14-line marker declaration, fully read with its
  dedicated fixture).
- Validation: `ThreadStaticAttributeTest.*` passed 3/3 in the 18-test marker
  attribute filter on 2026-07-26.
- Reference basis: local .NET `ThreadStaticAttribute.cs` field-isolation
  contract and C++ storage-duration semantics.

## SR-AUD-113 — medium — ThreadStaticAttribute has no attachment or storage mechanism and cannot provide its documented per-thread field value

The C++ class is an ordinary instantiable `Attribute` subclass with no macro,
compiler attribute, registry, or `thread_local` integration
(`ThreadStaticAttribute.hpp:9-13`). Instantiating it cannot affect any static
field, yet its public documentation says it indicates a value unique to each
thread. Current .NET applies the metadata attribute to a static field and the
runtime supplies separate storage.

The three green tests only construct two marker objects and check inheritance;
they do not declare a static field or create a second thread. Unlike the
STA/MTA headers, this file also does not state that the marker has no C++
runtime effect, so callers receive a silent contract break rather than an
explicit unsupported-feature boundary.

## Other missing assertions and diagnostics

- No API provides an equivalent `thread_local` declaration pattern or rejects
  attempts to use this class as one.
- Tests omit concurrent/static-field isolation, default initialization,
  inheritance policy, and reflection/metadata availability.

## Final assessment

The marker compiles but cannot implement the behavior its public comment
describes. No source or test was modified during this audit.
