# Audit: `modules/runtime/include/System/Runtime/Serialization/SerializationInfo.hpp`

## Metadata

- AUDITED: 33-line inline stub declaration, fully read.
- Validation: the shared `SerializationInfoTests.*` filter passed 2/2 on
  2026-07-27; a standalone C++20 `-Wall -Wextra -Werror` probe constructed
  both serialization stub types successfully.
- Reference basis: local current-.NET 467-line `SerializationInfo.cs`, local
  `Exception.cs`, source-consumer search, and `CLAUDE.md`'s permanent
  serialization-deviation decision.

## Assessment

Current .NET's sealed, obsolete legacy serialization object requires a
`Type` and formatter converter, then stores named typed values, exposes
metadata properties/enumeration, supports typed Add/Get overloads, and remains
the parameter type of obsolete exception serialization members.  C++ instead
offers only a default-constructible, virtual-destruction shell.

This is not a newly classified omission: the header explicitly says it exists
solely for unused ported method signatures, states that no first-party type
constructs or reads it, and points to the repository-wide permanent decision
that `[Serializable]`/`SerializationInfo` are ignored for game code.  Source
search confirms no production consumer.  The green direct tests prove only
the intended no-op constructibility; they do not claim serialization parity.

## Other missing assertions and diagnostics

- The shared tests sit in the otherwise unaudited
  `SystemTypesRemainingTests.cpp`; only their three focused cases are used as
  validation evidence here, not as completion evidence for that whole file.
- There is no compile diagnostic that a ported signature can name both stubs
  without accidentally promising `AddValue`, `GetValue`, formatter-converter,
  exception-constructor, or binary-formatter behavior.
- No source-level guard prevents a future type from taking this stub as an
  operational serialization argument.  Such a consumer must either implement
  a bounded native data contract or fail explicitly rather than silently
  treating the shell as real serialized data.

## Final assessment

The extremely narrow surface is an explicit, documented permanent adaptation
with no production consumer.  No new finding and no source or test change.
