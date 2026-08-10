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

## Remediation note (ticket #1778, 2026-07-27)

SR-AUD-360 is **remediated**. This note is added alongside the original
evidence above, which is left unmodified per this repository's practice of
preserving historical audit narrative.

Root cause confirmed against the current .NET reference
(`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Concurrent/ConcurrentDictionary.cs`,
`AddOrUpdate` and `TryUpdateInternal`): real .NET gates the commit on
`EqualityComparer<TValue>.Default.Equals(currentValue, observedValue)` and
retries the whole operation (re-observe, re-invoke the factory) when the gate
fails. Both `ConcurrentDictionary::AddOrUpdate` overloads instead
unconditionally overwrote the entry with the factory's result regardless of
whether the entry changed underneath, exactly as the finding above describes.

Fix: both overloads now loop. After computing the new value outside the lock
(preserving the existing no-lock-across-user-code / reentrancy guarantee), the
commit re-acquires the lock, re-reads the entry, and only writes if it still
equals the previously observed value (`operator==`); otherwise the whole
operation retries against the newly observed state. A key absent at the
initial observation that is concurrently added by another thread falls
through to the update branch on retry instead of double-adding. This requires
`TValue` to support `operator==`, the same requirement `TryUpdate` on this
class already carries.

Pre-fix reproduction (gitignored `build-probe-concurrentdict/probe1_lost_update.cpp`,
ASan+UBSan+TSan): a coordinated two-thread repro blocks the update factory
after it observes `0`, writes `10` through the indexer while the factory is
blocked, then releases it. Pre-fix: `add-or-update-result final=1` (5/5 runs),
matching the finding's own `add-or-update-result=1 final=1` reproduction.
Post-fix: `final=11` (20/20 runs under ASan+UBSan; 5/5 under TSan), clean, no
sanitizer diagnostic. A second stress probe
(`build-probe-concurrentdict/probe2_stress.cpp`, 16 threads x 2,000
`AddOrUpdate` calls on one shared key) is clean under TSan with no data race
and the exact expected total (32,000).

Closure evidence: 4 new permanent regressions in `ConcurrentDictionaryTests.cpp`
(deterministic coordinated intervening-write repro for both overloads, a
key-added-concurrently retry case, and an 8-thread/500-iteration contention
stress case verifying the total reflects every increment); the full
`ConcurrentDictionaryTest` suite (26/26) passes consistently across repeated
runs; `SharpRuntimeTests_Collections_Core` 1,736/1,736 (was 1,732); the
network-permitted `scripts/local_ci_check.sh build` gate passes 13,021/13,021
tests across 37 executables with zero warnings/errors (was 13,017). Module
boundaries stay at 41 modules/90 edges; validator tests 7/7; catalogue
current; database consistent; `git diff --check` clean. No public signature
changed and no virtual member was added or removed, so this is neither a
source nor an ABI break.
