# Audit: `modules/core/tests/System/RandomTests.cpp`

## Metadata

- Audit status: AUDITED (668 lines, 74 tests, full read).
- Validation: all 74 Random tests passed in the focused 92-test run.

## Assessment

The suite is unusually strong for seeded compatibility: it has concrete
cross-runtime vectors, edge/range checks, Span/vector consistency, collection
utilities, and shared-instance identity.  `Shared_ProducesValues` exercises
only one call in one thread, so it cannot validate the stated thread-safety
contract.

## Finding reference

**SR-AUD-010:** no concurrent access or TSan scenario covers
`Random::getSharedProperty()` despite its documented global thread-safety.

## Required post-audit verification

Add a bounded multi-thread invocation test that checks value ranges and
completion, then run it with TSan.  The regression should target the shared
instance, not incorrectly require all individually seeded instances to be
thread-safe.

## Final assessment

Excellent seeded parity evidence; missing concurrency coverage cannot detect
the shared-instance data race.
