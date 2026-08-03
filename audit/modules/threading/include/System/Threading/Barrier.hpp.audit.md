# Audit: `modules/threading/include/System/Threading/Barrier.hpp`

## Metadata

- AUDITED: 178-line phased barrier/state machine, fully read.
- Validation: existing Barrier fixture coverage was reviewed; direct C++ and
  .NET 10 post-phase property probes were run, and a standalone participant
  property probe was built with `-fsanitize=thread`.
- Reference basis: current .NET 10 Barrier post-phase callback and public
  property behavior.

## SR-AUD-210 — high — a valid post-phase callback deadlocks when it reads `CurrentPhaseNumber`

`FinishPhase` calls the user post-phase action while holding `mutex_`, but
`getCurrentPhaseNumberProperty()` attempts to lock that same non-recursive
mutex. The one-participant C++ child whose callback reads CurrentPhaseNumber
does not complete within two seconds (exit 124). The identical .NET 10 probe
prints `phase=0` and `completed` immediately. The local code also increments
`phaseCount_` before invoking the callback, so merely removing the self-lock
would expose phase 1 instead of the managed callback's completing phase 0.

## SR-AUD-212 — high — `ParticipantCount` has an unsynchronized public read that races participant changes

`getParticipantCountProperty()` returns `participantCount_` without the
mutex, while AddParticipant and RemoveParticipant modify it while holding the
mutex. A worker repeatedly adds/removes a participant while another thread
reads the property; TSan reports the race between the getter at line 75 and
AddParticipant at line 113. This is undefined C++ behavior on a thread-safe
managed synchronization primitive.

## Assessment

The existing one-/multi-participant phase, post-action fault wrapping, and
post-action reentrancy tests cover important prior repairs. They do not read
ordinary properties from a legal callback or concurrently observe participant
membership, so both high defects remain green under the fixture suite.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-210 callback reads of phase/participant properties,
  callback timing/current-phase semantics, and all non-mutating callback
  interactions.
- Tests omit SR-AUD-212 TSan participant-count reads with Add/Remove and the
  equivalent concurrent disposal/property routes.
- They also omit AddParticipants/RemoveParticipants batch APIs, cancellation
  and timeout SignalAndWait overloads, post-phase exception recovery in later
  phases, phase-count overflow, and disposal with blocked participants.

## Final assessment

SR-AUD-210 and SR-AUD-212 are confirmed by bounded direct probes and TSan. No
production or test source was changed.


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-212 → `remediated`

Cause **T-A** of `docs/ThreadingNamespaceReviewPlan.md`, "shared mutable state is observed
outside its own mutex". Evidence: `build-probe/1955_probe1_shared_state_races.cpp` under
`-fsanitize=thread`, logs `1955_probe1_tsan_before.log` (**13** data-race reports across
seven scenarios, exit 66) and `1955_probe1_tsan_after.log` (**zero** reports, exit 0, every
control value unchanged). Instrumentation was proved rather than assumed: 132 `__tsan_*`
symbols in the sanitized binary. The layout gate passed — `sizeof` and `alignof` are
byte-identical for all six affected types before and after
(`1955_probe1_layout_before.log` / `1955_probe1_layout_after.log`), so no user approval was
required, and the numbers are pinned by
`ThreadingSharedStateTests.RepairedTypes_LayoutUnchanged` in
`modules/threading/tests/System/Threading/ThreadingSharedStateTests.cpp`.

`participantCount_` is now `std::atomic<intcs>`, acquire-loaded by
`getParticipantCountProperty()` and still written under `mutex_` by `AddParticipant()` and
`RemoveParticipant()`. `disposed_` likewise became `std::atomic<bool>`.

### Why an atomic field and not a locked read

Two reasons, the second decisive.

1. .NET's `Barrier.ParticipantCount` reads its packed `_currentTotalCount` field **without**
   taking the barrier's lock, so an unlocked-but-well-defined read is the parity answer.
2. Taking `mutex_` in this property would have introduced a **new self-deadlock**.
   `FinishPhase()` invokes the post-phase action while it still holds `mutex_`, so a post-phase
   action that reads `ParticipantCount` — a legal thing for it to do — would block on the lock
   its own caller holds. That is precisely **SR-AUD-210**, which
   `getCurrentPhaseNumberProperty()` already suffers and which approval-gated ticket #1957
   exists to remove. A repair for SR-AUD-212 that manufactured a second instance of SR-AUD-210
   would be a regression wearing a fix's clothing.

`ThreadingSharedStateTests.Barrier_ParticipantCountReadableFromPostPhaseAction` pins that the
property is still callable from inside the action. It would hang rather than fail if that
regressed, which for a deadlock is the only signal available.

`sizeof(Barrier)` 160 → 160, `alignof` 8 → 8. Scenario `barrier.participantcount` reported one
race before and none after.

**SR-AUD-210 is untouched and remains `confirmed`** — the post-phase lock discipline is cause
T-E/2, approval-gated ticket #1957.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
