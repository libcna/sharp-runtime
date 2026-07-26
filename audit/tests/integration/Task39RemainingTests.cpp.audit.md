# Audit: `tests/integration/Task39RemainingTests.cpp`

## Metadata

- Audit status: AUDITED (496 lines, 56 tests in 12 suites, full read).
- Runtime evidence: the focused `SynchronizationContextTests`,
  `PeriodicTimerTests`, `WaitHandleTests`, text-encoding, collection-wrapper,
  storage, and experimental-property filter passed all 56 cases on 2026-07-25.

## Coverage observed

This aggregate regression file covers current-context storage and asynchronous
post dispatch, timer construction/disposal, constants, ASCII replacement width,
basic UTF-8/UTF-16 paths, encoding metadata, small collection wrappers,
dictionary extensions, a storage-root smoke test, and the experimental
property’s exception type.  The ASCII regression cases are particularly useful:
they distinguish UTF-8 byte iteration from .NET UTF-16-code-unit replacement
semantics.

## SR-AUD-013 — medium — `Send_InvokesCallbackSynchronously` has no observable assertion

The nominal `SynchronizationContext::Send` regression test (lines 64–70) passes
the address of an integer whose value is already zero to a callback that assigns
that same zero back.  It ends with `(void)value;` rather than an assertion.
Consequently the test passes if `Send` never invokes the callback, invokes it
asynchronously, or silently drops it; no behavior is observable after the call.
The 56-test focused command above reports this test green, demonstrating that
the test runner cannot diagnose the absent assertion.

### Required post-audit verification

Make the callback produce an unmistakable state change (for example write `42`)
and assert that state immediately after `Send` returns.  Add a bounded timing
or thread-state assertion only if it is necessary to distinguish the contract;
do not use a sleep as the primary proof of synchrony.

## Other missing assertions and diagnostics

- The 1 ms `PeriodicTimer` success test is scheduler-sensitive.  There is no
  explicit bounded timeout diagnostic for a late tick or a test for multiple
  ticks/one concurrent waiter.
- Unicode/UTF-8 tests cover ASCII round trips only.  They do not assert
  non-ASCII vectors, invalid byte behavior, byte-order configuration, or
  argument validation.  `EncodingInfo::GetEncoding` checks only non-nullness.
- Read-only wrappers are tested for initial contents but not live propagation
  of source mutations, index/error paths, or event forwarding.  Storage paths
  are tested only for non-emptiness, not stable platform policy.
- `Post_NullCallback_NoThrow` establishes a local choice but no diagnostic
  explains whether it is intentional parity or a benign no-op adaptation.

## Final assessment

The file contains several valuable, narrowly targeted regressions.  Its green
result overstates coverage for `Send`: the central synchronous-callback test is
currently a no-op assertion and needs repair after the audit.
