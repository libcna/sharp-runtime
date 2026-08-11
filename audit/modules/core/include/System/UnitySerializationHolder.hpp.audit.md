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

### Status: STILL CONFIRMED — approval boundary recorded (#2279 review, #2281 `needs_user`, 2026-08-11)

**Neither of this finding's two alternatives is available without a user
decision**, and that is what separates it from SR-AUD-127, which was ranked
beside it and was remediated by its own second alternative:

- the first alternative is a **conjunction**. "Explicitly project-specific" is
  documentation and costs nothing; "make the wrapper internal" withdraws
  `System::UnitySerializationHolder` — or at minimum its public constant,
  constructor and two getters — from the `Core.Base` include tree, which is a
  public source break. Doing the documentation half alone does not satisfy the
  option as written, and this batch may not inspect the downstream consumers that
  would pay for the other half.
- the second alternative needs `SerializationInfo` and `StreamingContext`
  parameters on the constructor, `GetObjectData` and `GetRealObject`. Neither type
  exists in this port, and `CLAUDE.md` records serialization infrastructure
  (`[Serializable]`, `SerializationInfo`) as a **permanent deviation** — "ignored;
  not needed for game code". Implementing it would reverse a standing project
  decision, which is a user decision rather than a remediation.

The finding therefore stays **confirmed** and ticket **#2281** carries the choice
with both costs stated. It was deliberately not promoted merely because it was
ranked next to a finding that turned out to be repairable.

**What was corrected anyway, and what it does not close.** One statement in the
header is wrong regardless of which option is eventually chosen: it claimed the
public API surface "is preserved for source-compatibility", which this report's
own comparison disproves — C# written against .NET's signatures does not compile
against this header. The doc-comment now states what the port actually provides,
why the serialization interfaces are absent, that an ordinary caller can fabricate
a holder state .NET only builds from a stream, and that the shape is under an open
decision. **The public surface itself is unchanged, so SR-AUD-137 is not closed.**

**Verdict on the ranking that paired this with SR-AUD-127: not a family.** They
share a characteristic — a .NET-internal type republished publicly with no
first-party consumer — not a cause: that one is republished *verbatim*, this one
with a *different* shape plus a false source-compatibility claim, and only this
one is blocked. No CCF minted; the same characteristic runs through
SR-AUD-124/125/126/128/129/136.

Consumer inventory re-measured rather than inherited: **zero** production
consumers; the type is covered by **two** suites whose names differ only by a
trailing `s` — `UnitySerializationHolderTest` (7 tests,
`UnitySerializationHolderTests.cpp`) and `UnitySerializationHolderTests` (8 tests,
`SystemTypesRemainingTests.cpp`) — the duplication this report's metadata already
flagged. It is left untouched: it belongs to whatever change settles the type's
shape. No test was added either, because pinning more of a surface #2281 may
withdraw would raise the cost of the decision the ticket exists to take.

**A separate defect found while reviewing (#2282, no `SR-AUD-*` identifier).**
`GetRealObject()` composes its rejection message as
`"Invalid unity type: " + (data_.empty() ? std::to_string(unityType_) : data_)`,
so whenever the optional data string is non-empty the message prints **the data,
not the unity type it names**: `UnitySerializationHolder(999, "UnknownType")`
reports `Invalid unity type: UnknownType` and the value 999 that caused the
rejection never appears. That is verifiable entirely inside this repository. It is
not folded into SR-AUD-137, which is about the public shape, and it is not
implemented here, because any repair changes the message text and the exact
replacement needs the .NET reference this report already defers.
`docs/CoreCrashReasonAndUnityHolderPlan.md`.

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
