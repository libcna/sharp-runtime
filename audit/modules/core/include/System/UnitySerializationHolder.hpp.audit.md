# Audit: `modules/core/include/System/UnitySerializationHolder.hpp`

## Metadata

- AUDITED: 94-line legacy-serialization compatibility wrapper, fully read.
- Validation: `UnitySerializationHolderTest.*` passed 7/7 in the combined
  12-test `VoidTest.*:UnitySerializationHolderTest.*` Core.Base filter on
  2026-07-26. Duplicate plural smoke cases remain in the pending mixed test
  source and are not counted as a completed test-file audit.
- Reference basis: local .NET `System/UnitySerializationHolder.cs:8-54`.

## SR-AUD-137 — medium — UnitySerializationHolder replaces the public serialization contract with an unrelated inspectable data object

Current .NET keeps `NullUnity` internal, stores its type/data fields privately,
and exposes a public constructor accepting `SerializationInfo` plus
`StreamingContext`, `GetObjectData(SerializationInfo, StreamingContext)`, and
`GetRealObject(StreamingContext)` through serialization interfaces. This C++
header instead makes `NullUnity` public, constructs from arbitrary raw
`intcs`/string data, exposes both private fields with getters, and substitutes
parameterless GetObjectData/GetRealObject methods. No first-party production
consumer uses the type.

Omitting unsupported BinaryFormatter infrastructure can be legitimate, but
this is not the claimed source-compatible public surface: it lets ordinary
callers fabricate internal serialization states and removes the SerializationInfo/
StreamingContext boundary. Either make the wrapper internal and explicitly
project-specific, or retain recognizable compatibility signatures and provide
deterministic unsupported diagnostics at those boundaries.

## Other missing assertions and diagnostics

- No test covers SerializationInfo, StreamingContext, ISerializable,
  IObjectReference, obsolete-use diagnostics, or nullable data semantics.
- No integration path actually deserializes DBNull or verifies a type-forward/
  legacy-format compatibility boundary.
- The exact invalid-unity diagnostic and `ArgumentException` parameter/context
  are not compared with the reference behavior.

## Final assessment

DBNull singleton return and unsupported serialization write are locally
coherent, but the public shape has the confirmed SR-AUD-137 drift. No source
or test was modified during this audit.
