# Audit: `modules/runtime/include/System/Runtime/Serialization/StreamingContext.hpp`

## Metadata

- AUDITED: 22-line inline stub declaration, fully read.
- Validation: the shared `StreamingContextTests.*` filter passed 1/1 on
  2026-07-27; the standalone C++20 warnings-as-errors stub probe passed.
- Reference basis: local current-.NET 64-line `StreamingContext.cs`, local
  source-consumer search, and the permanent serialization deviation in
  `CLAUDE.md`.

## Assessment

Current .NET supplies an obsolete readonly value type with a flags
`StreamingContextStates` value, nullable additional context, equality, and
hashing.  This C++ empty struct has none of those data or operations.

The header explicitly marks that mismatch as a permanent no-op adaptation and
states that it is retained only so ported code can reference the type.  Search
finds no production parameter, field, or behavior consumer.  Together with
the matching explicit `SerializationInfo` decision, that makes the absence
intentional and visible rather than an unsupported behavior silently claimed
as implemented.

## Other missing assertions and diagnostics

- The sole direct case only default-constructs the type.  It cannot signal a
  future accidental claim of state flags, context identity, equality, or
  serialization behavior.
- The test is embedded in the not-yet-complete
  `SystemTypesRemainingTests.cpp`; the passing one-case filter does not audit
  its unrelated sections.
- A future first-party API accepting `StreamingContext` needs a compile/test
  review of whether an empty marker remains sufficient for its contract.

## Final assessment

The marker-only C++ type is a documented permanent serialization adaptation
with no production consumer.  No new finding and no source or test change.
