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
