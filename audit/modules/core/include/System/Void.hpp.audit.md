# Audit: `modules/core/include/System/Void.hpp`

## Metadata

- AUDITED: 35-line public placeholder value type, fully read.
- Validation: `VoidTest.*` passed 5/5 in the combined 12-test
  `VoidTest.*:UnitySerializationHolderTest.*` Core.Base filter on 2026-07-26.
  The separately pending `SystemTypesRemainingTests.cpp` has duplicate plural
  Void smoke cases and is not marked audited here.
- Reproduction: local C# probe attempting construction, `ToString`, or
  `List<System.Void>` fails with CS0673 (and construction also CS0143).
- Reference basis: local .NET `System/Void.cs:8-12` and C# compiler output.

## SR-AUD-136 — medium — Void is presented as a normal C++ value/generic type although C# forbids the documented use cases

The header says its ordinary struct permits porting generic patterns such as
`Nullable<Void>` and `Task<Void>`, and it adds instantiation, equality, and an
empty-string `ToString`. C++ tests construct values, place them in vectors, and
assign them to Nullable. The local C# compiler rejects all analogous ordinary
`System.Void` uses with CS0673: the public runtime metadata type represents the
`void` return type, not a usable C# value or generic argument. The same source
has no declared fields or methods, while this header creates a user-observable
value API.

If a C++ unit/absence type is needed, make it a clearly project-specific type
and remove the claimed C# generic portability. If reflection compatibility is
the goal, retain only metadata-oriented handling and document that ordinary
construction, equality, and value text are C++ extensions.

## Other missing assertions and diagnostics

- No test distinguishes C++ adaptation behavior from a usable C# source/API
  contract or checks reflection/type-name handling.
- The empty `ToString` claim is not compared with the inherited object/type
  text behavior and has no documented consumer.
- No test covers type traits, hash/container behavior, serialization, or the
  interactions with Task/Nullable that the header advertises.

## Final assessment

The empty C++ struct is mechanically safe, but its advertised .NET/generic
meaning is the confirmed SR-AUD-136 contract drift. No source or test was
modified during this audit.
