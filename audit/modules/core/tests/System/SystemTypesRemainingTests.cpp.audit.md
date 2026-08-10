# Audit: `modules/core/tests/System/SystemTypesRemainingTests.cpp`

## Metadata

- AUDITED: 187 cases spanning enum metadata, nullable/range/index/value tuple,
  weak references, observable interfaces, serialization placeholders, and
  miscellaneous system adapters.
- Validation: the complete Core.Base fixture passed 4,946/4,946.

## Assessment

This large catch-all fixture gives useful nominal coverage for many small
types.  It combines unrelated contracts, however, which makes it unsuitable
as sole evidence for lifetime, concurrency, hashing, serialization, or
platform-specific behavior.  Its hash tests correctly require equal values to
have equal hashes but need not assert distinct values have distinct hashes.

## Other missing assertions and diagnostics

- Split high-risk ownership/concurrency tests from value-object smoke tests;
  exercise weak-reference expiry under deterministic ownership transitions.
- Add invalid enum/range boundaries, nullable move/copy behavior, nontrivial
  ValueTuple members, and `HashCode` collision-legal assertions.
- Keep SerializationInfo/StreamingContext explicitly marked as legacy stubs
  and add a consumer test only if their ignored serialization scope changes.

## Final assessment

No independent defect was demonstrated by this mixed fixture. No source or
test was changed.
