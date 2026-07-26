# Audit: `modules/collections/include/System/Collections/Concurrent/ConcurrentDictionary.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-360 — medium — ConcurrentDictionary AddOrUpdate loses a concurrent update instead of retrying against the observed value

Both `AddOrUpdate` forms snapshot an existing value, invoke the update factory outside the lock, then unconditionally assign the result.  The implementation documents this as a simplification, but it violates the atomic retry contract: a coordinated probe starts from 0, blocks the factory, writes 10 through the indexer, then resumes; it prints `add-or-update-result=1 final=1`.  .NET retries after the failed compare-and-update, so an incrementing factory must observe 10 and commit 11 rather than overwrite the intervening operation.

## Missing assertions and diagnostics

- Concurrent tests do not coordinate an intervening writer between factory observation and commit.
- Add a retry/compare-failure counter and a deterministic contention test for both overloads.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
